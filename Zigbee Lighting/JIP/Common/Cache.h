/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Cache.h
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


#ifndef __CACHE_H__
#define __CACHE_H__

#include <JIP.h>


/** Linked list structure of known device IDs */
typedef struct _tsDeviceIDCacheEntry
{
    tsNode          *psNode;                    /**< pointer to a node structure describing the ID */
    struct _tsDeviceIDCacheEntry *psNext;       /**< pointer to next element in list */
} tsDeviceIDCacheEntry;


/** Linked list structure of known MiB IDs */
typedef struct _tsMibIDCacheEntry
{
    tsMib      *psMib;                          /**< pointer to a MiB structure */
    struct _tsMibIDCacheEntry *psNext;          /**< pointer to next element in list */
} tsMibIDCacheEntry;


/** Cache structure */
typedef struct
{
    tsJIP_Context        *psParent_JIP_Context; /**< pointer to the parent JIP context */
    tsDeviceIDCacheEntry *psDeviceCacheHead;    /**< List head of known device IDs */
    tsMibIDCacheEntry    *psMibCacheHead;       /**< List head of known Mib IDs */
} tsCache;


/** Initialise the cache 
 *  \param psJIP_Context    Pointer to JIP context that the cache is associated with.
 *  \param psCache          Pointer to cache structure
 */
teJIP_Status Cache_Init(tsJIP_Context *psJIP_Context, tsCache *psCache);


/** Destroy the cache .
 *  Free up all memory used by members of the cache.
 *  \param psCache Pointer to cache structure
 */
teJIP_Status Cache_Destroy(tsCache *psCache);


/** Add a node (by device ID) to the cache 
 *  \param psCache Pointer to cache structure
 *  \param psNode  Pointer to the node type to add
 */
teJIP_Status Cache_Add_Node(tsCache *psCache, tsNode *psNode);


/** Add a Mib (by Mib ID) to the cache 
 *  \param psCache Pointer to cache structure
 *  \param psMib  Pointer to the Mib type to add
 */
teJIP_Status Cache_Add_Mib(tsCache *psCache, tsMib *psMib);


/** Populate a node from the cache 
 *  \param psCache Pointer to cache structure
 *  \param psNode  Pointer to the node to populate
 */
teJIP_Status Cache_Populate_Node(tsCache *psCache, tsNode *psNode);


/** Populate a Mib from the cache 
 *  \param psCache Pointer to cache structure
 *  \param psMib  Pointer to the node to populate
 */
teJIP_Status Cache_Populate_Mib(tsCache *psCache, tsMib *psMib);



#endif /* __CACHE_H__ */

