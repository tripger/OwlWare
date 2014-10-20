#ifndef __FirmwareLoader_H__
#define __FirmwareLoader_H__

#include <malloc.h>
#include <math.h>
#include "CRCC.hpp"
#include "sysex.h"
#include "clock.h"

#define MAX_SYSEX_FIRMWARE_SIZE (256*1024)

class FirmwareLoader {
private:
  enum SysexFirmwareStatus {
    NORMAL = 0,
    UPLOADING,
    ERROR = 0xff
  };

  SysexFirmwareStatus status = NORMAL;
  int packageIndex = 0;
  uint32_t timestamp;
  // unsigned long crc32(const unsigned char* buffer, const unsigned int length);
  uint8_t* buffer = NULL;
  uint32_t size;
  uint32_t index;
  uint32_t crc;

public:
  void clear(){
    free(buffer);
    buffer = NULL;
    index = 0;
    packageIndex = 0;
  }

  int error(int code){
    clear();
    return code;
  }

  uint8_t* getData(){
    return buffer;
  }

  uint32_t decodeInt(uint8_t *data){
    // decode a 32-bit unsigned integer from sysex encoded data
    uint8_t buf[4];
    sysex_to_data(data, buf, 5);
    uint32_t result = buf[3] | (buf[2] << 8L) | (buf[1] << 16L) | (buf[0] << 24L);
    return result;
  }

  int handleFirmwareUpload(uint8_t* data, uint16_t length){
    int offset = 3;
    if(packageIndex++ == 0){
      // first package
      timestamp = getSysTicks();
      if(length < 3+4+4)
	return error(-1);
      // disable audio processing
      codec.stop();
      patches.reset();
      // get firmware data size (decoded)
      size = decodeInt(data+offset);
      offset += 5; // it takes five 7-bit values to encode four bytes
      // uint8_t buf[4];
      // int len = sysex_to_data(data+offset, buf, 5);
      // size = buf[3] | (buf[2] << 8L) | (buf[1] << 16L) | (buf[0] << 24L);
      // // get checksum
      // sysex_to_data(data+offset, buf, 4);
      // offset += 5;
      // crc = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
      // allocate memory
      if(size > MAX_SYSEX_FIRMWARE_SIZE)
	return error(-2);
      buffer = (uint8_t*)malloc(size);
    }
    int len = floor((length-offset)*7/8.0f);
    if(index+len < size){
      // mid package
      len = sysex_to_data(data+offset, buffer+index, length-offset);
      index += len;
      return 0;
    }
    // last package
    len = floor((length-offset-5)*7/8.0f);
    if(index+len != size)
      return error(-3); // wrong size
    len = sysex_to_data(data+offset, buffer+index, length-offset-5);
    index += len;
    if(index != size)
      return error(-4); // size mismatch
    // get checksum: last 4 bytes of buffer
    crc = decodeInt(data+length-5);
    // crc = buffer[size-1] | (buffer[size-2] << 8L) | (buffer[size-3] << 16L) | (buffer[size-4] << 24L);
    // check crc
      // size -= 4;
    uint32_t checksum = CRCC().calc(size, buffer);
    if(crc != checksum)
      return error(-5);
    return index;
  }
};

#endif // __FirmwareLoader_H__