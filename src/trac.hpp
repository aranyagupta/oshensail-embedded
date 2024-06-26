#include "config.hpp"
#include "FreeRTOS_IP.h"

typedef enum {ERR, OK, INVALID, FINISH} tracStatus;

tracStatus parseDatagram(char* liveData, char* longStore);
