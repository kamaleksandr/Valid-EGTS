#ifndef egts_crcH
#define egts_crcH

#include <stdint.h>

uint16_t egts_crc16( uint8_t *data, uint16_t len );
uint8_t egts_crc8( uint8_t *data, uint8_t len );
uint8_t egts_crc_xor( uint8_t *data, uint8_t len );

#endif
