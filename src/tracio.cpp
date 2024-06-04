#include "trac.hpp"

// TODO: Enable Sampling High Pass filter on Porpoise at cutoff 2kHz to reduce processing on Arduino 
// May need to get better documentation 
// TODO: The slave may also send an asynchronous event packets which the master must respond to. If the slave
// does not receive a response within the allotted time, the event packet is re-sent. Events that have not
// been acknowledged will be continuously re-sent until an acknowledgement occurs or the event is no
// longer relevant.
// What is an acknowledgement? Is it its own command or something that TCP handles? 


const char ANNOUNCE = 0x00;
const char GET = 0x01;
const char _SET = 0x02;
const char _RESET = 0x03;
const char OP = 0x04;
const char BUSY = 0x05;

void get_tracio_flow(EthernetServer& server){
  char buf[7] = {0xF5, 0xA3, 0x00, 0x00, 0x07, GET, 0x00};
  server.write(buf, 7);
}

void get_tracio_info(EthernetServer& server){
  char buf[29] = {0xF5, 0xA3, 0x01, 0x00, 0x1D, GET};
  server.write(buf, 29);
}

void get_tracio_name(EthernetServer& server){
  char buf[6] = {0xF5, 0xA3, 0x02, 0x00, 0x06, GET};
  server.write(buf, 6);
}

// Name must be UTF-8 null terminated
void set_tracio_name(EthernetServer& server, char* name, uint8_t nameLength){
  const uint8_t bufLength = nameLength+6;
  char buf[bufLength] = {0xF5, 0xA3, 0x02, 0x00, 0x06, _SET};
  for (uint8_t i=5; i<bufLength; i++){
    buf[i] = name[i-5];
  }
  server.write(buf, 6);
}

void get_tracio_status(EthernetServer& server){
  char buf[25] = {0xF5, 0xA3, 0x03, 0x00, 0x19, GET};
  server.write(buf);
}

void get_tracio_ctrl(EthernetServer& server){
  char buf[10] = {0xF5, 0xA3, 0x04, 0x00, 0x19, GET};
  server.write(buf);
}

void set_tracio_ctrl(EthernetServer& server, char flgs, char disk){
    char buf[10] = {0xF5, 0xA3, 0x04, 0x00, 0x19, _SET, flgs, disk};
    server.write(buf);
}

void set_tracio_daq_ctrl(EthernetServer& server){
  // Set sample size in bits to default and sample rate to 96 kHz
  // tracstream protocol will handle 16 or 24 bit sample size
  char buf[12] = {0xF5, 0xA3, 0x07, 0x00, 0x0C, _SET, 0x00, 0x00, 0x00, 0x01, 0x77, 0x00};
  server.write(buf);
}

void get_tracio_daq_ctrl(EthernetServer& server){
  // Set sample size in bits to default and sample rate to 96 kHz
  // tracstream protocol will handle 16 or 24 bit sample size
  char buf[12] = {0xF5, 0xA3, 0x07, 0x00, 0x0C, GET, 0x00, 0x00, 0x00, 0x01, 0x77, 0x00};
  server.write(buf);
}

void get_tracio_daq_status(EthernetServer& server){
  char buf[7] = {0xF5, 0xA3, 0x08, 0x00, 0x07, GET};
  server.write(buf);
}

void get_tracio_daq_ch_status(EthernetServer& server){
  char buf[28] = {0xF5, 0xA3, 0x0A, 0x00, 0x1C, GET};
  server.write(buf);
}

void get_tracio_stream_ctrl(EthernetServer& server){
  char buf[7] = {0xF5, 0xA3, 0x0E, 0x00, 0x08, GET};
  server.write(buf);
}

void set_tracio_stream_ctrl(EthernetServer& server, bool enable){
  char buf[7] = {0xF5, 0xA3, 0x0E, 0x00, 0x08, _SET, (char) enable};
  server.write(buf);
}

void get_tracio_stream_status(EthernetServer& server){
  char buf[7] = {0xF5, 0xA3, 0x0F, 0x00, 0x07, GET, 0x01};
  server.write(buf);
}

void get_tracio_error_codelog(EthernetServer& server){
  char buf[6] = {0xF5, 0xA3, 0x19, 0x00, 0x06, GET};
  server.write(buf);
}

void reset_tracio_error_codelog(EthernetServer& server){
  char buf[6] = {0xF5, 0xA3, 0x19, 0x00, 0x06, _RESET};
  server.write(buf);
}

tracStatus decodeTracIO(char* buf){
  if (!(buf[5]==ANNOUNCE && buf[0]==0xF5 && buf[1]==0xA3)){
    return INVALID;
  }
  char id = buf[2];
  char flgs;
  char flow;
  char errCount;
  char itf0;
  char itf1;
  char itf2;
  char itf3;
  char featureByte1;
  tracStatus retVal; 
  switch (id){
    case 0x00:
    {
      flow = buf[6];
      retVal = (flow&0x01) ? OK : ERR;
      break;
    }
    case 0x01:
    {
      featureByte1 = buf[26]; // I think the documentation is wrong, there are only two bytes worth of information 
      retVal = (featureByte1 & 0x04) ? OK : ERR;
      break;
    }
    case 0x02:
    {
      retVal = OK; // announcement for this command only happens if name is changed, which is unlikely
      break;
    }
    case 0x03:
    {
      // Ensure device is active and not misconfigured. Checks ethernet too, but maybe redundant
      retVal = ERR;

      itf0 = buf[7];
      itf1 = buf[8];
      itf2 = buf[9];
      itf3 = buf[10];

      if ( ((itf0 | itf1 | itf2 | itf3) & 0x01)) retVal = OK; // ensure ITF is working
      break;
    }
    case 0x04:
    {
      retVal = OK; // Currently doesn't matter
      break;
    }
    case 0x07:
    {
      uint32_t sampleRate = (buf[8] << 24) + (buf[9] << 16) + (buf[10] << 8) + buf[11];
      uint8_t sampleSize = buf[7];
      retVal = (sampleRate==SAMPLE_RATE && (sampleSize==0x00 || sampleSize==SAMPLE_SIZE)) ? OK : ERR; //ensure sample rate and size are correct
      break;
    }
    case 0x08:
    {
      flgs = buf[6];
      retVal = ((flgs & 0x01) && !(flgs & 0x02)) ? OK : ERR; // Ensure DAQ is running
      break;
    }
    case 0x0A:
    {
      retVal = OK; // Currently doesn't matter
      break;
    }
    case 0x0E:
    {
      flgs = buf[6];
      retVal = (flgs & 0x01) ? OK : ERR;
      break;
    }
    case 0x0F:
    {
      flgs = buf[6];
      retVal = (flgs & 0x01) && !(flgs & 0x02) && !(flgs & 0x04) ? OK : ERR;
      break;
    }
    case 0x19:
    {
      errCount = buf[6];
      if (errCount != 0x00) retVal = ERR;
      // TODO: read and log the error timestamps and codes
      break;
    }
  }
  return retVal;
}

void readIntoBuffer(uint16_t size, char* buffer, EthernetClient& client){
  for (uint16_t i=0; i<size; i++){
    buffer[i] = client.read();
  }
}

void porpoiseSetup(EthernetServer& tracIOServer, EthernetClient& client, char* tracIOBuffer){
  // Ensure device has streaming capability and check device status
  tracStatus status = ERR;
  readIntoBuffer(512, tracIOBuffer, client); // clear out buffer
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);

  do {
    get_tracio_info(tracIOServer);
    readIntoBuffer(29, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);
  } while (status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);

  // Set data streaming to be on
  do {
    set_tracio_stream_ctrl(tracIOServer, true);
    readIntoBuffer(8, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);
  } while (status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);
  
  // Set DAQ to sample at 96kHz/24 bits
  do {
    set_tracio_daq_ctrl(tracIOServer);
    readIntoBuffer(12, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);
  } while(status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);

  // Ensure DAQ is acquiring data samples 
  do {
    get_tracio_daq_ctrl(tracIOServer);
    readIntoBuffer(12, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);

  } while (status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);

  // Check status is OK
  do{
    get_tracio_status(tracIOServer);
    readIntoBuffer(25, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);
  } while (status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);

  // Check stream status is OK
  do {
    get_tracio_stream_status(tracIOServer);
    readIntoBuffer(7, tracIOBuffer, client);
    status = decodeTracIO(tracIOBuffer);
  } while (status!=OK);
  memset(tracIOBuffer, 0, sizeof tracIOBuffer);
}