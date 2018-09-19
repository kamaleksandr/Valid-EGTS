/* 
 * File:   main.cpp
 * Author: kamyshev.a
 *
 * Created on 29 августа 2018 г., 15:50
 */

#include <cstdlib>
#include <iostream>
#include "egts_packet.h"
using namespace std;

static void OnError( egts_error_t* error )
{
  std::cout << "Error, sender: " << error->sender.c_str( );
  std::cout << "; message: " << error->message.c_str( ) << ";\r\n";
}

int main( int argc, char** argv )
{
  EGTSSetErrorCallback( OnError );
  TEGTSPacket pack;
  pack.head.pid = 10;
  try
  {
    TEGTSRecord *ath = pack.recs.New( );
    ath->rst = EGTS_AUTH_SERVICE;
    ath->head.rn = 0;
    TEGTSRecResp *rsp = new TEGTSRecResp;
    ath->sbrs.Add( rsp );

    TEGTSVehicleData *veh = new TEGTSVehicleData;
    memcpy( veh->body.vin, "test VIN\0", 10 );
    ath->sbrs.Add( veh );

    TEGTSTermIdent *tid = new TEGTSTermIdent;
    tid->head.flags.imeie = 1;
    memcpy( tid->imei, "test IMEI\0", 10 );
    ath->sbrs.Add( tid );

    TEGTSDispIdent *did = new TEGTSDispIdent;
    did->dscr = "test description";
    ath->sbrs.Add( did );

    TEGTSAuthParams *apm = new TEGTSAuthParams;
    ath->sbrs.Add( apm );

    TEGTSAuthInfo *inf = new TEGTSAuthInfo;
    inf->user_name = "test name";
    inf->user_pass = "test password";
    ath->sbrs.Add( inf );

    ath->sbrs.Add( new TEGTSResultCode );

    TEGTSTermIdent2 *tid2 = new TEGTSTermIdent2;
    memcpy( tid2->iccid, "test ICCID\0", 11 );
    tid2->flags.icce = 1;
    ath->sbrs.Add( tid2 );

    TEGTSRecord *tds = pack.recs.New( );
    tds->rst = EGTS_TELEDATA_SERVICE;
    tds->head.rn = 1;

    TEGTSPlusData *pls = new TEGTSPlusData;
    pls->protobuf.data.reset( new char[16] );
    pls->protobuf.size = 16;
    tds->sbrs.Add( pls );

    TEGTSPosData *pos = new TEGTSPosData;
    pos->SetLatitude( 55.5088 );
    pos->SetLongitude( 37.3345 );
    pos->SetSpeed( 100 );
    pos->SetCourse( 180 );
    pos->SetTime( time( 0 ) );
    tds->sbrs.Add( pos );

    tds->sbrs.Add( new TEGTSExtPosData );

    TEGTSAdSensors *ads = new TEGTSAdSensors;
    ads->SetAin( 2, 500 );
    ads->SetDin( 3, 1 );
    tds->sbrs.Add( ads );

    TEGTSCounters *cnt = new TEGTSCounters;
    cnt->SetCounter( 4, 3000 );
    tds->sbrs.Add( cnt );

    tds->sbrs.Add( new TEGTSStateData );

    TEGTSLiquidLevel *llv = new TEGTSLiquidLevel;
    llv->llcd.data.reset( new char[16] );
    llv->llcd.size = 16;
    tds->sbrs.Add( llv );

    TEGTSPassengers *psg = new TEGTSPassengers;
    TEGTSPassengers::door_num_t *door = psg->doors.New( );
    door->num = 3;
    door->door.psgs_in = 10;
    door->door.psgs_out = 5;
    tds->sbrs.Add( psg );

    TEGTSRecord *ecs = pack.recs.New( );
    ecs->head.rn = 2;
    ecs->rst = EGTS_ECALL_SERVICE;

    TEGTSECallRawMSD *raw = new TEGTSECallRawMSD;
    raw->msd.data.reset( new char[16] );
    raw->msd.size = 16;
    ecs->sbrs.Add( raw );

    TEGTSECallMSD *msd = new TEGTSECallMSD;
    msd->bf.lond2 = 1;
    msd->additional.data.reset( new char[16] );
    msd->additional.size = 16;
    ecs->sbrs.Add( msd );
    
    TEGTSRecord *ins = pack.recs.New( );
    ins->head.rn = 3;
    ins->rst = EGTS_INSURANCE_SERVICE;
    ins->sbrs.Add( new TEGTSInsAccel );

    TEGTSRecord *eps = pack.recs.New( );
    eps->rst = EGTS_EUROPROTOCOL_SERVICE;
    eps->head.rn = 4;

    eps->sbrs.Add( new TEGTSEPMainData );

    TEGTSEPTrackData *track = new TEGTSEPTrackData;
    eps->sbrs.Add( track );

    eps->sbrs.Add( new TEGTSEPSignature );

    std::unique_ptr<TEGTSEPCompData> cmp( new TEGTSEPCompData );
    if ( cmp->Compress( track ) ) eps->sbrs.Add( cmp.release() );

    eps->sbrs.Add( new TEGTSEPAccelData3 );
  } catch ( std::exception &e )
  {
    std::cout << e.what( ) << "\r\n";
    return 1;
  }

  uint16_t size;
  std::unique_ptr<char> ptr( pack.GetData( &size ) );
  pack.Init( );
  pack.SetData( ptr.get( ), size );
  for ( TEGTSRecord *r = pack.recs.First( ); r; r = pack.recs.Next( ) )
  {
    std::cout << "record, rn = " << r->head.rn << ", rl = " << r->head.rl;
    std::cout << ", rst = " << (int16_t)r->rst << "\r\n";
    for ( TEGTSSubrecord *sbr = r->sbrs.First( ); sbr; sbr = r->sbrs.Next( ) )
    {
      uint16_t srl;
      try
      {
        sbr->GetData( &srl );
      } catch ( std::exception &e )
      {
        std::cout << e.what() << "\r\n";
        return 1;
      }
      std::cout << "subrecord, srt = " << (int16_t)sbr->GetType( ) << ", srl = ";
      std::cout << srl - 3 << "\r\n";
      uint8_t type = sbr->GetType( );
      if ( r->rst == EGTS_AUTH_SERVICE )
      {
        if ( type == EGTS_SR_TERM_IDENTITY )
        {
          TEGTSTermIdent *tid = dynamic_cast<TEGTSTermIdent*>( sbr );
          if ( tid ) std::cout << tid->imei << "\r\n";
        }
        if ( type == EGTS_SR_TERM_IDENTITY2 )
        {
          TEGTSTermIdent2 *tid2 = dynamic_cast<TEGTSTermIdent2*>( sbr );
          if ( tid2 ) std::cout << tid2->iccid << "\r\n";
        }
        if ( type == EGTS_SR_DISPATCHER_IDENTITY )
        {
          TEGTSDispIdent *did = dynamic_cast<TEGTSDispIdent*>( sbr );
          if ( did ) std::cout << did->dscr << "\r\n";
        }
        if ( sbr->GetType( ) == EGTS_SR_AUTH_INFO )
        {
          TEGTSAuthInfo *inf = dynamic_cast<TEGTSAuthInfo*>( sbr );
          if ( inf ) std::cout << inf->user_name << ";" << inf->user_pass << "\r\n";
        }
        if ( type == EGTS_SR_VEHICLE_DATA )
        {
          TEGTSVehicleData *veh = dynamic_cast<TEGTSVehicleData*>( sbr );
          if ( veh ) std::cout << veh->body.vin << "\r\n";
        }
      }
      else if ( r->rst == EGTS_TELEDATA_SERVICE )
      {
        if ( type == EGTS_SR_POS_DATA )
        {
          TEGTSPosData *pos = dynamic_cast<TEGTSPosData*>( sbr );
          if ( !pos ) continue;
          std::cout << "lat = " << pos->GetLatitude( );
          std::cout << ", lon = " << pos->GetLongitude( ) << ", speed = ";
          std::cout << pos->GetSpeed( ) << ", course = " << pos->GetCourse( );
          std::cout << ", time = " << pos->GetTime( ) << "\r\n";
        }
        if ( type == EGTS_SR_AD_SENSORS_DATA )
        {
          TEGTSAdSensors *ads = dynamic_cast<TEGTSAdSensors*>( sbr );
          if ( !ads ) continue;
          std::cout << "ain2 = " << ads->GetAin( 2 );
          std::cout << ", din3 = " << (uint16_t)ads->GetDin( 3 ) << "\r\n";
        }
        if ( type == EGTS_SR_COUNTERS_DATA )
        {
          TEGTSCounters *cnt = dynamic_cast<TEGTSCounters*>( sbr );
          if ( cnt )
            std::cout << "counter4 = " << cnt->GetCounter( 4 ) << "\r\n";
        }
        if ( type == EGTS_SR_PASSENGERS_COUNTERS )
        {
          TEGTSPassengers *psg = dynamic_cast<TEGTSPassengers*>( sbr );
          if ( !psg ) continue;
          TEGTSPassengers::door_num_t *door = psg->doors.First( );
          for (; door; door = psg->doors.Next( ) )
          {
            std::cout << "door" << (uint16_t)door->num << ", in = " <<
                (uint16_t)door->door.psgs_in << ", out = " <<
                (uint16_t)door->door.psgs_out << "\r\n";
          }
        }
      }
      else if ( r->rst == EGTS_EUROPROTOCOL_SERVICE )
      {
        if ( type == EGTS_SR_EP_COMP_DATA )
        {
          TEGTSEPCompData *cmp = dynamic_cast<TEGTSEPCompData*>( sbr );
          if ( !cmp ) continue;
          std::unique_ptr<TEGTSSubrecord> unpacked = cmp->Extract();
          if ( !unpacked ) continue;
          std::cout << "unpacked subrecord, srt = " << (int16_t)unpacked->GetType( ) << "\r\n";
        }

      }
    }
  }
  return 0;
}


