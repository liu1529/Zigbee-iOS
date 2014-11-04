/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Thread.c
 *
 * REVISION:           $Revision: 53151 $
 *
 * DATED:              $Date: 2013-04-09 16:14:34 +0100 (Tue, 09 Apr 2013) $
 *
 * AUTHOR:             Matt Redfearn
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the 
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <Threads.h>
#include <Trace.h>
#include <unistd.h>

#define DBG_THREADS 0
#define DBG_LOCKS   0
#define DBG_QUEUE   0

#define THREAD_SIGNAL SIGUSR1

/************************** Threads Functionality ****************************/

/** Structure representing an OS independant thread */
typedef struct
{
#ifndef WIN32
    pthread_t thread;
#else
    int thread_pid;
    HANDLE thread_handle;
#endif /* WIN32 */
} tsThreadPrivate;


#ifndef WIN32
/** Signal handler to receive THREAD_SIGNAL.
 *  This is just used to interrupt system calls such as recv() and sleep().
 */
static void thread_signal_handler(int sig)
{
    DBG_vPrintf(DBG_THREADS, "Signal %d received\n", sig);
}
#endif /* WIN32 */


teThreadStatus eThreadStart(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState)
{
    tsThreadPrivate *psThreadPrivate;
    
    psThreadInfo->eState = E_THREAD_STOPPED;
    
    DBG_vPrintf(DBG_THREADS, "Start Thread %p to run function %p\n", psThreadInfo, prThreadFunction);
    
    psThreadPrivate = malloc(sizeof(tsThreadPrivate));
    if (!psThreadPrivate)
    {
        return E_THREAD_ERROR_NO_MEM;
    }
    
    psThreadInfo->pvPriv = psThreadPrivate;
    
    psThreadInfo->eThreadDetachState = eDetachState;
#ifndef WIN32
    {
        static int iFirstTime = 1;
        
        if (iFirstTime)
        {
            /* Set up sigmask to receive configured signal in the main thread. 
             * All created threads also get this signal mask, so all threads
             * get the signal. But we can use pthread_signal to direct it at one.
             */
            struct sigaction sa;

            sa.sa_handler = thread_signal_handler;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);

            if (sigaction(THREAD_SIGNAL, &sa, NULL) == -1) 
            {
                perror("sigaction");
            }
            else
            {
                DBG_vPrintf(DBG_THREADS, "Signal action registered\n\r");
                iFirstTime = 0;
            }
        }
    }
    
    if (pthread_create(&psThreadPrivate->thread, NULL,
        prThreadFunction, psThreadInfo))
    {
        perror("Could not start thread");
        return E_THREAD_ERROR_FAILED;
    }

    if (eDetachState == E_THREAD_DETACHED)
    {
        DBG_vPrintf(DBG_THREADS, "Detach Thread %p\n", psThreadInfo);
        if (pthread_detach(psThreadPrivate->thread))
        {
            perror("pthread_detach()");
            return E_THREAD_ERROR_FAILED;
        }
    }
#else
    psThreadPrivate->thread_handle = CreateThread(
        NULL,                           // default security attributes
        0,                              // use default stack size  
        prThreadFunction,               // thread function name
        psThreadInfo->pvData,           // argument to thread function 
        0,                              // use default creation flags 
        &psThreadPrivate->thread_pid);  // returns the thread identifier
    if (!psThreadPrivate->thread_handle)
    {
        perror("Could not start thread");
        return E_THREAD_ERROR_FAILED;
    }
#endif /* WIN32 */
    return  E_THREAD_OK;
}


teThreadStatus eThreadStop(tsThread *psThreadInfo)
{
    tsThreadPrivate *psThreadPrivate = (tsThreadPrivate *)psThreadInfo->pvPriv;
    
    DBG_vPrintf(DBG_THREADS, "Stopping Thread %p\n", psThreadInfo);
    
    psThreadInfo->eState = E_THREAD_STOPPING;
    
    if (psThreadPrivate)
    {
    
#ifndef WIN32
        /* Send signal to the thread to kick it out of any system call it was in */
        pthread_kill(psThreadPrivate->thread, THREAD_SIGNAL);
        DBG_vPrintf(DBG_THREADS, "Signaled Thread %p\n", psThreadInfo);
        
        if (psThreadInfo->eThreadDetachState == E_THREAD_JOINABLE)
        {
            /* Thread is joinable */
            if (pthread_join(psThreadPrivate->thread, NULL))
            {
                perror("Could not join thread");
                return E_THREAD_ERROR_FAILED;
            }
            /* We can now free the thread private info */
            free(psThreadPrivate);
        }
        else
        {
            DBG_vPrintf(DBG_THREADS, "Cannot join detached thread %p\n", psThreadInfo);
            return E_THREAD_ERROR_FAILED;
        }
#else

#endif /* WIN32 */
    }
    
    DBG_vPrintf(DBG_THREADS, "Stopped Thread %p\n", psThreadInfo);
    psThreadInfo->eState = E_THREAD_STOPPED;
    return  E_THREAD_OK;
}


teThreadStatus eThreadFinish(tsThread *psThreadInfo)
{
    /* Free the Private data */
    tsThreadPrivate *psThreadPrivate = (tsThreadPrivate *)psThreadInfo->pvPriv;
    
    if (psThreadInfo->eThreadDetachState == E_THREAD_DETACHED)
    {
        /* Only free this if we are detached, otherwise another thread won't be able to join. */
        free(psThreadPrivate);
    }
    psThreadInfo->eState = E_THREAD_STOPPED;
    
    DBG_vPrintf(DBG_THREADS, "Finish Thread %p\n", psThreadInfo);
    
#ifndef WIN32
    /* Cleanup function is called when pthread quits */
    pthread_exit(NULL);
#else
    ExitThread(0);
#endif /* WIN32 */
    return E_THREAD_OK; /* Control won't get here */
}


teThreadStatus eThreadYield(void)
{
#ifndef WIN32
    sched_yield();
#else
    SwitchToThread();
#endif
    return E_THREAD_OK;
}


/************************** Lock Functionality *******************************/


typedef struct
{
#ifndef WIN32
     pthread_mutex_t mutex;
#else
     
#endif /* WIN32 */
} tsLockPrivate;

teLockStatus eLockCreate(tsLock *psLock)
{
    tsLockPrivate *psLockPrivate;
    
    psLockPrivate = malloc(sizeof(tsLockPrivate));
    if (!psLockPrivate)
    {
        return E_LOCK_ERROR_NO_MEM;
    }
    
    psLock->pvPriv = psLockPrivate;
    
#ifndef WIN32
    {
        /* Create a recursive mutex, as we need to allow the same thread to lock mutexes a number of times */
        pthread_mutexattr_t     attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        if (pthread_mutex_init(&psLockPrivate->mutex, &attr) != 0)
        {
            DBG_vPrintf(DBG_LOCKS, "Error initialising mutex\n");
            return E_LOCK_ERROR_FAILED;
        }
    }
#else
    
#endif
    DBG_vPrintf(DBG_LOCKS, "Lock Create: %p\n", psLock);
    return E_LOCK_OK;
}


teLockStatus eLockDestroy(tsLock *psLock)
{
    tsLockPrivate *psLockPrivate = (tsLockPrivate *)psLock->pvPriv;
#ifndef WIN32
    pthread_mutex_destroy(&psLockPrivate->mutex);
#else
    
#endif
    free(psLockPrivate);
    DBG_vPrintf(DBG_LOCKS, "Lock Destroy: %p\n", psLock);
    return E_LOCK_OK;
}


teLockStatus eJIPLockLock(tsLock *psLock)
{
    tsLockPrivate *psLockPrivate = (tsLockPrivate *)psLock->pvPriv;
#ifndef WIN32
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx locking: %p\n", pthread_self(), psLock);
    pthread_mutex_lock(&psLockPrivate->mutex);
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx locked: %p\n", pthread_self(), psLock);
#else
    
    
#endif /* WIN32 */
    
    return E_LOCK_OK;
}


teLockStatus eJIPLockLockTimed(tsLock *psLock, uint32_t u32WaitTimeout)
{
    tsLockPrivate *psLockPrivate = (tsLockPrivate *)psLock->pvPriv;
    
#ifndef WIN32
    
    int32_t ecode = E_LOCK_ERROR_FAILED;
#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS - 200112L) >= 0L
    /* POSIX Timeouts are supported - option group [TMO] */
    {
        struct timeval sNow;
        struct timespec sTimeout;
        
        gettimeofday(&sNow, NULL);
        sTimeout.tv_sec = sNow.tv_sec + u32WaitTimeout;
        sTimeout.tv_nsec = sNow.tv_usec * 1000;
        
        DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx time locking: %p\n", pthread_self(), psLock);
        
        ecode = pthread_mutex_timedlock(&psLockPrivate->mutex, &sTimeout);
        
    }

    
#else
    
    /* Not OK to use the functions */
    ecode = pthread_mutex_trylock(&psLockPrivate->mutex);
    
    
#endif
    switch (ecode)
    {
    case (0):
        DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx: time locked: %p\n", pthread_self(), psLock);
        return E_LOCK_OK;
        break;
        
    case (ETIMEDOUT):
        DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx: time out locking: %p\n", pthread_self(), psLock);
        return E_LOCK_ERROR_TIMEOUT;
        break;
        
    default:
        DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx: error locking: %p\n", pthread_self(), psLock);
        return E_LOCK_ERROR_FAILED;
        break;
    }
    
#else
    //wind32
    
#endif /* WIN32 */
}


teLockStatus eJIPLockTryLock(tsLock *psLock)
{
    tsLockPrivate *psLockPrivate = (tsLockPrivate *)psLock->pvPriv;
#ifndef WIN32
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx try locking: %p\n", pthread_self(), psLock);
    if (pthread_mutex_trylock(&psLockPrivate->mutex) != 0)
    {
        DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx could not lock: %p\n", pthread_self(), psLock);
        return E_LOCK_ERROR_FAILED;
    }
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx locked: %p\n", pthread_self(), psLock);
#else
    
    
#endif /* WIN32 */
    
    return E_LOCK_OK;
}


teLockStatus eJIPLockUnlock(tsLock *psLock)
{
    tsLockPrivate *psLockPrivate = (tsLockPrivate *)psLock->pvPriv;
    
#ifndef WIN32
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx unlocking: %p\n", pthread_self(), psLock);
    pthread_mutex_unlock(&psLockPrivate->mutex);
    DBG_vPrintf(DBG_LOCKS, "Thread 0x%lx unlocked: %p\n", pthread_self(), psLock);
#else
    
    
#endif /* WIN32 */
    return E_LOCK_OK;
}


/************************** Queue Functionality ******************************/


typedef struct
{
    void **apvBuffer;
    uint32_t u32Capacity;
    uint32_t u32Size;
    uint32_t u32In;
    uint32_t u32Out;

#ifndef WIN32
    pthread_mutex_t mutex;    
    pthread_cond_t cond_space_available;
    pthread_cond_t cond_data_available;
#else
     
#endif /* WIN32 */
} tsQueuePrivate;


teQueueStatus eQueueCreate(tsQueue *psQueue, uint32_t u32Capacity)
{
    tsQueuePrivate *psQueuePrivate;
    
    psQueuePrivate = malloc(sizeof(tsQueuePrivate));
    if (!psQueuePrivate)
    {
        return E_QUEUE_ERROR_NO_MEM;
    }
    
    psQueue->pvPriv = psQueuePrivate;
    
    psQueuePrivate->apvBuffer = malloc(sizeof(void *) * u32Capacity);
    
    if (!psQueuePrivate->apvBuffer)
    {
        free(psQueue->pvPriv);
        return E_QUEUE_ERROR_NO_MEM;
    }
    
    psQueuePrivate->u32Capacity = u32Capacity;
    psQueuePrivate->u32Size = 0;
    psQueuePrivate->u32In = 0;
    psQueuePrivate->u32Out = 0;
    
#ifndef WIN32
    pthread_mutex_init(&psQueuePrivate->mutex, NULL);
    pthread_cond_init(&psQueuePrivate->cond_space_available, NULL);
    pthread_cond_init(&psQueuePrivate->cond_data_available, NULL);
#else
    
#endif 
    return E_LOCK_OK;
    
}


teQueueStatus eQueueDestroy(tsQueue *psQueue)
{
    tsQueuePrivate *psQueuePrivate = (tsQueuePrivate*)psQueue->pvPriv;
    if (!psQueuePrivate)
    {
        return E_QUEUE_ERROR_FAILED;
    }
    free(psQueuePrivate->apvBuffer);
    
#ifndef WIN32
    pthread_mutex_destroy(&psQueuePrivate->mutex);
    pthread_cond_destroy(&psQueuePrivate->cond_space_available);
    pthread_cond_destroy(&psQueuePrivate->cond_data_available);
#else
    
#endif 
    free(psQueuePrivate);
    return E_QUEUE_OK;
}


teQueueStatus eQueueQueue(tsQueue *psQueue, void *pvData)
{
    tsQueuePrivate *psQueuePrivate = (tsQueuePrivate*)psQueue->pvPriv;
#ifndef WIN32
    pthread_mutex_lock(&psQueuePrivate->mutex);
#endif /* WIN32 */
    while (psQueuePrivate->u32Size == psQueuePrivate->u32Capacity)
#ifndef WIN32
        pthread_cond_wait(&psQueuePrivate->cond_space_available, &psQueuePrivate->mutex);
#endif /* WIN32 */
    psQueuePrivate->apvBuffer[psQueuePrivate->u32In] = pvData;
    ++psQueuePrivate->u32Size;
    
    psQueuePrivate->u32In = (psQueuePrivate->u32In+1) % psQueuePrivate->u32Capacity;
    
#ifndef WIN32
    pthread_mutex_unlock(&psQueuePrivate->mutex);
    pthread_cond_broadcast(&psQueuePrivate->cond_data_available);
#endif /* WIN32 */
    return E_QUEUE_OK;
}


teQueueStatus eQueueDequeue(tsQueue *psQueue, void **ppvData)
{
    tsQueuePrivate *psQueuePrivate = (tsQueuePrivate*)psQueue->pvPriv;
#ifndef WIN32
    pthread_mutex_lock(&psQueuePrivate->mutex);
#endif /* WIN32 */
    while (psQueuePrivate->u32Size == 0)
#ifndef WIN32
        pthread_cond_wait(&psQueuePrivate->cond_data_available, &psQueuePrivate->mutex);
#endif /* WIN32 */
    
    *ppvData = psQueuePrivate->apvBuffer[psQueuePrivate->u32Out];
    --psQueuePrivate->u32Size;
    
    psQueuePrivate->u32Out = (psQueuePrivate->u32Out + 1) % psQueuePrivate->u32Capacity;
#ifndef WIN32
    pthread_mutex_unlock(&psQueuePrivate->mutex);
    pthread_cond_broadcast(&psQueuePrivate->cond_space_available);
#endif /* WIN32 */
    return E_QUEUE_OK;
}


teQueueStatus eQueueDequeueTimed(tsQueue *psQueue, uint32_t u32WaitTimeout, void **ppvData)
{
    tsQueuePrivate *psQueuePrivate = (tsQueuePrivate*)psQueue->pvPriv;
#ifndef WIN32
    pthread_mutex_lock(&psQueuePrivate->mutex);
#endif /* WIN32 */
    while (psQueuePrivate->u32Size == 0)
#ifndef WIN32
    {
        struct timeval sNow;
        struct timespec sTimeout;
        
        memset(&sNow, 0, sizeof(struct timeval));
        gettimeofday(&sNow, NULL);
        sTimeout.tv_sec = sNow.tv_sec + (u32WaitTimeout/1000);
        sTimeout.tv_nsec = (sNow.tv_usec + ((u32WaitTimeout % 1000) * 1000)) * 1000;
        if (sTimeout.tv_nsec > 1000000000)
        {
            sTimeout.tv_sec++;
            sTimeout.tv_nsec -= 1000000000;
        }
        DBG_vPrintf(DBG_QUEUE, "Dequeue timed: now    %lu s, %lu ns\n", sNow.tv_sec, sNow.tv_usec * 1000);
        DBG_vPrintf(DBG_QUEUE, "Dequeue timed: until  %lu s, %lu ns\n", sTimeout.tv_sec, sTimeout.tv_nsec);

        switch (pthread_cond_timedwait(&psQueuePrivate->cond_data_available, &psQueuePrivate->mutex, &sTimeout))
        {
            case (0):
                break;
            
            case (ETIMEDOUT):
                pthread_mutex_unlock(&psQueuePrivate->mutex);
                return E_QUEUE_ERROR_TIMEOUT;
                break;
            
            default:
                pthread_mutex_unlock(&psQueuePrivate->mutex);
                return E_QUEUE_ERROR_FAILED;
        }
    }
#endif /* WIN32 */
    
    *ppvData = psQueuePrivate->apvBuffer[psQueuePrivate->u32Out];
    --psQueuePrivate->u32Size;
    
    psQueuePrivate->u32Out = (psQueuePrivate->u32Out + 1) % psQueuePrivate->u32Capacity;
#ifndef WIN32
    pthread_mutex_unlock(&psQueuePrivate->mutex);
    pthread_cond_broadcast(&psQueuePrivate->cond_space_available);
#endif /* WIN32 */
    return E_QUEUE_OK;
}



uint32_t u32AtomicAdd(volatile uint32_t *pu32Value, uint32_t u32Operand)
{
#if defined(_MSC_VER)
    return add_value + InterlockedExchangeAdd(pu32Value, u32Operand);
#else
    return __sync_add_and_fetch (pu32Value, u32Operand);
#endif
}

