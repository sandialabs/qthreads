#ifndef OMP_AFFINITY_H
#define OMP_AFFINITY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef QTHREAD_OMP_AFFINITY
void qthread_disable_stealing (
    void);

void qthread_enable_stealing (
    unsigned int stealing_mode);

void qthread_child_task_affinity (
    unsigned int shep);
#endif

#ifdef __cplusplus
}
#endif

#endif
