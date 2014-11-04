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

#include <net/if.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_GROUPS 0


teJIP_Status eGroups_Init(tsNode *psNode)
{
    tsMib *psMib;
    tsVar *psVar;
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    psMib = psJIP_LookupMibId(psNode, NULL, 0xffffff02);
    if (psMib)
    {
        psVar = psJIP_LookupVarIndex(psMib, 0);
         
        if (psVar)
        {
            tsTable *psGroupsTable = malloc(sizeof(tsTable));
            if (!psGroupsTable)
            {
                return E_JIP_ERROR_NO_MEM;
            }
            memset(psGroupsTable, 0, sizeof(tsTable));
            
            // Set variables data pointer
            psVar->ptData = psGroupsTable;
            
            // Get callback to populate table rows with current group membership.
            psVar->prCbVarGet = eGroups_GroupsGet;

            // Enable variable
            psVar->eEnable = E_JIP_VAR_ENABLED;
        }
        
        // Add group variable
        psVar = psJIP_LookupVarIndex(psMib, 1); 
        if (psVar)
        {
            // Set callback
            psVar->prCbVarSet = eGroups_GroupAddSet;
            
            // Initial data before the first set
            psVar->pvData = malloc(sizeof(uint8_t));
            if (!psVar->pvData)
            {
                return E_JIP_ERROR_NO_MEM;
            }
            memset(psVar->pvData, 0, sizeof(uint8_t));
            psVar->u8Size = sizeof(uint8_t);
            
            // Enable variable
            psVar->eEnable = E_JIP_VAR_ENABLED;
        }
        
        // Remove group variable
        psVar = psJIP_LookupVarIndex(psMib, 2);
        if (psVar)
        {
            // Set callback
            psVar->prCbVarSet = eGroups_GroupRemoveSet;
            
            // Initial data before the first set
            psVar->pvData = malloc(sizeof(uint8_t));
            if (!psVar->pvData)
            {
                return E_JIP_ERROR_NO_MEM;
            }
            memset(psVar->pvData, 0, sizeof(uint8_t));
            psVar->u8Size = sizeof(uint8_t);

            // Enable variable
            psVar->eEnable = E_JIP_VAR_ENABLED;
        }
        
        // Clear group variable
        psVar = psJIP_LookupVarIndex(psMib, 3);
        if (psVar)
        {
            // Set callback
            psVar->prCbVarSet = eGroups_GroupClearSet;
            
            // Initial data before the first set
            psVar->pvData = malloc(sizeof(uint8_t));
            if (!psVar->pvData)
            {
                return E_JIP_ERROR_NO_MEM;
            }
            memset(psVar->pvData, 0, sizeof(uint8_t));
            psVar->u8Size = sizeof(uint8_t);

            // Enable variable
            psVar->eEnable = E_JIP_VAR_ENABLED;
        }
    }
    
    return E_JIP_OK;
}


static int iGroups_in6_to_compressed(struct in6_addr *psAddress, uint8_t *pau8Buffer)
{
    int iInPosition, iOutPosition;
    struct in6_addr sBlankAddress;
    memset(&sBlankAddress, 0, sizeof(struct in6_addr));
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s ", __FUNCTION__);
    DBG_vPrintf_IPv6Address(DBG_FUNCTION_CALLS, *psAddress);
    
    if (memcmp(psAddress, &sBlankAddress, sizeof(struct in6_addr)) == 0)
    {
        // Blank address
        return 0;
    }
    
    iInPosition     = 1; /* Start at the scope byte */
    iOutPosition    = 0; /* Start at the start of the buffer */
    
    pau8Buffer[iOutPosition] = psAddress->s6_addr[iInPosition];
    
    for(iOutPosition = 1, iInPosition = 2; 
        ((psAddress->s6_addr[iInPosition] == 0) && (iInPosition < 16)); 
        iInPosition++);
        
    for(; iInPosition < 16; iOutPosition++, iInPosition++)
    {
        pau8Buffer[iOutPosition] = psAddress->s6_addr[iInPosition];
    }
    
    return iOutPosition;
}


void vJIPserver_GroupMibCompressedAddressToIn6(struct in6_addr *psAddress, uint8_t *pau8Buffer, uint8_t u8BufferLength)
{
    int i;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s ", __FUNCTION__);
    
    for (i = 0; i < u8BufferLength; i++)
    {
        DBG_vPrintf(DBG_FUNCTION_CALLS, "0x%02x ", pau8Buffer[i]);
    }
    DBG_vPrintf(DBG_FUNCTION_CALLS, "\n");
    
    memset(psAddress, 0, sizeof(struct in6_addr));
    psAddress->s6_addr[0] = 0xFF;
    psAddress->s6_addr[1] = pau8Buffer[0];
    
    for (i = 15; u8BufferLength > 1; u8BufferLength--, i--)
    {
        psAddress->s6_addr[i] = pau8Buffer[u8BufferLength-1];
    }
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s Result: ", __FUNCTION__);
    DBG_vPrintf_IPv6Address(DBG_FUNCTION_CALLS, *psAddress);

    return;
}


/* This function updates the groups table variable with the current list of groups that the node is in. 
 * This is done each time the table is requested, to maintain consistency with the actual membership.
 */
teJIP_Status eGroups_GroupsGet(tsVar *psVar)
{
    tsMib *psMib = psVar->psOwnerMib;
    tsNode *psNode = psMib->psOwnerNode;
    tsNode_Private *psNode_Private = (tsNode_Private *)psNode->pvPriv;
    int iGroupAddressSlot;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
     /* Check if the node is in the group */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        uint8_t au8RowData[16];
        int iRowLength;
        
        // Compress the multicast address
        iRowLength = iGroups_in6_to_compressed(&psNode_Private->asGroupAddresses[iGroupAddressSlot], au8RowData);

        // And store it into the table.
        eJIP_Table_UpdateRow(psVar, iGroupAddressSlot, au8RowData, iRowLength);
    }

    return E_JIP_OK;
}


teJIP_Status eGroups_GroupAddSet(tsVar *psVar, tsJIPAddress *psDstAddress)
{
    struct in6_addr sAddress;
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (psVar->pvData)
    {
        vJIPserver_GroupMibCompressedAddressToIn6(&sAddress, psVar->pvData, psVar->u8Size);
        inet_ntop(AF_INET6, &sAddress, acAddr, INET6_ADDRSTRLEN);
        
        return eJIPserver_NodeGroupJoin(psVar->psOwnerMib->psOwnerNode, acAddr);
    }
    return E_JIP_ERROR_BAD_BUFFER_SIZE;
}


teJIP_Status eGroups_GroupRemoveSet(tsVar *psVar, tsJIPAddress *psDstAddress)
{
    struct in6_addr sAddress;
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (psVar->pvData)
    {
        vJIPserver_GroupMibCompressedAddressToIn6(&sAddress, psVar->pvData, psVar->u8Size);
        inet_ntop(AF_INET6, &sAddress, acAddr, INET6_ADDRSTRLEN);
        
        return eJIPserver_NodeGroupLeave(psVar->psOwnerMib->psOwnerNode, acAddr);
    }
    return E_JIP_ERROR_BAD_BUFFER_SIZE;
}


teJIP_Status eGroups_GroupClearSet(tsVar *psVar, tsJIPAddress *psDstAddress)
{
    tsMib *psMib = psVar->psOwnerMib;
    tsNode *psNode = psMib->psOwnerNode;
    tsNode_Private *psNode_Private = (tsNode_Private *)psNode->pvPriv;
    int iGroupAddressSlot;
    struct in6_addr sBlankAddress;
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    
    memset(&sBlankAddress, 0, sizeof(struct in6_addr));

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
     /* Iterate over all groups */
    for (iGroupAddressSlot = 0; 
         iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
         iGroupAddressSlot++)
    {
        
        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &sBlankAddress, sizeof(struct in6_addr)))
        {
            DBG_vPrintf(DBG_GROUPS, "%s: Leave group ", __FUNCTION__);
            DBG_vPrintf_IPv6Address(DBG_GROUPS, psNode_Private->asGroupAddresses[iGroupAddressSlot]);
            
            inet_ntop(AF_INET6, &psNode_Private->asGroupAddresses[iGroupAddressSlot], acAddr, INET6_ADDRSTRLEN);
            eJIPserver_NodeGroupLeave(psNode, acAddr);
        }
    }
    return E_JIP_OK;
}


