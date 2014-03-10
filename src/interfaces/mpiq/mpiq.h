#ifndef MPIQ_H
#define MPIQ_H

int MPIQ_Init(int * argc, char *** argv);
int MPIQ_Init_thread(int * argc, char *** argv, int required, int * provided);
int MPIQ_Finalize(void);

#endif
