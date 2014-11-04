/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Node.c
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <Trace.h>
#include <Threads.h>

#define DBG_FUNCTION_CALLS  0
#define DBG_NODES           0
#define DBG_MIBS            0
#define DBG_VARS            0


static inline void vJIP_NodeListPrint(tsNode **ppsNodeListHead)
{
#if DBG_NODES
    tsNode *psllPosition = *ppsNodeListHead;
    DBG_vPrintf(DBG_NODES,      "Node List: Head %p\n", psllPosition);
    while (psllPosition)
    {
        DBG_vPrintf(DBG_NODES,  "Node List: Node %p, next: %p\n", psllPosition, psllPosition->psNext);
        psllPosition = psllPosition->psNext;
    }
#endif /* DBG_NODES */
}


tsNode *psJIP_NodeListAdd(tsNode **ppsNodeListHead, tsNode *psNode)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s Add Node %p to list head %p\n", __FUNCTION__, psNode, *ppsNodeListHead);
    
    if (*ppsNodeListHead == NULL)
    {
        /* First in list */
        *ppsNodeListHead = psNode;
    }
    else
    {
        tsNode *psllPosition = *ppsNodeListHead;
        while (psllPosition->psNext)
        {
            psllPosition = psllPosition->psNext;
        }
        /* Now we have a pointer to the last element in the list */
        psllPosition->psNext = psNode;
    }
    
    vJIP_NodeListPrint(ppsNodeListHead);
    return psNode;
}


tsNode *psJIP_NodeListRemove(tsNode **ppsNodeListHead, tsNode *psNode)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s Remove Node %p from list head %p\n", __FUNCTION__, psNode, *ppsNodeListHead);

    vJIP_NodeListPrint(ppsNodeListHead);
    
    if (*ppsNodeListHead == psNode)
    {
        DBG_vPrintf(DBG_NODES,  "Remove Node %p from head of list\n", psNode);
        *ppsNodeListHead = (*ppsNodeListHead)->psNext;
    }
    else
    {
        tsNode *psllPosition = *ppsNodeListHead;
        while (psllPosition->psNext)
        {
            if (psllPosition->psNext == psNode)
            {
                DBG_vPrintf(DBG_NODES,  "Remove Node %p from list at %p\n", psNode, psllPosition->psNext);
                psllPosition->psNext = psllPosition->psNext->psNext;
                break;
            }
            psllPosition = psllPosition->psNext;
        }
    }
    
    vJIP_NodeListPrint(ppsNodeListHead);
    return NULL;    
}



tsNode *psJIP_NetAllocateNode(tsNetwork *psNet, tsJIPAddress *psAddress, uint32_t u32DeviceId)
{
    tsNode *psNewNode;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s to Net at %p\n", __FUNCTION__, psNet);
    
    psNewNode = malloc(sizeof(tsNode));
    
    if (!psNewNode)
    {
        DBG_vPrintf(DBG_NODES, "Error allocating space for Node\n");
        return NULL;
    }

    memset(psNewNode, 0, sizeof(tsNode));
    
    psNewNode->psOwnerNetwork = psNet;
    if (psAddress)
    {
        psNewNode->sNode_Address = *psAddress;
    }
    psNewNode->u32DeviceId = u32DeviceId;
    
    eLockCreate(&psNewNode->sLock);
    eJIPLockLock(&psNewNode->sLock);

    DBG_vPrintf(DBG_NODES, "New Node allocated at %p\n", psNewNode);
    DBG_vPrintf_IPv6Address(DBG_NODES, psNewNode->sNode_Address.sin6_addr);

    return psNewNode;
}


teJIP_Status eJIP_NetAddNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, uint32_t u32DeviceId, tsNode** ppsNode)
{
    tsNode* psNewNode;
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    /* Allocate the new node - locked to this thread */
    psNewNode = psJIP_NetAllocateNode(&psJIP_Context->sNetwork, psAddress, u32DeviceId);

    if (!psNewNode)
    {
        DBG_vPrintf(DBG_NODES, "Could not allocate memory for node\n");
        return E_JIP_ERROR_NO_MEM;
    }
    
    if (psJIP_Private->eJIP_ContextType == E_JIP_CONTEXT_SERVER)
    {
        tsNode_Private *psNode_Private;
        psNode_Private = malloc(sizeof(tsNode_Private));
        if (!psNode_Private)
        {
            DBG_vPrintf(DBG_NODES, "Failed allocate private node data\n");
            if (eJIP_NetFreeNode(psJIP_Context, psNewNode) != E_JIP_OK)
            {
                DBG_vPrintf(DBG_NODES, "Could not free node!\n");
                /* Not much we can do about it though */
            }
            return E_JIP_ERROR_NO_MEM;
        }
        memset(psNode_Private, 0, sizeof(tsNode_Private));
        psNewNode->pvPriv = psNode_Private;
    }
    
    eJIP_Lock(psJIP_Context);

    /* Attempt to populate the node from the cache */
    if (Cache_Populate_Node(&psJIP_Private->sCache, psNewNode) != E_JIP_OK)
    {
        if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
        {
            /* Only populate the node if we are running as a client, otherwise we can't populate this node */
            DBG_vPrintf(DBG_NODES, "Failed to populate node - unknown device id\n");
            if (eJIP_NetFreeNode(psJIP_Context, psNewNode) != E_JIP_OK)
            {
                DBG_vPrintf(DBG_NODES, "Could not free node!\n");
                /* Not much we can do about it though */
            }
            eJIP_Unlock(psJIP_Context);
            return E_JIP_ERROR_BAD_DEVICE_ID;
        }
        else
        {
            DBG_vPrintf(DBG_NODES, "Failed to populate node from device id, falling back to query\n");
            
            if (eJIP_DiscoverNode(psJIP_Context, psNewNode) != E_JIP_OK)
            {
                /* We Failed to populate the node with device information.
                * The best thing to do now is to not add it into the network
                * Then the next time the network is discovered we will get 
                * another attempt at populating it.
                */

                if (eJIP_NetFreeNode(psJIP_Context, psNewNode) != E_JIP_OK)
                {
                    DBG_vPrintf(DBG_NODES, "Could not free node!\n");
                    /* Not much we can do about it though */
                }
                eJIP_Unlock(psJIP_Context);
                return E_JIP_ERROR_FAILED;
            }
        }
    }
    
    /* Insert the new node into the linked list of nodes */
    (void)psJIP_NodeListAdd(&psJIP_Context->sNetwork.psNodes, psNewNode);
    
    psJIP_Context->sNetwork.u32NumNodes++;
    
    /* If the app wants feedback of the newly added node, return it here */
    if (ppsNode)
    {
        /* Return it to the caller locked */
        *ppsNode = psNewNode;
    }
    else
    {
        eJIP_UnlockNode(psNewNode);
    }

    DBG_vPrintf(DBG_NODES, "Node    :   %p\n", psNewNode);
    DBG_vPrintf(DBG_NODES, "Net     :   %p\n", psNewNode->psOwnerNetwork);
    DBG_vPrintf(DBG_NODES, "Context :   %p\n", psNewNode->psOwnerNetwork->psOwnerContext);
    DBG_vPrintf(DBG_NODES, "Real Context :   %p\n", psJIP_Context);
    
    eJIP_Unlock(psJIP_Context);
    
    return E_JIP_OK;
}


teJIP_Status eJIP_NetRemoveNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, tsNode **ppsNode)
{
    tsNode* psNode;
    tsNetwork *psNet = &psJIP_Context->sNetwork;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;

    /* Get a locked pointer to the node structure */
    psNode = psJIP_LookupNode(psJIP_Context, psAddress);
    
    if (psNode)
    {
        /* Got pointer to the node, lock the linked list now so that we can remove it. */
        eJIP_Lock(psJIP_Context);
        (void)psJIP_NodeListRemove(&psJIP_Context->sNetwork.psNodes, psNode);
        
        /* Decrement count of nodes */
        psNet->u32NumNodes--;
        eStatus = E_JIP_OK;
        
        if (ppsNode)
        {
            /* Return pointer to node if we were passed a location */
            *ppsNode = psNode;
        }
        
        eJIP_Unlock(psJIP_Context);
    }  
    return eStatus;
}


teJIP_Status eJIP_FreeMib(tsJIP_Context *psJIP_Context, tsMib *psMib)
{
    /* Run length of vars, freeing each one */
    tsVar *psFreeVar, *psVar = psMib->psVars;
    
    DBG_vPrintf(DBG_MIBS, "Freeing Mib ID 0x%08x at %p (%s)\n", psMib->u32MibId, psMib, psMib->pcName ? psMib->pcName: "?");
    
    while(psVar)
    {
        /* Run length of Vars, freeing each one */
        
        DBG_vPrintf(DBG_VARS, "Freeing Var at %p (%s)\n", psVar, psVar->pcName ? psVar->pcName: "?");
        
        if (psVar->prCbVarTrap)
        {
            DBG_vPrintf(DBG_VARS, "Removing trap on variable at %p (%s)\n", psVar, psVar->pcName ? psVar->pcName: "?");
            eJIP_UntrapVar(psJIP_Context, psVar, psVar->u8TrapHandle);
        }
        
        switch (psVar->eVarType)
        {
            case(E_JIP_VAR_TYPE_TABLE_BLOB):
            {
                tsTable *psTable;
                uint32_t i;
                
                if (psVar->pvData)
                {
                    psTable = (tsTable *)psVar->pvData;
                    for (i = 0; i < psTable->u32NumRows; i++)
                    {
                        if (psTable->psRows[i].pvData)
                        {
                            free(psTable->psRows[i].pvData);
                        }
                    }
                    free(psTable->psRows);
                    free(psVar->pvData);
                }
                break;
            }
            default:
                if (psVar->pvData)
                {
                    free(psVar->pvData);
                }
                break;
        }
        
        if (psVar->pcName)
        {
            free(psVar->pcName);
        }
        
        psFreeVar = psVar;
        psVar = psVar->psNext;
        free(psFreeVar);
    }
    
    if (psMib->pcName)
    {
        free(psMib->pcName);
    }
    
    free(psMib);

    return E_JIP_OK;
}


teJIP_Status eJIP_NetFreeNode(tsJIP_Context *psJIP_Context, tsNode *psNode)
{
    PRIVATE_CONTEXT(psJIP_Context);
    if (psNode)
    {
        /* We found the node to be deleted. Now it all needs freeing */
        tsMib *psNextMib, *psMib = psNode->psMibs;
        
        DBG_vPrintf(DBG_NODES, "Freeing node at %p\n", psNode);
        
        while (psMib)
        {
            /* Take a copy of the next miB pointer before freeing the mib. */
            psNextMib = psMib->psNext;
            (void) eJIP_FreeMib(psJIP_Context, psMib);
            psMib = psNextMib;
        }
        
        if (psJIP_Private->eJIP_ContextType == E_JIP_CONTEXT_SERVER)
        {
            if (psNode->pvPriv)
            {
                free(psNode->pvPriv);
            }
        }
        
        eLockDestroy(&psNode->sLock);
        free(psNode);
    }
    else
    {
        return E_JIP_ERROR_FAILED;
    }
    return E_JIP_OK;
}


teJIP_Status eJIP_LockNode(tsNode *psNode, bool_t bWait)
{
    if (bWait)
    {
        DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: Waiting for lock node %p (lock %p)\n", __FUNCTION__, psNode, &psNode->sLock);
        eJIPLockLock(&psNode->sLock);
    }
    else
    {
        DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: Trying to lock node %p (lock %p)\n", __FUNCTION__, psNode, &psNode->sLock);
        if (eJIPLockTryLock(&psNode->sLock) == E_LOCK_ERROR_FAILED)
        {
            return E_JIP_ERROR_WOULD_BLOCK;
        }
    }
    return E_JIP_OK;
}


teJIP_Status eJIP_UnlockNode(tsNode *psNode)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: %p\n", __FUNCTION__, &psNode->sLock);
    eJIPLockUnlock(&psNode->sLock);
    return E_JIP_OK;
}


teJIP_Status eJIP_GetNodeAddressList(tsJIP_Context *psJIP_Context, const uint32_t u32DeviceIdFilter, tsJIPAddress **ppsAddresses, uint32_t *pu32NumAddresses)
{
    tsNode *psNode;
    teJIP_Status eStatus = E_JIP_OK;
    tsJIPAddress *psAddresses = NULL;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    eJIP_Lock(psJIP_Context);
    
    *pu32NumAddresses = 0;
    
    DBG_vPrintf(DBG_NODES, "Currently %d nodes in network\n", psJIP_Context->sNetwork.u32NumNodes);
    
    // Malloc enough space to hold every device, if we had to return everything.
    psAddresses = malloc(sizeof(tsJIPAddress) * (psJIP_Context->sNetwork.u32NumNodes));
    
    if (psAddresses)
    {
        uint32_t i = 0;
        psNode = psJIP_Context->sNetwork.psNodes;
        
        while (psNode)
        {
            if ((u32DeviceIdFilter == JIP_DEVICEID_ALL) || (psNode->u32DeviceId == u32DeviceIdFilter))
            {
                memcpy(&psAddresses[i], &psNode->sNode_Address, sizeof(tsJIPAddress));
                DBG_vPrintf(DBG_NODES, "Adding node ");
                DBG_vPrintf_IPv6Address(DBG_NODES, psNode->sNode_Address.sin6_addr);
                i++;
                (*pu32NumAddresses)++;
            }
            psNode = psNode->psNext;
        }
        *ppsAddresses = psAddresses;
    }
    else
    {
        DBG_vPrintf(DBG_NODES, "Could not malloc space for node list\n");
        eStatus = E_JIP_ERROR_NO_MEM;
    }

    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


tsNode *psJIP_LookupNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress)
{
    tsNode *psNode;
    tsNetwork *psNet;
    uint32_t u32Attempts = 0;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
 
    DBG_vPrintf(DBG_NODES, "Looking for ");
    DBG_vPrintf_IPv6Address(DBG_NODES, psAddress->sin6_addr);
start:
    eJIP_Lock(psJIP_Context);

    psNet = &psJIP_Context->sNetwork;

    psNode = psNet->psNodes;

    while (psNode)
    {
        DBG_vPrintf(DBG_NODES, "Looking at ");
        DBG_vPrintf_IPv6Address(DBG_NODES, psNode->sNode_Address.sin6_addr);
        if (memcmp(&psNode->sNode_Address, psAddress, sizeof(tsJIPAddress)) == 0)
        {
            if (eJIP_LockNode(psNode, False) == E_JIP_ERROR_WOULD_BLOCK)
            {
                DBG_vPrintf(DBG_NODES, "Locking node %p would block\n", psNode);
                eJIP_Unlock(psJIP_Context);
                if (++u32Attempts > 10)
                {
                    DBG_vPrintf(DBG_NODES, "Error locking node:");
                    DBG_vPrintf_IPv6Address(DBG_NODES, psNode->sNode_Address.sin6_addr);
                    sleep(1);
                    u32Attempts = 0;
                }
                eThreadYield();
                goto start;
            }
            eJIP_Unlock(psJIP_Context);
            return psNode;
        }
        psNode = psNode->psNext;
    }
    
    eJIP_Unlock(psJIP_Context);

    return NULL;
}


tsMib *psJIP_NodeAddMib(tsNode *psNode, uint32_t u32MibId, uint8_t u8Index, const char *pcName)
{
    tsMib *NewMib;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(0x%08x, %s) to Node at %p\n", __FUNCTION__, u32MibId, pcName, psNode);

    NewMib = malloc(sizeof(tsMib));
    
    if (!NewMib)
    {
        DBG_vPrintf(DBG_MIBS, "Error allocating space for Mib\n");
        return NULL;
    }
    
    memset(NewMib, 0, sizeof(tsMib));
    
    NewMib->u32MibId = u32MibId;
    NewMib->u8Index = u8Index;
    NewMib->pcName = strdup(pcName);
    NewMib->psOwnerNode = psNode;
    
    psNode->u32NumMibs++;
    
    DBG_vPrintf(DBG_MIBS, "New Mib allocated at %p, name at %p\n", NewMib, NewMib->pcName);

    /* Insert the new Mib into the linked list of Mibs */
    if (psNode->psMibs == NULL)
    {
        /* First in list */
        psNode->psMibs = NewMib;
    }
    else
    {
        tsMib *psllPosition = psNode->psMibs;
        while (psllPosition->psNext)
        {
            psllPosition = psllPosition->psNext;
        }
        /* Now we have a pointer to the last element in the list */
        psllPosition->psNext = NewMib;
    }
    
    {
        tsMib *psllPosition = psNode->psMibs;
        DBG_vPrintf(DBG_MIBS, "Mibs Head %p, next: %p\n", psllPosition, psllPosition->psNext);
        while (psllPosition->psNext)
        {
            psllPosition = psllPosition->psNext;
            DBG_vPrintf(DBG_MIBS, "  Mib at %p, next: %p\n", psllPosition, psllPosition->psNext);
        }
    }
    
    return NewMib;
}


tsMib *psJIP_LookupMib(tsNode *psNode, tsMib *psStartMib, const char *pcName)
{
    tsMib *psMib;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(%s)\n", __FUNCTION__, pcName);
  
    if (psStartMib == NULL)
    {
        psMib = psNode->psMibs;
    }
    else
    {
        psMib = psStartMib->psNext;
    }
    while (psMib)
    {
        if (strcmp(psMib->pcName, pcName) == 0)
        {
            return psMib;
        }
        psMib = psMib->psNext;
    }
    
    return NULL;
}


tsMib *psJIP_LookupMibId(tsNode *psNode, tsMib *psStartMib, uint32_t u32MibId)
{
    tsMib *psMib;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(0x%08x)\n", __FUNCTION__, u32MibId);
  
    if (psStartMib == NULL)
    {
        psMib = psNode->psMibs;
    }
    else
    {
        psMib = psStartMib->psNext;
    }
    while (psMib)
    {
        if (psMib->u32MibId == u32MibId)
        {
            return psMib;
        }
        psMib = psMib->psNext;
    }
    
    return NULL;
}


tsVar *psJIP_MibAddVar(tsMib *psMib, uint8_t u8Index, const char *pcName, teJIP_VarType eVarType, 
                  teJIP_AccessType eAccessType, teJIP_Security eSecurity)
{
    tsVar *NewVar;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(%s) to Mib 0x%08x at %p\n", __FUNCTION__, pcName, psMib->u32MibId, psMib);

    NewVar = malloc(sizeof(tsVar));
    
    if (!NewVar)
    {
        DBG_vPrintf(DBG_VARS, "Error allocating space for Var\n");
        return NULL;
    }

    memset(NewVar, 0, sizeof(tsVar));
    
    NewVar->pcName = strdup(pcName);
    NewVar->u8Index = u8Index;
    NewVar->eVarType = eVarType;
    NewVar->eAccessType = eAccessType;
    NewVar->eSecurity = eSecurity;
    NewVar->psOwnerMib = psMib;
    NewVar->eEnable = E_JIP_VAR_ENABLED; /* All vars enabled by default */

    psMib->u32NumVars++;
    
    DBG_vPrintf(DBG_VARS, "New Var allocated at %p, name at %p\n", NewVar, NewVar->pcName);

    /* Insert the new Mib into the linked list of Mibs */
    if (psMib->psVars == NULL)
    {
        /* First in list */
        psMib->psVars = NewVar;
    }
    else
    {
        tsVar *psllPosition = psMib->psVars;
        while (psllPosition->psNext)
        {
            psllPosition = psllPosition->psNext;
        }
        /* Now we have a pointer to the last element in the list */
        psllPosition->psNext = NewVar;
    }
    
    {
        tsVar *psllPosition = psMib->psVars;
        DBG_vPrintf(DBG_VARS, "Vars Head %p, next: %p\n", psllPosition, psllPosition->psNext);
        while (psllPosition->psNext)
        {
            psllPosition = psllPosition->psNext;
            DBG_vPrintf(DBG_VARS, "  Var at %p, next: %p\n", psllPosition, psllPosition->psNext);
        }
    }
    
    return NewVar;
}


tsVar *psJIP_LookupVar(tsMib *psMib, tsVar *psStartVar, const char *pcName)
{
    tsVar *psVar;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(%s)\n", __FUNCTION__, pcName);
  
    if (psStartVar == NULL)
    {
        psVar = psMib->psVars;
    }
    else
    {
        psVar = psStartVar->psNext;
    }
    while (psVar)
    {
        if (strcmp(psVar->pcName, pcName) == 0)
        {
            return psVar;
        }
        psVar = psVar->psNext;
    }
    
    return NULL;
}


tsVar *psJIP_LookupVarIndex(tsMib *psMib, uint8_t u8Index)
{
    tsVar *psVar;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(%d)\n", __FUNCTION__, u8Index & 0xFF);
  
    psVar = psMib->psVars;

    while (psVar)
    {
        if (psVar->u8Index == u8Index)
        {
            return psVar;
        }
        psVar = psVar->psNext;
    }
    
    return NULL;
}





