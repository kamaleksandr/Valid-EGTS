/*
 * File:   egts_subrecords.cpp
 * Author: kamyshev.a
 *
 * Created on 1 August 2018, 14:17
 */

#include <math.h>
#include <memory>
#include "egts_subrecord.h"
#include "../zlib/my_zlib.h"

using namespace EGTS;

static uint8_t bit_cnt( uint8_t n )
{
  n = ( ( n >> 1 ) & 0x55 ) + ( n & 0x55 );
  n = ( ( n >> 2 ) & 0x33 ) + ( n & 0x33 );
  n = ( ( n >> 4 ) & 0x0F ) + ( n & 0x0F );
  return n;
}

static std::unique_ptr<char> SafeReadCString( const char *data, uint16_t len )
{
  if ( !len ) return 0;
  std::unique_ptr<char> buf( new char[len + 1] );
  memcpy( buf.get( ), data, len );
  buf.get( )[len] = 0;
  return buf;
}

const char* TSubrecord::SubrecordData( const char *data, uint16_t *size )
{
  if ( *size <= EGTS_SBR_HDR_SIZE ) return 0;
  subrec_header_t *header = (subrec_header_t*)data;
  if ( *size < header->len ) return 0;
  *size = header->len;
  return data + EGTS_SBR_HDR_SIZE;
}

char* TSubrecord::PrepareGetData( uint16_t *size )
{
  char* data = new char[*size + EGTS_SBR_HDR_SIZE];
  subrec_header_t *header = (subrec_header_t*)data;
  header->type = type; //srt
  header->len = *size; // srl
  *size += EGTS_SBR_HDR_SIZE;
  return data;
}

uint8_t TECallRawMSD::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  fm = data[0];
  msd.size = size - 1;
  if ( msd.size )
  {
    msd.data.reset( new char[ msd.size ] );
    memcpy( msd.data.get( ), data + 1, msd.size );
  }
  else msd.data.reset( );
  if ( ppos ) *ppos = size;
  return 1;
}

std::unique_ptr<char> TECallRawMSD::GetData( uint16_t *size )
{
  *size = msd.size + 1;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  data[0] = fm;
  if ( msd.size ) memcpy( data + 1, msd.data.get( ), msd.size );
  return ptr;
}

TECallMSD::TECallMSD( )
{
  type = EGTS_SR_MSD_DATA;
  bf.latd1 = 0x7fff;
  bf.lond1 = 0x7fff;
  bf.latd2 = 0x7fff;
  bf.lond2 = 0x7fff;
  bf.nop = 0xff;
  memset( &head, 0, sizeof( head ) );
  additional.data.reset( );
}

uint8_t TECallMSD::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < sizeof( head ) ) return 0;
  uint16_t pos = sizeof( head );
  memcpy( &head, data, pos );
  uint16_t len = size - pos;
  if ( ppos ) *ppos = pos;
  if ( !len ) return 1;
  if ( len > 9 ) len = 9;
  memcpy( &bf, data + pos, len );
  pos += len;
  additional.size = size - pos;
  if ( additional.size )
  {
    if ( additional.size > 56 ) additional.size = 56;
    additional.data.reset( new char[additional.size] );
    memcpy( additional.data.get( ), data + pos, additional.size );
    pos += additional.size;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TECallMSD::GetData( uint16_t *size )
{
  uint8_t opt_size = 0;
  if ( bf.nop != 0xff ) opt_size = 9;
  else if ( bf.lond2 != 0x7fff ) opt_size = 8;
  else if ( bf.latd2 != 0x7fff ) opt_size = 6;
  else if ( bf.lond1 != 0x7fff ) opt_size = 4;
  else if ( bf.latd1 != 0x7fff ) opt_size = 2;
  if ( additional.size ) opt_size = 9;
  uint16_t pos = sizeof( head );
  *size = pos + opt_size + additional.size;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, pos );
  if ( opt_size )
  {
    memcpy( data + pos, &bf, opt_size );
    pos += opt_size;
  }
  if ( additional.size )
    memcpy( data + pos, additional.data.get( ), additional.size );
  return ptr;
}

TEPCompData::TEPCompData( )
{
  memset( &head, 0, sizeof(head ) );
  type = EGTS_SR_EP_COMP_DATA;
}

void TEPCompData::SetBlockNumber( uint16_t val )
{
  head.bnl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf.bnh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPCompData::GetBlockNumber( )
{
  uint16_t val = head.bf.bnh;
  val = ( val << 8 ) + head.bnl;
  return val;
}

uint8_t TEPCompData::Compress( TSubrecord* sbr )
{
  head.srt = sbr->GetType( );
  std::unique_ptr<char> ptr = sbr->GetData( &head.cdl );
  if ( !ptr ) return 0;
  char *data = ( ptr.get( ) + 3 );
  uint32_t size = head.cdl - 3;
  cd.reset( (char*)my_zlib_deflate( (uint8_t*)data, &size ) );
  head.cdl = size;
  return cd != 0;
}

std::unique_ptr<TSubrecord> TEPCompData::Extract( )
{
  std::unique_ptr<TSubrecord> sbr;
  if ( !cd ) return sbr;
  if ( head.srt == EGTS_SR_EP_TRACK_DATA ) sbr.reset( new TEPTrackData( ) );
  else if ( head.srt == EGTS_SR_EP_ACCEL_DATA3 ) sbr.reset( new TEPAccelData3( ) );
  else return sbr;
  uint32_t size = head.cdl;
  std::unique_ptr<char> ptr( (char*)my_zlib_inflate( (uint8_t*)cd.get( ), &size ) );
  if ( ptr )
    if ( sbr->SetSRD( (const char*)ptr.get( ), size ) )
      return sbr;
  return std::unique_ptr<TSubrecord>( );
}

uint8_t TEPCompData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  uint16_t pos = sizeof( head );
  if ( size < pos ) return 0;
  memcpy( &head, data, pos );
  if ( head.cdl + pos > size ) return 0;
  cd.reset( new char[head.cdl] );
  memcpy( cd.get( ), data + pos, head.cdl );
  pos += head.cdl;
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TEPCompData::GetData( uint16_t *size )
{
  *size = sizeof( head ) + head.cdl;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, sizeof( head ) );
  if ( head.cdl && cd.get( ) )
    memcpy( data + sizeof( head ), cd.get( ), head.cdl );
  return ptr;
}

void TEPSignData::SetBlockNumber( uint16_t val )
{
  head.bnl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf.bnh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPSignData::GetBlockNumber( )
{
  uint16_t val = head.bf.bnh;
  val = ( val << 8 ) + head.bnl;
  return val;
}

void TEPSignData::SetSignLength( uint16_t val )
{
  head.slnl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf.slnh = ( val & 0x3f00 ) >> 8; // 0011 1111 0000 0000
}

uint16_t TEPSignData::GetSignLength( )
{
  uint16_t val = head.bf.slnh;
  val = ( val << 8 ) + head.slnl;
  return val;
}

TEPSignature::TEPSignature( )
{
  ver = 1;
  sa = 0;
  type = EGTS_SR_EP_SIGNATURE;
}

uint8_t TEPSignature::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t sd_head_size = sizeof( TEPSignData::sign_data_head_t );
  if ( 2 > size ) return 0;
  ver = data[0];
  sa = data[1];
  uint16_t pos = 2, len;
  for ( int i = 0; i < sa; i++ )
  {
    if ( pos + sd_head_size > size ) return 0;
    std::unique_ptr< TEPSignData > sd( new TEPSignData );
    memcpy( &sd->head, data + pos, sd_head_size );
    pos += sd_head_size;
    len = sd->GetSignLength( );
    if ( !len ) continue;
    if ( pos + len > size ) return 0;
    sd->sd.reset( new char[len] );
    memcpy( sd->sd.get( ), data + pos, len );
    pos += len;
    sd_list.Add( sd.release( ) );
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TEPSignature::GetData( uint16_t *size )
{
  const uint16_t sd_head_size = sizeof( TEPSignData::sign_data_head_t );
  *size = 2;
  sa = sd_list.Count( );
  TEPSignData *sd = sd_list.First( );
  for (; sd; sd = sd_list.Next( ) )
    *size += sd_head_size + sd->GetSignLength( );
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  data[0] = ver;
  data[1] = sa;
  uint16_t pos = 2, len;
  for ( sd = sd_list.First( ); sd; sd = sd_list.Next( ) )
  {
    memcpy( data + pos, &sd->head, sd_head_size );
    pos += sd_head_size;
    len = sd->GetSignLength( );
    if ( !len ) continue;
    memcpy( data + pos, sd->sd.get( ), len );
    pos += len;
  }
  return ptr;
}

TEPAccelData3::TEPAccelData3( )
{
  memset( &head, 0, sizeof( head ) );
  head.bnl = 1;
  head.bf2.mu = 1;
  type = EGTS_SR_EP_ACCEL_DATA3;
}

void TEPAccelData3::SetBlockNumber( uint16_t val )
{
  head.bnl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf1.bnh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPAccelData3::GetBlockNumber( )
{
  uint16_t val = head.bf1.bnh;
  val = ( val << 8 ) + head.bnl;
  return val;
}

void TEPAccelData3::SetTime( unsigned int val )
{
  if ( val < 1262304000 ) head.at = 0;
  else head.at = val - 1262304000;
}

void TEPAccelData3::SetMSec( uint16_t val )
{
  head.atmsl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf1.atmsh = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TEPAccelData3::GetMSec( )
{
  uint16_t val = head.bf1.atmsh;
  return( val << 8 ) +head.atmsl;
}

void TEPAccelData3::SetRSA( uint16_t val )
{
  head.rsal = val & 0x00ff; // 0000 0000 1111 1111
  head.bf2.rsah = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TEPAccelData3::GetRSA( )
{
  uint16_t val = head.bf2.rsah;
  return( val << 8 ) +head.rsal;
}

uint8_t TEPAccelData3::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t ad_head_size = sizeof( accel_data_head_t );
  const uint16_t ad_size = sizeof( accel_data_struct_t );
  if ( head_size > size ) return 0;
  memcpy( &head, data, head_size );
  uint16_t pos = head_size, rsa = GetRSA( );
  for ( int i = 0; i < rsa; i++ )
  {
    if ( pos + ad_head_size > size ) return 0;

    accel_data_t *ad = ad_list.New( );

    memcpy( &ad->head, data + pos, ad_head_size );
    pos += ad_head_size;
    if ( ad->head.rst ) continue;
    if ( pos + ad_size > size ) return 0;
    memcpy( &ad->ads, data + pos, ad_size );
    pos += ad_size;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TEPAccelData3::GetData( uint16_t *size )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t ad_head_size = sizeof( accel_data_head_t );
  const uint16_t ad_size = sizeof( accel_data_struct_t );
  *size = head_size;
  SetRSA( ad_list.Count( ) );
  for ( accel_data_t *ad = ad_list.First( ); ad; ad = ad_list.Next( ) )
  {
    *size += ad_head_size;
    if ( !ad->head.rst ) *size += ad_size;
  }
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, head_size );
  uint16_t pos = head_size;
  for ( accel_data_t *ad = ad_list.First( ); ad; ad = ad_list.Next( ) )
  {
    memcpy( data + pos, &ad->head, ad_head_size );
    pos += ad_head_size;
    if ( ad->head.rst ) continue;
    memcpy( data + pos, &ad->ads, ad_size );
    pos += ad_size;
  }
  return ptr;
}

TEPRelativeTrack::TEPRelativeTrack( )
{
  memset( &head0, 0, sizeof( head0 ) );
  memset( &body, 0, sizeof( body ) );
  memset( &alt, 0, sizeof( alt ) );
  spdl = 0;
}

void TEPRelativeTrack::SetLonDelta( double val )
{
  body.bf2.lons = val > 0 ? 1 : 0;
  uint16_t lond = fabs( val ) / 180.0 * 0xffffffff;
  body.londl = lond & 0x00ff; // 0000 0000 1111 1111
  body.bf2.londh = ( lond & 0x3f00 ) >> 8; // 0011 1111 0000 0000
}

double TEPRelativeTrack::GetLonDelta( )
{
  uint16_t lond = body.bf2.londh;
  lond = ( lond << 8 ) + body.londl;
  if ( !body.bf2.lons ) lond *= -1;
  return(double)lond / 0xffffffff * 180;
}

void TEPRelativeTrack::SetLatDelta( double val )
{
  body.bf1.lats = val > 0 ? 1 : 0;
  uint16_t latd = fabs( val ) / 90.0 * 0xffffffff;
  body.latdl = latd & 0x00ff; // 0000 0000 1111 1111
  body.bf1.latdh = ( latd & 0x3f00 ) >> 8; // 0011 1111 0000 0000
}

double TEPRelativeTrack::GetLatDelta( )
{
  uint16_t latd = body.bf1.latdh;
  latd = ( latd << 8 ) + body.latdl;
  if ( !body.bf1.lats ) latd *= -1;
  return(double)latd / 0xffffffff * 90;
}

void TEPRelativeTrack::SetAltDelta( short val )
{
  alt.alts = val > 0 ? 1 : 0;
  alt.altd = abs( val );
}

short TEPRelativeTrack::GetAltDelta( )
{
  if ( !alt.alts ) return -alt.altd;
  return alt.altd;
}

void TEPRelativeTrack::SetSpeedDelta( uint16_t val )
{
  val *= 10;
  spdl = val & 0x00ff; // 0000 0000 1111 1111
  head1.spdh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPRelativeTrack::GetSpeedDelta( )
{
  uint16_t val = head1.spdh;
  val = ( val << 8 ) + spdl;
  return round( val / 10.0 );
}

void TEPRelativeTrack::SetCourse( uint16_t val )
{
  body.dirl = val & 0x00ff; // 0000 0000 1111 1111
  body.bf1.dirh = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TEPRelativeTrack::GetCourse( )
{
  uint16_t val = body.bf1.dirh;
  return( val << 8 ) +body.dirl;
}

TEPTrackData::TEPTrackData( )
{
  memset( &head, 0, sizeof( head ) );
  memset( &tds, 0, sizeof( tds ) );
  head.bnl = 1;
  type = EGTS_SR_EP_TRACK_DATA;
}

void TEPTrackData::SetBlockNumber( uint16_t val )
{
  head.bnl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf1.bnh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPTrackData::GetBlockNumber( )
{
  uint16_t val = head.bf1.bnh;
  val = ( val << 8 ) + head.bnl;
  return val;
}

void TEPTrackData::SetTime( unsigned int value )
{
  if ( value < 1262304000 ) head.at = 0;
  else head.at = value - 1262304000;
}

uint8_t TEPTrackData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t tds_size = sizeof( tds );
  const uint16_t rt_body_size = sizeof( TEPRelativeTrack::rt_body_t );
  if ( head_size > size ) return 0;
  memcpy( &head, data, head_size );
  uint16_t pos = head_size;
  if ( head.bf2.tde )
  {
    if ( pos + tds_size > size ) return 0;
    memcpy( &tds, data + pos, tds_size );
    pos += tds_size;
  }
  for ( int i = 0; i < head.rsa; i++ )
  {
    if ( pos + 1 > size ) return 0;
    TEPRelativeTrack *rt = rt_list.New( );
    memcpy( &rt->head0, data + pos, 1 );
    pos += 1;
    if ( !rt->head0.rst ) continue;
    if ( pos + rt_body_size > size ) return 0;
    memcpy( &rt->body, data + pos, rt_body_size );
    pos += rt_body_size;
    if ( rt->head1.spde )
    {
      if ( pos + 1 > size ) return 0;
      memcpy( &rt->spdl, data + pos, 1 );
      pos += 1;
    }
    if ( rt->body.bf2.alte )
    {
      if ( pos + 1 > size ) return 0;
      memcpy( &rt->alt, data + pos, 1 );
      pos += 1;
    }
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TEPTrackData::GetData( uint16_t *size )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t tds_size = sizeof( tds );
  const uint16_t rt_body_size = sizeof( TEPRelativeTrack::rt_body_t );
  *size = head_size;
  if ( head.bf2.tde ) *size += tds_size;
  head.rsa = rt_list.Count( );
  TEPRelativeTrack *rt = rt_list.First( );
  for (; rt; rt = rt_list.Next( ) )
  {
    *size += 1; // rt_head_size
    if ( !rt->head0.rst ) continue;
    *size += rt_body_size;
    if ( rt->head1.spde ) *size += 1;
    if ( rt->body.bf2.alte ) *size += 1;
  }
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, head_size );
  uint16_t pos = head_size;
  if ( head.bf2.tde )
  {
    memcpy( data + pos, &tds, tds_size );
    pos += tds_size;
  }
  for ( rt = rt_list.First( ); rt; rt = rt_list.Next( ) )
  {
    memcpy( data + pos, &rt->head0, 1 );
    pos += 1;
    if ( !rt->head0.rst ) continue;
    memcpy( data + pos, &rt->body, rt_body_size );
    pos += rt_body_size;
    if ( rt->head1.spde )
    {
      memcpy( data + pos, &rt->spdl, 1 );
      pos += 1;
    }
    if ( rt->body.bf2.alte )
    {
      memcpy( data + pos, &rt->alt, 1 );
      pos += 1;
    }
  }
  return ptr;
}

void TEPTrackData::SetAltitude( int16_t val )
{
  tds.bf.alts = val < 0 ? 1 : 0;
  val = abs( val );
  tds.altl = val & 0x00ff; // 0000 0000 1111 1111
  tds.bf.alth = ( val & 0x3f00 ) >> 8; // 0011 1111 0000 0000
}

short TEPTrackData::GetAltitude( )
{
  short val = tds.bf.alth;
  val = ( val << 8 ) + tds.altl;
  if ( tds.bf.alts ) return -val;
  return val;
}

void TEPTrackData::SetSpeed( uint16_t val )
{
  val *= 10;
  tds.spdl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf2.spdh = ( val & 0x1f00 ) >> 8; // 0001 1111 0000 0000
}

uint16_t TEPTrackData::GetSpeed( )
{
  uint16_t val = head.bf2.spdh;
  val = ( val << 8 ) + tds.spdl;
  return round( val / 10.0 );
}

void TEPTrackData::SetCourse( uint16_t val )
{
  tds.dirl = val & 0x00ff; // 0000 0000 1111 1111
  tds.bf.dirh = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TEPTrackData::GetCourse( )
{
  uint16_t val = tds.bf.dirh;
  return( val << 8 ) +tds.dirl;
}

TEPMainData::TEPMainData( )
{
  memset( &head, 0, sizeof( head ) );
  head.fv = 1;
  head.bnl = 1;
  head.cn.clt = 1; // test call
  type = EGTS_SR_EP_MAIN_DATA;
}

void TEPMainData::SetBlockNumber( uint16_t val )
{
  head.bnl = val & 0x00ff; // 0000 0000 1111 1111
  head.cn.bnh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPMainData::GetBlockNumber( )
{
  uint16_t val = head.cn.bnh;
  val = ( val << 8 ) + head.bnl;
  return val;
}

void TEPMainData::SetMSec( uint16_t val )
{
  head.msl = val & 0x00ff; // 0000 0000 1111 1111
  head.bf.msh = ( val & 0x0300 ) >> 8; // 0000 0011 0000 0000
}

uint16_t TEPMainData::GetMSec( )
{
  uint16_t val = head.bf.msh;
  val = ( val << 8 ) + head.msl;
  return val;
}

void TEPMainData::SetTime( uint32_t value )
{
  if ( value < 1262304000 ) head.ts = 0;
  else head.ts = value - 1262304000;
}

void TEPMainData::SetSpeed( uint16_t val )
{
  val *= 10;
  location.spdl = val & 0x00ff; // 0000 0000 1111 1111
  location.bf.spdh = ( val & 0x1f00 ) >> 8; // 0001 1111 0000 0000
}

uint16_t TEPMainData::GetSpeed( )
{
  uint16_t val = location.bf.spdh;
  val = ( val << 8 ) + location.spdl;
  return round( val / 10.0 );
}

void TEPMainData::SetCourse( uint16_t val )
{
  location.dirl = val & 0x00ff; // 0000 0000 1111 1111
  location.bf.dirh = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TEPMainData::GetCourse( )
{
  uint16_t val = location.bf.dirh;
  return( val << 8 ) +location.dirl;
}

uint8_t TEPMainData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t loc_size = sizeof( location );
  const uint16_t bs_size = sizeof( base_station_t );
  if ( head_size > size ) return 0;
  memcpy( &head, data, head_size );
  uint16_t pos = head_size;
  if ( head.cn.locp )
  {
    if ( pos + loc_size > size ) return 0;
    memcpy( &location, data + pos, loc_size );
    pos += loc_size;
  }
  if ( head.cn.lbsn )
  {
    uint16_t sts_size = head.cn.lbsn * bs_size;
    if ( pos + sts_size > size ) return 0;
    for ( int i = 0; i < head.cn.lbsn; i++ )
    {
      base_station_t *bs = stations.New( );
      memcpy( bs, data + pos, bs_size );
      pos += bs_size;
    }
  }
  if ( head.cn.evp )
  {
    if ( pos + 1 > size ) return 0;
    epevc = *( data + pos );
    pos += 1;
    if ( pos + epevc * 4 > size ) return 0;
    for ( int i = 0; i < epevc; i++ )
    {
      uint32_t id = *( data + pos );
      events.push_back( id );
      pos += 4;
    }
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TEPMainData::GetData( uint16_t *size )
{
  const uint16_t head_size = sizeof( head );
  const uint16_t loc_size = sizeof( location );
  const uint16_t bs_size = sizeof( base_station_t );
  *size = head_size;
  if ( head.cn.locp ) *size += loc_size;
  head.cn.lbsn = stations.Count( );
  if ( head.cn.lbsn ) *size += head.cn.lbsn * bs_size;
  epevc = events.size( );
  head.cn.evp = epevc > 0;
  if ( head.cn.evp ) *size += 1 + epevc * 4;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, head_size );
  uint32_t pos = head_size;
  if ( head.cn.locp )
  {
    memcpy( data + pos, &location, loc_size );
    pos += loc_size;
  }
  for ( base_station_t *bs = stations.First( ); bs; bs = stations.Next( ) )
  {
    memcpy( data + pos, bs, bs_size );
    pos += bs_size;
  }
  if ( head.cn.evp )
  {
    memcpy( ( data + pos ), &epevc, 1 );
    pos += 1;
    for ( int i = 0; i < epevc; i++ )
    {
      memcpy( data + pos, &events[i], 4 );
      pos += 4;
    }
  }
  return ptr;
}

TRecResp::TRecResp( )
{
  type = EGTS_SR_RECORD_RESPONSE;
  crn = 0;
  rst = 0;
}

uint8_t TRecResp::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 3 ) return 0;
  crn = *(uint16_t*)data;
  rst = data[2];
  if ( ppos ) *ppos = 3;
  return 1;
}

std::unique_ptr<char> TRecResp::GetData( uint16_t *size )
{
  *size = 3;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &crn, 2 );
  data[2] = rst;
  return ptr;
}

TTermIdent::TTermIdent( )
{
  type = EGTS_SR_TERM_IDENTITY;
  memset( &head, 0, sizeof( head ) );
  head.flags.ssra = 1;
  hdid = 0;
  memcpy( imei, "000000000000000\0", 16 );
  memcpy( imsi, "0000000000000000\0", 17 );
  memcpy( lngc, "rus", 3 );
  nid.mcc = 0;
  nid.mnc = 0;
  bs = EGTS_TERM_IN_BUF_SIZE;
  memcpy( msisdn, "000000000000000\0", 16 );
}

uint8_t TTermIdent::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 5 ) return 0;
  memcpy( &head, data, 5 );
  uint16_t need_size = GetSize( );
  if ( size < need_size ) return 0;
  uint16_t pos = 5;
  if ( head.flags.hdide )
  {
    memcpy( &hdid, data + pos, 2 );
    pos += 2;
  }
  if ( head.flags.imeie )
  {
    memcpy( imei, data + pos, 15 );
    pos += 15;
  }
  if ( head.flags.imsie )
  {
    memcpy( imsi, data + pos, 16 );
    pos += 16;
  }
  if ( head.flags.lngce )
  {
    memcpy( lngc, data + pos, 3 );
    pos += 3;
  }
  if ( head.flags.nide )
  {
    memcpy( &nid, data + pos, 3 );
    pos += 3;
  }
  if ( head.flags.bse )
  {
    memcpy( &bs, data + pos, 2 );
    pos += 2;
  }
  if ( head.flags.mne )
  {
    memcpy( (char*)( data + pos ), msisdn, 15 );
    pos += 15;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

uint16_t TTermIdent::GetSize( )
{
  return 5 + 2 * head.flags.hdide + 15 * head.flags.imeie + 16 *
      head.flags.imsie + 3 * head.flags.lngce + 3 * head.flags.nide + 2 *
      head.flags.bse + 15 * head.flags.mne;
}

std::unique_ptr<char> TTermIdent::GetData( uint16_t *size )
{
  *size = GetSize( );
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char* data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  GetData( data );
  return ptr;
}

void TTermIdent::GetData( char* data )
{
  uint16_t pos = sizeof( head );
  memcpy( data, &head, pos );
  if ( head.flags.hdide )
  {
    memcpy( data + pos, &hdid, 2 );
    pos += 2;
  }
  if ( head.flags.imeie )
  {
    memcpy( data + pos, imei, 15 );
    pos += 15;
  }
  if ( head.flags.imsie )
  {
    memcpy( data + pos, imsi, 16 );
    pos += 16;
  }
  if ( head.flags.lngce )
  {
    memcpy( data + pos, lngc, 3 );
    pos += 3;
  }
  if ( head.flags.nide )
  {
    memcpy( data + pos, &nid, 3 );
    pos += 3;
  }
  if ( head.flags.bse )
  {
    memcpy( data + pos, &bs, 2 );
    pos += 2;
  }
  if ( head.flags.mne )
    memcpy( data + pos, msisdn, 15 );
}

TTermIdent2::TTermIdent2( )
{
  type = EGTS_SR_TERM_IDENTITY2;
  memcpy( &flags, "0", 1 );
  memcpy( iccid, "0000000000000000000\0", 20 );
}

uint8_t TTermIdent2::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  uint16_t pos = 0;
  if ( !TTermIdent::SetSRD( data, size, &pos ) ) return 0;
  size -= pos;
  if ( size ) memcpy( &flags, data + pos, 1 );
  if ( ppos ) *ppos = pos + 1;
  if ( !flags.icce ) return 1;
  if ( size >= 20 ) memcpy( iccid, data + pos + 1, 19 );
  if ( ppos ) *ppos = pos + 20;
  return 1;
}

std::unique_ptr<char> TTermIdent2::GetData( uint16_t *size )
{
  uint16_t base_size = TTermIdent::GetSize( );
  *size = base_size + 1;
  if ( flags.icce ) *size += 19;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  TTermIdent::GetData( data );
  memcpy( data + base_size, &flags, 1 );
  if ( flags.icce ) memcpy( data + base_size + 1, &iccid, 19 );
  return ptr;
}

TDispIdent::TDispIdent( )
{
  type = EGTS_SR_DISPATCHER_IDENTITY;
  head.dt = 0;
  head.did = 0;
}

uint8_t TDispIdent::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 5 ) return 0;
  memcpy( &head, data, 5 );
  if ( ppos ) *ppos = 5;
  if ( size == 5 ) return 1;
  uint16_t len = size - 5;
  dscr = SafeReadCString( data + 5, len ).get( );
  if ( ppos ) *ppos = 5 + len;
  return 1;
}

std::unique_ptr<char> TDispIdent::GetData( uint16_t *size )
{
  uint8_t len = dscr.size( ) + 1;
  *size = 5 + len;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, 5 );
  if ( len )
  {
    memcpy( data + 5, dscr.c_str( ), len - 1 );
    data[4 + len] = 0; // 5 + len - 1
  }
  return ptr;
}

TAuthParams::TAuthParams( )
{
  type = EGTS_SR_AUTH_PARAMS;
  memset( &flags, 0, sizeof( flags ) );
}

uint8_t TAuthParams::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 1 ) return 0;
  memcpy( &flags, data, 1 );
  if ( ppos ) *ppos = 1;
  return 1;
}

std::unique_ptr<char> TAuthParams::GetData( uint16_t *size )
{
  *size = 1;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &flags, 1 );
  return ptr;
}

uint8_t TAuthInfo::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  user_name.clear( );
  user_pass.clear( );
  server_sequence.clear( );
  if ( size < 2 ) return 0;
  if ( ppos ) *ppos = size;
  user_name = SafeReadCString( data, size ).get( );
  uint16_t pos = user_name.size( ) + 1;
  if ( pos >= size ) return 0;
  user_pass = SafeReadCString( data + pos, size - pos ).get( );
  pos += user_pass.size( ) + 1;
  if ( pos >= size ) return 1;
  server_sequence = SafeReadCString( data + pos, size - pos ).get( );
  return 1;
}

std::unique_ptr<char> TAuthInfo::GetData( uint16_t *size )
{
  const uint16_t user_name_size = user_name.size( ) + 1;
  const uint16_t user_pass_size = user_pass.size( ) + 1;
  *size = user_name_size + user_pass_size;
  if ( server_sequence.size( ) ) *size += server_sequence.size( ) + 1;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, user_name.c_str( ), user_name_size );
  memcpy( data + user_name_size, user_pass.c_str( ), user_pass_size );
  if ( !server_sequence.size( ) ) return ptr;
  memcpy(
      data + user_name_size + user_pass_size,
      server_sequence.c_str( ),
      server_sequence.size( ) + 1 );
  return ptr;
}

TServiceInfo::TServiceInfo( )
{
  type = EGTS_SR_SERVICE_INFO;
  memset( &body, 0, sizeof( body ) );
}

uint8_t TServiceInfo::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t body_size = sizeof( body );
  if ( size < body_size ) return 0;
  memcpy( &body, data, body_size );
  if ( ppos ) *ppos = body_size;
  return 1;
}

std::unique_ptr<char> TServiceInfo::GetData( uint16_t *size )
{
  *size = sizeof( body );
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &body, *size );
  return ptr;
}

TVehicleData::TVehicleData( )
{
  type = EGTS_SR_VEHICLE_DATA;
  memset( &body, 0, sizeof(body ) );
}

uint8_t TVehicleData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  const uint16_t body_size = sizeof( body );
  if ( size < body_size ) return 0;
  memcpy( &body, data, body_size );
  if ( ppos ) *ppos = body_size;
  return 1;
}

std::unique_ptr<char> TVehicleData::GetData( uint16_t *size )
{
  *size = sizeof( body );
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &body, *size );
  return ptr;
}

uint8_t TPlusData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  protobuf.data.reset( new char[size] );
  memcpy( protobuf.data.get( ), data, size );
  protobuf.size = size;
  if ( ppos ) *ppos = size;
  return 1;
}

TInsAccel::TInsAccel( )
{
  type = EGTS_SR_INSURANCE_ACCEL_DATA;
  head.hver = 0x01;
  head.enc.format = 1; // 16 bit signed
  head.enc.zlib = 0; // not packed
  head.enc.tf = 2; // data converted
  head.enc.mode = 0;
  head.pid = 0;
  body.tnp = 0; // 1 packet
  body.rtm = 0;
  body.atm = 0;
  body.freq = 10; // 100 Hz
  body.xmod.adc_bits = 15;
  body.xmod.range = 328;
  body.ymod = body.xmod;
  body.zmod = body.xmod;
  dl = 0;
  memset( &x, 0, sizeof( calibration_data_t ) );
  memset( &y, 0, sizeof( calibration_data_t ) );
  memset( &z, 0, sizeof( calibration_data_t ) );
}

TSubrecord* TInsAccel::Copy( )
{
  TInsAccel *acc = new TInsAccel;
  acc->head = head;
  acc->body = body;
  acc->dl = dl;
  acc->x = x;
  acc->y = y;
  acc->z = z;
  acc->xyz_data.reset( new char[dl] );
  memcpy( acc->xyz_data.get( ), xyz_data.get( ), dl );
  return acc;
}

void TInsAccel::SetTime( uint32_t value )
{ // time in utc in seconds since 2010
  if ( value < 1262304000 ) body.atm = 0;
  else body.atm = value - 1262304000;
}

uint8_t TInsAccel::GetXsize( )
{
  uint8_t val = ( body.xmod.adc_bits + 8 ) / 8;
  if ( val > 4 ) return 4;
  return val;
}

uint8_t TInsAccel::GetYsize( )
{
  uint8_t val = ( body.ymod.adc_bits + 8 ) / 8;
  if ( val > 4 ) return 4;
  return val;
}

uint8_t TInsAccel::GetZsize( )
{
  uint8_t val = ( body.zmod.adc_bits + 8 ) / 8;
  if ( val > 4 ) return 4;
  return val;
}

uint8_t TInsAccel::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 4 ) return 0;
  memcpy( &head, data, 4 );
  uint16_t pos = 4;
  if ( !head.pid )
  {
    if ( size < 20 ) return 0;
    memcpy( &body, data + 4, 16 );
    pos = 20;
  }
  if ( size < pos + 2 ) return 0;
  memcpy( &dl, data + pos, 2 );
  pos += 2;
  if ( !head.pid && !head.enc.tf )
  {
    x.size = GetXsize( );
    y.size = GetYsize( );
    z.size = GetZsize( );
    if ( size < pos + x.size + y.size + z.size ) return 0;
    memcpy( x.cdata, data + pos, x.size );
    memcpy( y.cdata, data + pos + x.size, y.size );
    memcpy( z.cdata, data + pos + x.size + y.size, z.size );
    pos += x.size + y.size + z.size;
  }
  if ( dl )
  {
    if ( size < pos + dl ) return 0;
    xyz_data.reset( new char[dl] );
    memcpy( xyz_data.get( ), data + pos, dl );
    pos += dl;
  }
  if ( ppos ) *ppos = size;
  return 1;
}

std::unique_ptr<char> TInsAccel::GetData( uint16_t *size )
{
  *size = sizeof( head );
  if ( !head.pid )
  {
    *size += 16;
    x.size = GetXsize( );
    y.size = GetYsize( );
    z.size = GetZsize( );
    if ( !head.enc.tf ) *size += x.size + y.size + z.size;
  }
  *size += 2 + dl;
  *size = ( ( *size + 3 ) / 4 ) * 4; // alignment
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, 4 );
  uint16_t pos = 4;
  if ( !head.pid )
  {
    memcpy( data + 4, &body, 16 );
    memcpy( data + 20, &dl, 2 );
    pos = 22;
    if ( !head.enc.tf )
    {
      memcpy( data + pos, x.cdata, x.size );
      memcpy( data + pos + x.size, y.cdata, y.size );
      memcpy( data + pos + x.size + y.size, z.cdata, z.size );
      pos = pos + x.size + y.size + z.size;
    }
  }
  else
  {
    memcpy( data + 4, &dl, 2 );
    pos = 6;
  }
  if ( !dl || !xyz_data ) return ptr;
  memcpy( data + pos, xyz_data.get( ), dl );
  return ptr;
}

std::unique_ptr<char> TPlusData::GetData( uint16_t *size )
{
  *size = protobuf.size;
  if ( !protobuf.size ) return 0;
  if ( !protobuf.data.get( ) ) return 0;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, protobuf.data.get( ), protobuf.size );
  return ptr;
}

TPosData::TPosData( )
{
  type = EGTS_SR_POS_DATA;
  memset( &body, 0, sizeof( body ) );
  body.flags.vld = 1;
  memset( alt, 0, 3 );
  srcd = 0;
}

void TPosData::SetTime( uint32_t value )
{
  if ( value < 1262304000 ) body.ntm = 0;
  else body.ntm = value - 1262304000;
}

void TPosData::SetSpeed( uint16_t val )
{
  val *= 10;
  body.spd = val & 0x00ff; // 0000 0000 1111 1111
  body.bf.spd = ( val & 0x3f00 ) >> 8; // 0011 1111 0000 0000
}

uint16_t TPosData::GetSpeed( )
{
  uint16_t val = body.bf.spd;
  val = ( val << 8 ) + body.spd;
  return round( val / 10.0 );
}

void TPosData::SetCourse( uint16_t val )
{
  body.dir = val & 0x00ff; // 0000 0000 1111 1111
  body.bf.dirh = ( val & 0x0100 ) >> 8; // 0000 0001 0000 0000
}

uint16_t TPosData::GetCourse( )
{
  uint16_t val = body.bf.dirh;
  return( val << 8 ) +body.dir;
}

void TPosData::SetOdometer( uint32_t val )
{
  val = round( val / 100.0 );
  memcpy( body.odm, &val, 3 );
}

uint32_t TPosData::GetOdometer( )
{
  uint32_t val = 0;
  memcpy( &val, body.odm, 3 );
  return val * 100;
}

void TPosData::SetAltitude( int val )
{
  body.flags.alte = 1;
  body.bf.alts = ( val < 0 );
  memcpy( alt, &val, 3 );
}

int TPosData::GetAltitude( )
{
  int val = 0;
  memcpy( &val, alt, 3 );
  if ( body.bf.alts ) return -val;
  else return val;
}

uint8_t TPosData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  uint16_t body_size = sizeof( body );
  if ( size < body_size ) return 0;
  memcpy( &body, data, body_size );
  uint16_t pos = body_size;
  if ( body.flags.alte )
    if ( size >= pos + 3 )
    {
      memcpy( alt, data + pos, 3 );
      pos += 3;
    }
  if ( size >= pos + 2 ) memcpy( &srcd, data + pos, 2 );
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TPosData::GetData( uint16_t *size )
{
  *size = 0;
  uint16_t body_size = sizeof( body );
  *size = body_size + 3 * body.flags.alte + 2 * body.src;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &body, body_size );
  uint16_t pos = body_size;
  if ( body.flags.alte )
  {
    memcpy( data + pos, alt, 3 );
    pos += 3;
  }
  if ( body.src ) memcpy( data + pos, &srcd, 2 );
  return ptr;
}

TExtPosData::TExtPosData( )
{
  type = EGTS_SR_EXT_POS_DATA;
  uint8_t val = 0;
  memcpy( &flags, &val, 1 );
  vdop = 0;
  hdop = 0;
  pdop = 0;
  sat = 0;
  ns = 0;
}

uint8_t TExtPosData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 1 ) return 0;
  memcpy( &flags, data, 1 );
  uint16_t need_size = 1 + 2 * flags.vfe + 2 * flags.hfe + 2 * flags.pfe + 1 * flags.sfe + 2 * flags.nsfe;
  if ( size < need_size ) return 0;
  uint16_t pos = 1;
  if ( flags.vfe )
  {
    memcpy( &vdop, data + pos, 2 );
    pos += 2;
  }
  if ( flags.hfe )
  {
    memcpy( &hdop, data + pos, 2 );
    pos += 2;
  }
  if ( flags.pfe )
  {
    memcpy( &pdop, data + pos, 2 );
    pos += 2;
  }
  if ( flags.sfe )
  {
    memcpy( &sat, data + pos, 1 );
    pos += 1;
  }
  if ( flags.nsfe )
  {
    memcpy( &ns, data + pos, 2 );
    pos += 2;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TExtPosData::GetData( uint16_t *size )
{
  if ( flags.sfe ) flags.nsfe = 1;
  *size = 1 + 2 * flags.vfe + 2 * flags.hfe + 2 * flags.pfe + 1 * flags.sfe + 2 * flags.nsfe;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &flags, 1 );
  uint16_t pos = 1;
  if ( flags.vfe )
  {
    memcpy( data + pos, &vdop, 2 );
    pos += 2;
  }
  if ( flags.hfe )
  {
    memcpy( data + pos, &hdop, 2 );
    pos += 2;
  }
  if ( flags.pfe )
  {
    memcpy( data + pos, &pdop, 2 );
    pos += 2;
  }
  if ( flags.sfe )
  {
    memcpy( data + pos, &sat, 1 );
    pos += 1;
  }
  if ( flags.nsfe )
  {
    memcpy( data + pos, &ns, 2 );
    pos += 2;
  }
  return ptr;
}

uint8_t TResultCode::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 1 ) return 0;
  if ( ppos ) *ppos = 1;
  rcd = data[0];
  return 1;
}

std::unique_ptr<char> TResultCode::GetData( uint16_t *size )
{
  *size = 1;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  data[0] = rcd;
  return ptr;
}

TAdSensors::TAdSensors( ) // EGTS_SR_AD_SENSORS_DATA
{
  type = EGTS_SR_AD_SENSORS_DATA;
  memset( &dins, 0, 8 );
  memset( &ains, 0, 24 );
  memset( &head, 0, 3 );
}

void TAdSensors::SetDin( uint8_t num, uint8_t val )
{
  if ( num > 7 ) return;
  dins[num] = val;
  head.dinfe.val |= ( 1 << num );
}

uint8_t TAdSensors::GetDin( uint8_t num )
{
  if ( num > 7 ) return 0;
  return dins[num];
}

void TAdSensors::SetAin( uint8_t num, uint32_t val )
{
  if ( num > 7 ) return;
  memcpy( ains + num * 3, &val, 3 );
  head.ainfe.val |= ( 1 << num );
}

uint32_t TAdSensors::GetAin( uint8_t num )
{
  if ( num > 7 ) return 0;
  uint32_t val = 0;
  memcpy( &val, ains + num * 3, 3 );
  return val;
}

uint8_t TAdSensors::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 3 ) return 0;
  memcpy( &head, data, 3 );
  uint16_t need_size = 3 + bit_cnt( head.dinfe.val ) + bit_cnt( head.ainfe.val ) * 3;
  if ( size < need_size ) return 0;
  uint16_t pos = 3;
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( head.dinfe.val & ( 1 << i ) ) ) continue;
    dins[i] = data[pos++];
  }
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( head.ainfe.val & ( 1 << i ) ) ) continue;
    memcpy( ains + 3 * i, data + pos, 3 );
    pos += 3;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TAdSensors::GetData( uint16_t *size )
{
  *size = 3 + bit_cnt( head.dinfe.val ) + bit_cnt( head.ainfe.val ) * 3;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, 3 );
  uint16_t pos = 3;
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( head.dinfe.val & ( 1 << i ) ) ) continue;
    data[pos++] = dins[i];
  }
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( head.ainfe.val & ( 1 << i ) ) ) continue;
    memcpy( data + pos, ains + 3 * i, 3 );
    pos += 3;
  }
  return ptr;
}

TCounters::TCounters( ) // EGTS_SR_COUNTERS_DATA
{
  type = EGTS_SR_COUNTERS_DATA;
  memset( cnts, 0, 24 );
  memset( &flags, 0, 1 );
}

void TCounters::SetCounter( uint8_t num, uint32_t val )
{
  if ( num > 7 ) return;
  memcpy( cnts + num * 3, &val, 3 );
  cntfe |= ( 1 << num );
}

uint32_t TCounters::GetCounter( uint8_t num )
{
  if ( num > 7 ) return 0;
  uint32_t val = 0;
  memcpy( &val, cnts + num * 3, 3 );
  return val;
}

uint8_t TCounters::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 1 ) return 0;
  memcpy( &cntfe, data, 1 );
  if ( size < 1 + bit_cnt( cntfe ) * 3 ) return 0;
  uint16_t pos = 1;
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( cntfe & ( 1 << i ) ) ) continue;
    memcpy( cnts + 3 * i, data + pos, 3 );
    pos += 3;
  }
  if ( ppos ) *ppos = pos;
  return 1;
}

std::unique_ptr<char> TCounters::GetData( uint16_t *size )
{
  *size = 1 + bit_cnt( cntfe ) * 3;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &flags, 1 );
  uint16_t pos = 1;
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( cntfe & ( 1 << i ) ) ) continue;
    memcpy( data + pos, cnts + 3 * i, 3 );
    pos += 3;
  }
  return ptr;
}

TStateData::TStateData( )
{
  type = EGTS_SR_STATE_DATA;
  memset( &body, 0, 5 );
}

uint8_t TStateData::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 5 ) return 0;
  if ( ppos ) *ppos = 5;
  memcpy( &body, data, 5 );
  return 1;
}

std::unique_ptr<char> TStateData::GetData( uint16_t *size )
{
  *size = 5;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &body, 5 );
  return ptr;
}

TLiquidLevel::TLiquidLevel( )
{
  type = EGTS_SR_LIQUID_LEVEL_SENSOR;
  memset( &head.flags, 0, 3 );
}

uint8_t TLiquidLevel::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 3 ) return 0;
  llcd.size = size - 3;
  if ( !llcd.size ) return 0;
  memcpy( &head, data, 3 );
  llcd.data.reset( new char[llcd.size] );
  memcpy( llcd.data.get( ), data + 3, llcd.size );
  if ( ppos ) *ppos = size;
  return 1;
}

std::unique_ptr<char> TLiquidLevel::GetData( uint16_t *size )
{
  if ( llcd.size > 512 ) return 0;
  if ( !llcd.data ) return 0;
  *size = 3 + llcd.size;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  memcpy( data, &head, 3 );
  memcpy( data + 3, llcd.data.get( ), llcd.size );
  return ptr;
}

TPassengers::TPassengers( )
{
  type = EGTS_SR_PASSENGERS_COUNTERS;
  memset( &head, 0, 5 );
}

uint8_t TPassengers::SetSRD( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( size < 5 ) return 0;
  if ( ppos ) *ppos = size;
  memcpy( &head, data, 5 );
  if ( size < 5 + bit_cnt( head.dpr.val ) * 2 ) return 0;
  uint16_t pos = 5;
  for ( uint8_t i = 0; i < 8; i++ )
  {
    if ( !( head.dpr.val & ( 1 << i ) ) ) continue;
    door_num_t *door = doors.New( );
    door->num = i;
    memcpy( &door->door, data + pos, 2 );
    pos += 2;
  }
  return 1;
}

std::unique_ptr<char> TPassengers::GetData( uint16_t *size )
{
  *size = 5 + doors.Count( ) * 2;
  std::unique_ptr<char> ptr( PrepareGetData( size ) );
  char *data = ( ptr.get( ) + EGTS_SBR_HDR_SIZE );
  uint16_t pos = 5;
  head.dpr.val = 0;
  for ( door_num_t *door = doors.First( ); door; door = doors.Next( ) )
  {
    head.dpr.val |= ( 1 << door->num );
    memcpy( data + pos, &door->door, 2 );
    pos += 2;
  }
  memcpy( data, &head, 5 );
  return ptr;
}

