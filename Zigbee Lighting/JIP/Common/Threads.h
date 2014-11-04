/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Threads.h
 *
 * REVISION:           $Revision: 52723 $
 *
 * DATED:              $Date: 2013-03-14 12:04:13 +0000 (Thu, 14 Mar 2013) $
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

#ifndef __THREADS_H__
#define __THREADS_H__

#include <JIP.h>


/** Enumerated type of thread status's */
typedef enum
{
    E_THREAD_OK,
    E_THREAD_ERROR_FAILED,
    E_THREAD_ERROR_TIMEOUT,
    E_THREAD_ERROR_NO_MEM,
} teThreadStatus;


/** Enumerated type for thread detached / joinable states */
typedef enum
{
    E_THREAD_JOINABLE,      /**< Thread is created so that it can be waited on and joined */
    E_THREAD_DETACHED,      /**< Thread is created detached so all resources are automatically free'd at exit. */
} teThreadDetachState;


/** Structure to represent a thread */
typedef struct
{
    volatile enum
    {
        E_THREAD_STOPPED,   /**< Thread stopped */
        E_THREAD_RUNNING,   /**< Thread running */
        E_THREAD_STOPPING,  /**< Thread signaled to stop */
    } eState;               /**< Enumerated type of thread states */
    teThreadDetachState
        eThreadDetachState; /**< Detach state of the thread */
    void *pvPriv;           /**< Implementation specfific private structure */
    void *pvThreadData;     /**< Pointer to threads data parameter */
} tsThread;


typedef void *(*tprThreadFunction)(void *psThreadInfoVoid);

/** Function to start a thread */
teThreadStatus eThreadStart(tprThreadFunction prThreadFunction, tsThread *psThreadInfo, teThreadDetachState eDetachState);


/** Function to stop a thread 
 *  This function blocks until the specified thread exits.
 */
teThreadStatus eThreadStop(tsThread *psThreadInfo);


/** Function to be called within the thread when it is finished to clean up memory */
teThreadStatus eThreadFinish(tsThread *psThreadInfo);


/** Function to yield the CPU to another thread 
 *  \return E_THREAD_OK on success.
 */
teThreadStatus eThreadYield(void);


/** Enumerated type of thread status's */
typedef enum
{
    E_LOCK_OK,
    E_LOCK_ERROR_FAILED,
    E_LOCK_ERROR_TIMEOUT,
    E_LOCK_ERROR_NO_MEM,
} teLockStatus;

teLockStatus eLockCreate(tsLock *psLock);

teLockStatus eLockDestroy(tsLock *psLock);

/** Lock the data structure associated with this lock
 *  \param  psLock  Pointer to lock structure
 *  \return E_LOCK_OK if locked ok
 */
teLockStatus eJIPLockLock(tsLock *psLock);

/** Lock the data structure associated with this lock
 *  If possible within \param u32WaitTimeout seconds.
 *  \param  psLock  Pointer to lock structure
 *  \param  u32WaitTimeout  Number of seconds to attempt to lock the structure for
 *  \return E_LOCK_OK if locked ok, E_LOCK_ERROR_TIMEOUT if structure could not be acquired
 */
teLockStatus eJIPLockLockTimed(tsLock *psLock, uint32_t u32WaitTimeout);


/** Attempt to lock the data structure associated with this lock
 *  If the lock could not be acquired immediately, return E_LOCK_ERROR_FAILED.
 *  \param  psLock  Pointer to lock structure
 *  \return E_LOCK_OK if locked ok or E_LOCK_ERROR_FAILED if not
 */
teLockStatus eJIPLockTryLock(tsLock *psLock);


/** Unlock the data structure associated with this lock
 *  \param  psLock  Pointer to lock structure
 *  \return E_LOCK_OK if unlocked ok
 */
teLockStatus eJIPLockUnlock(tsLock *psLock);


typedef enum
{
    E_QUEUE_OK,
    E_QUEUE_ERROR_FAILED,
    E_QUEUE_ERROR_TIMEOUT,
    E_QUEUE_ERROR_NO_MEM,
} teQueueStatus;

typedef struct
{
    void *pvPriv;
} tsQueue;

teQueueStatus eQueueCreate(tsQueue *psQueue, uint32_t u32Capacity);

teQueueStatus eQueueDestroy(tsQueue *psQueue);

teQueueStatus eQueueQueue(tsQueue *psQueue, void *pvData);

teQueueStatus eQueueDequeue(tsQueue *psQueue, void **ppvData);

teQueueStatus eQueueDequeueTimed(tsQueue *psQueue, uint32_t u32WaitTimeout, void **ppvData);


/** Atomically add a 32 bit value to another.
 *  \param pu32Value        Pointer to value to update
 *  \param u32Operand       Value to add
 *  \return New value
 */
uint32_t u32AtomicAdd(volatile uint32_t *pu32Value, uint32_t u32Operand);


/** Atomically get the value of a 32 bit value */
#define u32AtomicGet(pu32Value) u32AtomicAdd(pu32Value, 0)



#endif /* __THREADS_H__ */


