/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          libJIP.c
 *
 * REVISION:           $Revision: 54927 $
 *
 * DATED:              $Date: 2013-06-26 11:08:44 +0100 (Wed, 26 Jun 2013) $
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
#include <unistd.h>

#include <net/if.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <Network.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_JIP 0

#define LIBJIP_VERSION_MAJOR    "1"
#define LIBJIP_VERSION_MINOR    "5"
#define LIBJIP_VERSION          "1.5"

#ifndef LIBJIP_VERSION
#error Version is not defined!
#else
#ifndef LIBJIP_VERSION_MAJOR
#error Major Version is not defined!
#else
#ifndef LIBJIP_VERSION_MINOR
#error Minor Version is not defined!
#else
const char *JIP_Version = LIBJIP_VERSION_MAJOR "." LIBJIP_VERSION_MINOR " (r" LIBJIP_VERSION ")";
#endif
#endif
#endif


teJIP_Status eJIP_Init(tsJIP_Context *psJIP_Context, teJIP_ContextType eJIP_ContextType)
{
    tsJIP_Private *psJIP_Private;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    psJIP_Private = malloc(sizeof(tsJIP_Private));
    if (!psJIP_Private)
    {
        return E_JIP_ERROR_NO_MEM;
    }
    
    memset(psJIP_Context, 0, sizeof(tsJIP_Context));
    memset(psJIP_Private, 0, sizeof(tsJIP_Private));
    
    psJIP_Private->eJIP_ContextType = eJIP_ContextType;
    
    eLockCreate(&psJIP_Private->sLock);
    eJIPLockLock(&psJIP_Private->sLock);
    
    if (Network_Init(&psJIP_Private->sNetworkContext, psJIP_Context) != E_NETWORK_OK)
    {
        free(psJIP_Private);
        return E_JIP_ERROR_FAILED;
    }
    
    if (Cache_Init(psJIP_Context, &psJIP_Private->sCache) != E_JIP_OK)
    {
        free(psJIP_Private);
        return E_JIP_ERROR_FAILED;
    }
    
    /* No registered network change handler */
    psJIP_Private->prCbNetworkChange = NULL;
    
    /* Save private context into public */
    psJIP_Context->pvPriv = psJIP_Private;
    
    /* No nodes in the network */
    memset(&psJIP_Context->sNetwork, 0, sizeof(tsNetwork));
    
    /* Point the network's owner context to this context */
    psJIP_Context->sNetwork.psOwnerContext = psJIP_Context;

    /* Set up the multicast interface index - send on all interfaces by default. */
    psJIP_Context->iMulticastInterface = -1;
    
    /* Set up the multicast attempts to the default */
    psJIP_Context->iMulticastSendCount = 2;
    
    eJIPLockUnlock(&psJIP_Private->sLock);
    
    return E_JIP_OK;
}


teJIP_Status eJIP_Destroy(tsJIP_Context *psJIP_Context)
{
    PRIVATE_CONTEXT(psJIP_Context);
    teJIP_Status eStatus = E_JIP_OK;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    /* Stop the network monitor if it is running */
    eJIPService_MonitorNetworkStop(psJIP_Context);
    
    eJIP_Lock(psJIP_Context);
    
    /* Remove all traps on variables first */
    {
        tsNode *psNode = psJIP_Context->sNetwork.psNodes;
        tsMib  *psMib;
        tsVar  *psVar;
        
        while (psNode)
        {
            psMib = psNode->psMibs;
            while (psMib)
            {
                psVar = psMib->psVars;
                while(psVar)
                {
                    /* Run length of Vars, removing traps if necessary */
                    if (psVar->prCbVarTrap)
                    {
                        DBG_vPrintf(DBG_JIP, "Removing trap on variable at %p (%s)\n", psVar, psVar->pcName ? psVar->pcName: "?");
                        eJIP_UntrapVar(psJIP_Context, psVar, psVar->u8TrapHandle);
                    }
                    psVar = psVar->psNext;
                }
                psMib = psMib->psNext;
            }
            psNode = psNode->psNext;
        }
    }
    eJIP_Unlock(psJIP_Context);

    /* We should now be done with the network connection, so we can now tear it down and stop receiving any trap notifications */
    Network_Destroy(&psJIP_Private->sNetworkContext);
    
    /* Now destroy all context data */
    eJIP_Lock(psJIP_Context); // There should be no other threads at this point.
    while (psJIP_Context->sNetwork.psNodes)
    {
        tsNode *psNode;
        if (eJIP_NetRemoveNode(psJIP_Context, &psJIP_Context->sNetwork.psNodes->sNode_Address, &psNode) != E_JIP_OK)
        {
            DBG_vPrintf(DBG_JIP, "Error removing node\n");
            eStatus = E_JIP_ERROR_FAILED;
            break;
        }
        else
        {
            if (eJIP_NetFreeNode(psJIP_Context, psNode) != E_JIP_OK)
            {
                DBG_vPrintf(DBG_JIP, "Error Free'ing node\n");
            }
        }
    }
    
    Cache_Destroy(&psJIP_Private->sCache);
    
    eLockDestroy(&psJIP_Private->sLock);
    
    free(psJIP_Private);
    
    return eStatus;
}


teJIP_Status eJIP_PrintNetworkContent(tsJIP_Context *psJIP_Context)
{
    //PRIVATE_CONTEXT(psJIP_Context);
    tsNode *psNode;
    tsMib *psMib;
    tsVar *psVar;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    printf("Network: \n");
    
    eJIP_Lock(psJIP_Context);
    
    psNode = psJIP_Context->sNetwork.psNodes;
    while (psNode)
    {
        printf("  Node: ");
        DBG_vPrintf_IPv6Address(1, psNode->sNode_Address.sin6_addr);
        printf("    Device ID: 0x%08x\n", psNode->u32DeviceId);
        
        psMib = psNode->psMibs;
        while (psMib)
        {
            printf("    Mib: 0x%08x, %s\n", psMib->u32MibId, psMib->pcName);
            
            psVar = psMib->psVars;
            while (psVar)
            {
                printf("      Var: %s%s\n", psVar->pcName, psVar->eEnable == E_JIP_VAR_DISABLED ? " (disabled)": "");
                //DBG_vPrintf_IPv6Address(1, psNode->sNode_Address);
                
                printf("        ");
                switch (psVar->eVarType)
                {
#define TEST(a) case  (a): printf(#a); break
                    TEST(E_JIP_VAR_TYPE_INT8);
                    TEST(E_JIP_VAR_TYPE_UINT8);
                    TEST(E_JIP_VAR_TYPE_INT16);
                    TEST(E_JIP_VAR_TYPE_UINT16);
                    TEST(E_JIP_VAR_TYPE_INT32);
                    TEST(E_JIP_VAR_TYPE_UINT32);
                    TEST(E_JIP_VAR_TYPE_INT64);
                    TEST(E_JIP_VAR_TYPE_UINT64);
                    TEST(E_JIP_VAR_TYPE_FLT);
                    TEST(E_JIP_VAR_TYPE_DBL);
                    TEST(E_JIP_VAR_TYPE_STR);
                    TEST(E_JIP_VAR_TYPE_BLOB);
                    TEST(E_JIP_VAR_TYPE_TABLE_BLOB);
                    default: printf("Unknown Type");
#undef TEST
                }
                
                printf(", ");
                switch (psVar->eAccessType)
                {
#define TEST(a) case  (a): printf(#a); break
                    TEST(E_JIP_ACCESS_TYPE_CONST);
                    TEST(E_JIP_ACCESS_TYPE_READ_ONLY);
                    TEST(E_JIP_ACCESS_TYPE_READ_WRITE);
                    default: printf("Unknown Access Type");
#undef TEST
                }             

                printf(", ");
                switch (psVar->eSecurity)
                {
#define TEST(a) case  (a): printf(#a); break
                    TEST(E_JIP_SECURITY_NONE);
                    default: printf("Unknown Security Type");
#undef TEST
                }   
                printf("\n");
                
                printf("          Value: ");
                if (psVar->pvData)
                {
                    switch (psVar->eVarType)
                    {
#define TEST(a, b, c) case  (a): printf(b, *psVar->c); break
                        TEST(E_JIP_VAR_TYPE_INT8,   "%d\n\r",   pi8Data);
                        TEST(E_JIP_VAR_TYPE_UINT8,  "%u\n\r",   pu8Data);
                        TEST(E_JIP_VAR_TYPE_INT16,  "%d\n\r",   pi16Data);
                        TEST(E_JIP_VAR_TYPE_UINT16, "%u\n\r",   pu16Data);
                        TEST(E_JIP_VAR_TYPE_INT32,  "%d\n\r",   pi32Data);
                        TEST(E_JIP_VAR_TYPE_UINT32, "%u\n\r",   pu32Data);
                        TEST(E_JIP_VAR_TYPE_INT64,  "%lld\n\r", pi64Data);
                        TEST(E_JIP_VAR_TYPE_UINT64, "%llu\n\r", pu64Data);
                        TEST(E_JIP_VAR_TYPE_FLT,    "%f\n\r",   pfData);
                        TEST(E_JIP_VAR_TYPE_DBL,    "%f\n\r",   pdData);
                        case  (E_JIP_VAR_TYPE_STR): 
                            printf("%s\n", psVar->pcData); 
                            break;
                        case (E_JIP_VAR_TYPE_BLOB):
                        {
                            uint32_t i;
                            printf("{");
                            for (i = 0; i < psVar->u8Size; i++)
                            {
                                printf(" 0x%02x", psVar->pbData[i]);
                            }
                            printf(" }\n");
                            break;
                        }
                        
                        case (E_JIP_VAR_TYPE_TABLE_BLOB):
                        {
                            tsTableRow *psTableRow;
                            int i;
                            printf("\n");
                            for (i = 0; i < psVar->ptData->u32NumRows; i++)
                            {
                                psTableRow = &psVar->ptData->psRows[i];
                                if (psTableRow->pvData)
                                {
                                    uint32_t j;
                                    printf("            %03d {", i);
                                    for (j = 0; j < psTableRow->u32Length; j++)
                                    {
                                        printf(" 0x%02x", psTableRow->pbData[j]);
                                    }
                                    printf(" }\n");
                                }
                            }
                            break;
                        }
                        default: printf("Unknown Type\n");
#undef TEST
                    }
                }
                else
                {
                    printf("?\n");
                }
                printf("\n");
                psVar = psVar->psNext;
            }
            psMib = psMib->psNext;
        }
        psNode = psNode->psNext;
    }

    eJIP_Unlock(psJIP_Context);
    
    return E_JIP_OK;
}


const char *pcJIP_strerror(teJIP_Status eStatus)
{
    const char *pcResult;
    
    switch (eStatus)
    {
#define TEST(a, b) case (a): pcResult = b; break
        /* These statuses are used by libJIP and pass over the network */
        TEST(E_JIP_OK,                      "Success");
        TEST(E_JIP_ERROR_TIMEOUT,           "Operation timed out");
        TEST(E_JIP_ERROR_BAD_MIB_INDEX,     "Bad MIB Index/ID");
        TEST(E_JIP_ERROR_BAD_VAR_INDEX,     "Bad variable index");
        TEST(E_JIP_ERROR_NO_ACCESS,         "Access denied");
        TEST(E_JIP_ERROR_BAD_BUFFER_SIZE,   "Bad buffer size");
        TEST(E_JIP_ERROR_WRONG_TYPE,        "Incorrect variable data type");
        TEST(E_JIP_ERROR_BAD_VALUE,         "Invalid value for variable");
        TEST(E_JIP_ERROR_DISABLED,          "Variable is disabled");
        TEST(E_JIP_ERROR_FAILED,            "Failed");
        
        /* These statuses are used by libJIP */
        TEST(E_JIP_ERROR_BAD_DEVICE_ID,     "Bad Device ID");
        TEST(E_JIP_ERROR_NETWORK,           "Network Error");
        TEST(E_JIP_ERROR_WOULD_BLOCK,       "Operation would block");
        TEST(E_JIP_ERROR_NO_MEM,            "Memory allocation failed");
        TEST(E_JIP_ERROR_WRONG_CONTEXT,     "Wrong context");

        default: pcResult = "Unknown status";
    }
    return pcResult;
}


/* Implementation is common to client and server - do local update here for server, or call eJIP_MulticastSetVar for client. */
teJIP_Status eJIP_SetVar(tsJIP_Context *psJIP_Context, tsVar *psVar, void *pvData, uint32_t u32Size)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);  
    
    if (psJIP_Private->eJIP_ContextType == E_JIP_CONTEXT_SERVER)
    {
        teJIP_Status eStatus;
        tsNode *psNode;
        tsMib *psMib;
        
        psMib = psVar->psOwnerMib;
        psNode = psMib->psOwnerNode;
        
        if (psVar->eVarType == E_JIP_VAR_TYPE_STR)
        {
            /* Assume we are being passed a NULL terminated string to set the variable to */
            u32Size++;
        }
        
        eJIP_LockNode(psNode, True);
        eStatus = eJIP_SetVarValue(psVar, pvData, u32Size);
        eJIP_UnlockNode(psNode);
        return eStatus;
    }
    
    return eJIP_MulticastSetVar(psJIP_Context, psVar, pvData, u32Size, NULL, 1);
}


teJIP_Status eJIP_SetVarValue(tsVar *psVar, void *pvData, uint32_t u32Size)
{
    void *pvNewData;
    
    pvNewData = realloc(psVar->pvData, u32Size);
    if (!pvNewData && u32Size > 0) 
    {
        return E_JIP_ERROR_NO_MEM;
    }
    psVar->pvData = pvNewData;
    memcpy(psVar->pvData, pvData, u32Size);
    psVar->u8Size = u32Size;
        
    return E_JIP_OK;
}


/* Lock mutex on JIP context */
teJIP_Status eJIP_Lock(tsJIP_Context *psJIP_Context)
{
    PRIVATE_CONTEXT(psJIP_Context);
    eJIPLockLock(&psJIP_Private->sLock);
    return E_JIP_OK;
}


/* Unlock mutex on JIP Context */
teJIP_Status eJIP_Unlock(tsJIP_Context *psJIP_Context)
{
    PRIVATE_CONTEXT(psJIP_Context);
    eJIPLockUnlock(&psJIP_Private->sLock);
    return E_JIP_OK;
}


