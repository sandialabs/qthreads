#include<qthread/logging.h>
#define LVL1 1
#define LVL2 0
#define LVL3 1

int main(){
  log(LOGERR, "This is LOGERR");
  log(LOGWARN, "This is LOGWARN");
  logargs(LOGERR, "This is LOGERR with arg of 5 in parentheses: (%d)", 5);
  log(LVL1, "You should see this as LVL1");
  logargs(LVL1, "Again with LVL1, with args 1,2,\"foo\" -> (%d, %d, %s)", 1, 2, "foo");
  log(LVL2, "You should NOT see this message.");
  log(LVL3, "You should see this as LVL3");
  return 0;
}
