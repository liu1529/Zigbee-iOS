/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Cache.c
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

#include <stdlib.h>
#include <string.h>


#include <JIP.h>
#include <JIP_Private.h>
#include <Cache.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_CACHE 0



teJIP_Status Cache_Init(tsJIP_Context *psJIP_Context, tsCache *psCache)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    psCache->psParent_JIP_Context = psJIP_Context;
    psCache->psDeviceCacheHead = NULL;
    psCache->psMibCacheHead = NULL;
    
    return E_JIP_OK;
}


teJIP_Status Cache_Destroy(tsCache *psCache)
{
    tsDeviceIDCacheEntry    *psDeviceCacheEntry, *psDeviceCacheNext;
    tsMibIDCacheEntry       *psMibCacheEntry, *psMibCacheNext;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    /* First, clear up the Device ID cache */
    psDeviceCacheEntry = psCache->psDeviceCacheHead;
    while (psDeviceCacheEntry)
    {
        psDeviceCacheNext = psDeviceCacheEntry->psNext;
        
        eJIP_NetFreeNode(psCache->psParent_JIP_Context, psDeviceCacheEntry->psNode);
        free(psDeviceCacheEntry);
        psDeviceCacheEntry = psDeviceCacheNext;
    }
    
    /* Next, clear up the Mib ID cache */
    psMibCacheEntry = psCache->psMibCacheHead;
    while (psMibCacheEntry)
    {
        psMibCacheNext = psMibCacheEntry->psNext;
        
        eJIP_FreeMib(psCache->psParent_JIP_Context, psMibCacheEntry->psMib);
        
        free(psMibCacheEntry);
        psMibCacheEntry = psMibCacheNext;
    }
    
    return E_JIP_OK;
}


static teJIP_Status Cache_Add_Node_Impl(tsDeviceIDCacheEntry **psNewEntry, tsNode *psNode)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    tsNode *NewNode;
 
    *psNewEntry = malloc(sizeof(tsDeviceIDCacheEntry));
    
    if (!*psNewEntry)
    {
        DBG_vPrintf(DBG_CACHE, "Error allocating space for Entry\n");
        return E_JIP_ERROR_NO_MEM;
    }
    
    memset(*psNewEntry, 0, sizeof(tsDeviceIDCacheEntry));
    
    NewNode = malloc(sizeof(tsNode));
    
    if (!NewNode)
    {
        DBG_vPrintf(DBG_CACHE, "Error allocating space for Node\n");
        free(*psNewEntry);
        *psNewEntry = NULL;
        return E_JIP_ERROR_NO_MEM;
    }

    memset(NewNode, 0, sizeof(tsNode));

    NewNode->u32DeviceId    = psNode->u32DeviceId;
    
    eLockCreate(&NewNode->sLock);
    
    (*psNewEntry)->psNode = NewNode;
    
    {
        /* Add the Node's Mibs */
        tsMib *psMib = psNode->psMibs;
        
        while (psMib)
        {
            tsMib *psNewMib;
            DBG_vPrintf(DBG_CACHE, "  Adding Mib \"%s\" to cache\n", psMib->pcName);
            psNewMib = psJIP_NodeAddMib(NewNode, psMib->u32MibId, psMib->u8Index, psMib->pcName);
            
            {
                /* Add the Mib's vars */
                tsVar *psVar = psMib->psVars;
                while (psVar)
                {
                    DBG_vPrintf(DBG_CACHE, "    Adding Var \"%s\" to cache\n", psVar->pcName);
                    psJIP_MibAddVar(psNewMib, psVar->u8Index, psVar->pcName, psVar->eVarType, psVar->eAccessType, psVar->eSecurity);
                    
                    psVar = psVar->psNext;
                }
            }
            psMib = psMib->psNext;
        }
    }
    
    DBG_vPrintf(DBG_CACHE, "Added Node device ID 0x%08x to cache\n", psNode->u32DeviceId);
    
    return E_JIP_OK;
}


static teJIP_Status Cache_Add_Mib_Impl(tsMibIDCacheEntry **psNewEntry, tsMib *psMib)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    tsMib *psNewMib;
 
    *psNewEntry = malloc(sizeof(tsMibIDCacheEntry));
    
    if (!*psNewEntry)
    {
        DBG_vPrintf(DBG_CACHE, "Error allocating space for Entry\n");
        return E_JIP_ERROR_NO_MEM;
    }
    
    memset(*psNewEntry, 0, sizeof(tsMibIDCacheEntry));
    
    psNewMib = malloc(sizeof(tsMib));
    
    if (!psNewMib)
    {
        DBG_vPrintf(DBG_CACHE, "Error allocating space for Mib\n");
        free(*psNewEntry);
        *psNewEntry = NULL;
        return E_JIP_ERROR_NO_MEM;
    }

    memset(psNewMib, 0, sizeof(tsMib));

    psNewMib->u32MibId          = psMib->u32MibId;
    
    (*psNewEntry)->psMib = psNewMib;
    
    /* Add the Mib's vars */
    tsVar *psVar = psMib->psVars;
    while (psVar)
    {
        DBG_vPrintf(DBG_CACHE, "    Adding Var \"%s\" to cache\n", psVar->pcName);
        psJIP_MibAddVar(psNewMib, psVar->u8Index, psVar->pcName, psVar->eVarType, psVar->eAccessType, psVar->eSecurity);
        
        psVar = psVar->psNext;
    }

    DBG_vPrintf(DBG_CACHE, "Added Mib ID 0x%08x to cache\n", psMib->u32MibId);
    
    return E_JIP_OK;
}


teJIP_Status Cache_Add_Node(tsCache *psCache, tsNode *psNode)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (!psCache->psDeviceCacheHead)
    {
        return Cache_Add_Node_Impl(&psCache->psDeviceCacheHead, psNode);
    }
    else
    {
        tsDeviceIDCacheEntry *psEntry = psCache->psDeviceCacheHead;
        while (psEntry)
        {
            if (psEntry->psNode->u32DeviceId == psNode->u32DeviceId)
            {
                DBG_vPrintf(DBG_CACHE, "Device ID 0x%08x is already in the cache\n", psNode->u32DeviceId);
                return E_JIP_ERROR_FAILED;
            }
            if (!psEntry->psNext)
            {
                break;
            }
            psEntry = psEntry->psNext;
        }
        return Cache_Add_Node_Impl(&psEntry->psNext, psNode);
    }
    
    return E_JIP_OK;
}


teJIP_Status Cache_Add_Mib(tsCache *psCache, tsMib *psMib)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (!psCache->psMibCacheHead)
    {
        return Cache_Add_Mib_Impl(&psCache->psMibCacheHead, psMib);
    }
    else
    {
        tsMibIDCacheEntry *psEntry = psCache->psMibCacheHead;
        while (psEntry)
        {
            if (psEntry->psMib->u32MibId == psMib->u32MibId)
            {
                DBG_vPrintf(DBG_CACHE, "Mib ID 0x%08x is already in the cache\n", psMib->u32MibId);
                return E_JIP_ERROR_FAILED;
            }
            if (!psEntry->psNext)
            {
                break;
            }
            psEntry = psEntry->psNext;
        }
        return Cache_Add_Mib_Impl(&psEntry->psNext, psMib);
    }
    
    return E_JIP_OK;
}


teJIP_Status Cache_Populate_Node(tsCache *psCache, tsNode *psNode)
{
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    tsDeviceIDCacheEntry *psEntry = psCache->psDeviceCacheHead;
    
    while (psEntry)
    {
        if (psEntry->psNode->u32DeviceId == psNode->u32DeviceId)
        {
            DBG_vPrintf(DBG_CACHE, "Device ID 0x%08x is in the cache\n", psNode->u32DeviceId);
            
            {
                /* Add the Node's Mibs */
                tsMib *psMib = psEntry->psNode->psMibs;
                
                while (psMib)
                {
                    tsMib *psNewMib;
                    DBG_vPrintf(DBG_CACHE, "  Adding Mib \"%s\" from cache\n", psMib->pcName);
                    psNewMib = psJIP_NodeAddMib(psNode, psMib->u32MibId, psMib->u8Index, psMib->pcName);
                    
                    {
                        /* Add the Mib's vars */
                        tsVar *psVar = psMib->psVars;
                        while (psVar)
                        {
                            DBG_vPrintf(DBG_CACHE, "    Adding Var \"%s\" from cache\n", psVar->pcName);
                            psJIP_MibAddVar(psNewMib, psVar->u8Index, psVar->pcName, psVar->eVarType, psVar->eAccessType, psVar->eSecurity);
                            
                            psVar = psVar->psNext;
                        }
                    }
                    psMib = psMib->psNext;
                }
            }
            
            return E_JIP_OK;
        }
        psEntry = psEntry->psNext;
    }
    
    return E_JIP_ERROR_FAILED;
}


teJIP_Status Cache_Populate_Mib(tsCache *psCache, tsMib *psMib)
{
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    tsMibIDCacheEntry *psEntry = psCache->psMibCacheHead;
    
    while (psEntry)
    {
        if (psEntry->psMib->u32MibId == psMib->u32MibId)
        {
            DBG_vPrintf(DBG_CACHE, "Mib ID 0x%08x is in the cache\n", psMib->u32MibId);
            
            {
                /* Add the Mib's vars */
                tsVar *psVar = psEntry->psMib->psVars;
                while (psVar)
                {
                    DBG_vPrintf(DBG_CACHE, "    Adding Var \"%s\" from cache\n", psVar->pcName);
                    psJIP_MibAddVar(psMib, psVar->u8Index, psVar->pcName, psVar->eVarType, psVar->eAccessType, psVar->eSecurity);
                    
                    psVar = psVar->psNext;
                }
            }
            
            return E_JIP_OK;
        }
        psEntry = psEntry->psNext;
    }
    
    return E_JIP_ERROR_FAILED;
}


