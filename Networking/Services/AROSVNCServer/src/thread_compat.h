/*
**         $Filename: thread_compat.h $
**
**         Minimal thread.library compatible API implemented over pthreads.
**         thread.library is no longer part of AROS, so the original calls
**         are mapped 1:1 onto pthread.library.
**
**         GNU General Public License
*/

#ifndef THREAD_COMPAT_H
#define THREAD_COMPAT_H

#include <stdint.h>

void *CreateMutex(void);
void  DestroyMutex(void *mutex);
void  LockMutex(void *mutex);
void  UnlockMutex(void *mutex);

void *CreateCondition(void);
void  DestroyCondition(void *cond);
void  SignalCondition(void *cond);
void  WaitCondition(void *cond, void *mutex);

uint32_t CreateThread(void (*entry)(void *), void *data);
void     WaitThread(uint32_t thread, void **result);
void     ExitThread(void *result);

#endif /* THREAD_COMPAT_H */
