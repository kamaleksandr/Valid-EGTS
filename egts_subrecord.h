/*
 * File:   egts_subrecords.h
 * Author: kamyshev.a
 *
 * Created on 1 August 2018, 14:17
 */

#ifndef egts_subrecordH
#define egts_subrecordH

#ifdef __linux__
#include <arpa/inet.h>
#elif _WIN32
#include <winsock.h>
#endif

#include "egts_common.h"

class TEGTSSubrecord
{
protected:
  uint8_t type;
  char* PrepareGetData( uint16_t *size );
  
public:
  TEGTSSubrecord( ){ type = 0; }
  virtual ~TEGTSSubrecord( ){}
  virtual TEGTSSubrecord* Copy(){ return 0; }
  uint8_t GetType(){ return type; }
  const char* SubrecordData( const char *data, uint16_t *size );
  virtual uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 )
  { return 0; }
  virtual std::unique_ptr<char> GetData( uint16_t *size )
  { return std::unique_ptr<char>(); }
};

class TEGTSECallRawMSD : public TEGTSSubrecord  // EGTS_SR_RAW_MSD_DATA
{
public:
  TEGTSECallRawMSD(){ type = EGTS_SR_RAW_MSD_DATA; fm = 0; }
  ~TEGTSECallRawMSD(){}
  uint8_t fm;
  egts_data_size_t msd;
#if __cplusplus > 199711L
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 ) override;
  std::unique_ptr<char> GetData( uint16_t *size ) override;
#else
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
#endif  
};

class TEGTSECallMSD : public TEGTSSubrecord // EGTS_SR_MSD_DATA
{
public:
  TEGTSECallMSD();
  ~TEGTSECallMSD(){}
#pragma pack(push,1)
  struct
  {
    uint8_t fv;
    uint8_t mi;
    uint8_t act : 1;
    uint8_t clt : 1;
    uint8_t pocn : 1;
    uint8_t vt : 4;
    char vin[17];
    uint8_t vpst;
    uint32_t ts;
    int32_t plat;
    int32_t plon;
    uint8_t vd;
  } head;
  struct
  {
    int16_t latd1;
    int16_t lond1;
    int16_t latd2;
    int16_t lond2;
    uint8_t nop;
  } bf;
#pragma pack(pop)
  egts_data_size_t additional;

  void SetTime( uint32_t val ){ head.ts = htonl( val ); }
  uint32_t GetTime( ){ return ntohl( head.ts ); }
  void SetLatitude( double val ){ head.plat = htonl( val * 3600000.0 ); }
  double GetLatitude( ){ return ntohl( head.plat ) / 3600000.0; }
  void SetLongitude( double val ){ head.plon = htonl( val * 3600000.0 ); }
  double GetLongitude( ){ return ntohl( head.plon ) / 3600000.0; }
  void SetCourse( uint16_t val ){ head.vd = val / 2; }
  uint16_t GetCourse( ) { return head.vd * 2; }
  void SetLatDelta1( int16_t val ){ bf.latd1 = htonl( val ); }
  int16_t GetLatDelta1( ){ return ntohl( bf.latd1 ); }
  void SetLonDelta1( int16_t val ){ bf.lond1 = htonl( val ); }
  int16_t GetLonDelta1( ){ return ntohl( bf.lond1 ); }
  void SetLatDelta2( int16_t val ){ bf.latd2 = htonl( val ); }
  int16_t GetLatDelta2( ){ return ntohl( bf.latd2 ); }
  void SetLonDelta2( int16_t val ){ bf.lond2 = htonl( val ); }
  int16_t GetLonDelta2( ){ return ntohl( bf.lond2 ); }
  
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSEPCompData : public TEGTSSubrecord // EGTS_SR_EP_COMP_DATA
{
public:
  TEGTSEPCompData( );
  ~TEGTSEPCompData( ) { }
#pragma pack(push,1)
  struct
  {
    uint8_t bnl;
    uint8_t srt;
    struct
    {
      uint8_t cm : 1;
      uint8_t void_field : 6;
      uint8_t bnh : 1;
    } bf;
    uint16_t cdl;
  } head;
#pragma pack(pop)
  std::unique_ptr<char> cd;
  void SetBlockNumber( uint16_t value );
  uint16_t GetBlockNumber( );
  std::unique_ptr<TEGTSSubrecord> Extract();
  uint8_t Compress( TEGTSSubrecord* );
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSEPSignature : public TEGTSSubrecord // EGTS_SR_EP_SIGNATURE
{
public:
  TEGTSEPSignature( );
  ~TEGTSEPSignature( ){}
  uint8_t ver;
  uint8_t sa;
#pragma pack(push,1)
  struct sign_data_head_t
  {
    uint16_t keyn;
    uint32_t algid;
    uint16_t sln;
  };
#pragma pack(pop)
  struct sign_data_t
  {
    sign_data_t( ){ memset( &head, 0, 8 ); }
    sign_data_head_t head;
    std::unique_ptr<char> sd;
  };
  egts_object_list_t< sign_data_t > sd_list;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSEPAccelData3 : public TEGTSSubrecord //EGTS_SR_EP_ACCEL_DATA3
{
public:
	TEGTSEPAccelData3();
	~TEGTSEPAccelData3(){ }
#pragma pack(push,1)
  struct accel_data_head_t
  {
    uint8_t rst : 1;
    uint8_t tms : 7;
  };
  struct accel_data_struct_t
  {
    short xaav;
    short yaav;
    short zaav;
  };
  struct
  {
    uint8_t bnl;
    uint32_t at;
    uint8_t atmsl;
    struct
    {
      uint8_t atmsh : 2;
      uint8_t void_field : 5;
      uint8_t bnh : 1;
    } bf1;
    uint8_t rsal;
    struct
    {
      uint8_t mu : 4;
      uint8_t rtu : 1;
      uint8_t void_field1 : 1;
      uint8_t rsah : 1;
      uint8_t void_field2 : 1;
    } bf2;
    accel_data_struct_t ads;
  } head;
#pragma pack(pop)
  struct accel_data_t
  {
    accel_data_t( ) { memset( &head, 0, sizeof ( accel_data_head_t) ); }
    accel_data_head_t head;
    accel_data_struct_t ads;
  };
  egts_object_list_t< accel_data_t > ad_list;

  void SetBlockNumber( uint16_t value );
  uint16_t GetBlockNumber( );
  void SetTime( uint32_t value );
  uint32_t GetTime( ){ return head.at + 1262304000; }
  void SetMSec( uint16_t value );
  uint16_t GetMSec( );
  void SetRSA( uint16_t value );
  uint16_t GetRSA( );

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSEPRelativeTrack
{
public:
  TEGTSEPRelativeTrack( );
#pragma pack(push,1)
  union
  {
    struct
    {
      uint8_t rst : 1;
      uint8_t tms : 6;
      uint8_t tdv : 1;
    } head0;
    struct
    {
      uint8_t rst : 1;
      uint8_t tms : 3;
      uint8_t spde : 1;
      uint8_t spdh : 2;
      uint8_t spds : 1;
    } head1;
  };
  struct rt_body_t
  {
    uint8_t latdl;
    struct
    {
      uint8_t latdh : 6;
      uint8_t lats : 1;
      uint8_t dirh : 1;
    } bf1;
    uint8_t londl;
    struct
    {
      uint8_t londh : 6;
      uint8_t lons : 1;
      uint8_t alte : 1;
    } bf2;
    uint8_t dirl;
  };
  uint8_t spdl;
  struct
  {
    uint8_t altd : 7;
    uint8_t alts : 1;
  } alt;
#pragma pack(pop)
  rt_body_t body;
  void SetLonDelta( double val );
  double GetLonDelta( );
  void SetLatDelta( double val );
  double GetLatDelta( );
  void SetAltDelta( short val );
  short GetAltDelta( );
  void SetSpeedDelta( uint16_t val );
  uint16_t GetSpeedDelta( );
  void SetCourse( uint16_t val );
  uint16_t GetCourse( );
};

class TEGTSEPTrackData : public TEGTSSubrecord // EGTS_SR_EP_TRACK_DATA
{
public:
  TEGTSEPTrackData();
  ~TEGTSEPTrackData(){}
#pragma pack(push,1)
  struct
  {
    uint8_t bnl;
    uint32_t at;
    uint8_t rsa;
    struct
    {
      uint8_t rtu : 1;
      uint8_t bnh : 2;
      uint8_t cs : 1;
      uint8_t ts : 4;
    } bf1;
    struct
    {
      uint8_t tde : 1;
      uint8_t spdh : 5;
      uint8_t lahs : 1;
      uint8_t lohs : 1;
    } bf2;
  } head;
  struct
  {
    uint32_t lat;
    uint32_t lon;
    uint8_t spdl;
    uint8_t altl;
    struct
    {
      uint8_t alth : 6;
      uint8_t alts : 1;
      uint8_t dirh : 1;
    } bf;
    uint8_t dirl;
  } tds;
#pragma pack(pop)

  egts_object_list_t< TEGTSEPRelativeTrack > rt_list;
  void SetBlockNumber( uint16_t value );
  uint16_t GetBlockNumber( );
  void SetTime( uint32_t value );
  uint32_t GetTime( ){ return head.at + 1262304000; }
  void SetLongitude( double val ){ tds.lon = val / 180.0 * 0xffffffff; }
  double GetLongitude( ){ return (double)tds.lon / 0xffffffff * 180; }
  void SetLatitude( double val ){ tds.lat = val / 90.0 * 0xffffffff; }
  double GetLatitude( ){ return (double)tds.lat / 0xffffffff * 90; }
  void SetAltitude( short val );
  short GetAltitude( );
  void SetSpeed( uint16_t val );
  uint16_t GetSpeed( );
  void SetCourse( uint16_t val );
  uint16_t GetCourse( );

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSEPMainData : public TEGTSSubrecord // EGTS_SR_EP_MAIN_DATA
{
public:
  TEGTSEPMainData( );
#pragma pack(push,1)
  struct
  {
    uint8_t fv;
    uint8_t bnl;
    uint32_t evid;
    struct
    {
      uint8_t act : 1;
      uint8_t clt : 1;
      uint8_t locp : 1;
      uint8_t lbsn : 3;
      uint8_t bnh : 2;

      uint8_t trp : 1;
      uint8_t rwp : 1;
      uint8_t was : 1;
      uint8_t rs : 1;
      uint8_t as : 1;
      uint8_t trs : 1;
      uint8_t evp : 1;
      uint8_t acp : 1;
    } cn;
    uint32_t ts;
    uint32_t msl;
    struct
    {
      uint8_t msh : 2;
      uint8_t cau : 3;
      uint8_t void_field : 2;
    } bf;
  } head;
  struct
  {
    uint32_t lat;
    uint32_t lon;
    uint8_t spdl;
    struct
    {
      uint8_t spdh : 5;
      uint8_t lahs : 1;
      uint8_t lohs : 1;
      uint8_t dirh : 1;
    } bf;
    int16_t alt;
    uint8_t dirl;
  } location;
  struct base_station_t
  {
    uint8_t nid[3];
    uint32_t lac;
    int16_t cid;
    uint8_t ss;
  };
#pragma pack(pop)
  egts_object_list_t< base_station_t > stations;
  uint8_t epevc;
  std::deque< uint32_t > events;

  void SetBlockNumber( uint16_t value );
  uint16_t GetBlockNumber( );
  void SetTime( uint32_t value );
  uint32_t GetTime( ){ return head.ts + 1262304000; }
  void SetMSec( uint16_t value );
  uint16_t GetMSec( );
  void SetLongitude( double val ){ location.lon = val / 180.0 * 0xffffffff; }
  double GetLongitude( ){ return (double)location.lon / 0xffffffff * 180;	}
  void SetLatitude( double val ){ location.lat = val / 90.0 * 0xffffffff; }
  double GetLatitude( ) { return (double)location.lat / 0xffffffff * 90; }
  void SetSpeed( uint16_t value );
  uint16_t GetSpeed( );
  void SetCourse( uint16_t value );
  uint16_t GetCourse( );

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSRecResp : public TEGTSSubrecord  // EGTS_SR_RECORD_RESPONSE
{
public:
  TEGTSRecResp();
  uint16_t crn;
  uint8_t rst;
  
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSTermIdent : public TEGTSSubrecord // EGTS_SR_TERM_IDENTITY
{
public:
  TEGTSTermIdent( );
  ~TEGTSTermIdent( ){}
#pragma pack( push, 1 )
  struct
  {
    uint32_t tid; // veh_id
    struct
    {
      uint8_t hdide : 1;
      uint8_t imeie : 1;
      uint8_t imsie : 1;
      uint8_t lngce : 1;
      uint8_t ssra : 1;
      uint8_t nide : 1;
      uint8_t bse : 1;
      uint8_t mne : 1;
    } flags;
  } head;
  struct
  {
    uint32_t mnc : 10;
    uint32_t mcc : 10;
  } nid;
#pragma pack(pop)
  uint16_t hdid;
  char imei[16];
  char imsi[17];
  char lngc[3];
  uint16_t bs;
  char msisdn[16];
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
  
protected:
  void GetData( char *data );
  uint16_t GetSize( ); 
};

class TEGTSTermIdent2 : public TEGTSTermIdent //EGTS_SR_TERM_IDENTITY2
{
public:
  TEGTSTermIdent2( );
#pragma pack( push, 1 )
  struct
  {
    uint8_t icce : 1;
    uint8_t void_field : 7;
  } flags;
  char iccid[20];
#pragma pack(pop)
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSDispIdent : public TEGTSSubrecord // EGTS_SR_DISPATCHER_IDENTITY
{
public:
  TEGTSDispIdent();
#pragma pack(push,1)
  struct
  {
    uint8_t dt;
    uint32_t did;
  } head;
#pragma pack(pop)
  std::string dscr;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSAuthParams : public TEGTSSubrecord // EGTS_SR_AUTH_PARAMS
{
public:
	TEGTSAuthParams();
#pragma pack(push,1)
  struct
	{
    uint8_t ena : 2;
    uint8_t pke : 1;
    uint8_t isle : 1;
    uint8_t mse : 1;
    uint8_t sse : 1;
    uint8_t exe : 1;
    uint8_t void_field : 1;
  } flags;
#pragma pack(pop)
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSAuthInfo : public TEGTSSubrecord // EGTS_SR_AUTH_INFO
{
public:
  TEGTSAuthInfo( ){ type = EGTS_SR_AUTH_INFO; }
  std::string user_name;
  std::string user_pass;
  std::string server_sequence;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSServiceInfo : public TEGTSSubrecord //EGTS_SR_SERVICE_INFO
{
public:
  TEGTSServiceInfo();
#pragma pack(push,1)
  struct
  {
    uint8_t st;
    uint8_t sst;
    struct
    {
      uint8_t srvrp : 2;
      uint8_t void_field : 5;
      uint8_t srva : 1;
    } srvp;
  } body;
#pragma pack(pop)
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSVehicleData : public TEGTSSubrecord // EGTS_SR_VEHICLE_DATA
{
public:
  TEGTSVehicleData( );
#pragma pack(push,1)
  struct
  {
    char vin[17];
    uint32_t vht;
    uint32_t vpst;
  } body;
#pragma pack(pop)
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSResultCode : public TEGTSSubrecord // EGTS_SR_RESULT_CODE
{
public:
  TEGTSResultCode( ){ type = EGTS_SR_RESULT_CODE; rcd = 0; }
  uint8_t rcd;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSInsAccel : public TEGTSSubrecord // EGTS_SR_INSURANCE_ACCEL_DATA
{
public:
  TEGTSInsAccel( );
  TEGTSSubrecord* Copy();
#pragma pack(push,1)
  struct mod_xyz_t
  {
    uint16_t adc_bits : 5;
    uint16_t range : 11;
  };
  struct
  {
    uint8_t hver;
    struct
    {
      uint8_t format : 2;
      uint8_t zlib : 2;
      uint8_t tf : 2;
      uint8_t mode : 2;
    } enc;
    uint16_t pid;
  } head;
  struct
  {
    uint16_t tnp;
    uint16_t rtm; // increment to the atm in milliseconds
    uint32_t atm; // start time, seconds
    uint16_t freq;
    mod_xyz_t xmod;
    mod_xyz_t ymod;
    mod_xyz_t zmod;
  } body;
#pragma pack(pop)
  uint16_t dl;
  struct calibration_data_t
  {
    uint8_t size;
    char cdata[4];
  };
  calibration_data_t x, y, z;
  std::unique_ptr<char> xyz_data;
  uint8_t GetXsize( );
  uint8_t GetYsize( );
  uint8_t GetZsize( );
  void SetTime( uint32_t value );
  uint32_t GetTime( ){ return body.atm + 1262304000; }

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSPlusData : public TEGTSSubrecord // EGTS_SR_EGTSPLUS_DATA
{
public:
  TEGTSPlusData(){ type = EGTS_SR_EGTSPLUS_DATA; }
  egts_data_size_t protobuf;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSPosData : public TEGTSSubrecord // EGTS_SR_POS_DATA
{
public:
  TEGTSPosData( );
#pragma pack(push,1)
  struct
  {
    uint32_t ntm; // seconds since 2010, utc
    uint32_t lat; // degrees / 90 * 0xffffffff
    uint32_t lon; // degrees / 180 * 0xffffffff
    struct
    {
      uint8_t vld : 1; // valid data
      uint8_t fix : 1;
      uint8_t cs : 1;
      uint8_t bb : 1; // realtime
      uint8_t mv : 1; // stand
      uint8_t lahs : 1; // northern latitude
      uint8_t lohs : 1; // eastern longitude
      uint8_t alte : 1; // presence of the elevation fields
    } flags;
    uint8_t spd; // speed 0.1 * km / h
    struct
    {
      uint8_t spd : 6; // upper 6 bit of speed
      uint8_t alts : 1; // presence of an alt field
      uint8_t dirh : 1; // 8 bits of the dir parameter
    } bf;
    uint8_t dir; // course
    uint8_t odm[3]; // mileage in 0.1 km
    uint8_t din;
    uint8_t src;
  } body;
#pragma pack(pop)
  uint8_t alt[3]; // altitude
  uint16_t srcd;
  void SetTime( uint32_t value );
  uint32_t GetTime( ){ return body.ntm + 1262304000; }
  void SetLongitude( double val ){ body.lon = val / 180.0 * 0xffffffff;}
  double GetLongitude( ){ return (double)body.lon / 0xffffffff * 180; }
  void SetLatitude( double val ){ body.lat = val / 90.0 * 0xffffffff; }
  double GetLatitude( ){ return (double)body.lat / 0xffffffff * 90; }
  void SetSpeed( uint16_t value );
  uint16_t GetSpeed( );
  void SetCourse( uint16_t value );
  uint16_t GetCourse( );
  void SetOdometer( uint32_t val );
  uint32_t GetOdometer( );
  void SetAltitude( int value );
  int GetAltitude( );

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSExtPosData : public TEGTSSubrecord // EGTS_SR_EXT_POS_DATA
{
public:
  TEGTSExtPosData();
#pragma pack(push,1)
  struct
  {
    uint8_t vfe : 1;
    uint8_t hfe : 1;
    uint8_t pfe : 1;
    uint8_t sfe : 1;
    uint8_t nsfe : 1;
    uint8_t void_field : 3;
  } flags;
#pragma pack(pop)
  uint16_t vdop;
  uint16_t hdop;
  uint16_t pdop;
  uint8_t sat;
  uint16_t ns;

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSAdSensors : public TEGTSSubrecord // EGTS_SR_AD_SENSORS_DATA
{
private:
  uint8_t dins[8];
  uint8_t ains[24];
public:
  TEGTSAdSensors( );
#pragma pack(push,1)
  struct
  {
    union
    {
      struct
      {
        uint8_t dioe0 : 1;
        uint8_t dioe1 : 1;
        uint8_t dioe2 : 1;
        uint8_t dioe3 : 1;
        uint8_t dioe4 : 1;
        uint8_t dioe5 : 1;
        uint8_t dioe6 : 1;
        uint8_t dioe7 : 1;
      } flags;
      uint8_t val;
    } dinfe;
    uint8_t dout;
    union
    {
      struct
      {
        uint8_t asfe0 : 1;
        uint8_t asfe1 : 1;
        uint8_t asfe2 : 1;
        uint8_t asfe3 : 1;
        uint8_t asfe4 : 1;
        uint8_t asfe5 : 1;
        uint8_t asfe6 : 1;
        uint8_t asfe7 : 1;
      } flags;
      uint8_t val;
    } ainfe;
  } head;
#pragma pack(pop)
  
  void SetDin( uint8_t num, uint8_t val );
  uint8_t GetDin( uint8_t num );
  void SetAin( uint8_t num, uint32_t val );
  uint32_t GetAin( uint8_t num );

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSCounters : public TEGTSSubrecord // EGTS_SR_COUNTERS_DATA
{
private:
  char cnts[24];
public:
  TEGTSCounters( );
#pragma pack(push,1)
  union
  {
  struct
  {
    uint8_t cfe0 : 1;
    uint8_t cfe1 : 1;
    uint8_t cfe2 : 1;
    uint8_t cfe3 : 1;
    uint8_t cfe4 : 1;
    uint8_t cfe5 : 1;
    uint8_t cfe6 : 1;
    uint8_t cfe7 : 1;
  } flags;
  uint8_t cntfe;
  };
#pragma pack(pop)
  void SetCounter( uint8_t num, uint32_t val );
  uint32_t GetCounter( uint8_t num );
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSStateData : public TEGTSSubrecord // EGTS_SR_STATE_DATA
{
public:
	TEGTSStateData();
#pragma pack(push,1)
  struct
  {
    uint8_t st;
    uint8_t mpsv;
    uint8_t bbv;
    uint8_t ibv;
    struct
    {
      uint8_t bbu : 1;
      uint8_t ibu : 1;
      uint8_t nms : 1;
      uint8_t void_field : 5;
    } flags;
  } body;
#pragma pack(pop)
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSLiquidLevel : public TEGTSSubrecord // EGTS_SR_LIQUID_LEVEL_SENSOR
{
public:
  TEGTSLiquidLevel();
#pragma pack(push,1)
  struct
  {
    struct
    {
      uint8_t llsn : 3;
      uint8_t rdf : 1;
      uint8_t llsvu : 2;
      uint8_t llsef : 1;
      uint8_t void_field : 1;
    } flags;
    uint16_t maddr;
  } head;
#pragma pack(pop)
  egts_data_size_t llcd;
  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSPassengers : public TEGTSSubrecord // EGTS_SR_PASSENGERS_COUNTERS
{
public:
  TEGTSPassengers();
#pragma pack(push,1)
  struct
  {
    struct
    {
      uint8_t rdf : 1;
      uint8_t void_field : 7;
    } flags;
    union
    {
      struct
      {
        uint8_t door0_present : 1;
        uint8_t door1_present : 1;
        uint8_t door2_present : 1;
        uint8_t door3_present : 1;
        uint8_t door4_present : 1;
        uint8_t door5_present : 1;
        uint8_t door6_present : 1;
        uint8_t door7_present : 1;
      } flags;
      uint8_t val;
    } dpr;
    union
    {
      struct
      {
        uint8_t door0_release : 1;
        uint8_t door1_release : 1;
        uint8_t door2_release : 1;
        uint8_t door3_release : 1;
        uint8_t door4_release : 1;
        uint8_t door5_release : 1;
        uint8_t door6_release : 1;
        uint8_t door7_release : 1;
      } flags;
      uint8_t val;
    } drl;
    uint16_t maddr;
  } head;
  struct door_t
  {
    uint8_t psgs_in;
    uint8_t psgs_out;
  };  
#pragma pack(pop)
  struct door_num_t
  {
    uint8_t num;
    door_t door;
  };
  egts_object_list_t< door_num_t > doors;

  uint8_t SetSRD( const char *data, uint16_t size, uint16_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

#endif
