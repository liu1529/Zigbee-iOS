/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Tables.c
 *
 * REVISION:           $Revision: 54637 $
 *
 * DATED:              $Date: 2013-06-12 15:24:05 +0100 (Wed, 12 Jun 2013) $
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
#include <Network.h>
#include <Trace.h>

#define DBG_FUNCTION_CALLS 0
#define DBG_TABLES 0


static teJIP_Status JIP_Table_Check_Storage(tsVar *psVar, uint32_t u32LastRow)
{
    tsTable *psTable;
    uint32_t u32NumRows = u32LastRow + 1;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    if (!psVar->pvData)
    {
        DBG_vPrintf(DBG_TABLES, "Allocating table storage for %d rows\n", u32NumRows);
        psVar->pvData = malloc(sizeof(tsTable));
        if (!psVar->pvData)
        {
            DBG_vPrintf(DBG_TABLES, "Could not allocate table storage\n");
            return E_JIP_ERROR_NO_MEM;
        }
        psTable = (tsTable *)psVar->pvData;
        psTable->u32NumRows = u32NumRows;
        /* Now allocate stirage for the number of rows */
        psTable->psRows = malloc(sizeof(tsTableRow) * u32NumRows);
        if (!psTable->psRows)
        {
            DBG_vPrintf(DBG_TABLES, "Could not allocate table rows storage\n");
            free(psVar->pvData);
            psVar->pvData = NULL;
            return E_JIP_ERROR_NO_MEM;
        }
        memset(psTable->psRows, 0, sizeof(tsTableRow) * u32NumRows);
    }
    else
    {
        /* already have storage, make sure that the rows are big enough */
        psTable = (tsTable *)psVar->pvData;
        if (u32NumRows > psTable->u32NumRows)
        {
            tsTableRow *psNewRows;
            DBG_vPrintf(DBG_TABLES, "Allocating extra storage for %d rows (old %d)\n", u32NumRows, psTable->u32NumRows);
            psNewRows = realloc(psTable->psRows, sizeof(tsTableRow) * u32NumRows);
            if (!psNewRows)
            {
                DBG_vPrintf(DBG_TABLES, "Could not allocate new table rows storage\n");
                return E_JIP_ERROR_NO_MEM;
            }
            psTable->psRows = psNewRows;
            /* Set the new storage to 0 */
            memset(&psTable->psRows[psTable->u32NumRows], 0, sizeof(tsTableRow) *(u32NumRows - psTable->u32NumRows));
            psTable->u32NumRows = u32NumRows;
        }
        else
        {
            DBG_vPrintf(DBG_TABLES, "Allocated table memory is good for %d rows\n", u32NumRows);
            /** \todo Do we need to shrink the allocated rows vector when the table size is reduced? */
        }
    }
   
    return E_JIP_OK;
}


teJIP_Status eJIP_Table_UpdateRow(tsVar *psVar, uint32_t u32Index, void *pvData, uint32_t u32Length)
{
    teJIP_Status eStatus;
    
    tsTable *psTable;
    tsTableRow *psTableRow;
    void *pvNewData;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    if ((eStatus = JIP_Table_Check_Storage(psVar, u32Index)) != E_JIP_OK)
    {
        return eStatus;
    }
    
    psTable = (tsTable *)psVar->pvData;
    psTableRow = &psTable->psRows[u32Index];
    
    if (u32Length == 0)
    {
        /* New length is 0 - free the old data and set the pointer to NULL */
        free(psTableRow->pvData);
        psTableRow->pvData = NULL;
        psTableRow->u32Length = 0;
        DBG_vPrintf(DBG_TABLES, "Table entry %d : Emptied\n", u32Index);
    }
    else
    {
        /* Reallocate the storage for the new data or allocate it if it was previously NULL */
        pvNewData = realloc(psTableRow->pvData, u32Length);
        if (!pvNewData)
        {
            return E_JIP_ERROR_NO_MEM;
        }
        psTableRow->pvData = pvNewData;
        memcpy(psTableRow->pvData, pvData, u32Length);
        psTableRow->u32Length = u32Length;
        
        DBG_vPrintf(DBG_TABLES, "Table entry %d : \n", u32Index);
        
        {
            uint32_t i;
            for (i = 0; i < psTableRow->u32Length; i++)
            {
                DBG_vPrintf(DBG_TABLES, " 0x%02x", ((uint8_t *)psTableRow->pvData)[i]);
            }
        }
        DBG_vPrintf(DBG_TABLES, "\n");
    }
    return E_JIP_OK;
}


teJIP_Status eJIP_GetTableVar(tsJIP_Context *psJIP_Context, tsVar *psVar)
{
    PRIVATE_CONTEXT(psJIP_Context);
    uint16_t u16TableEntriesRemainaing = 255;
    uint16_t u16StartIndex = 0;
    uint16_t u16TableVersion = 0;
    uint32_t u32FirstTime = 1;
    uint32_t u32TotalEntries = 0;
    tsMib *psMib = psVar->psOwnerMib;
    tsNode *psNode = psMib->psOwnerNode;
    teJIP_Status eStatus = E_JIP_OK;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    DBG_vPrintf(DBG_TABLES, "Get variable %d, MiB 0x%08x, %d, Node:", 
                psVar->u8Index, psVar->psOwnerMib->u32MibId, psVar->psOwnerMib->u8Index);
    DBG_vPrintf_IPv6Address(DBG_TABLES, psNode->sNode_Address.sin6_addr);
    
    eJIP_LockNode(psNode, True);
    
    if (psVar->pvData)
    {
        /* Remove all existing entries before we start reading the new content */
        uint32_t i;
        tsTable *psTable = (tsTable *)psVar->pvData;
        for (i = 0; i < psTable->u32NumRows; i++)
        {
            eJIP_Table_UpdateRow(psVar, i, NULL, 0);
        }
    }

    do
    {
        char buffer[255];
        tsJIP_Msg_GetMibRequest *psJIP_Msg_GetMibRequest = (tsJIP_Msg_GetMibRequest *)buffer;
        uint32_t u32ResponseLen = 255;
        tsJIP_Msg_VarDescriptionHeader *psVarDescriptionHeader;
        tsJIP_Msg_VarDescription_Table *psVarDescriptionTable;
        
        psJIP_Msg_GetMibRequest->u32MibId               = htonl(psVar->psOwnerMib->u32MibId);
        psJIP_Msg_GetMibRequest->sRequest.u8VarIndex    = psVar->u8Index;
        psJIP_Msg_GetMibRequest->sRequest.u16FirstEntry = htons(u16StartIndex);
        psJIP_Msg_GetMibRequest->sRequest.u8EntryCount  = 1;
    
        if (Network_ExchangeJIP(&psJIP_Private->sNetworkContext, psVar->psOwnerMib->psOwnerNode, 3, EXCHANGE_FLAG_NONE,
                                E_JIP_COMMAND_GET_MIB_REQUEST, buffer, sizeof(tsJIP_Msg_GetMibRequest), 
                                E_JIP_COMMAND_GET_RESPONSE, buffer, &u32ResponseLen) != E_NETWORK_OK)
        {
            DBG_vPrintf(DBG_TABLES, "Error\n");
            eJIP_UnlockNode(psNode);
            return E_JIP_ERROR_FAILED;
        }
        
        psVarDescriptionHeader = (tsJIP_Msg_VarDescriptionHeader *)buffer;
        psVarDescriptionTable  = (tsJIP_Msg_VarDescription_Table *)buffer;
        
        if (psVarDescriptionHeader->eStatus == E_JIP_ERROR_DISABLED)
        {
            DBG_vPrintf(DBG_TABLES, "Variable disabled\n");
            psVar->eEnable = E_JIP_VAR_DISABLED;
            eJIP_UnlockNode(psNode);
            return E_JIP_ERROR_DISABLED;
        }
        else if (psVarDescriptionHeader->eStatus != E_JIP_OK)
        {
            DBG_vPrintf(DBG_TABLES, "Error reading (status 0x%02x)\n", psVarDescriptionHeader->eStatus);
            eJIP_UnlockNode(psNode);
            return E_JIP_ERROR_FAILED;
        }
        
        if (psVarDescriptionHeader->eVarType != psVar->eVarType)
        {
            DBG_vPrintf(DBG_TABLES, "Type mismatch (got %d, expected %d)\n", psVarDescriptionHeader->eVarType, psVar->eVarType);
            eJIP_UnlockNode(psNode);
            return E_JIP_ERROR_FAILED;
        }
        
        u16TableEntriesRemainaing = ntohs(psVarDescriptionTable->u16Remaining);
        
        if (u32FirstTime)
        {
            u16TableVersion = ntohs(psVarDescriptionTable->u16TableVersion);
        }
        else
        {
            if (u16TableVersion != ntohs(psVarDescriptionTable->u16TableVersion))
            {
                u16StartIndex = 0;
                break;
            }
        }
        
        DBG_vPrintf(DBG_TABLES, "Table version: 0x%04x, remaining: %d\n", ntohs(psVarDescriptionTable->u16TableVersion), ntohs(psVarDescriptionTable->u16Remaining));
        {
            uint32_t u32Packet_Offset = 0;
            
            while (u32Packet_Offset < (u32ResponseLen - sizeof(tsJIP_Msg_VarDescription_Table)))
            {
                u32TotalEntries++;
                switch (psVarDescriptionHeader->eVarType)
                {
                    case(E_JIP_VAR_TYPE_TABLE_BLOB):
                    {
                        tsJIP_Msg_VarDescription_Table_Entry *Table_Entry = (tsJIP_Msg_VarDescription_Table_Entry *)((uint8_t *)psVarDescriptionTable->au8Table + u32Packet_Offset);
                        
                        DBG_vPrintf(DBG_TABLES, "Got table entry at offset %d (%p): index %d, length %d\n", u32Packet_Offset, Table_Entry, ntohs(Table_Entry->u16Entry), Table_Entry->u8Len);
                        
                        {
                            uint32_t u32Index = ntohs(Table_Entry->u16Entry);
                            eJIP_Table_UpdateRow(psVar, u32Index, Table_Entry->au8Blob, Table_Entry->u8Len);
                            u32Packet_Offset += (3 + Table_Entry->u8Len);
                            u16StartIndex = u32Index + 1;
                        }
                        break;
                    }
                    default:
                        DBG_vPrintf(DBG_TABLES, "Not a table variable\n");
                        break;
                }
            }
        }
    } while (u16TableEntriesRemainaing > 0);
    
    if (u32TotalEntries == 0)
    {
        eStatus = JIP_Table_Check_Storage(psVar, -1);
    }
    
    // Set the variable as enabled
    psVar->eEnable = E_JIP_VAR_ENABLED;
    
    eJIP_UnlockNode(psNode);
    return eStatus;
}



teJIP_Status eJIPserver_HandleGetTableVar(tsJIP_Context *psJIP_Context, tsVar *psVar, 
                                          uint16_t u16FirstEntry, uint8_t u8EntryCount,
                                          uint8_t *pcSendData, unsigned int *piSendDataLength)
{
    tsJIP_Msg_VarDescription_Table *psVarDescriptionTable  = (tsJIP_Msg_VarDescription_Table *)pcSendData;
    tsTable *psTable = (tsTable *)psVar->pvData;
    tsTableRow *psTableRow;
    int i, j;
    uint32_t u32Packet_Offset, u32Hash = 0;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s: Get table rows start %d, num %d\n", 
                __FUNCTION__, u16FirstEntry, u8EntryCount);
    
    DBG_vPrintf(DBG_TABLES, "%s: Table has %d rows\n", __FUNCTION__, psTable->u32NumRows);
    for (i = 0; i < psTable->u32NumRows; i++)
    {
        psTableRow = &psTable->psRows[i];
        DBG_vPrintf(DBG_TABLES, "%s: Hash row %d (length %d, data: %p\n", __FUNCTION__, i, psTableRow->u32Length, psTableRow->pvData);
        
        if (psTableRow->pvData)
        {
            /* XOR 32 bit words from the row */
            for (j = 0; j < (psTableRow->u32Length / sizeof(uint32_t)); j++)
            {
                u32Hash ^= ((uint32_t *)psTableRow->pvData)[j];
            }
            /* Now any remaining bytes */
            for (; j < psTableRow->u32Length; j++)
            {
                u32Hash ^= ((uint8_t *)psTableRow->pvData)[j];
            }
        }
        // Rotate hash
        u32Hash = u32Hash >> 1 | (u32Hash << (sizeof(uint32_t) * 8 - 1));
    }
    
    DBG_vPrintf(DBG_TABLES, "%s: Table Version: 0x%04x\n", __FUNCTION__, u32Hash & 0xFFFF);
    
    u32Packet_Offset = sizeof(tsJIP_Msg_VarDescription_Table);
    
    for (i = u16FirstEntry, j = 0;
        (i < psTable->u32NumRows) && (j < u8EntryCount);
        i++)
    {
        DBG_vPrintf(DBG_TABLES, "%s: Examine table row %d\n", __FUNCTION__, i);
        psTableRow = &psTable->psRows[i];
        if (psTableRow->pvData)
        {
            tsJIP_Msg_VarDescription_Table_Entry *psEntry = (tsJIP_Msg_VarDescription_Table_Entry *)&pcSendData[u32Packet_Offset];
            
            DBG_vPrintf(DBG_TABLES, "%s: Add row %d (length %d) to packet\n", __FUNCTION__, i, psTableRow->u32Length);
            
            psEntry->u16Entry   = htons(i);
            psEntry->u8Len      = psTableRow->u32Length;
            memcpy(psEntry->au8Blob, psTableRow->pvData, psTableRow->u32Length);
            
            u32Packet_Offset += sizeof(tsJIP_Msg_VarDescription_Table_Entry) + psTableRow->u32Length;
            j++;
        }    
    }
    /* Count remaining rows */
    for (j = 0; 
         i < psTable->u32NumRows;
         i++)
    {
        psTableRow = &psTable->psRows[i];
        if (psTableRow->pvData)
        {
            j++;
        }
    }
    
    DBG_vPrintf(DBG_TABLES, "%s: Remaining rows: %d\n", __FUNCTION__, j);
    
    psVarDescriptionTable->sHeader.eStatus  = E_JIP_OK;
    psVarDescriptionTable->u16Remaining     = htons(j);
    psVarDescriptionTable->u16TableVersion  = (uint16_t)u32Hash;
    
    *piSendDataLength                       = u32Packet_Offset;
    
    return E_JIP_OK;
}

