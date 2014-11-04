/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          DiscoverNetwork.c
 *
 * REVISION:           $Revision: 56335 $
 *
 * DATED:              $Date: 2013-08-29 16:15:27 +0100 (Thu, 29 Aug 2013) $
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
#include <endian.h>


#include <JIP.h>
#include <JIP_Packets.h>
#include <JIP_Private.h>
#include <Cache.h>
#include <Network.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_DISCOVERY 0

#define DISCOVER_NETWORK_CHILD_TABLE

#define QUERY_MAX_ATTEMPTS 5


static teJIP_Status eGet_Node_Mibs(tsJIP_Private *psJIP_Private, tsNode *psNode)
{
    tsJIP_Msg_QueryMibResponseHeader *QueryMibResponseHeader;
    tsJIP_Msg_QueryMibResponseListEntryHeader *QueryMibResponseListEntryHeader;
    uint8_t u8StartMib = 0;
    uint8_t u8NumMibsOutstanding = 0;
    const uint8_t u8NumMibs = 4;
    uint8_t u8Attempts = 0;
                        
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);  

    do
    {
        char buffer[255];
        tsJIP_Msg_QueryMibRequest *psJIP_Msg_QueryMibRequest = (tsJIP_Msg_QueryMibRequest *)buffer;
        uint32_t u32ResponseLen = 255;
        
        memset(buffer, 0, 255);
        psJIP_Msg_QueryMibRequest->u8MibStartIndex  = u8StartMib;
        psJIP_Msg_QueryMibRequest->u8NumMibs        = u8NumMibs;
        
        
        DBG_vPrintf(DBG_DISCOVERY, "%s: Requesting Mibs starting %d, max %d\n", __FUNCTION__, u8StartMib, u8NumMibs);
        
        if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psNode, 3, EXCHANGE_FLAG_STAY_AWAKE,
                                E_JIP_COMMAND_QUERY_MIB_REQUEST, buffer, sizeof(tsJIP_Msg_QueryMibRequest), 
                                E_JIP_COMMAND_QUERY_MIB_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
        {
            DBG_vPrintf(DBG_DISCOVERY, "Error communicating with node\n");
            return E_JIP_ERROR_FAILED;
        }
        
        QueryMibResponseHeader = (tsJIP_Msg_QueryMibResponseHeader *)&buffer[0];
        
        if (QueryMibResponseHeader->eStatus != E_JIP_OK)
        {
            if (++u8Attempts < QUERY_MAX_ATTEMPTS)
            {
                /* Try again until we get a successful response */
                continue;
            }
            // Or fail the discovery
            return E_JIP_ERROR_FAILED;
        }
        u8Attempts = 0;

        {
            uint8_t i, j;
            uint8_t u8NumMibsReturned = QueryMibResponseHeader->u8NumMibsReturned;
            
            u8NumMibsOutstanding = QueryMibResponseHeader->u8NumMibsOutstanding;
            u8StartMib += u8NumMibsReturned;
            
            DBG_vPrintf(DBG_DISCOVERY, "%s: %d Mibs returned, %d outstanding\n", __FUNCTION__, u8NumMibsReturned, u8NumMibsOutstanding);
            j = 6;
            for (i = 0; i < u8NumMibsReturned; i++)
            {
                QueryMibResponseListEntryHeader = (tsJIP_Msg_QueryMibResponseListEntryHeader *)&buffer[j];
                char namebuf[QueryMibResponseListEntryHeader->u8NameLen + 1];
                uint32_t u32MibID = ntohl(QueryMibResponseListEntryHeader->u32MibID);
                
                memcpy(namebuf, QueryMibResponseListEntryHeader->acName, QueryMibResponseListEntryHeader->u8NameLen);
                namebuf[QueryMibResponseListEntryHeader->u8NameLen] = '\0';
                DBG_vPrintf(DBG_DISCOVERY, "Mib Id 0x%08x, Index %d, Name: %s\n", u32MibID, QueryMibResponseListEntryHeader->u8MibIndex, namebuf);
                
                if (!psJIP_NodeAddMib(psNode, u32MibID, QueryMibResponseListEntryHeader->u8MibIndex, namebuf))
                {
                    return E_JIP_ERROR_FAILED;
                }

                j += 2 + 4 + QueryMibResponseListEntryHeader->u8NameLen;
            }
            
        }
    } while (u8NumMibsOutstanding > 0);
    return E_JIP_OK;
}


static teJIP_Status eGet_Node_Mib_Variable_Descriptions(tsJIP_Private *psJIP_Private, tsNode *psNode, tsMib *psMib)
{
    tsJIP_Msg_QueryVarResponseHeader *QueryVarResponseHeader;
    tsJIP_Msg_QueryVarResponseListEntryHeader *QueryVarResponseListEntryHeader;
    uint8_t u8StartVar = 0, u8NumVarsOutstanding = 0;
    uint8_t u8Attempts = 0;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);   
    
    do
    {
        char buffer[255];
        tsJIP_Msg_QueryVarRequest *psJIP_Msg_QueryVarRequest = (tsJIP_Msg_QueryVarRequest *)buffer;
        uint32_t u32ResponseLen = 255;
        
        psJIP_Msg_QueryVarRequest->u8MibIndex       = psMib->u8Index;
        psJIP_Msg_QueryVarRequest->u8VarStartIndex  = u8StartVar;
        psJIP_Msg_QueryVarRequest->u8NumVars        = 4;

        DBG_vPrintf(DBG_DISCOVERY, "Get variables starting index %d\n", u8StartVar);
               
        if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psNode, 3, EXCHANGE_FLAG_STAY_AWAKE,
                                E_JIP_COMMAND_QUERY_VAR_REQUEST, buffer, sizeof(tsJIP_Msg_QueryVarRequest), 
                                E_JIP_COMMAND_QUERY_VAR_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
        {
            DBG_vPrintf(DBG_DISCOVERY, "Error\n");
            return E_JIP_ERROR_FAILED;
        }
        
        QueryVarResponseHeader = (tsJIP_Msg_QueryVarResponseHeader *)&buffer[0];

        if (QueryVarResponseHeader->eStatus != E_JIP_OK)
        {
            if (++u8Attempts < QUERY_MAX_ATTEMPTS)
            {
                /* Try again until we get a successful response */
                continue;
            }
            // Or fail the discovery
            return E_JIP_ERROR_FAILED;
        }
        u8Attempts = 0;
        
        {
            uint8_t u8MibIndex = QueryVarResponseHeader->u8MibIndex;
            uint8_t u8NumVarsReturned = QueryVarResponseHeader->u8NumVarsReturned;
            uint8_t i, j;
            u8NumVarsOutstanding = QueryVarResponseHeader->u8NumVarsOutstanding;
            
            u8StartVar += u8NumVarsReturned;
            
            DBG_vPrintf(DBG_DISCOVERY, "%s: Mib %d: %d Vars returned, %d outstanding\n", __FUNCTION__, u8MibIndex, u8NumVarsReturned, u8NumVarsOutstanding);
            j = 7;
            for (i = 0; i < u8NumVarsReturned; i++)
            {
                QueryVarResponseListEntryHeader = (tsJIP_Msg_QueryVarResponseListEntryHeader *)&buffer[j];
                char namebuf[QueryVarResponseListEntryHeader->u8NameLen + 1];
                memcpy(namebuf, QueryVarResponseListEntryHeader->acName, QueryVarResponseListEntryHeader->u8NameLen);
                namebuf[QueryVarResponseListEntryHeader->u8NameLen] = '\0';
                DBG_vPrintf(DBG_DISCOVERY, "Var Index %d, Name: %s\n", QueryVarResponseListEntryHeader->u8VarIndex, namebuf);

                j += 2 + QueryVarResponseListEntryHeader->u8NameLen;
                
                tsJIP_Msg_QueryVarResponseListEntryFooter *QueryVarResponseListEntryFooter;
                
                QueryVarResponseListEntryFooter = (tsJIP_Msg_QueryVarResponseListEntryFooter *)&buffer[j];
                
                if (!psJIP_MibAddVar(psMib, QueryVarResponseListEntryHeader->u8VarIndex, 
                                     namebuf, 
                                     QueryVarResponseListEntryFooter->eVarType, 
                                     QueryVarResponseListEntryFooter->eAccessType, 
                                     QueryVarResponseListEntryFooter->eSecurity))
                {
                    return E_JIP_ERROR_FAILED;
                }
                
                j += 3;
            }
            
        }
    } while (u8NumVarsOutstanding > 0);
 
    return E_JIP_OK;
}


teJIP_Status eJIP_DiscoverNode(tsJIP_Context *psJIP_Context, tsNode* psNode)
{
    PRIVATE_CONTEXT(psJIP_Context);
    tsMib *psMib;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    if (eGet_Node_Mibs(psJIP_Private, psNode) != E_JIP_OK)
    {
        DBG_vPrintf(DBG_DISCOVERY, "Failed to read mibs from node\n");
        return E_JIP_ERROR_FAILED;
    }
 
    psMib = psNode->psMibs;
    while (psMib)
    {
        /* Attempt to populate the Mib from the cache */
        if (Cache_Populate_Mib(&psJIP_Private->sCache, psMib) != E_JIP_OK)
        {
            DBG_vPrintf(DBG_DISCOVERY, "Failed to populate mib from cache, falling back to query\n");
            if (eGet_Node_Mib_Variable_Descriptions(psJIP_Private, psNode, psMib) != E_JIP_OK)
            {
                DBG_vPrintf(DBG_DISCOVERY, "Failed to query variables from mib\n");
                return E_JIP_ERROR_FAILED;
            }
            /* Add this new Mib to the Mib cache */
            (void)Cache_Add_Mib(&psJIP_Private->sCache, psMib);
        }

        psMib = psMib->psNext;
    }
    
    /* Add this new node to the device cache */
    (void)Cache_Add_Node(&psJIP_Private->sCache, psNode);

    return E_JIP_OK;
}


/* Discover network based on child table. psNode is the coordinator node, locked to this thread */
static teJIP_Status eJIPService_DiscoverNetworkChildTable(tsJIP_Context *psJIP_Context, tsNode* psNode)
{
    tsMib *psMib;
    tsVar *psVar;
    PRIVATE_CONTEXT(psJIP_Context);
    teJIP_Status eStatus = E_JIP_ERROR_FAILED;
    
    struct sNetworkTableRow
    {
        uint64_t u64IfaceID;
        uint32_t u32DeviceId;
    };
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    DBG_vPrintf(DBG_DISCOVERY, "Reading Child information: \n");

    DBG_vPrintf(DBG_DISCOVERY, "  Reading Node: ");
    DBG_vPrintf_IPv6Address(DBG_DISCOVERY, psNode->sNode_Address.sin6_addr);
    
    psMib = psJIP_LookupMib(psNode, NULL, "JenNet");
    
    if (psMib)
    {
        psVar = psJIP_LookupVar(psMib, NULL, "NetworkTable");
        
        if (psVar)
        {
            /* Temporary storage for copy of the current address list */
            tsJIPAddress   *NodeAddressList = NULL;
            uint32_t        u32NumNodes = 0;
            
            if (eJIP_GetNodeAddressList(psJIP_Context, JIP_DEVICEID_ALL, &NodeAddressList, &u32NumNodes) != E_JIP_OK)
            {
                DBG_vPrintf(DBG_DISCOVERY, "Error reading current node list\n");
                return E_JIP_ERROR_FAILED;
            }
            DBG_vPrintf(DBG_DISCOVERY, "Currently %d nodes in the network\n", u32NumNodes);

            DBG_vPrintf(DBG_DISCOVERY, "      Reading Var: %s\n", psVar->pcName);
            if (eJIP_GetTableVar(psJIP_Context, psVar) == E_JIP_OK)
            {
                tsJIPAddress sJIPAddress;
                uint64_t u64ChildAddress;
                uint32_t u32DeviceId;
                tsTableRow *psTableRow;
                int i;
                
                if (psVar->pvData != NULL)
                {
                    /* Nodes in routing table */
                    DBG_vPrintf(DBG_DISCOVERY, "Network Table now has %d entries\n", psVar->ptData->u32NumRows);
                    
                    eStatus = E_JIP_OK;
                    
                    for (i = 0; i < psVar->ptData->u32NumRows; i++)
                    {
                        psTableRow = &psVar->ptData->psRows[i];
                        if (psTableRow->pvData)
                        {
                            struct sNetworkTableRow *psRow = psTableRow->pvData;
                            
                            DBG_vPrintf(DBG_DISCOVERY, "Network table index %d\n", i);    
                            
                            u64ChildAddress = be64toh(psRow->u64IfaceID);
                            u32DeviceId = ntohl(psRow->u32DeviceId);

                            DBG_vPrintf(DBG_DISCOVERY, "Got child Iface ID: 0x%llx, device id 0x%08x\n", u64ChildAddress, u32DeviceId);

                            sJIPAddress = Network_MAC_to_IPv6(&psJIP_Private->sNetworkContext, u64ChildAddress);

                            DBG_vPrintf(DBG_DISCOVERY, "Child address: ");
                            DBG_vPrintf_IPv6Address(DBG_DISCOVERY, sJIPAddress.sin6_addr);
                            
                            psNode = psJIP_LookupNode(psJIP_Context, &sJIPAddress);
                            if (!psNode)
                            {
                                DBG_vPrintf(DBG_DISCOVERY, "Node join, Iface ID: 0x%llx\n", u64ChildAddress);
                                /* Adding node returns it with mutex locked */
                                eStatus = eJIP_NetAddNode(psJIP_Context, &sJIPAddress, u32DeviceId, &psNode);
                                if (eStatus != E_JIP_OK) 
                                {
                                    continue;
                                }
                                if (psJIP_Private->prCbNetworkChange)
                                {
                                    DBG_vPrintf(DBG_DISCOVERY, "Callback NetworkChange for node: 0x%llx\n", u64ChildAddress);
                                    
                                    psJIP_Private->prCbNetworkChange(E_JIP_NODE_JOIN, psNode);
                                }
                            }
                            eJIP_UnlockNode(psNode);
                        }
                    }
                    
                    /* Now we need to check for nodes that have left the network, using the copy we took before */
                    
                    for (i = 0; i < u32NumNodes; i++)
                    {
                        uint32_t j;
                        uint32_t RemovedNode = 1;

                        DBG_vPrintf(DBG_DISCOVERY, "  Check if existing device left: ");
                        DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);

                        /* For each row in the copy, see if it has a match in the new version */
                        for (j = 0; j < psVar->ptData->u32NumRows; j++)
                        {
                            psTableRow = &psVar->ptData->psRows[j];
                            if (psTableRow->pvData)
                            {
                                struct sNetworkTableRow *psRow = psTableRow->pvData;
                            
                                DBG_vPrintf(DBG_DISCOVERY, "   Check new network table index %d: ", j);    
                                
                                u64ChildAddress = be64toh(psRow->u64IfaceID);
                                u32DeviceId = ntohl(psRow->u32DeviceId);

                                DBG_vPrintf(DBG_DISCOVERY, "Iface ID: 0x%llx, device id 0x%08x\n", u64ChildAddress, u32DeviceId);

                                sJIPAddress = Network_MAC_to_IPv6(&psJIP_Private->sNetworkContext, u64ChildAddress);
                                
                                if (memcmp(&sJIPAddress, &NodeAddressList[i], sizeof(tsJIPAddress)) == 0)
                                {
                                    /* Node in the new table - it has not left */
                                    DBG_vPrintf(DBG_DISCOVERY, "     Node is in the new network table: %d: ", j);
                                    DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);
                                    RemovedNode = 0;
                                    break;
                                }
                            }
                            else
                            {
                                DBG_vPrintf(DBG_DISCOVERY, "   New Network table row empty: %d\n", j);
                            }
                        }
                        if (RemovedNode)
                        {
                            // Check if the node in the old device list is the border router
                            if (memcmp( &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address, 
                                        &NodeAddressList[i], sizeof(tsJIPAddress)) == 0)
                            {
                                /* Node is the border router - it has not left */
                                DBG_vPrintf(DBG_DISCOVERY, "     Node is the border router: ");
                                DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);
                                RemovedNode = 0;
                            }
                        }    
                        
                        if (RemovedNode)
                        {
                            DBG_vPrintf(DBG_DISCOVERY, "      Node left: ");
                            DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);

                            /* Remove node from network, and get locked pointer to it. */
                            if (eJIP_NetRemoveNode(psJIP_Context, &NodeAddressList[i], &psNode) != E_JIP_OK)
                            {
                                DBG_vPrintf(DBG_DISCOVERY, "      Couldn't remove node:");
                                DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);
                            }
                            else
                            {
                                if (psJIP_Private->prCbNetworkChange)
                                {
                                    DBG_vPrintf(DBG_DISCOVERY, "        Callback NetworkChange for node:");
                                    DBG_vPrintf_IPv6Address(DBG_DISCOVERY, NodeAddressList[i].sin6_addr);

                                    psJIP_Private->prCbNetworkChange(E_JIP_NODE_LEAVE, psNode);
                                }
                                
                                eJIP_NetFreeNode(psJIP_Context, psNode);
                            }
                        }
                    }
                    
                }
            }
            /* Free the copy of node list */
            free(NodeAddressList);
        }
    }
    return eStatus;
}


teJIP_Status eJIPService_DiscoverNetwork(tsJIP_Context *psJIP_Context)
{
    tsNode*     psNode;
    PRIVATE_CONTEXT(psJIP_Context);
    teJIP_Status eStatus = E_JIP_OK;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);   
    
    if (psJIP_Private->eJIP_ContextType != E_JIP_CONTEXT_CLIENT)
    {
        return E_JIP_ERROR_WRONG_CONTEXT;
    }

    psNode = psJIP_LookupNode(psJIP_Context, &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address);
    if (!psNode)
    {
        DBG_vPrintf(DBG_DISCOVERY, "Adding coordinator to network\n");  

        /* Add the node for the coordinator to the network */
        if (eJIP_NetAddNode(psJIP_Context, &psJIP_Private->sNetworkContext.sBorder_Router_IPv6_Address, 0x08010001, &psNode) != E_JIP_OK)
        {
            eJIP_Unlock(psJIP_Context);
            return E_JIP_ERROR_FAILED;
        }
    }
        
    /* Go off and discover all it's descendents */
    if (eJIPService_DiscoverNetworkChildTable(psJIP_Context, psNode) != E_JIP_OK)
    {
        eStatus = E_JIP_ERROR_FAILED;
    }

    eJIP_UnlockNode(psNode);
    
    return eStatus;
}



