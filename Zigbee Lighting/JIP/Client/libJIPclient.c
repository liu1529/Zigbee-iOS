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
#include <errno.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <Threads.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_JIP_CLIENT 0

static void *pvNetworkChangeMonitorThread(void *psThreadInfoVoid);

static teJIP_Status eJIP_SetVarFromPacket(tsVar *psVar, uint8_t *buffer);


teJIP_Status eJIP_Connect(tsJIP_Context *psJIP_Context, const char *pcAddress, const int iPort)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    eJIP_Lock(psJIP_Context);
    
    if (Network_Connect(&psJIP_Private->sNetworkContext, pcAddress, iPort) == E_NETWORK_OK)
    {
        eStatus = E_JIP_OK;
    }
    
    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


teJIP_Status eJIP_Connect4(tsJIP_Context *psJIP_Context, const char *pcIPv4Address, const char *pcIPv6Address, const int iPort, const bool_t bTCP)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    eJIP_Lock(psJIP_Context);
    
    if (Network_Connect4(&psJIP_Private->sNetworkContext, pcIPv4Address, pcIPv6Address, iPort, bTCP) == E_NETWORK_OK)
    {
        eStatus = E_JIP_OK;
    }
    
    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


teJIP_Status eJIP_GroupJoin(tsJIP_Context *psJIP_Context, const char *pcAddress)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    eJIP_Lock(psJIP_Context);
    
    if (Network_ClientGroupJoin(&psJIP_Private->sNetworkContext, pcAddress) == E_NETWORK_OK)
    {
        eStatus = E_JIP_OK;
    }
    
    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


teJIP_Status eJIP_GroupLeave(tsJIP_Context *psJIP_Context, const char *pcAddress)
{
    PRIVATE_CONTEXT(psJIP_Context);
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    eJIP_Lock(psJIP_Context);
    
    if (Network_ClientGroupLeave(&psJIP_Private->sNetworkContext, pcAddress) == E_NETWORK_OK)
    {
        eStatus = E_JIP_OK;
    }
    
    eJIP_Unlock(psJIP_Context);
    
    return eStatus;
}


teJIP_Status eJIP_MulticastSetVar(tsJIP_Context *psJIP_Context, tsVar *psVar, void *pvData, uint32_t u32Size, tsJIPAddress *psAddress, int iMaxHops)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsJIP_Msg_SetMibRequest *psSetRequest;
    tsNode *psNode;
    tsMib *psMib;
     
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);   
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    psMib = psVar->psOwnerMib;
    psNode = psMib->psOwnerNode;
    
    eJIP_LockNode(psNode, True);
    
    {
        char buffer[255];
        uint32_t u32ResponseLen = 255, u32CommandLen;
        
        psSetRequest = (tsJIP_Msg_SetMibRequest *)buffer;
        
        psSetRequest->u32MibId                  = htonl(psMib->u32MibId);
        psSetRequest->sRequest.u8VarIndex       = psVar->u8Index;
        psSetRequest->sRequest.sVar.eVarType    = psVar->eVarType;

        DBG_vPrintf(DBG_JIP_CLIENT, "Setting Mib 0x%08x, variable %d, type %d\n", 
                    psMib->u32MibId, psVar->u8Index, psVar->eVarType);
        
        u32CommandLen = sizeof(tsJIP_Msg_SetMibRequest);

        switch (psVar->eVarType)
        {
            case (E_JIP_VAR_TYPE_INT8):
            case (E_JIP_VAR_TYPE_UINT8):
                u32Size = sizeof(uint8_t);
                buffer[u32CommandLen] = *((uint8_t *)pvData);
                u32CommandLen += sizeof(uint8_t);
                break;

            case (E_JIP_VAR_TYPE_INT16):
            case (E_JIP_VAR_TYPE_UINT16):
            {
                uint16_t u16Var = htons(*((uint16_t *)pvData));
                u32Size = sizeof(uint16_t);
                memcpy(&buffer[u32CommandLen], &u16Var, sizeof(uint16_t));
                u32CommandLen += sizeof(uint16_t);
                break;
            }
                
            case (E_JIP_VAR_TYPE_INT32):
            case (E_JIP_VAR_TYPE_UINT32):
            case (E_JIP_VAR_TYPE_FLT):
            {
                uint32_t u32Var = htonl(*((uint32_t *)pvData));
                u32Size = sizeof(uint32_t);
                memcpy(&buffer[u32CommandLen], &u32Var, sizeof(uint32_t));
                u32CommandLen += sizeof(uint32_t);
                break;
            }
            
            case (E_JIP_VAR_TYPE_INT64):
            case (E_JIP_VAR_TYPE_UINT64):
            case (E_JIP_VAR_TYPE_DBL):
            {
                uint64_t u64Var = htobe64(*((uint64_t *)pvData));
                u32Size = sizeof(uint64_t);
                memcpy(&buffer[u32CommandLen], &u64Var, sizeof(uint64_t));
                u32CommandLen += sizeof(uint64_t);
                break;
            }

            case(E_JIP_VAR_TYPE_STR):
            {
                if (u32Size > 255)
                {
                    eJIP_Unlock(psJIP_Context);
                    return E_JIP_ERROR_BAD_BUFFER_SIZE;
                }
                buffer[u32CommandLen++] = u32Size;
                memcpy(&buffer[u32CommandLen], (uint8_t *)pvData, u32Size);
                u32CommandLen += u32Size;
                /* Increment size to include NULL terminator when local copy is updated */
                u32Size++;
                break;
            }
            
            case(E_JIP_VAR_TYPE_BLOB):
            {
                buffer[u32CommandLen++] = u32Size;
                memcpy(&buffer[u32CommandLen], (uint8_t *)pvData, u32Size);
                u32CommandLen += u32Size;
                break;
            }
            
            default:
                DBG_vPrintf(DBG_JIP_CLIENT, "Set not supported for this type\n");
                eJIP_UnlockNode(psNode);
                return E_JIP_ERROR_FAILED;
        }

        if (!psAddress)
        {
            teNetworkStatus eNetStatus;
            
            // If we wern't given an address, send to the node
            eNetStatus = Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psNode, 3, EXCHANGE_FLAG_NONE,
                                            E_JIP_COMMAND_SET_MIB_REQUEST, buffer, u32CommandLen, 
                                            E_JIP_COMMAND_SET_RESPONSE, buffer, &u32ResponseLen);
            if (eNetStatus!= E_NETWORK_OK)
            {
                DBG_vPrintf(DBG_JIP_CLIENT, "Error setting variable\n");
                eJIP_UnlockNode(psNode);
                
                if (eNetStatus == E_NETWORK_ERROR_TIMEOUT)
                {
                    return E_JIP_ERROR_TIMEOUT;
                }
                else if (eNetStatus == E_NETWORK_ERROR_NO_MEM)
                {
                    return E_JIP_ERROR_NO_MEM;
                }
                else
                {
                    return E_JIP_ERROR_FAILED;
                }
            }
            else
            {
                tsJIP_Msg_VarStatus *psJIP_Msg_VarStatus = (tsJIP_Msg_VarStatus *)buffer;
                if (psJIP_Msg_VarStatus->eStatus != E_JIP_OK)
                {
                    DBG_vPrintf(DBG_JIP_CLIENT, "Node reported error setting variable (%d)\n", psJIP_Msg_VarStatus->eStatus);
                    eJIP_UnlockNode(psNode);
                    return psJIP_Msg_VarStatus->eStatus;
                }
            }
  
            // Update local copy 
            {
                teJIP_Status eStatus = eJIP_SetVarValue(psVar, pvData, u32Size);
                eJIP_UnlockNode(psNode);
                return eStatus;
            }
        }
        else
        {
            int i, j;
            
            if (psJIP_Private->sNetworkContext.eProtocol == E_NETWORK_PROTO_IPV6)
            {
                // For Mcast needs to be at least 2 Hops for now - enough to go across the border router from the local network
                if (setsockopt(psJIP_Private->sNetworkContext.iSocket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &iMaxHops, sizeof(int)) < 0)
                {
                    DBG_vPrintf(DBG_JIP_CLIENT, "Error setting Number of hops\n");
                    eJIP_UnlockNode(psNode);
                    return E_JIP_ERROR_FAILED;
                }
            }
            
            if ((psJIP_Context->iMulticastInterface == -1) && (psJIP_Private->sNetworkContext.eProtocol == E_NETWORK_PROTO_IPV6))
            {
                // Send the multicast up each interface
                struct ifaddrs *ifp, *ifs;
                int iInterfaceIndex = -1;
                int iLastInterfaceIndex = -1;
                char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
                
                if (getifaddrs(&ifs) < 0) 
                {
                    DBG_vPrintf(DBG_JIP_CLIENT, "%s: getifaddrs failed (%s)\n", __FUNCTION__, strerror(errno));
                    return E_JIP_ERROR_FAILED;
                }

                for (ifp = ifs; ifp; ifp = ifp->ifa_next)
                {
                    if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_INET6)
                    {
                        DBG_vPrintf(DBG_JIP_CLIENT, "Found address [%s] on interface %s\n",
                            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)(ifp->ifa_addr))->sin6_addr, acAddr, INET6_ADDRSTRLEN), ifp->ifa_name);
                        
                        iInterfaceIndex = if_nametoindex(ifp->ifa_name);
                        
                        if ((iLastInterfaceIndex != iInterfaceIndex) && 
                            (strcmp(ifp->ifa_name, "lo")))
                        {
                            /* Only multicast on each interface once, and don't bother with the loopback interface */
                            
                            DBG_vPrintf(DBG_JIP_CLIENT, "Multicast on interface %s\n", ifp->ifa_name);
                            iLastInterfaceIndex = iInterfaceIndex;
                            
                            if (psJIP_Private->sNetworkContext.eProtocol == E_NETWORK_PROTO_IPV6)
                            {
                                if (setsockopt(psJIP_Private->sNetworkContext.iSocket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                                    &iInterfaceIndex, sizeof(int)) < 0)
                                {
                                    perror("setsockopt IPV6_MULTICAST_IF");
                                }
                            }
                            
                            for (i = 0; i < psJIP_Context->iMulticastSendCount; i++)
                            {
                                // Send multicast packet.
                                if (Network_SendJIP(&psJIP_Private->sNetworkContext, psAddress,
                                                    E_JIP_COMMAND_SET_MIB_REQUEST, buffer, u32CommandLen) != E_NETWORK_OK)
                                {
                                    // There is no response to a multicast command
                                    eJIP_UnlockNode(psNode);
                                    freeifaddrs(ifs);
                                    return E_JIP_ERROR_FAILED;
                                }
                                
                                if (psJIP_Context->iMulticastSendCount > 0)
                                {
                                    /* Retransmit delay - 200ms */
                                    usleep(200000);
                                }
                            }
                            
                        }
                    }
                }

                freeifaddrs(ifs);                
            }
            else
            {
                // Just send up the one interface.
                if (psJIP_Private->sNetworkContext.eProtocol == E_NETWORK_PROTO_IPV6)
                {
                    if (setsockopt(psJIP_Private->sNetworkContext.iSocket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                        &psJIP_Context->iMulticastInterface, sizeof(psJIP_Context->iMulticastInterface)) < 0)
                    {
                        perror("setsockopt IPV6_MULTICAST_IF");
                    }
                }
                
                for (i = 0; i < psJIP_Context->iMulticastSendCount; i++)
                {
                    // Send multicast packet.
                    if (Network_SendJIP(&psJIP_Private->sNetworkContext, psAddress,
                                        E_JIP_COMMAND_SET_MIB_REQUEST, buffer, u32CommandLen) != E_NETWORK_OK)
                    {
                        // There is no response to a multicast command
                        eJIP_UnlockNode(psNode);
                        return E_JIP_ERROR_FAILED;
                    }
                    
                    if (psJIP_Context->iMulticastSendCount > 0)
                    {
                        /* Retransmit delay - 200ms */
                        usleep(200000);
                    }
                }
            }
            eJIP_UnlockNode(psNode);
        }
    }
    return E_JIP_OK;
}


teJIP_Status eJIP_GetVar(tsJIP_Context *psJIP_Context, tsVar *psVar)
{
    PRIVATE_CONTEXT(psJIP_Context);
    char buffer[255];
    uint32_t u32ResponseLen = 255;
    tsJIP_Msg_GetMibRequest *psJIP_Msg_GetMibRequest = (tsJIP_Msg_GetMibRequest *)buffer;
    tsJIP_Msg_VarDescriptionHeader *psJIP_Msg_VarDescriptionHeader;
    tsMib *psMib = psVar->psOwnerMib;
    tsNode *psNode = psMib->psOwnerNode;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    switch (psVar->eVarType)
    {
        case(E_JIP_VAR_TYPE_TABLE_BLOB):
            return eJIP_GetTableVar(psJIP_Context, psVar);
            break;
        default:
            break;
    }
    
    eJIP_LockNode(psNode, True);
    
    psJIP_Msg_GetMibRequest->u32MibId = htonl(psVar->psOwnerMib->u32MibId);
    psJIP_Msg_GetMibRequest->sRequest.u8VarIndex = psVar->u8Index;
    psJIP_Msg_GetMibRequest->sRequest.u8VarCount = 1;
    
    DBG_vPrintf(DBG_JIP_CLIENT, "Get variable %d, MiB 0x%08x, Node:", psVar->u8Index, psVar->psOwnerMib->u32MibId);
    DBG_vPrintf_IPv6Address(DBG_JIP_CLIENT, psNode->sNode_Address.sin6_addr);

    if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psVar->psOwnerMib->psOwnerNode, 3, EXCHANGE_FLAG_NONE,
                            E_JIP_COMMAND_GET_MIB_REQUEST, buffer, sizeof(tsJIP_Msg_GetMibRequest) - 2, 
                            E_JIP_COMMAND_GET_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Error\n");
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_FAILED;
    }
    
    psJIP_Msg_VarDescriptionHeader = (tsJIP_Msg_VarDescriptionHeader *)buffer;

    if (psJIP_Msg_VarDescriptionHeader->eStatus == E_JIP_ERROR_DISABLED)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Variable is disabled\n");
        psVar->eEnable = E_JIP_VAR_DISABLED;
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_DISABLED;
    }
    else if (psJIP_Msg_VarDescriptionHeader->eStatus != E_JIP_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Error reading (status 0x%02x)\n", psJIP_Msg_VarDescriptionHeader->eStatus);
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_FAILED;
    }
    
    if (psJIP_Msg_VarDescriptionHeader->eVarType != psVar->eVarType)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Type mismatch (got %d, expected %d)\n", psJIP_Msg_VarDescriptionHeader->eVarType, psVar->eVarType);
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_FAILED;
    }
    
    // Set the variable as enabled
    psVar->eEnable = E_JIP_VAR_ENABLED;
    
    {
        teJIP_Status eStatus = eJIP_SetVarFromPacket(psVar, (uint8_t *)buffer);
        eJIP_UnlockNode(psNode);
        return eStatus;
    }
}
 

teJIP_Status eJIP_TrapEvent(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, char *pcPacket)
{
    //PRIVATE_CONTEXT(psJIP_Context);
    tsJIP_Msg_VarDescriptionHeader *psJIP_Msg_VarDescriptionHeader = (tsJIP_Msg_VarDescriptionHeader *)pcPacket;
    tsNode *psNode;
    tsMib *psMib;
    tsVar *psVar;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s Mib %d, Var %d\n", __FUNCTION__, psJIP_Msg_VarDescriptionHeader->u8MibIndex, psJIP_Msg_VarDescriptionHeader->u8VarIndex); 

    psNode = psJIP_LookupNode(psJIP_Context, psAddress);
    
    if (psNode)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Got Node\n");
         
        psMib = psNode->psMibs;
        while (psMib)
        {
            if (psMib->u8Index == psJIP_Msg_VarDescriptionHeader->u8MibIndex)
            {
                DBG_vPrintf(DBG_JIP_CLIENT, "Got Mib\n");
                 
                psVar = psMib->psVars;
                while (psVar)
                {
                    if (psVar->u8Index == psJIP_Msg_VarDescriptionHeader->u8VarIndex)
                    {
                        DBG_vPrintf(DBG_JIP_CLIENT, "Got Var\n");
                        eJIP_SetVarFromPacket(psVar, (uint8_t*)pcPacket);
                        
                        if (psVar->prCbVarTrap)
                        {
                            DBG_vPrintf(DBG_JIP_CLIENT, "Calling Var Trap Callback\n");
                            psVar->prCbVarTrap(psVar);
                        }
                        break;
                    }

                    psVar = psVar->psNext;
                }
                break;
            }
            psMib = psMib->psNext;
        }
        eJIP_UnlockNode(psNode);
    }
    return E_JIP_OK;
}


teJIP_Status eJIP_TrapVar(tsJIP_Context *psJIP_Context, tsVar *psVar, uint8_t u8NotificationHandle, tprCbVarTrap prCbVarTrap)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsJIP_Msg_TrapRequest *psJIP_Msg_TrapRequest;
    tsJIP_Msg_VarStatus   *psJIP_Msg_VarStatus;
    tsNode *psNode;
    tsMib *psMib;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);   

    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }

    psMib = psVar->psOwnerMib;
    psNode = psMib->psOwnerNode;
    
    eJIP_LockNode(psNode, True);
    
    char buffer[255];
    uint32_t u32ResponseLen = 255;
    
    psJIP_Msg_TrapRequest = (tsJIP_Msg_TrapRequest *)buffer;
    
    psJIP_Msg_TrapRequest->u8NotificationHandle = u8NotificationHandle;
    psJIP_Msg_TrapRequest->u8MibIndex = psMib->u8Index;
    psJIP_Msg_TrapRequest->u8VarIndex = psVar->u8Index;

    DBG_vPrintf(DBG_JIP_CLIENT, "Requesting trap on Mib %d, variable %d\n", psJIP_Msg_TrapRequest->u8MibIndex, psJIP_Msg_TrapRequest->u8VarIndex);

    if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psNode, 3, EXCHANGE_FLAG_NONE,
                            E_JIP_COMMAND_TRAP_REQUEST, buffer, sizeof(tsJIP_Msg_TrapRequest), 
                            E_JIP_COMMAND_TRAP_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Error requesting trap on variable\n");
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_FAILED;
    }
    
    psJIP_Msg_VarStatus = (tsJIP_Msg_VarStatus *)buffer;
    DBG_vPrintf(DBG_JIP_CLIENT, "Status: %d \n", psJIP_Msg_VarStatus->eStatus);
    
    if (psJIP_Msg_VarStatus->eStatus == E_JIP_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Trap set up ok, setting callback\n");
        psVar->u8TrapHandle = u8NotificationHandle;
        psVar->prCbVarTrap = prCbVarTrap;
        eJIP_UnlockNode(psNode);
        return E_JIP_OK;
    }
    else
    {
        eJIP_UnlockNode(psNode);
        return psJIP_Msg_VarStatus->eStatus;
    }
}


teJIP_Status eJIP_UntrapVar(tsJIP_Context *psJIP_Context, tsVar *psVar, uint8_t u8NotificationHandle)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsJIP_Msg_TrapRequest *psJIP_Msg_TrapRequest;
    tsJIP_Msg_VarStatus   *psJIP_Msg_VarStatus;
    tsNode *psNode;
    tsMib *psMib;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);   

    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }

    psMib = psVar->psOwnerMib;
    psNode = psMib->psOwnerNode;
    
    eJIP_LockNode(psNode, True);
    
    /* Remove callback function pointer */
    psVar->prCbVarTrap = NULL;
    
    char buffer[255];
    uint32_t u32ResponseLen = 255;
    
    psJIP_Msg_TrapRequest = (tsJIP_Msg_TrapRequest *)buffer;
    
    psJIP_Msg_TrapRequest->u8NotificationHandle = u8NotificationHandle;
    psJIP_Msg_TrapRequest->u8MibIndex = psMib->u8Index;
    psJIP_Msg_TrapRequest->u8VarIndex = psVar->u8Index;
    
    DBG_vPrintf(DBG_JIP_CLIENT, "Requesting removal of trap on Mib %d, variable %d\n", psJIP_Msg_TrapRequest->u8MibIndex, psJIP_Msg_TrapRequest->u8VarIndex);
    
    if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psNode, 3, EXCHANGE_FLAG_NONE,
                            E_JIP_COMMAND_UNTRAP_REQUEST, buffer, sizeof(tsJIP_Msg_TrapRequest), 
                            E_JIP_COMMAND_TRAP_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Error requesting removal of trap on variable\n");
        eJIP_UnlockNode(psNode);
        return E_JIP_ERROR_FAILED;
    }
    
    psJIP_Msg_VarStatus = (tsJIP_Msg_VarStatus *)buffer;
    DBG_vPrintf(DBG_JIP_CLIENT, "Status: %d \n", psJIP_Msg_VarStatus->eStatus);
    
    if (psJIP_Msg_VarStatus->eStatus == E_JIP_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Trap removed ok\n");
        psVar->prCbVarTrap = NULL;
        eJIP_UnlockNode(psNode);
        return E_JIP_OK;
    }
    else
    {
        eJIP_UnlockNode(psNode);
        return psJIP_Msg_VarStatus->eStatus;
    }
}


static teJIP_Status eJIP_SetVarFromPacket(tsVar *psVar, uint8_t *buffer)
{
    tsJIP_Msg_VarDescriptionHeader *psVarDescriptionHeader;
    void *pvNewData;
    
    psVarDescriptionHeader = (tsJIP_Msg_VarDescriptionHeader *)buffer;
 
    DBG_vPrintf(DBG_JIP_CLIENT, "Set var type %d\n", psVar->eVarType);
    
    switch (psVar->eVarType)
    {
        case(E_JIP_VAR_TYPE_INT8):
        case(E_JIP_VAR_TYPE_UINT8):
        {
            tsJIP_Msg_VarDescription_Int8 *VarDescription_Int8 = (tsJIP_Msg_VarDescription_Int8 *)buffer;
            return eJIP_SetVarValue(psVar, &VarDescription_Int8->u8Val, sizeof(int8_t));
        }
        
        case(E_JIP_VAR_TYPE_INT16):
        case(E_JIP_VAR_TYPE_UINT16):
        {
            tsJIP_Msg_VarDescription_Int16 *VarDescription_Int16 = (tsJIP_Msg_VarDescription_Int16 *)buffer;
            VarDescription_Int16->u16Val = ntohs(VarDescription_Int16->u16Val);
            return eJIP_SetVarValue(psVar, &VarDescription_Int16->u16Val, sizeof(int16_t));
        }
        
        case (E_JIP_VAR_TYPE_INT32):
        case (E_JIP_VAR_TYPE_UINT32):
        {
            tsJIP_Msg_VarDescription_Int32 *VarDescription_Int32 = (tsJIP_Msg_VarDescription_Int32 *)buffer;
            VarDescription_Int32->u32Val = ntohl(VarDescription_Int32->u32Val);
            return eJIP_SetVarValue(psVar, &VarDescription_Int32->u32Val, sizeof(int32_t));
        }

        case (E_JIP_VAR_TYPE_INT64):
        case (E_JIP_VAR_TYPE_UINT64):
        {
            tsJIP_Msg_VarDescription_Int64 *VarDescription_Int64 = (tsJIP_Msg_VarDescription_Int64 *)buffer;
            VarDescription_Int64->u64Val = be64toh(VarDescription_Int64->u64Val);
            return eJIP_SetVarValue(psVar, &VarDescription_Int64->u64Val, sizeof(int64_t));
        }
        
        case (E_JIP_VAR_TYPE_FLT):
        {
            tsJIP_Msg_VarDescription_Int32 *VarDescription_Int32 = (tsJIP_Msg_VarDescription_Int32 *)buffer;
            VarDescription_Int32->u32Val = ntohl(VarDescription_Int32->u32Val);
            return eJIP_SetVarValue(psVar, &VarDescription_Int32->u32Val, sizeof(int32_t));
        }
        case (E_JIP_VAR_TYPE_DBL):
        {
            tsJIP_Msg_VarDescription_Int64 *VarDescription_Int64 = (tsJIP_Msg_VarDescription_Int64 *)buffer;
            VarDescription_Int64->u64Val = be64toh(VarDescription_Int64->u64Val);
            return eJIP_SetVarValue(psVar, &VarDescription_Int64->u64Val, sizeof(int64_t));
        }
        
        case (E_JIP_VAR_TYPE_STR):
        {
            /* Special case for string due to incoming packet missing the NULL terminator */
            tsJIP_Msg_VarDescription_Str *VarDescription_Str = (tsJIP_Msg_VarDescription_Str *)buffer;
            pvNewData = realloc(psVar->pcData, VarDescription_Str->u8StringLen + 1);
            if (!pvNewData) 
            {
                return E_JIP_ERROR_NO_MEM;
            }
            psVar->pcData = pvNewData;
            memcpy(psVar->pcData, VarDescription_Str->acString, VarDescription_Str->u8StringLen);
            psVar->pcData[VarDescription_Str->u8StringLen] = '\0';
            psVar->u8Size = VarDescription_Str->u8StringLen + 1;
            break;
        }
        
        case (E_JIP_VAR_TYPE_BLOB):
        {
            tsJIP_Msg_VarDescription_Blob *VarDescription_Blob = (tsJIP_Msg_VarDescription_Blob *)buffer;
            return eJIP_SetVarValue(psVar, VarDescription_Blob->au8Blob, VarDescription_Blob->u8Len);
        }
        default:
            DBG_vPrintf(DBG_JIP_CLIENT, "WARNING Unknown variable type (%d)\n", psVarDescriptionHeader->eVarType);
    }
    return E_JIP_OK;
}
    

teJIP_Status eJIPService_MonitorNetwork(tsJIP_Context *psJIP_Context, tprCbNetworkChange prCbNetworkChange)
{
    PRIVATE_CONTEXT(psJIP_Context);
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);  
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    eJIP_Lock(psJIP_Context);
    
    /* Set up callback function */
    psJIP_Private->prCbNetworkChange = prCbNetworkChange;
    
    /* Thread data is the JIP context pointer */
    psJIP_Private->sNetworkChangeMonitor.pvThreadData = psJIP_Context;
    if (eThreadStart(pvNetworkChangeMonitorThread, &psJIP_Private->sNetworkChangeMonitor, E_THREAD_JOINABLE) != E_THREAD_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Failed to start network monitor thread\n");
        eJIP_Unlock(psJIP_Context);
        return E_NETWORK_ERROR_FAILED;
    }
    
    eJIP_Unlock(psJIP_Context);
    return E_JIP_OK;
}


teJIP_Status eJIPService_MonitorNetworkStop(tsJIP_Context *psJIP_Context)
{
    PRIVATE_CONTEXT(psJIP_Context);
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);  
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }
    
    if ((volatile void *)psJIP_Private->sNetworkChangeMonitor.pvThreadData)
    {
        /* Monitor thread is running */
        DBG_vPrintf(DBG_JIP_CLIENT, "Stopping network monitor thread\n");

        if (eThreadStop(&psJIP_Private->sNetworkChangeMonitor) != E_THREAD_OK)
        {
            DBG_vPrintf(DBG_JIP_CLIENT, "Failed to stop network monitor thread\n");
            return E_JIP_ERROR_FAILED;
        }
        eJIP_Lock(psJIP_Context);
        psJIP_Private->prCbNetworkChange = NULL;
        psJIP_Private->sNetworkChangeMonitor.pvThreadData = NULL;
        eJIP_Unlock(psJIP_Context);
    }
    
    return E_JIP_OK;
}


static void NetworkChangeTreeVersionTrap(tsVar *psVersionVar)
{
    tsJIP_Context *psJIP_Context;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    /* Get pointer to the JIP context from the Variable */
    psJIP_Context = psVersionVar->psOwnerMib->psOwnerNode->psOwnerNetwork->psOwnerContext;

    DBG_vPrintf(DBG_JIP_CLIENT, "TreeVersion Trap - Calling DiscoverNetwork()\n");
    
    if (eJIPService_DiscoverNetwork(psJIP_Context) != E_JIP_OK)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Error discovering network\n");
    }
    
    return;
}


static void *pvNetworkChangeMonitorThread(void *psThreadInfoVoid)
{
    tsThread *psThreadInfo = (tsThread *)psThreadInfoVoid;
    tsJIP_Context *psJIP_Context = (tsJIP_Context *)psThreadInfo->pvThreadData;
    PRIVATE_CONTEXT(psJIP_Context);
    uint32_t u32TrapRegistered = 0;
    tsNode *psNode = NULL;
    tsMib *psMib = NULL;
    tsVar *psVar = NULL;
    const uint8_t u8TreeVersionTrapHandle = 0x12;
    
    psThreadInfo->eState = E_THREAD_RUNNING;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    /* Ensure that at least the coordinator exists in the network representation before starting thread to monitor it */
    if (psJIP_Context->sNetwork.u32NumNodes == 0)
    {
        /* Run first discovery before starting to monitor */
        DBG_vPrintf(DBG_JIP_CLIENT, "Running discovery before registering trap\n");
        if (eJIPService_DiscoverNetwork(psJIP_Context) != E_JIP_OK)
        {
            DBG_vPrintf(DBG_JIP_CLIENT, "Error discovering network\n");
        }
    }
    
    DBG_vPrintf(DBG_JIP_CLIENT, "Register trap on TreeVersion");

    psNode = psJIP_LookupNode(psJIP_Context, &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address);
    if (psNode)
    {
        psMib = psJIP_LookupMib(psNode, NULL, "JenNet");
        
        if (psMib)
        {
            psVar = psJIP_LookupVar(psMib, NULL, "TreeVersion");
            
            if (psVar)
            {
                if (eJIP_TrapVar(psJIP_Context, psVar, u8TreeVersionTrapHandle, NetworkChangeTreeVersionTrap) == E_JIP_OK)
                {
                    DBG_vPrintf(DBG_JIP_CLIENT, "Trap registered\n");
                    u32TrapRegistered = 1;
                }
            }
        }
        eJIP_UnlockNode(psNode);
    }

    if (!u32TrapRegistered)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Trap registration failed, falling back to polling\n");
    }
    
    while (psThreadInfo->eState == E_THREAD_RUNNING)
    {
        /* This thread can now spin here and rediscover every 60s to catch any changes
         * that haven't been picked up by the trap
         */
#define DISCOVERY_TIME (60)
        if (eJIPService_DiscoverNetwork(psJIP_Context) != E_JIP_OK)
        {
            DBG_vPrintf(DBG_JIP_CLIENT, "Error discovering network\n");
        }
        sleep(DISCOVERY_TIME);
#undef DISCOVERY_TIME
    }
    
    if (u32TrapRegistered)
    {
        DBG_vPrintf(DBG_JIP_CLIENT, "Untrapping tree version\n");
        
        if (eJIP_UntrapVar(psJIP_Context, psVar, u8TreeVersionTrapHandle) != E_JIP_OK)
        {
            DBG_vPrintf(DBG_JIP_CLIENT, "Error untrapping tree version\n");
        }
    }
    
    eThreadFinish(psThreadInfo);
    
    return NULL;
}

