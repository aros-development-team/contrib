/*
**         $Filename: thread_compat.c $
**
**         Minimal thread.library compatible API implemented over pthreads.
**
**         GNU General Public License
*/

#include <stdlib.h>
#include <pthread.h>

#include "thread_compat.h"

void *CreateMutex(void)
{
    pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));

    if (mutex && pthread_mutex_init(mutex, NULL) != 0)
    {
        free(mutex);
        mutex = NULL;
    }
    return mutex;
}

void DestroyMutex(void *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    free(mutex);
}

void LockMutex(void *mutex)
{
    pthread_mutex_lock((pthread_mutex_t *)mutex);
}

void UnlockMutex(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

void *CreateCondition(void)
{
    pthread_cond_t *cond = malloc(sizeof(pthread_cond_t));

    if (cond && pthread_cond_init(cond, NULL) != 0)
    {
        free(cond);
        cond = NULL;
    }
    return cond;
}

void DestroyCondition(void *cond)
{
    pthread_cond_destroy((pthread_cond_t *)cond);
    free(cond);
}

void SignalCondition(void *cond)
{
    pthread_cond_signal((pthread_cond_t *)cond);
}

void WaitCondition(void *cond, void *mutex)
{
    pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

struct threadstart
{
    void (*entry)(void *);
    void  *data;
};

static void *threadtrampoline(void *arg)
{
    struct threadstart start = *(struct threadstart *)arg;

    free(arg);
    start.entry(start.data);
    return NULL;
}

uint32_t CreateThread(void (*entry)(void *), void *data)
{
    pthread_t thread;
    pthread_attr_t attr;
    struct threadstart *start = malloc(sizeof(struct threadstart));
    int ret;

    if (!start)
        return 0;

    start->entry = entry;
    start->data  = data;

    /*
     * The default pthread stack (PTHREAD_STACK_MIN, 40KB) is too small for
     * the framebuffer/encoder threads once the captured screen gets large:
     * a 3840-wide screen overran it and took the whole machine down.
     */
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 512 * 1024);
    ret = pthread_create(&thread, &attr, threadtrampoline, start);
    pthread_attr_destroy(&attr);
    if (ret != 0)
    {
        free(start);
        return 0;
    }
    return (uint32_t)thread;
}

void WaitThread(uint32_t thread, void **result)
{
    pthread_join((pthread_t)thread, result);
}

void ExitThread(void *result)
{
    pthread_exit(result);
}
