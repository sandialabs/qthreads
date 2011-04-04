#ifndef __COMMON_RCRTOOL_H__
#define __COMMON_RCRTOOL_H__

#include "perf_util.h"
#include "bcGen.h"

/**
 *  Resolve to "TRUE" if given thing is true, else "FALSE"
 */
#define tf(b) ((b)?"TRUE":"FALSE")

//Daemon configuration variable
extern char bDaemon;
extern char bVerbose;
extern char bOutputToNull;

struct MeterValue {
   double data;         // meter value
   uint64_t timestamp;  // nanosecond unit
   struct MeterValue *next;
};

void addMeterValue(struct MeterValue **q, double val, uint64_t ts);
double getAvgMeterValue(struct MeterValue **q, uint64_t tw);
double getMaxMeterValue(struct MeterValue **q, uint64_t tw);
int initSystemConfiguration(void);
int getNodeDirPath(int processorID, char *str);
int getSocketDirPath(int processorID, char *str);
int getCoreDirPath(int processorID, char *coreDirPath);
int getCoreMeterState(int processorID, int meterNum);
uint64_t getCoreMeterInterval(int processorID, int meterNum);
uint64_t getCoreMeterTimeWindow(int processorID, int meterNum);
uint64_t getCoreMeter(int processorID, int meterNum, int type);
int setCoreMeter(int processorID, int meterNum, int type, uint64_t value);
int getSocketMeterState(int processorID, int meterNum);
uint64_t getSocketMeterInterval(int processorID, int meterNum);
uint64_t getSocketMeterTimeWindow(int processorID, int meterNum);
uint64_t getSocketMeter(int processorID, int meterNum, int type);
int setSocketMeter(int processorID, int meterNum, int type, uint64_t value);
uint64_t getResetValue(void);
int setResetValue(void);

void die(char* msg);
void daemonize(char bOutputToNull);
void doWork(void);

#endif
