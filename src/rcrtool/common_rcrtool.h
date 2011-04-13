#ifndef __COMMON_RCRTOOL_H__
#define __COMMON_RCRTOOL_H__

#include "bcGen.h"

/**
 *  Resolve to "TRUE" if given thing is true, else "FALSE"
 */
#define tf(b) ((b)?"TRUE":"FALSE")

//Daemon configuration variable
extern char bDaemon;
extern char bVerbose;
extern char bOutputToNull;

typedef struct _MeterValue {
    double             value;      // meter value
    uint64_t           timestamp;  // nanosecond unit
    struct _MeterValue* next;
} MeterValue;

// RCRTool meter types
typedef enum _rcr_type {
    RCR_TYPE_IMMEDIATE = 0,
    RCR_TYPE_AVERAGE,
    RCR_TYPE_MAXIMUM,
    RCR_TYPE_SUM,
} RCR_type;

void     addMeterValue(MeterValue **q, double val, uint64_t ts);
double   getAvgMeterValue(MeterValue **q, uint64_t tw);
double   getMaxMeterValue(MeterValue **q, uint64_t tw);
int      initSystemConfiguration(void);
int      getNodeDirPath(int processorID, char* nodeDirPath);
int      getSocketDirPath(int processorID, char* socketDirPath);
int      getCoreDirPath(int processorID, char* coreDirPath);
int      getCoreMeterState(int processorID, int meterNum);
uint64_t getCoreMeterInterval(int processorID, int meterNum);
uint64_t getCoreMeterTimeWindow(int processorID, int meterNum);
uint64_t getCoreMeter(int processorID, int meterNum, RCR_type meterType);
int      setCoreMeter(int processorID, int meterNum, RCR_type meterType, uint64_t value);
int      getSocketMeterState(int processorID, int meterNum);
uint64_t getSocketMeterInterval(int processorID, int meterNum);
uint64_t getSocketMeterTimeWindow(int processorID, int meterNum);
uint64_t getSocketMeter(int processorID, int meterNum, RCR_type meterType);
int      setSocketMeter(int processorID, int meterNum, RCR_type meterType, uint64_t value);
uint64_t getResetValue(void);
int      setResetValue(void);

void     die(char* msg);
void     daemonize(char bOutputToNull);
void     doWork(int nshepherds, int nworkerspershep);

#endif
