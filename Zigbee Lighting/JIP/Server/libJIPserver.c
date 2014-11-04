/****************************************************************************
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

 * Copyright NXP B.V. 2013. All rights reserved
 *
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <Threads.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_JIP_SERVER 0


static teJIP_Status eJIPserver_HandleGet(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_GetIndexRequest *psGetVar,
                                         uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleSet(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psDstAddress, tsJIP_Msg_SetIndexRequest *psSetVar,
                                         unsigned int iReceiveDataLength, uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleQueryMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_QueryMibRequest *psQueryMib,
                                              uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleQueryVar(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_QueryVarRequest *psQueryVar,
                                              uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleTrap(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleUntrap(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength);


static teJIP_Status eJIPserver_HandleGetMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_GetMibRequest *psGetVar,
                                            uint8_t *pcSendData, unsigned int *piSendDataLength);

static teJIP_Status eJIPserver_HandleSetMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psDstAddress, tsJIP_Msg_SetMibRequest *psSetVar,
                                            unsigned int iReceiveDataLength, uint8_t *pcSendData, unsigned int *piSendDataLength);



teJIP_Status eJIPserver_Listen(tsJIP_Context *psJIP_Context)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_SERVER)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }

    eJIP_Lock(psJIP_Context);
    
    if (Network_Listen(&psJIP_Private->sNetworkContext) == E_NETWORK_OK)
    {
        eStatus = E_JIP_OK;
    }
    
    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


teJIP_Status eJIPserver_NodeAdd(tsJIP_Context *psJIP_Context, const char *pcAddress, const int iPort, 
                                uint32_t u32DeviceId, char *pcName, const char *pcVersion, 
                                tsNode **ppsNode)
{
    PRIVATE_CONTEXT(psJIP_Context);
    teJIP_Status eStatus;
    tsNode *psNewNode;
    tsMib *psMib;
    tsVar *psVar;
    char acPort[10];
    struct addrinfo hints, *res;
    int s;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: %s\n", __FUNCTION__, pcAddress);

    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_SERVER)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }

    /* Get the port back into a string for getaddrinfo */
    snprintf(acPort, 10, "%d", iPort);
    
    // load up address structs with getaddrinfo() for the socket:
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo(pcAddress, acPort, &hints, &res);
    
    if (s != 0)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "getaddrinfo failed: %s\n", gai_strerror(s));
        return E_JIP_ERROR_NETWORK;
    }

    /* Add the new node to the net */
    eStatus = eJIP_NetAddNode(psJIP_Context, (tsJIPAddress *)res->ai_addr, u32DeviceId, &psNewNode);
    if (eStatus != E_JIP_OK)
    {
        freeaddrinfo(res);
        return eStatus;
    }
    
    freeaddrinfo(res);

    /* Set all variables to disabled state */
    psMib = psNewNode->psMibs;
    while (psMib)
    {
        psVar = psMib->psVars;
        while (psVar)
        {
            psVar->eEnable = E_JIP_VAR_DISABLED;
            psVar = psVar->psNext;
        }
        psMib = psMib->psNext;
    }

    /* Now populate the standard MIBs with data */
    
    psMib = psJIP_LookupMibId(psNewNode, NULL, 0xffffff00);
    if (psMib)
    {
        if (pcName)
        {
            psVar = psJIP_LookupVarIndex(psMib, 1);
            
            if (psVar)
            {
                if (eJIP_SetVar(psJIP_Context, psVar, pcName, strlen(pcName)) != E_JIP_OK)
                {
                    return E_JIP_ERROR_FAILED;
                }
                // Enable variable
                psVar->eEnable = E_JIP_VAR_ENABLED;
            }
        }
        
        if (pcVersion)
        {
            psVar = psJIP_LookupVarIndex(psMib, 2);
            
            if (psVar)
            {
                if (eJIP_SetVar(psJIP_Context, psVar, (char *)pcVersion, strlen(pcVersion)) != E_JIP_OK)
                {
                    return E_JIP_ERROR_FAILED;
                }
                // Enable variable
                psVar->eEnable = E_JIP_VAR_ENABLED;
            }
        }
    }
    
    if ((eStatus = eGroups_Init(psNewNode)) != E_JIP_OK)
    {
        return eStatus;
    }
        
    
    DBG_vPrintf(DBG_JIP_SERVER, "Node created\n");
    
    /* If the network change callback has been registered, call it here */
    if (psJIP_Private->prCbNetworkChange)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "Callback NetworkChange for node: %s\n", pcAddress);
        
        psJIP_Private->prCbNetworkChange(E_JIP_NODE_JOIN, psNewNode);
    }
    
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

    return E_JIP_OK;
}


teJIP_Status eJIPserver_NodeRemove(tsJIP_Context *psJIP_Context, tsNode *psNode)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsNode *psRemovedNode;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_SERVER)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    if (eJIP_NetRemoveNode(psJIP_Context, &psNode->sNode_Address, &psRemovedNode) != E_JIP_OK)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "Failed to remove node\n");
        return E_JIP_ERROR_FAILED;
    }
        
    /* Node found to remove from the network */
    
    /* If the network change callback has been registered, call it here */
    if (psJIP_Private->prCbNetworkChange)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "Callback NetworkChange for node:\n");
        DBG_vPrintf_IPv6Address(DBG_JIP_SERVER, psRemovedNode->sNode_Address.sin6_addr);
        
        psJIP_Private->prCbNetworkChange(E_JIP_NODE_LEAVE, psRemovedNode);
    }
    
    /* Now we can free the node */
    return eJIP_NetFreeNode(psJIP_Context, psRemovedNode);
}




teJIP_Status eJIPserver_HandlePacket(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress, tsJIPAddress *psDstAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    switch (eReceiveCommand)
    {
        case (E_JIP_COMMAND_GET_REQUEST):
        {
            tsJIP_Msg_GetIndexRequest *psGetVar = (tsJIP_Msg_GetIndexRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_GET_RESPONSE;
            
            return eJIPserver_HandleGet(psJIP_Context, psNode, psGetVar, pcSendData, piSendDataLength);
        }
            
        case (E_JIP_COMMAND_SET_REQUEST):
        {
            tsJIP_Msg_SetIndexRequest *psSetVar = (tsJIP_Msg_SetIndexRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_SET_RESPONSE;
            
            return eJIPserver_HandleSet(psJIP_Context, psNode, psDstAddress, psSetVar, iReceiveDataLength, pcSendData, piSendDataLength);
        }
            
        case (E_JIP_COMMAND_QUERY_MIB_REQUEST):
        {
            tsJIP_Msg_QueryMibRequest *psQueryMib = (tsJIP_Msg_QueryMibRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_QUERY_MIB_RESPONSE;
            
            return eJIPserver_HandleQueryMib(psJIP_Context, psNode, psQueryMib, pcSendData, piSendDataLength);
        }
            
        case (E_JIP_COMMAND_QUERY_VAR_REQUEST):
        {
            tsJIP_Msg_QueryVarRequest *psQueryVar = (tsJIP_Msg_QueryVarRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_QUERY_VAR_RESPONSE;
            
            return eJIPserver_HandleQueryVar(psJIP_Context, psNode, psQueryVar, pcSendData, piSendDataLength);
        }
            
        case (E_JIP_COMMAND_TRAP_REQUEST):
            break;
            
        case (E_JIP_COMMAND_UNTRAP_REQUEST):
            break;
            
        case (E_JIP_COMMAND_GET_MIB_REQUEST):
        {
            tsJIP_Msg_GetMibRequest *psGetVar = (tsJIP_Msg_GetMibRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_GET_RESPONSE;
            
            if (iReceiveDataLength == 8)
            {
                /* Java doesn't bother to send the number of variables required if it is one */
                psGetVar->sRequest.u8VarCount = 1;
            }
            
            return eJIPserver_HandleGetMib(psJIP_Context, psNode, psGetVar, pcSendData, piSendDataLength);
        }
        
        case (E_JIP_COMMAND_SET_MIB_REQUEST):
        {
            tsJIP_Msg_SetMibRequest *psSetVar = (tsJIP_Msg_SetMibRequest *)pcReceiveData;
            *peSendCommand = E_JIP_COMMAND_SET_RESPONSE;
            
            return eJIPserver_HandleSetMib(psJIP_Context, psNode, psDstAddress, psSetVar, iReceiveDataLength, pcSendData, piSendDataLength);
        }
            
        default:
            DBG_vPrintf(DBG_JIP_SERVER, "Unhandled command: 0x%02x\n", eReceiveCommand);
            break;
    }

    return eStatus;
}


static teJIP_Status eJIPserver_HandleGet(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_GetIndexRequest *psGetVar,
                                              uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    tsMib *psMib;
    tsJIP_Msg_VarDescriptionHeader *psGetResponseHeader = (tsJIP_Msg_VarDescriptionHeader *)pcSendData;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Mib Index %d, Var %d)\n", __FUNCTION__, 
                psGetVar->u8MibIndex, psGetVar->sRequest.u8VarIndex);
    
    for(psMib = psNode->psMibs; psMib; psMib = psMib->psNext)
    {
        if (psMib->u8Index == psGetVar->u8MibIndex)
        {
            // Convert get by MIB Index into get by MIB ID request.
            tsJIP_Msg_GetMibRequest psGetVarByMib;
            
            psGetVarByMib.u32MibId = psMib->u32MibId;
            psGetVarByMib.sRequest = psGetVar->sRequest;
            
            return eJIPserver_HandleGetMib(psJIP_Context, psNode, &psGetVarByMib, pcSendData, piSendDataLength);
        }
    }
    
    // If we get here, we couldn't find a matching MIB index
    DBG_vPrintf(DBG_JIP_SERVER, "%s: MIB %d not found\n", __FUNCTION__, psGetVar->u8MibIndex);
    psGetResponseHeader->u8MibIndex  = 0;
    psGetResponseHeader->u8VarIndex  = 0;
    psGetResponseHeader->eStatus     = E_JIP_ERROR_BAD_MIB_INDEX;
    psGetResponseHeader->eVarType    = 0;
    *piSendDataLength = sizeof(tsJIP_Msg_VarDescriptionHeaderError);
    return E_JIP_OK;
}


static teJIP_Status eJIPserver_HandleSet(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psDstAddress, tsJIP_Msg_SetIndexRequest *psSetVar,
                                         unsigned int iReceiveDataLength, uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    tsMib *psMib;
    tsJIP_Msg_VarStatus *psSetResponse = (tsJIP_Msg_VarStatus *)pcSendData;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Mib Index %d, Var %d)\n", __FUNCTION__, 
                psSetVar->u8MibIndex, psSetVar->sRequest.u8VarIndex);
    
    for(psMib = psNode->psMibs; psMib; psMib = psMib->psNext)
    {
        if (psMib->u8Index == psSetVar->u8MibIndex)
        {
            // Convert get by MIB Index into get by MIB ID request.
            tsJIP_Msg_SetMibRequest psSetVarByMib;
            
            psSetVarByMib.u32MibId = psMib->u32MibId;
            psSetVarByMib.sRequest = psSetVar->sRequest;
            
            return eJIPserver_HandleSetMib(psJIP_Context, psNode, psDstAddress, &psSetVarByMib, iReceiveDataLength, pcSendData, piSendDataLength);
        }
    }
    
    // If we get here, we couldn't find a matching MIB index
    DBG_vPrintf(DBG_JIP_SERVER, "%s: MIB %d not found\n", __FUNCTION__, psSetVar->u8MibIndex);
    psSetResponse->u8MibIndex  = 0;
    psSetResponse->u8VarIndex  = 0;
    psSetResponse->eStatus     = E_JIP_ERROR_BAD_MIB_INDEX;
    *piSendDataLength = sizeof(tsJIP_Msg_VarStatus);
    return E_JIP_OK;
}


static teJIP_Status eJIPserver_HandleQueryMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_QueryMibRequest *psQueryMib,
                                 uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    int i, j, iPacketOffset;
    tsMib *psMib;
    tsJIP_Msg_QueryMibResponseHeader *psQueryMibResponseHeader = (tsJIP_Msg_QueryMibResponseHeader*)pcSendData;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Start %d, Num %d)\n", __FUNCTION__, psQueryMib->u8MibStartIndex, psQueryMib->u8NumMibs);
    
    /* Skip initial MIBs */
    for (i = 0, psMib = psNode->psMibs; 
        (i < psQueryMib->u8MibStartIndex) && (psMib);
        i++, psMib = psMib->psNext);
    
    /* Now check if we have any MIBs left */
    if (!psMib)
    {
        /* No Mibs left to return */
        psQueryMibResponseHeader->eStatus               = E_JIP_OK;
        psQueryMibResponseHeader->u8NumMibsReturned     = 0;
        psQueryMibResponseHeader->u8NumMibsOutstanding  = 0;
        return E_JIP_OK;
    }
    
    iPacketOffset = sizeof(tsJIP_Msg_QueryMibResponseHeader);
    
    /* For each requested MIB in range */
    for (j = 0;
        (j < psQueryMib->u8NumMibs) && (psMib);
        i++, j++, psMib = psMib->psNext)
    {
        tsJIP_Msg_QueryMibResponseListEntryHeader* psMibResposeEntry = (tsJIP_Msg_QueryMibResponseListEntryHeader*)&pcSendData[iPacketOffset];
        
        DBG_vPrintf(DBG_JIP_SERVER, "Adding MIB %d(0x%08x): %s\n", psMib->u8Index, psMib->u32MibId, psMib->pcName);

        psMibResposeEntry->u8MibIndex   = psMib->u8Index;
        psMibResposeEntry->u32MibID     = htonl(psMib->u32MibId);
        psMibResposeEntry->u8NameLen    = strlen(psMib->pcName);
        strcpy(psMibResposeEntry->acName, psMib->pcName);
        
        iPacketOffset += sizeof(tsJIP_Msg_QueryMibResponseListEntryHeader) + psMibResposeEntry->u8NameLen;
    }
    
    DBG_vPrintf(DBG_JIP_SERVER, "Return %d MIBs, %d remaining\n", j, psNode->u32NumMibs - i);
    
    psQueryMibResponseHeader->eStatus                   = E_JIP_OK;
    psQueryMibResponseHeader->u8NumMibsReturned         = j;
    psQueryMibResponseHeader->u8NumMibsOutstanding      = psNode->u32NumMibs - i;

    *piSendDataLength = iPacketOffset;
    return E_JIP_OK;
}


static teJIP_Status eJIPserver_HandleQueryVar(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_QueryVarRequest *psQueryVar,
                                              uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    int i, j, iPacketOffset;
    tsMib *psMib;
    tsVar *psVar;
    tsJIP_Msg_QueryVarResponseHeader *psQueryVarResponseHeader = (tsJIP_Msg_QueryVarResponseHeader*)pcSendData;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Mib Index %d, Start %d, Num %d)\n", __FUNCTION__, 
                psQueryVar->u8MibIndex, psQueryVar->u8VarStartIndex, psQueryVar->u8NumVars);
    
    /* Skip initial Mibs */
    for (i = 0, psMib = psNode->psMibs; 
        (i < psQueryVar->u8MibIndex) && (psMib);
        i++, psMib = psMib->psNext);
    
    /* Check that the MIB was valid*/
    if (!psMib)
    {
        /* No Mibs left to return */
        psQueryVarResponseHeader->eStatus               = E_JIP_ERROR_BAD_MIB_INDEX;
        psQueryVarResponseHeader->u8NumVarsReturned     = 0;
        psQueryVarResponseHeader->u8NumVarsOutstanding  = 0;
        return E_JIP_OK;
    }
    
    /* Skip initial Vars */
    for (i = 0, psVar = psMib->psVars; 
        (i < psQueryVar->u8VarStartIndex) && (psVar);
        i++, psVar = psVar->psNext);

    /* Now check if we have any Vars left */
    if (!psVar)
    {
        /* No Mibs left to return */
        psQueryVarResponseHeader->eStatus               = E_JIP_OK;
        psQueryVarResponseHeader->u8NumVarsReturned     = 0;
        psQueryVarResponseHeader->u8NumVarsOutstanding  = 0;
        return E_JIP_OK;
    }
    
    iPacketOffset = sizeof(tsJIP_Msg_QueryVarResponseHeader);
    
    /* For each requested Var in range */
    for (j = 0;
        (j < psQueryVar->u8NumVars) && (psVar);
        i++, j++, psVar = psVar->psNext)
    {
        tsJIP_Msg_QueryVarResponseListEntryHeader* psVarResposeEntry = (tsJIP_Msg_QueryVarResponseListEntryHeader*)&pcSendData[iPacketOffset];
        tsJIP_Msg_QueryVarResponseListEntryFooter* psVarResposeEntryFooter;
        
        DBG_vPrintf(DBG_JIP_SERVER, "Adding Var %d: %s\n", psVar->u8Index, psVar->pcName);

        psVarResposeEntry->u8VarIndex           = psVar->u8Index;
        psVarResposeEntry->u8NameLen            = strlen(psVar->pcName);
        strcpy(psVarResposeEntry->acName, psVar->pcName);
        
        psVarResposeEntryFooter = (tsJIP_Msg_QueryVarResponseListEntryFooter*)(psVarResposeEntry->acName + strlen(psVar->pcName));
        psVarResposeEntryFooter->eVarType       = psVar->eVarType;
        psVarResposeEntryFooter->eAccessType    = psVar->eAccessType;
        psVarResposeEntryFooter->eSecurity      = psVar->eSecurity;

        iPacketOffset += sizeof(tsJIP_Msg_QueryVarResponseListEntryHeader) + sizeof(tsJIP_Msg_QueryVarResponseListEntryFooter) + psVarResposeEntry->u8NameLen;
    }
    
    DBG_vPrintf(DBG_JIP_SERVER, "Return %d vars, %d remaining\n", j, psMib->u32NumVars - i);

    psQueryVarResponseHeader->eStatus                   = E_JIP_OK;
    psQueryVarResponseHeader->u8NumVarsReturned         = j;
    psQueryVarResponseHeader->u8NumVarsOutstanding      = psMib->u32NumVars - i;
    
    *piSendDataLength = iPacketOffset;
    return E_JIP_OK;
}


static teJIP_Status eJIPserver_HandleTrap(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    return E_JIP_ERROR_FAILED;
}


static teJIP_Status eJIPserver_HandleUntrap(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    return E_JIP_ERROR_FAILED;
}


static teJIP_Status eJIPserver_HandleGetMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIP_Msg_GetMibRequest *psGetVar,
                                              uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    tsMib *psMib;
    tsVar *psVar;
    tsJIP_Msg_VarDescriptionHeader *psGetMibResponseHeader = (tsJIP_Msg_VarDescriptionHeader *)pcSendData;
    teJIP_Status eStatus = E_JIP_OK;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Mib ID 0x%08x, Var %d)\n", __FUNCTION__, 
                ntohl(psGetVar->u32MibId), psGetVar->sRequest.u8VarIndex);
    
    psMib = psJIP_LookupMibId(psNode, NULL, ntohl(psGetVar->u32MibId));
    if (!psMib)
    {
        /* MIB not found */
        DBG_vPrintf(DBG_JIP_SERVER, "%s: MIB 0x%08x not found\n", __FUNCTION__, ntohl(psGetVar->u32MibId));
        psGetMibResponseHeader->u8MibIndex  = 0;
        psGetMibResponseHeader->u8VarIndex  = 0;
        psGetMibResponseHeader->eStatus     = E_JIP_ERROR_BAD_MIB_INDEX;
        *piSendDataLength = sizeof(tsJIP_Msg_VarDescriptionHeaderError);
        return E_JIP_OK;
    }
    
    /* MIB found */
    psVar = psJIP_LookupVarIndex(psMib, psGetVar->sRequest.u8VarIndex);
    if (!psVar)
    {
        /* Variable not found */
        DBG_vPrintf(DBG_JIP_SERVER, "%s: Variable %d in MIB 0x%08x not found\n", __FUNCTION__, psGetVar->sRequest.u8VarIndex, ntohl(psGetVar->u32MibId));
        psGetMibResponseHeader->u8MibIndex  = 0;
        psGetMibResponseHeader->u8VarIndex  = 0;
        psGetMibResponseHeader->eStatus     = E_JIP_ERROR_BAD_VAR_INDEX;
        *piSendDataLength = sizeof(tsJIP_Msg_VarDescriptionHeaderError);
        return E_JIP_OK;
    }
    
    /* Var found */
    if (psVar->eVarType == E_JIP_VAR_TYPE_TABLE_BLOB)
    {
        /* And it's of Table type */
        
        psGetVar->sRequest.u16FirstEntry = ntohs(psGetVar->sRequest.u16FirstEntry);
        
        DBG_vPrintf(DBG_JIP_SERVER, "%s: Get table rows start %d, num %d variable index %d in MIB 0x%08x\n", 
                    __FUNCTION__, psGetVar->sRequest.u16FirstEntry, psGetVar->sRequest.u8EntryCount, 
                    psGetVar->sRequest.u8VarIndex, ntohl(psGetVar->u32MibId));
 
        psGetMibResponseHeader->eVarType    = psVar->eVarType;
        psGetMibResponseHeader->u8MibIndex  = psMib->u8Index;
        psGetMibResponseHeader->u8VarIndex  = psVar->u8Index;
 
        if (psVar->prCbVarGet)
        {
            /* Variable has get callback - call it */
            eStatus = psVar->prCbVarGet(psVar);
            
            if (eStatus == E_JIP_ERROR_TIMEOUT)
            {
                /* In case of a timeout, don't return a response */
                return eStatus;
            }
            else if (eStatus != E_JIP_OK)
            {
                /* Get function returned an error - send it back now */
                DBG_vPrintf(DBG_JIP_SERVER, "%s: Get function returned error %d\n", __FUNCTION__, eStatus);
                psGetMibResponseHeader->eStatus     = eStatus;
                *piSendDataLength = sizeof(tsJIP_Msg_VarDescriptionHeaderError);
                return E_JIP_OK;
            }
        }
        
        if ((psVar->eEnable != E_JIP_VAR_ENABLED) || !psVar->pvData)
        {
            /* Variable is disabled or has no data - return DISABLED */
            DBG_vPrintf(DBG_JIP_SERVER, "%s: No data - variable %d disabled\n", __FUNCTION__, psMib->u8Index);
            psGetMibResponseHeader->eStatus     = E_JIP_ERROR_DISABLED;
            *piSendDataLength = sizeof(tsJIP_Msg_VarDescriptionHeaderError);
            return E_JIP_OK;
        }

        return eJIPserver_HandleGetTableVar(psJIP_Context, psVar, 
                                            psGetVar->sRequest.u16FirstEntry, psGetVar->sRequest.u8EntryCount,
                                            pcSendData, piSendDataLength);
    }
    else
    {
        int iVarIndex;
        int iPacketOffset;
        /* Non table variable */
        DBG_vPrintf(DBG_JIP_SERVER, "%s: Get variable start index %d, count %d in MIB 0x%08x\n", 
                    __FUNCTION__, psGetVar->sRequest.u8VarIndex, psGetVar->sRequest.u8VarCount, ntohl(psGetVar->u32MibId));
        
        psGetMibResponseHeader->u8MibIndex  = psMib->u8Index;
        psGetMibResponseHeader->u8VarIndex  = psVar->u8Index;
        
        iPacketOffset = sizeof(tsJIP_Msg_VarDescriptionHeader) - sizeof(tsJIP_Msg_VarDescriptionEntry);
        
        for (iVarIndex = psGetVar->sRequest.u8VarIndex; 
            iVarIndex < (psGetVar->sRequest.u8VarIndex + psGetVar->sRequest.u8VarCount) && psVar;
            iVarIndex++, psVar = psVar->psNext)
        {
            uint32_t u32Size = 0;
            tsJIP_Msg_VarDescriptionEntry *psEntry = (tsJIP_Msg_VarDescriptionEntry *)&pcSendData[iPacketOffset];
            
            DBG_vPrintf(DBG_JIP_SERVER, "%s: Add var index %d\n", __FUNCTION__, iVarIndex);
            
            psEntry->eVarType    = psVar->eVarType;
            
            if (psVar->prCbVarGet)
            {
                /* Variable has get callback - call it */
                eStatus = psVar->prCbVarGet(psVar);
                
                if (eStatus == E_JIP_ERROR_TIMEOUT)
                {
                    /* In case of a timeout, don't return a response */
                    return eStatus;
                }
                else if (eStatus != E_JIP_OK)
                {
                    /* Get function returned an error - send it back now */
                    DBG_vPrintf(DBG_JIP_SERVER, "%s: Get function returned error %d\n", __FUNCTION__, eStatus);
                    psGetMibResponseHeader->eStatus     = eStatus;
                    iPacketOffset                      += sizeof(tsJIP_Msg_VarDescriptionEntryError);
                    continue;
                }
            }
            
            if ((psVar->eEnable != E_JIP_VAR_ENABLED) || !psVar->pvData)
            {
                /* Variable has no data - return DISABLED */
                DBG_vPrintf(DBG_JIP_SERVER, "%s: No data - variable %d disabled\n", __FUNCTION__, iVarIndex);
                psEntry->eStatus     = E_JIP_ERROR_DISABLED;
                iPacketOffset       += sizeof(tsJIP_Msg_VarDescriptionEntryError);
                continue;
            }

            switch(psVar->eVarType)
            {
                case (E_JIP_VAR_TYPE_INT8):
                case (E_JIP_VAR_TYPE_UINT8):
                    u32Size = sizeof(uint8_t);
                    *(uint8_t *)psEntry->au8Data = *psVar->pu8Data;
                    break;
                    
                case (E_JIP_VAR_TYPE_INT16):
                case (E_JIP_VAR_TYPE_UINT16):
                {
                    uint16_t u16Var = htons(*psVar->pu16Data);
                    u32Size = sizeof(uint16_t);
                    memcpy(psEntry->au8Data, &u16Var, sizeof(uint16_t));
                    break;
                }
                
                case (E_JIP_VAR_TYPE_INT32):
                case (E_JIP_VAR_TYPE_UINT32):
                case (E_JIP_VAR_TYPE_FLT):
                {
                    uint32_t u32Var = htonl(*psVar->pu32Data);
                    u32Size = sizeof(uint32_t);
                    memcpy(psEntry->au8Data, &u32Var, sizeof(uint32_t));
                    break;
                }
                
                case (E_JIP_VAR_TYPE_INT64):
                case (E_JIP_VAR_TYPE_UINT64):
                case (E_JIP_VAR_TYPE_DBL):
                {
                    uint64_t u64Var = htobe64(*psVar->pu64Data);
                    u32Size = sizeof(uint64_t);
                    memcpy(psEntry->au8Data, &u64Var, sizeof(uint64_t));
                    break;
                }
                
                case (E_JIP_VAR_TYPE_STR):
                    u32Size = strlen(psVar->pcData);
                    *(uint8_t *)psEntry->au8Data = (uint8_t)u32Size;
                    memcpy(&psEntry->au8Data[1], psVar->pcData, u32Size);
                    u32Size += sizeof(uint8_t); /* Size of length component */
                    break;
                    
                case (E_JIP_VAR_TYPE_BLOB):
                    u32Size = psVar->u8Size;
                    *(uint8_t *)psEntry->au8Data = (uint8_t)u32Size;
                    memcpy(&psEntry->au8Data[1], psVar->pbData, u32Size);
                    u32Size += sizeof(uint8_t); /* Size of length component */
                    break;
                    
                default:
                    break;
            }    
            
            psEntry->eStatus     = E_JIP_OK;
            iPacketOffset += sizeof(tsJIP_Msg_VarDescriptionEntry) + u32Size;
        }

        *piSendDataLength = iPacketOffset;
        return E_JIP_OK;
    }
    return E_JIP_ERROR_FAILED;
}


static teJIP_Status eJIPserver_HandleSetMib(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psDstAddress, tsJIP_Msg_SetMibRequest *psSetVar,
                                            unsigned int iReceiveDataLength, uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    tsMib *psMib;
    tsVar *psVar;
    teJIP_Status eStatus;
    tsJIP_Msg_VarStatus *psSetMibResponse = (tsJIP_Msg_VarStatus *)pcSendData;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s(Mib ID 0x%08x, Var %d)\n", __FUNCTION__, 
                ntohl(psSetVar->u32MibId), psSetVar->sRequest.u8VarIndex);
    
    /* Set response length */
    *piSendDataLength = sizeof(tsJIP_Msg_VarStatus);
    
    psMib = psJIP_LookupMibId(psNode, NULL, ntohl(psSetVar->u32MibId));
    if (!psMib)
    {
        /* MIB not found */
        DBG_vPrintf(DBG_JIP_SERVER, "%s: MIB 0x%08x not found\n", __FUNCTION__, ntohl(psSetVar->u32MibId));
        psSetMibResponse->u8MibIndex  = 0;
        psSetMibResponse->u8VarIndex  = 0;
        psSetMibResponse->eStatus     = E_JIP_ERROR_BAD_MIB_INDEX;
        return E_JIP_OK;
    }
    
    /* MIB found */
    psVar = psJIP_LookupVarIndex(psMib, psSetVar->sRequest.u8VarIndex);
    if (!psVar)
    {
        /* Variable not found */
        DBG_vPrintf(DBG_JIP_SERVER, "%s: Variable %d in MIB 0x%08x not found\n", __FUNCTION__, psSetVar->sRequest.u8VarIndex, ntohl(psSetVar->u32MibId));
        psSetMibResponse->u8MibIndex  = 0;
        psSetMibResponse->u8VarIndex  = 0;
        psSetMibResponse->eStatus     = E_JIP_ERROR_BAD_VAR_INDEX;
        return E_JIP_OK;
    }
    
    /* Set up response */
    psSetMibResponse->u8MibIndex  = psMib->u8Index;
    psSetMibResponse->u8VarIndex  = psVar->u8Index;
    
    if (psSetVar->sRequest.sVar.eVarType != psVar->eVarType)
    {
        /* Wrong type specified in set message */
        psSetMibResponse->eStatus = E_JIP_ERROR_WRONG_TYPE;
        return E_JIP_OK;
    }
    
    if ((psVar->eAccessType == E_JIP_ACCESS_TYPE_CONST) || (psVar->eAccessType == E_JIP_ACCESS_TYPE_READ_ONLY))
    {
        /* Can't set const or read only variables */
        psSetMibResponse->eStatus = E_JIP_ERROR_NO_ACCESS;
        return E_JIP_OK;
    }
    
    if (psVar->eEnable != E_JIP_VAR_ENABLED)
    {
        /* Variable is disabled */
        psSetMibResponse->eStatus = E_JIP_ERROR_DISABLED;
        return E_JIP_OK;
    }
    
    /* Assume that the buffer size is going to be wrong */
    eStatus = E_JIP_ERROR_BAD_BUFFER_SIZE;
    
    iReceiveDataLength -= sizeof(tsJIP_Msg_SetMibRequest);
    DBG_vPrintf(DBG_JIP_SERVER, "%s: Data buffer length: %d\n", __FUNCTION__, iReceiveDataLength);
 
    switch (psVar->eVarType)
    {
        case (E_JIP_VAR_TYPE_INT8):
        case (E_JIP_VAR_TYPE_UINT8):
            if (iReceiveDataLength == sizeof(uint8_t))
            {
                eStatus = eJIP_SetVar(psJIP_Context, psVar, psSetVar->sRequest.sVar.au8Data, sizeof(uint8_t));
            }
            break;
         
        case (E_JIP_VAR_TYPE_INT16):
        case (E_JIP_VAR_TYPE_UINT16):
            if (iReceiveDataLength == sizeof(uint16_t))
            {
                uint16_t u16Var;
                memcpy(&u16Var, psSetVar->sRequest.sVar.au8Data, sizeof(uint16_t));
                u16Var = ntohs(u16Var);
                eStatus = eJIP_SetVar(psJIP_Context, psVar, &u16Var, sizeof(uint16_t));
            }
            break;
        
        case (E_JIP_VAR_TYPE_INT32):
        case (E_JIP_VAR_TYPE_UINT32):
        case (E_JIP_VAR_TYPE_FLT):
            if (iReceiveDataLength == sizeof(uint32_t))
            {
                uint32_t u32Var;
                memcpy(&u32Var, psSetVar->sRequest.sVar.au8Data, sizeof(uint32_t));
                u32Var = ntohl(u32Var);
                eStatus = eJIP_SetVar(psJIP_Context, psVar, &u32Var, sizeof(uint32_t));
            }
            break;
        
        case (E_JIP_VAR_TYPE_INT64):
        case (E_JIP_VAR_TYPE_UINT64):
        case (E_JIP_VAR_TYPE_DBL):
            if (iReceiveDataLength == sizeof(uint64_t))
            {
                uint64_t u64Var;
                memcpy(&u64Var, psSetVar->sRequest.sVar.au8Data, sizeof(uint64_t));
                u64Var = be64toh(u64Var);                
                eStatus = eJIP_SetVar(psJIP_Context, psVar, &u64Var, sizeof(uint64_t));
            }
            break;
        
        case (E_JIP_VAR_TYPE_STR):
            if (iReceiveDataLength >= sizeof(uint8_t))
            {
                uint8_t u8StringLen = *((uint8_t *)psSetVar->sRequest.sVar.au8Data);
                
                DBG_vPrintf(DBG_JIP_SERVER, "%s: Received length: %d\n", __FUNCTION__, u8StringLen);
                
                if (u8StringLen == (iReceiveDataLength - 1))
                {
                    /* Special case for string due to incoming packet missing the NULL terminator */
                    void *pvNewData;
                    
                    pvNewData = realloc(psVar->pvData, u8StringLen + 1);
                    if (!pvNewData) 
                    {
                        eStatus = E_JIP_ERROR_NO_MEM;
                    }
                    else
                    {
                        psVar->pvData = pvNewData;
                        memcpy(psVar->pvData, ((uint8_t *)psSetVar->sRequest.sVar.au8Data)+1, u8StringLen);
                        ((char *)psVar->pvData)[u8StringLen] = '\0';
                        psVar->u8Size = u8StringLen + 1;
                        eStatus = E_JIP_OK;
                    }
                }
            }
            break;
            
        case (E_JIP_VAR_TYPE_BLOB):
            if (iReceiveDataLength >= sizeof(uint8_t))
            {
                uint8_t u8BlobLen = *((uint8_t *)psSetVar->sRequest.sVar.au8Data);
                
                DBG_vPrintf(DBG_JIP_SERVER, "%s: Received length: %d\n", __FUNCTION__, u8BlobLen);
                
                if (u8BlobLen == (iReceiveDataLength - 1))
                {
                    eStatus = eJIP_SetVar(psJIP_Context, psVar, ((uint8_t *)psSetVar->sRequest.sVar.au8Data)+1, u8BlobLen);
                }
            }
            break;
            
        default:
            eStatus = E_JIP_ERROR_WRONG_TYPE;
            break;
        
    }
    
    if (eStatus == E_JIP_OK)
    {
        /* Only call the set callback if the data has been set ok */
        if (psVar->prCbVarSet)
        {
            /* Variable has set callback - call it */
            
            if (psDstAddress->sin6_addr.s6_addr[0] == 0xFF)
            {
                /* Multicast */
                DBG_vPrintf(DBG_JIP_SERVER, "Multicast set request to ");
                DBG_vPrintf_IPv6Address(DBG_JIP_SERVER, psDstAddress->sin6_addr);
                
                eStatus = psVar->prCbVarSet(psVar, psDstAddress);
                /* No responses to multicast sets */
            }
            else
            {
                eStatus = psVar->prCbVarSet(psVar, NULL);
                if (eStatus == E_JIP_ERROR_TIMEOUT)
                {
                    /* In case of a timeout, don't return a response */
                    return eStatus;
                }
            }
        }
    }
    
    DBG_vPrintf(DBG_JIP_SERVER, "%s: Set Variable %d in MIB 0x%08x status: %d\n", __FUNCTION__, psSetVar->sRequest.u8VarIndex, ntohl(psSetVar->u32MibId), eStatus);
    psSetMibResponse->eStatus     = eStatus;

    return E_JIP_OK;
}


teJIP_Status eJIPserver_NodeGroupJoin(tsNode *psNode, const char *pcMulticastAddress)
{
    tsNode_Private *psNode_Private = (tsNode_Private *)psNode->pvPriv;
    tsJIP_Context *psJIP_Context = psNode->psOwnerNetwork->psOwnerContext;
    PRIVATE_CONTEXT(psJIP_Context);
    
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    struct in6_addr     sBlankAddress, sMulticastAddress;
    int iGroupAddressSlot;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: %s\n", __FUNCTION__, pcMulticastAddress);

    memset(&sBlankAddress, 0, sizeof(struct in6_addr));

    if (inet_pton(AF_INET6, pcMulticastAddress, &sMulticastAddress) <= 0)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "Could not determine network address from [%s]\n", pcMulticastAddress);
        return E_JIP_ERROR_FAILED;
    }

    /* Check if the node is already in the group */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sMulticastAddress, sizeof(struct in6_addr)) == 0)
        {
            DBG_vPrintf(DBG_JIP_SERVER, "%s: Node is already in group at slot index %d\n", __FUNCTION__, iGroupAddressSlot);
            return E_JIP_OK;
        }
    }
    
    /* Now see if we can find an empty slot */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sBlankAddress, sizeof(struct in6_addr)) == 0)
        {
            DBG_vPrintf(DBG_JIP_SERVER, "%s: Found empty group entry at slot index %d\n", __FUNCTION__, iGroupAddressSlot);
            break;
        }
    }
    
    if (iGroupAddressSlot >= JIP_DEVICE_MAX_GROUPS)
    {
        // No free slots
        return E_JIP_ERROR_FAILED;
    }

    // Now we join the group
    if (Network_ServerGroupJoin(&psJIP_Private->sNetworkContext, psNode, &sMulticastAddress) != E_NETWORK_OK)
    {
        return E_JIP_ERROR_FAILED;
    }

    // And add it into the groups table.
    memcpy(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sMulticastAddress, sizeof(struct in6_addr));
    
    DBG_vPrintf(DBG_JIP_SERVER, "Node added to group [%s] (slot %d)\n", 
                inet_ntop(AF_INET6, &sMulticastAddress, acAddr, INET6_ADDRSTRLEN), iGroupAddressSlot);

    return E_JIP_OK;
}


teJIP_Status eJIPserver_NodeGroupLeave(tsNode *psNode, const char *pcMulticastAddress)
{
    tsNode_Private *psNode_Private = (tsNode_Private *)psNode->pvPriv;
    tsJIP_Context *psJIP_Context = psNode->psOwnerNetwork->psOwnerContext;
    PRIVATE_CONTEXT(psJIP_Context);
    
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    struct in6_addr     sBlankAddress, sMulticastAddress;
    int iGroupAddressSlot;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: %s\n", __FUNCTION__, pcMulticastAddress);
    
    memset(&sBlankAddress, 0, sizeof(struct in6_addr));
    
    if (inet_pton(AF_INET6, pcMulticastAddress, &sMulticastAddress) <= 0)
    {
        DBG_vPrintf(DBG_JIP_SERVER, "Could not determine network address from [%s]\n", pcMulticastAddress);
        return E_JIP_ERROR_FAILED;
    }
    
    /* Check if the node is in the group */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sMulticastAddress, sizeof(struct in6_addr)) == 0)
        {
            DBG_vPrintf(DBG_JIP_SERVER, "%s: Node is in group at slot index %d\n", __FUNCTION__, iGroupAddressSlot);
            break;
        }
    }

    if (iGroupAddressSlot >= JIP_DEVICE_MAX_GROUPS)
    {
        // Not in table
        DBG_vPrintf(DBG_JIP_SERVER, "%s: Node is not in group [%s]\n", __FUNCTION__, pcMulticastAddress);
        return E_JIP_ERROR_FAILED;
    }

    // Now we leave the group
    if (Network_ServerGroupLeave(&psJIP_Private->sNetworkContext, psNode, &sMulticastAddress) != E_NETWORK_OK)
    {
        return E_JIP_ERROR_FAILED;
    }

    // And remove it from the groups table.
    memcpy(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sBlankAddress, sizeof(struct in6_addr));
    
    DBG_vPrintf(DBG_JIP_SERVER, "Node removed from group [%s] (slot %d)\n", 
                inet_ntop(AF_INET6, &sMulticastAddress, acAddr, INET6_ADDRSTRLEN), iGroupAddressSlot);

    return E_JIP_OK;
}


teJIP_Status eJIPserver_NodeGroupMembership(tsNode *psNode, uint32_t *pu32NumGroups, struct in6_addr **pasGroupAddresses)
{
    tsNode_Private *psNode_Private = (tsNode_Private *)psNode->pvPriv;
    int iGroupAddressSlot;
    struct in6_addr sBlankAddress;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    memset(&sBlankAddress, 0, sizeof(struct in6_addr));
    
    *pasGroupAddresses = malloc(sizeof(struct in6_addr) * JIP_DEVICE_MAX_GROUPS);
    if (!*pasGroupAddresses)
    {
        return E_JIP_ERROR_NO_MEM;
    }
    
    *pu32NumGroups = 0;

     /* Iterate over all groups */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sBlankAddress, sizeof(struct in6_addr)))
        {
            *pasGroupAddresses[*pu32NumGroups] = psNode_Private->asGroupAddresses[iGroupAddressSlot];
            (*pu32NumGroups)++;
        }
    }
    return E_JIP_OK;
}



