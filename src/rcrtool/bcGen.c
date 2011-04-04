/**
 * Breadcrumbs generation for RCR Daemon. 
 * By Anirban Mandal
 *
 */

#include "bcGen.h"
#include "qt_rcrtool.h"
#include <stdio.h>
#include <errno.h>

/***********************************************************************
* definition for the trigger map; used by all methods
***********************************************************************/
Trigger** triggerMap  = NULL; // allocated and pupulated in buildTriggerMap()
int       numTriggers = 0;    // populated in buildTriggerMap()

int getNumTriggers(void) {
    return numTriggers;
}

Trigger** getTriggers(void) {
    return triggerMap;
}

/***********************************************************************************************
* Reads from a trigger file and builds the triggerMap
* triggerMap is an array of TriggerMap structs
***********************************************************************************************/
/**
 * Reads from a trigger file and builds the <i>triggerMap<i/>. <i>triggerMap</i>
 * is an array of TriggerMap structs 
 * 
 * @param fileName Name of the trigger definition file.
 */
void buildTriggerMap(const char *fileName){
    int triggerCount = 0; // counts the number of triggers in the triggerFile
    char line[BUFSIZ];
    char *nextField;

    printf("Reading from trigger file: %s \n", fileName);

    // Check if file exists
    FILE *triggerFile = fopen(fileName, "r");
    if (triggerFile == NULL) {
        printf("Can't open trigger file !! \n");
        return;
    }

    // Read each line, allocate memory for triggerMap and populate elements of the triggerMap array
    while (fgets(line, BUFSIZ, triggerFile) != NULL) {

        // Allocate memory for the next trigger entry
        triggerMap = (Trigger **) realloc(triggerMap, (triggerCount + 1) * sizeof(Trigger *));
        triggerMap[triggerCount] = (Trigger *) malloc(sizeof(Trigger));

        // Parsing type
        nextField = (char *) strtok(line, ",");
        nextField = trim(nextField); // remove trailing and leading white spaces
        if (strcmp(nextField, "TYPE_CORE") == 0) {
            triggerMap[triggerCount]->type = TYPE_CORE;
        } else if (strcmp(nextField, "TYPE_SOCKET") == 0) {
            triggerMap[triggerCount]->type = TYPE_SOCKET;
        } else if (strcmp(nextField, "TYPE_NODE") == 0) {
            triggerMap[triggerCount]->type = TYPE_NODE;
        } else {
            printf("Unknown trigger type: type must be TYPE_CORE or TYPE_SOCKET or TYPE_NODE \n");
            printf("Ignoring the rest of the trigger file \n");
            break;
        }

        // Parsing id
        nextField = (char *) strtok(NULL, ",");
        triggerMap[triggerCount]->id = atoi(nextField);

        // Parsing trigger flag shared memory key
        nextField = (char *) strtok(NULL, ",");
        triggerMap[triggerCount]->flagShmKey = atoi(nextField);

        // Parsing application state shared memory key
        nextField = (char *) strtok(NULL, ",");
        triggerMap[triggerCount]->appStateShmKey = atoi(nextField);

        // Parsing meterName
        nextField = (char *) strtok(NULL, ",");
        nextField = trim(nextField);
        triggerMap[triggerCount]->meterName = (char *) malloc((1+strlen(nextField))*sizeof(char));
        strcpy(triggerMap[triggerCount]->meterName , nextField);

        // Parsing threshold lower bound
        nextField = (char *) strtok(NULL, ",");
        triggerMap[triggerCount]->threshold_lb = atof(nextField);

        // Parsing threshold upper bound
        nextField = (char *) strtok(NULL, ",");
        triggerMap[triggerCount]->threshold_ub = atof(nextField);

        triggerCount++;

    } // End reading from trigger file

    fclose(triggerFile);

    numTriggers = triggerCount;

    printTriggerMap();

}

/*!
 * Check if condition for leaving breadcrumbs is met and leave breadcrumbs. Will
 * only put breadcrumbs for meters appearing in the trigger file.
 * 
 * \param triggerType
 * \param socketOrCoreID
 * \param meterName
 * \param currentVal
 * 
 * \return 1 if a a trigger is hit, otherwise return 0.
 */
int putBreadcrumbs(rcrtool_trigger_type triggerType, int socketOrCoreID, const char *meterName, double currentVal){
#ifdef QTHREAD_RCRTOOL
#define MAX_TRIGGERS 64
    static int flipped[MAX_TRIGGERS];// = 0;
#endif
    int i = 0;
    int triggerHit = 0;
    char c;
    // Check if a triggerMap exists; if not, just return
    if (triggerMap == NULL || numTriggers == 0) {
        printf("** BC: triggerMap is NULL; Can't put breadcrumbs\n");
        return 0;
    }

    for (i = 0; i < numTriggers; i++) {
        if ((triggerMap[i]->type == triggerType) && (triggerMap[i]->id == socketOrCoreID) && (strcmp(triggerMap[i]->meterName, meterName) == 0)) {
            if ((currentVal < triggerMap[i]->threshold_lb) || (currentVal > triggerMap[i]->threshold_ub)) {//trigger cond
#ifdef QTHREAD_RCRTOOL
                if (i < MAX_TRIGGERS && flipped[i] != 1) {
                    flipped[i] = 1;
                    rcrtool_log(RCR_RATTABLE_DEBUG, XOMP_RAT_DEBUG, -1, 0, triggerMap[i]->meterName);
                    rcrtool_log(RCR_RATTABLE_DEBUG, XOMP_RAT_DEBUG, -1, triggerMap[i]->id, " %d ****************************************************\n");
                }
                dumpAppState(triggerMap[i]->appStateShmKey, triggerType, socketOrCoreID, meterName);
#endif
                triggerHit = 1;
                c = '1'; // Set the flag
                shmPutFlag(triggerMap[i]->flagShmKey , c);
            } else {
#ifdef QTHREAD_RCRTOOL
                if (i < MAX_TRIGGERS && flipped[i] != 0) {
                    flipped[i] = 0;
                    rcrtool_log(RCR_RATTABLE_DEBUG, XOMP_RAT_DEBUG, -1, 0, triggerMap[i]->meterName);
                    rcrtool_log(RCR_RATTABLE_DEBUG, XOMP_RAT_DEBUG, -1, triggerMap[i]->id, " %d ####################################################\n");
                }
#endif
                c = '0'; // Unset
                shmPutFlag(triggerMap[i]->flagShmKey , c);
            }
        }
    }
    return triggerHit;
}

/**
 * Check if any breadcrumbs have been dropped. Will only check breadcrumbs for
 * meters appearing in the trigger file.
 * 
 * @param type Type of trigger.  From the <i>rcrtool_trigger_type</i> enum.
 * @param id Corresponding Node, Socket, or Core id number.
 * @param meterName Name of trigger as defined in the triggers definition file.
 *  
 * @return Value of the breadcrumb.  Should normally be either '0' or '1'. 
 *         Returns '0' on failure to read breadcrumb.
 */
char getBreadcrumbs(rcrtool_trigger_type triggerType, int id, char *meterName){

    int i = 0;
    // Check if a triggerMap exists; if not, just return
    if (triggerMap == NULL || numTriggers == 0) {
        printf("** BC: triggerMap is NULL; Can't get breadcrumbs\n");
        return '\0';
    }

    for (i = 0; i < numTriggers; i++) {
        if ((triggerMap[i]->type == triggerType) && (triggerMap[i]->id == id) && (strcmp(triggerMap[i]->meterName, meterName) == 0)) {
            return shmGet(triggerMap[i]->flagShmKey);
        }
    }
    return '\0';
}

/** Shared memory get function. 
 * 
 * @param key 
 * 
 * @return char 
 */
char shmGet(key_t key){

    int shmid;
    char *shm;
    char returnVal;

    if ((shmid = shmget(key, sizeof(char), 0666)) < 0) {
        printf("** BC: Could not locate shared memory segment for key: %d\n", key);
        // shm location for this key doesn't have a shared memory id
        // inserting breadcrumb for the first time
        // create a shared mameory id for this key
        if ((shmid = shmget(key, sizeof(char), IPC_CREAT | 0666)) < 0) {
            printf("** BC: Could not create new shared memory segment for key: %d\n", key);
            perror("shmget");
            return '\0';
        }
    }

    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        printf("** BC: Could not attach to the shared memory segment with shmid: %d \n ", shmid);
        return '\0';
    }

    returnVal = *shm;

    releaseShmLoc(shm);

    return returnVal;
}

/***********************************************************************************************
* shared memory put function
***********************************************************************************************/

void shmPutFlag(key_t key, char val){

    int shmid;
    char *shm;

    if ((shmid = shmget(key, sizeof(char), 0666)) < 0) {
        printf("** BC: Could not locate shared memory segment for key: %d\n", key);
        // shm location for this key doesn't have a shared memory id
        // inserting breadcrumb for the first time
        // create a shared memory id for this key
        if ((shmid = shmget(key, sizeof(char), IPC_CREAT | 0666)) < 0) {
            printf("** BC: Could not create new shared memory segment for key: %d\n", key);
            perror("shmget");
            return;
        }
    }

    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        printf("** BC: Could not attach to the shared memory segment with shmid: %d \n ", shmid);
        return;
    }

    // Set the val passed at the shm location
    //printf("** BC: Putting %c in shared memory location with id: %d \n", val, shmid);
    *shm = val;

    releaseShmLoc(shm);
}

char* getShmStringLoc(key_t key, size_t size) {
    static size_t allocatedSize = 0;
    key = 9253;
    size = 64;
    //if (size <= allocatedSize) {
    //} else ;
    int shmid;
    char *shm;

    if ((shmid = shmget(key, sizeof(char) * size, 0666)) < 0) {
        printf("** BC: Could not locate shared memory segment for key: %d\n", key);
        if (0 && errno == EINVAL) {
            //exists but size too small.  Get it by asking for smallest size and delete it.
            if ((shmid = shmget(key, 1, 0666)) < 0) {
                //failure
                perror("shmget");
                return 0;
            }
            struct shmid_ds my_shmid_ds;
            shmctl(shmid, IPC_RMID, &my_shmid_ds);
            //if (shmdt(shm) == -1) {
            //    printf("shmdt failed\n");
            //    return 0;
            //}
        }
        // shm location for this key doesn't have a shared memory id inserting
        // breadcrumb for the first time or after reallocation create a shared
        // memory id for this key
        if ((shmid = shmget(key, sizeof(char) * size, IPC_CREAT | 0666)) < 0) {
            printf("** BC: Could not create new shared memory segment for key: %d\n", key);
            perror("shmget");
            return 0;
        }
    }

    if ((shm = shmat(shmid, NULL, 0)) == (void*)-1) {
        perror("shmat");
        printf("** BC: Could not attach to the shared memory segment with shmid: %d \n ", shmid);
        return 0;
    }

    return (char*)shm;
}

void releaseShmLoc(const void* shm) {
  if (shmdt(shm) == -1) {
      perror("shmdt");
      printf("shmdt failed\n");
  }
}

# ifdef QTHREAD_RCRTOOL
/*!
 * Write the application state out to shared memory.
 * 
 * \param key Shared memory key.
 * \param triggerType Not currently used.
 * \param socketOrCoreID Not currently used.
 * \param meterName Not currently used.
 */
void dumpAppState(key_t key, rcrtool_trigger_type triggerType, int socketOrCoreID, const char *meterName) {
    size_t appStateSize = 1024;
    int i = 0;
    //printf("%d @@\n", RCRParallelSectionStackPos);
    appStateSize += (RCR_HASH_ENTRY_SIZE + 10) * RCRParallelSectionStackPos + RCR_APP_NAME_MAX_SIZE;
    char *shmDest = getShmStringLoc(key, appStateSize);
    char *shmRover = shmDest;
    strncpy(shmRover, RCRAppName, RCR_APP_NAME_MAX_SIZE);
    shmRover += strlen(RCRAppName);
    for (i = 0; i <= RCRParallelSectionStackPos; i++) {
        printf("%s %d ^^^^\n", hashTable[RCRParallelSectionStack[i]].funcName, hashTable[RCRParallelSectionStack[i]].count);
        *shmRover++ = '|';
        strncpy(shmRover, hashTable[RCRParallelSectionStack[i]].funcName, RCR_HASH_ENTRY_SIZE);
        shmRover += strlen(hashTable[RCRParallelSectionStack[i]].funcName);
        *shmRover++ = '|';
        sprintf(shmRover, "%8d", hashTable[RCRParallelSectionStack[i]].count);
        shmRover += 8;
    }

    //Do the writing.
    releaseShmLoc(shmDest);
}
# endif

/***********************************************************************************************
* Prints triggerMap
***********************************************************************************************/
void printTriggerMap(){
    int i;
    printf("Number of triggers: %d\n", numTriggers);
    for (i = 0; i < numTriggers; i++) {
        printf("%d \t %d \t %d \t %d \t %s \t %f \t %f \n", triggerMap[i]->type, triggerMap[i]->id, triggerMap[i]->flagShmKey, triggerMap[i]->appStateShmKey, triggerMap[i]->meterName, triggerMap[i]->threshold_lb, triggerMap[i]->threshold_ub);
    }
}
/**************************************************************************************
Trim strings with white spaces
***************************************************************************************/
/* Remove leading whitespaces */
char *ltrim(char *const s)
{
    size_t len;
    char *cur;

    if (s && *s) {
        len = strlen(s);
        cur = s;

        while (*cur && isspace(*cur))
            ++cur, --len;

        if (s != cur)
            memmove(s, cur, len + 1);
    }

    return s;
}

/* Remove trailing whitespaces */
char *rtrim(char *const s)
{
    if (s && *s) {
        char *cur = s + strlen(s) - 1;

        while (cur != s && isspace(*cur))
            --cur;

        cur[isspace(*cur) ? 0 : 1] = '\0';
    }

    return s;
}

/* Remove leading and trailing whitespaces */
char *trim(char *const s)
{
    rtrim(s);
    ltrim(s);

    return s;
}
