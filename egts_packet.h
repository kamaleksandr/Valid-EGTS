/*
 * File:   egts_packet.h
 * Author: kamyshev.a
 *
 * Created on 1 August 2018, 14:10
 */

#ifndef egts_packetH
#define egts_packetH

#include "egts_subrecord.h"

class TEGTSRecord
{
private:
  TEGTSSubrecord* GetInheritor( uint8_t );
public:
  TEGTSRecord();
  ~TEGTSRecord() { }
#pragma pack(push,1)
  struct
  {
    uint16_t rl; // record length
    uint16_t rn; // record number
    struct
    {
      uint8_t obfe : 1;
      uint8_t evfe : 1;
      uint8_t tmfe : 1;
      uint8_t rpp : 2;
      uint8_t grp : 1;
      uint8_t rsod : 1;
      uint8_t ssod : 1;
    } rfl;
  } head;
#pragma pack(pop)
  unsigned int oid; // object ID
  unsigned int evid; // event ID
  unsigned int tm; // time in seconds from 00:00:00 01.01.2010 utc
  uint8_t sst; // sender service type ID
  uint8_t rst; // recipient service type ID
  egts_object_list_t< TEGTSSubrecord > sbrs;
  uint8_t SetData( const char *data, uint16_t size, uint16_t *ppos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};

class TEGTSPacket
{
public:
  TEGTSPacket(){ Init( ); }
  TEGTSPacket( TEGTSPacket& );
  void operator=(TEGTSPacket&);
  void Init( );
  uint16_t rpid; // ID of the request packet
  uint8_t pr; // processing result code
  char crypto_key[8];
  egts_data_size_t signature; // digital signature
#pragma pack(push,1)
  struct
  {
    uint8_t prv; // protocol version
    uint8_t skid; // key
    struct
    {
      uint8_t pr : 2; // priority, bit 0-1, 0 - high, 3-low
      uint8_t cmp : 1; // compression, bit 2
      uint8_t ena : 2; // encryption algorithm, bit 3-4
      uint8_t rte : 1; // routing, bit 5
      uint8_t prf : 2; // prefix, bit 6-7
    } bf;
    uint8_t hl; // header length
    uint8_t he; // reserve
    uint16_t fdl; // data length
    uint16_t pid; // packet ID
    uint8_t pt; // packet_type
  } head;
  struct
  {
    uint16_t pra; // sender address
    uint16_t rca; // recipient address
    uint8_t ttl; // package life time
  } route;
#pragma pack(pop)
  egts_object_list_t< TEGTSRecord > recs;
  uint8_t SetData( const char *data, uint32_t size, uint32_t *pos = 0 );
  std::unique_ptr<char> GetData( uint16_t *size );
};
#endif
