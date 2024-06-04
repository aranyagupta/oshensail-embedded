#include <LwIP.h>
#include <STM32Ethernet.h>
#include "EthernetUdp.h"
#include "config.hpp"

typedef enum {ERR, OK, INVALID, FINISH} tracStatus;

void readIntoBuffer(uint16_t size, char* buffer, EthernetClient& client);

void porpoiseSetup(EthernetServer& tracIOServer, EthernetClient& client, char* tracIOBuffer);

tracStatus parseDatagram(char* liveData, char* longStore);

void receiveDatagram(EthernetUDP& tracStreamServer, char* tracStreamBuffer);

tracStatus decodeTracIO(char* buf);