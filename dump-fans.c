
/**
 *
 * #define FANS_COUNT		    "FNum" //r-o length 1
 * #define FANS_MANUAL		  "FS! " //r-w length 2
 *
 * #define FAN_ACTUAL_SPEED	"F0Ac" //r-o length 2
 * #define FAN_MIN_SPEED		"F0Mn" //r-o length 2
 * #define FAN_MAX_SPEED		"F0Mx" //r-o length 2
 * #define FAN_SAFE_SPEED		"F0Sf" //r-o length 2
 * #define FAN_TARGET_SPEED	"F0Tg" //r-w length 2
 *
 * static const char* temperature_sensors_sets[][8] = {
 * 	{ "TB0T", "TC0D", "TC0P", "Th0H", "Ts0P", "Th1H", "Ts1P", NULL },
 * };
 *
 * Most values are FP78 coded, which means shifted by 2:
 *   fp78 = value << 2
 *   
 **/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include "dump-fans.h"


io_connect_t _smcPort=(io_connect_t)0;


UInt32 _strtoul(char *str, int size, int base) {
  UInt32 total=0;
  for(int i=0;i<size;i++)
    if(base==16)
      total+=(str[i]<<(size-1-i)*8);
    else
      total+=(unsigned char)(str[i]<<(size-1-i)*8);
  return total;
}

void _ultostr(char *str, UInt32 val) {
  str[0]='\0';
  sprintf(str, "%c%c%c%c", (unsigned int)val>>24,
                           (unsigned int)val>>16,
                           (unsigned int)val>>8,
                           (unsigned int)val);
}

float _strtof(char *str, int size, int e) {
  float total=0;
  for(int i=0;i<size;i++)
    if(i==(size-1))
      total+=(str[i] & 0xff)>>e;
    else
      total+=str[i]<<(size-1-i)*(8-e);
  return total;
}


// open an IOKit connection to the SMC service
int smcOpen(void) {
  // get a service handle
  io_service_t service;
  service=IOServiceGetMatchingService(kIOMasterPortDefault,
                                      IOServiceMatching("AppleSMC"));
  if(!service)
    return -1;

  // open the service
  kern_return_t kr=IOServiceOpen(service, mach_task_self(), 0, &_smcPort);
  IOObjectRelease(service);
  if(kr!=kIOReturnSuccess)
    return -2;

  // everything went fine
  return 0;
}

// close the IOKit SMC service connection
void smcClose(void) {
  IOServiceClose(_smcPort);
}

// execute a call to the SMC service
kern_return_t smcCall(int index, SMCKeyData_t *inputStruct, SMCKeyData_t *outputStruct) {
  size_t inputStructCnt=sizeof(SMCKeyData_t);
  size_t outputStructCnt=sizeof(SMCKeyData_t);

  return IOConnectCallStructMethod(_smcPort,
                                   index,
                                   inputStruct, inputStructCnt,
                                   outputStruct, &outputStructCnt);
}

// read the value of an SMC key
kern_return_t smcReadKey(UInt32Char_t key, SMCVal_t *val) {
  // zuerst die Datenstrukturen initialisiseren
  SMCKeyData_t  inputStruct;
  memset(&inputStruct, 0, sizeof(SMCKeyData_t));
  SMCKeyData_t  outputStruct;
  memset(&outputStruct, 0, sizeof(SMCKeyData_t));
  kern_return_t result;
  memset(val, 0, sizeof(SMCVal_t));

  // jetzt lesen wir die Anzahl Datenbytes fuer den Key aus
  inputStruct.key=_strtoul(key, 4, 16);
  inputStruct.data8=SMC_CMD_READ_KEYINFO;    
  result=smcCall(KERNEL_INDEX_SMC, &inputStruct, &outputStruct);
  if(result!=kIOReturnSuccess)
      return result;

  // nun koennen wir die eigentlichen Daten lesen
  val->dataSize=outputStruct.keyInfo.dataSize;
  _ultostr(val->dataType, outputStruct.keyInfo.dataType);
  inputStruct.keyInfo.dataSize = val->dataSize;
  inputStruct.data8=SMC_CMD_READ_BYTES;
  result=smcCall(KERNEL_INDEX_SMC, &inputStruct, &outputStruct);
  if(result!=kIOReturnSuccess)
      return result;

  // Daten speichern und raus
  memcpy(val->bytes, outputStruct.bytes, sizeof(outputStruct.bytes));
  return kIOReturnSuccess;
}

// print information about all fans in the machine
void dumpFans(void) {
  SMCVal_t val;
  char key[5];

  smcReadKey("FNum", &val);
  int fanCnt=_strtoul(val.bytes, val.dataSize, 10); 
  printf("Found %d fans:\n", fanCnt);
  for(int fan=0; fan<fanCnt; fan++) {
    printf("  fan %d:\n", fan);

    sprintf(key, "F%dAc", fan);
    smcReadKey(key, &val); 
    printf("    Actual speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));

    sprintf(key, "F%dMn", fan);
    smcReadKey(key, &val);
    printf("    Minimum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));

    sprintf(key, "F%dMx", fan);
    smcReadKey(key, &val);
    printf("    Maximum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));

    sprintf(key, "F%dSf", fan);
    smcReadKey(key, &val);
    printf("    Safe speed   : %.0f\n", _strtof(val.bytes, val.dataSize, 2));

    sprintf(key, "F%dTg", fan);
    smcReadKey(key, &val);
    printf("    Target speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));

    smcReadKey("FS! ", &val);
    if((_strtoul(val.bytes, 2, 16) & (1<<fan))==0)
      printf("    Mode         : auto\n"); 
    else
      printf("    Mode         : forced\n");
  }
}

void dumpTemperature(char *key, char *desc) {
  SMCVal_t val;
  kern_return_t result=smcReadKey(key, &val);
  if(result==kIOReturnSuccess) {
    int intValue=(val.bytes[0]*256+val.bytes[1])>>2;
    float temp=intValue/64.0;
    printf("%-25s (%s): %f\n", desc, key, temp);
  }
  else 
    printf("%-25s (%s): unable to query sensor!\n", desc, key);
}

// program entry point
int main(void) {
  smcOpen();

  dumpFans();

  printf("\nTemperatures:\n");
  dumpTemperature("TC0D", "  CPU (diode)");
  dumpTemperature("TC0P", "  CPU (pin)");
  dumpTemperature("TG0H", "  GPU (heatsink)");
  dumpTemperature("TG0P", "  GPU (pin)");
  dumpTemperature("TG0T", "  GPU (transistor)");
  dumpTemperature("TM0P", "  Memory 0 (pin)");
  dumpTemperature("Tm0P", "  Memory Controller (pin)");
  dumpTemperature("TB0T", "  Enclosure");
  dumpTemperature("Th0H", "  Heatsink 0");
  dumpTemperature("Th1H", "  Heatsink 1");
  dumpTemperature("Ts0P", "  Slot 0 (pin)");
  //dumpTemperature("TTF0", "  TTF0"); ???

  smcClose();
}
