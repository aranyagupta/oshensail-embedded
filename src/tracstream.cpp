#include "trac.hpp"

uint16_t longStoreTracker(0);
// Receive a datagram from UDP and put it into liveData
void receiveDatagram(EthernetUDP& tracStreamServer, char* liveData){
  int32_t packetSize = tracStreamServer.parsePacket();
  if (packetSize <= LIVE_DATA_LENGTH){
    tracStreamServer.read(liveData, packetSize);

  }
  else{
    memset(liveData, 0, sizeof liveData);
  }
}


// Parse datagram headers to ensure everything is valid 
tracStatus parseDatagram(char* liveData, char* longStore){
  /* Check tracStream headers */
  if (liveData[0]!=0x0A || liveData[1]!=0x06 || liveData[4]!=0x01) return INVALID;

  // Size of raw PCM data in bytes
  int16_t dataSize = (liveData[2] << 8) + liveData[3] - LIVE_DATA_HEADER_SIZE;

  /* Check DAQ data headers */
  char DAQType = liveData[5];
  if (DAQType == 0x00 || DAQType == 0x01) return INVALID; // DAQ type is unknown or PCM 16 (not handled by porpoise) 

  /* Check PCM headers */ 
  uint32_t sampleRate = (liveData[8] << 24) + (liveData[9] << 16) + (liveData[10] << 8) + liveData[11];
  uint16_t sampleSize = (liveData[16] << 8) + liveData[17];
  if (sampleRate!=SAMPLE_RATE && 3*sampleSize!=SAMPLE_SIZE) return INVALID; // Double check sampler rate and size

  if (longStoreTracker+dataSize>LONG_STORE_LENGTH){
    longStoreTracker = 0;
    return FINISH;
  }
  
  memmove(longStore+longStoreTracker, liveData+LIVE_DATA_HEADER_SIZE, dataSize);
  longStoreTracker += dataSize;

  return OK;
}