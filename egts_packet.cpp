/*
 * File:   egts_packet.cpp
 * Author: kamyshev.a
 *
 * Created on 1 August 2018, 14:10
 */

#include "egts_packet.h"
#include "egts_crc.h"
#include "../crypto_gost/nv_crypto_gost_28147_89.h"

#include <sstream>

static std::string to_string( int32_t val )
{
  std::stringstream ss;
  ss << val;
  return ss.str( );
}

TEGTSRecord::TEGTSRecord( )
{
  memset( &head, 0, sizeof( head ) );
  head.rfl.ssod = 1;
  head.rfl.rpp = 3;
  oid = 0;
  evid = 0;
  tm = 0;
  sst = EGTS_AUTH_SERVICE;
  rst = EGTS_AUTH_SERVICE;
}

inline TEGTSSubrecord* TEGTSRecord::GetInheritor( uint8_t type )
{
  if ( type == EGTS_SR_RECORD_RESPONSE )
    return new TEGTSRecResp;
  else if ( rst == EGTS_ECALL_SERVICE )
    switch ( type )
    {
      case EGTS_SR_RAW_MSD_DATA: return new TEGTSECallRawMSD;
      case EGTS_SR_MSD_DATA: return new TEGTSECallMSD;
    }
  else if ( rst == EGTS_EUROPROTOCOL_SERVICE )
    switch ( type )
    {
      case EGTS_SR_EP_MAIN_DATA: return new TEGTSEPMainData;
      case EGTS_SR_EP_TRACK_DATA: return new TEGTSEPTrackData;
      case EGTS_SR_EP_COMP_DATA: return new TEGTSEPCompData;
      case EGTS_SR_EP_ACCEL_DATA3: return new TEGTSEPAccelData3;
      case EGTS_SR_EP_SIGNATURE: return new TEGTSEPSignature;
    }
  else if ( rst == EGTS_AUTH_SERVICE )
    switch ( type )
    {
      case EGTS_SR_TERM_IDENTITY: return new TEGTSTermIdent;
      case EGTS_SR_TERM_IDENTITY2: return new TEGTSTermIdent2;
      case EGTS_SR_DISPATCHER_IDENTITY: return new TEGTSDispIdent;
      case EGTS_SR_VEHICLE_DATA: return new TEGTSVehicleData;
      case EGTS_SR_RESULT_CODE: return new TEGTSResultCode;
      case EGTS_SR_AUTH_PARAMS: return new TEGTSAuthParams;
      case EGTS_SR_AUTH_INFO: return new TEGTSAuthInfo;

    }
  else if ( rst == EGTS_TELEDATA_SERVICE )
    switch ( type )
    {
      case EGTS_SR_EGTSPLUS_DATA: return new TEGTSPlusData;
      case EGTS_SR_POS_DATA: return new TEGTSPosData;
      case EGTS_SR_EXT_POS_DATA: return new TEGTSExtPosData;
      case EGTS_SR_AD_SENSORS_DATA: return new TEGTSAdSensors;
      case EGTS_SR_COUNTERS_DATA: return new TEGTSCounters;
      case EGTS_SR_STATE_DATA: return new TEGTSStateData;
      case EGTS_SR_LIQUID_LEVEL_SENSOR: return new TEGTSLiquidLevel;
      case EGTS_SR_PASSENGERS_COUNTERS: return new TEGTSPassengers;
    }
  else if ( rst == EGTS_INSURANCE_SERVICE )
    switch ( type )
    {
      case EGTS_SR_INSURANCE_ACCEL_DATA: return new TEGTSInsAccel;
    }
  return 0;
}

uint8_t TEGTSRecord::SetData( const char *data, uint16_t size, uint16_t *ppos )
{
  if ( ppos ) *ppos = 0;
  if ( size < 7 ) return 0;
  memcpy( &head, data, 5 );
  uint16_t pos = 5;
  if ( head.rfl.obfe )
  {
    if ( size < pos + 5 ) return 0;
    memcpy( &oid, data + pos, 4 );
    pos += 4;
  }
  if ( head.rfl.evfe )
  {
    if ( size < pos + 5 ) return 0;
    memcpy( &evid, data + pos, 4 );
    pos += 4;
  }
  if ( head.rfl.tmfe )
  {
    if ( size < pos + 5 ) return 0;
    memcpy( &tm, data + pos, 4 );
    pos += 4;
  }
  if ( pos + 1 >= size ) return 0;
  sst = data[pos];
  rst = data[pos + 1];
  pos += 2;
  if ( head.rl )
  {
    if ( pos >= size ) return 0;
    uint16_t offset, rl = 0, srl;
    const char *srd;
    TEGTSSubrecord *sbr;
    while ( pos + 3 < size )
    {
      egts_subrec_header_t *hdr = (egts_subrec_header_t*)( data + pos );
      try
      {
        sbr = GetInheritor( hdr->type );
      } catch ( std::exception &e )
      {
        egts_error_t error = {0};
        error.sender = "TEGTSRecord::SetSRD, RN = " + to_string( head.rn );
        error.message = "TEGTSRecord::GetInheritor( " + to_string( hdr->type )
            + " ) throw an exception: " + ( std::string )e.what( );
        EGTSHandleAnError( error );
        return 0;
      }
      if ( !sbr )
      {
#ifdef AUX_MODE
        egts_error_t error = {0};
        error.sender = "TEGTSRecord::SetSRD, RN = " + to_string( head.rn );
        error.message = "TEGTSRecord::GetInheritor( " + to_string( hdr->type )
            + " ), rst = " + to_string( this->rst ) + ", return 0";
        EGTSHandleAnError( error );
#endif
        return 0;
      }
      std::unique_ptr<TEGTSSubrecord> ptr( sbr );
      srl = size - pos;
      srd = sbr->SubrecordData( data + pos, &srl );
      if ( !srd ) break;
      try
      {
        if ( !sbr->SetSRD( srd, srl, &offset ) ) break;
      } catch ( std::exception &e )
      {
        egts_error_t error = {0};
        error.sender = "TEGTSRecord::SetSRD, RN = " + to_string( head.rn );
        error.message = "TEGTSSubrecord::SetSRD throw an exception: "
            + ( std::string )e.what( );
        EGTSHandleAnError( error );
        return 0;
      }
      sbrs.Add( ptr.release( ) );
      pos += offset + EGTS_SBR_HDR_SIZE;
      rl += offset + EGTS_SBR_HDR_SIZE;
      if ( rl >= head.rl ) break;
    }
  }
  if ( ppos ) *ppos = pos;
  return sbrs.Count( );
}

std::unique_ptr<char> TEGTSRecord::GetData( uint16_t *size )
{
  *size = sizeof( head ) + 2 + head.rfl.tmfe * 4
      + head.rfl.evfe * 4 + head.rfl.obfe * 4;
  egts_object_list_t< egts_data_size_t > ds_list;
  head.rl = 0;
  for ( TEGTSSubrecord *sbr = sbrs.First( ); sbr; sbr = sbrs.Next( ) )
  {
    egts_data_size_t *ds = ds_list.New( );
    if ( !ds )
    {
#ifdef AUX_MODE
      egts_error_t error = {0};
      error.sender = "TEGTSRecord::GetData, RN = " + to_string( head.rn );
      error.message = "ds_list.New() return 0";
      EGTSHandleAnError( error );
#endif
      return std::unique_ptr<char>( );
    }
    try
    {
      ds->data = std::move( sbr->GetData( &ds->size ) );
    } catch ( std::exception &e )
    {
      egts_error_t error = {0};
      error.sender = "TEGTSRecord::GetData, RN = " + to_string( head.rn );
      error.message = "TEGTSSubrecord::GetData throw an exception: "
          + ( std::string )e.what( );
      EGTSHandleAnError( error );
      return std::unique_ptr<char>( );
    }
    if ( !ds->data )
    {
#ifdef AUX_MODE
      egts_error_t error = {0};
      error.sender = "TEGTSRecord::GetData, RN = " + to_string( head.rn );
      error.message = "TEGTSSubrecord::GetData return 0";
      EGTSHandleAnError( error );
#endif
      return std::unique_ptr<char>( );
    }
    head.rl += ds->size;
  }
  *size += head.rl;
  char *data = new char[*size];
  std::unique_ptr<char> pdata( data );

  uint16_t pos = 0;
  memcpy( data, &head, sizeof( head ) );
  pos += sizeof( head );
  if ( head.rfl.obfe )
  {
    memcpy( data + pos, &oid, 4 );
    pos += 4;
  }
  if ( head.rfl.evfe )
  {
    memcpy( data + pos, &evid, 4 );
    pos += 4;
  }
  if ( head.rfl.tmfe )
  {
    memcpy( data + pos, &tm, 4 );
    pos += 4;
  }
  data[pos] = sst;
  data[ pos + 1 ] = rst;
  pos += 2;
  for ( egts_data_size_t *ds = ds_list.First( ); ds; ds = ds_list.Next( ) )
  {
    memcpy( data + pos, ds->data.get(), ds->size );
    pos += ds->size;
  }
  return pdata;
}

TEGTSPacket::TEGTSPacket( TEGTSPacket& pack )
{
  Init( );
  if ( pack.head.bf.ena )
  {
    memcpy( crypto_key, pack.crypto_key, 8 );
    head.bf.ena = 1;
    pack.head.bf.ena = 0;
  }
  uint16_t size;
  std::unique_ptr<char> data = pack.GetData( &size );
  SetData( data.get( ), size );
}

void TEGTSPacket::operator =( TEGTSPacket& pack )
{
  Init( );
  if ( pack.head.bf.ena )
  {
    memcpy( crypto_key, pack.crypto_key, 8 );
    head.bf.ena = 1;
    pack.head.bf.ena = 0;
  }
  uint16_t size;
  std::unique_ptr<char> data = pack.GetData( &size );
  SetData( data.get( ), size );
}

void TEGTSPacket::Init( )
{
  memset( crypto_key, 0, sizeof( crypto_key ) );
  memset( &head, 0, sizeof( head ) );
  head.prv = 0x01;
  head.bf.pr = 3;
  head.hl = 11;
  head.pt = EGTS_PT_APPDATA;
  memset( &route, 0, sizeof( route ) );
  rpid = 0;
  pr = 0;
  recs.Clear( );
}

uint8_t TEGTSPacket::SetData( const char *data, uint32_t size, unsigned int *ppos )
{ // data, length of data, readed size, result - packet accepted
  if ( ppos ) *ppos = 0;
  uint32_t pos = 0, pack_size;
  for (; pos < size; pos++ )
  {
    // header search
    if ( pos + 10 > size ) break;
    memcpy( &head, data + pos, sizeof( head ) );
    if ( head.prv != 0x01 ) continue; // /*|| head.skid != 0x00*/ || head.btflds.prf != 0 ) continue;
    if ( head.hl != 11 && head.hl != 16 ) continue; // invalid header length
    if ( head.pt > 2 ) continue;
    pack_size = head.hl;
    if ( head.bf.rte ) pack_size += 5;
    if ( head.fdl ) pack_size = pack_size + head.fdl + 2;
    if ( pos + pack_size > size ) break; // not enough data - expect more

    // length is enough, set package length and check crc
    if ( ppos ) *ppos = pos + pack_size;
    if ( head.bf.rte ) memcpy( &route, data + pos + 10, sizeof( route ) );
    uint8_t hcs = data[pos + head.hl - 1];
    if ( hcs != egts_crc8( (uint8_t*)( data + pos ), head.hl - 1 ) )
    {
#ifdef AUX_MODE
      egts_error_t error = {0, "TEGTSPacket::SetData", "Bad checksum of header"};
      EGTSHandleAnError( error );
#endif
      return 0;
    }
    if ( !head.fdl ) return 1;
    data = data + pos + head.hl;
    pos = 0;
    uint16_t sfrcs = *(uint16_t*)( data + head.fdl );
    if ( sfrcs != egts_crc16( (uint8_t*)data, head.fdl ) )
    {
#ifdef AUX_MODE
      egts_error_t error = {0, "TEGTSPacket::SetData",
        "Bad checksum of service frame data"};
      EGTSHandleAnError( error );
#else
      return 0;
#endif
    }
    if ( head.bf.ena == 1 )
      nv_GOST_28147_89_decrypt_same_buf( crypto_key, 7, (char*)data, head.fdl );

    if ( head.pt == EGTS_PT_RESPONCE )
    {
      if ( head.fdl < 3 ) return 0;
      rpid = *(uint16_t*)data;
      pr = data[2];
      pos += 3;
    }
    else if ( head.pt == EGTS_PT_APPDATA )
    {
    }
    else if ( head.pt == EGTS_PT_SIGNED_APPDATA )
    {
      if ( head.fdl < 2 ) return 0;
      signature.size = *(uint16_t*)data;
      if ( head.fdl < signature.size + 2 ) return 0;
      signature.data.reset( new char[signature.size] );
      memcpy( signature.data.get( ), data + 2, signature.size );
      pos += 2 + signature.size;
    }
    if ( pos >= (uint32_t)head.fdl ) return 1;
    uint16_t offset;
    while ( pos < (uint32_t)head.fdl )
    {
      std::unique_ptr< TEGTSRecord > rec( new TEGTSRecord );
      if ( !rec->SetData( data + pos, head.fdl - pos, &offset ) )
        if ( !offset ) break;
      recs.Add( rec.release( ) );
      pos += offset;
    }
    return 1;
  }
  if ( ppos ) *ppos = pos;
  return 0; // header not found, data up to position can be free
}

std::unique_ptr<char> TEGTSPacket::GetData( uint16_t *size )
{
  *size = 0;
  uint16_t frame_size = 0;
  if ( head.pt == EGTS_PT_RESPONCE ) frame_size = 3;
  else if ( head.pt == EGTS_PT_APPDATA )
  {
  }
  else if ( head.pt == EGTS_PT_SIGNED_APPDATA ) frame_size = 2 + signature.size;
  else return std::unique_ptr<char>( );
  egts_object_list_t< egts_data_size_t > ds_list;
  for ( TEGTSRecord* sr = recs.First( ); sr; sr = recs.Next( ) )
  { // retrieving records length and data
    egts_data_size_t *ds = ds_list.New( );
    if ( !ds )
    {
#ifdef AUX_MODE
      egts_error_t error = {0};
      error.sender = "TEGTSPacket::GetData, PID = " + to_string( head.pid );
      error.message = "ds_list.New return 0";
      EGTSHandleAnError( error );
#endif
      return std::unique_ptr<char>( );
    }
    ds->data = sr->GetData( &ds->size );
    if ( !ds->data )
    {
#ifdef AUX_MODE
      egts_error_t error = {0};
      error.sender = "TEGTSPacket::GetData, PID = " + to_string( head.pid );
      error.message = "TEGTSRecord::GetData return 0";
      EGTSHandleAnError( error );
#endif
      return std::unique_ptr<char>( );
    }
    frame_size += ds->size;
    if ( frame_size > EGTS_MAX_FRAME_SIZE )
    {
#ifdef AUX_MODE
      egts_error_t error = {0};
      error.sender = "TEGTSPacket::GetData, PID = " + to_string( head.pid );
      error.message = "frame_size > EGTS_MAX_FRAME_SIZE";
      EGTSHandleAnError( error );
#endif
      return std::unique_ptr<char>( );
    }
  }
  head.hl = head.bf.rte ? 16 : 11;
  head.fdl = frame_size;
  if ( head.bf.ena == 1 )
  {
    uint16_t rmndr = head.fdl % 8;
    if ( rmndr ) head.fdl += ( 8 - rmndr );
  }
  *size = head.fdl ? head.hl + head.fdl + 2 : head.hl;

  char *data;
  try
  {
    data = new char[*size];
  } catch ( std::exception &e )
  {
    egts_error_t error = {0};
    error.sender = "TEGTSPacket::GetData, RN = " + to_string( head.pid );
    error.message = "new char[" + to_string( *size ) + "] throw an exception: "
        + ( std::string )e.what( );
    EGTSHandleAnError( error );
    return std::unique_ptr<char>( );
  }
  std::unique_ptr<char> pdata( data );

  memcpy( data, &head, sizeof( head ) );
  if ( head.bf.rte ) memcpy( data + sizeof( head ), &route, sizeof( route ) );
  data[head.hl - 1] = egts_crc8( (uint8_t*)data, head.hl - 1 );
  if ( !head.fdl ) return pdata;

  char *frame_data = data + head.hl;
  uint16_t pos = 0;
  if ( head.pt == EGTS_PT_RESPONCE )
  {
    ( *(uint16_t*)frame_data ) = rpid;
    frame_data[2] = pr;
    pos = 3;
  }
  else if ( head.pt == EGTS_PT_SIGNED_APPDATA )
  {
    ( *(uint16_t*)frame_data ) = signature.size;
    if ( signature.size ) memcpy( frame_data + 2, signature.data.get( ), signature.size );
    pos = 2 + signature.size;
  }
  for ( egts_data_size_t *ds = ds_list.First( ); ds; ds = ds_list.Next( ) )
  {
    memcpy( frame_data + pos, ds->data.get( ), ds->size );
    pos += ds->size;
  }
  if ( head.bf.ena == 1 )
  {
    memset( frame_data + frame_size, 0, head.fdl - frame_size );
    nv_GOST_28147_89_encrypt_same_buf( crypto_key, 7, frame_data, head.fdl );
  }
  *(uint16_t*)( frame_data + head.fdl ) = egts_crc16( (uint8_t*)frame_data, head.fdl );
  return pdata;
}

