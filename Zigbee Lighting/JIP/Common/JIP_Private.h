/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          JIP_Private.h
 *
 * REVISION:           $Revision: 54345 $
 *
 * DATED:              $Date: 2013-05-29 10:21:20 +0100 (Wed, 29 May 2013) $
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

#ifndef __JIPPRIVATE_H__
#define __JIPPRIVATE_H__

#include <Network.h>
#include <Cache.h>
#include "portable_endian.h"

#define JIP_DEVICE_MAX_GROUPS 16


#define PRIVATE_CONTEXT(context) tsJIP_Private *psJIP_Private = (tsJIP_Private*)context->pvPriv;

#ifndef htobe64

#warning not using toolchain 64bit swapping routines.

#define USE_INTERNAL_BYTESWAP_64

//defined __UCLIBC__ && __UCLIBC_MAJOR__ == 0 && __UCLIBC_MINOR__ == 9 &&  __UCLIBC_SUBLEVEL__ == 30

#include <stdint.h>

uint64_t bswap_64(uint64_t x);

# if __BYTE_ORDER == __LITTLE_ENDIAN
#warning little endian
#  define htobe64(x) __bswap_64 (x)
#  define htole64(x) (x)
#  define be64toh(x) __bswap_64 (x)
#  define le64toh(x) (x)
#else
#warning big endian
#  define htobe64(x) (x)
#  define htole64(x) __bswap_64 (x)
#  define be64toh(x) (x)
#  define le64toh(x) __bswap_64 (x)
#endif /* Byte order */

#endif /* __UCLIBC__ */

/** Private structure used by the library */
typedef struct
{
    teJIP_ContextType   eJIP_ContextType;   /**< The JIP Context type */
    tsNetworkContext    sNetworkContext;
    tsCache             sCache;
    
    /* Thread used by server context to handle requests */
    tsThread            sServerNetworkThread;
    
    /* Network change monitoring thread information */
    tsThread            sNetworkChangeMonitor;
    tprCbNetworkChange  prCbNetworkChange;
    
    /* Lock for all library structures */
    tsLock              sLock;
} tsJIP_Private;


/** Private structure used by the library for a node */
typedef struct
{
    struct in6_addr     asGroupAddresses[JIP_DEVICE_MAX_GROUPS];
} tsNode_Private;


teJIP_Status eJIP_TrapEvent(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, char *pcPacket);


teJIP_Status eJIP_GetTableVar(tsJIP_Context *psJIP_Context, tsVar *psVar);

teJIP_Status eJIPserver_HandleGetTableVar(tsJIP_Context *psJIP_Context, tsVar *psVar, 
                                          uint16_t u16FirstEntry, uint8_t u8EntryCount,
                                          uint8_t *pcSendData, unsigned int *piSendDataLength);


teJIP_Status eJIP_DiscoverNode(tsJIP_Context *psJIP_Context, tsNode* psNode);


/** Allocate storage for a node and put it into the network structure
 *  returns the node locked using \ref JIP_Node_Lock
 *  \param psNet            Pointer to network tree
 *  \param psAddress        Pointer to IPv6 address structure
 *  \param u32DeviceId      Device ID of the node
 *  \return NULL if could not allocate the node, otherwise it's address
 */
tsNode *psJIP_NetAllocateNode(tsNetwork *psNet, tsJIPAddress *psAddress, uint32_t u32DeviceId);

/** Add a new node to the network
 *  The node is added and it's MiBs and variables discovered.
 *  If the device ID is already known, this is filled from the cache,
 *  Otherwise it is read from the device.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param psAddress            Pointer to IPv6 Address structure
 *  \param u32DeviceId          32bit device type identifier
 *  \param ppsNode              [out] Pointer to location in which to store the newly added node, for feedback to the callee.
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_NetAddNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, uint32_t u32DeviceId, tsNode** ppsNode);


/** Remove a node from the network tree.
 *  Removes the node from the network.
 *  If the node is found in the network, it is removed from the network tree.
 *  It's storage is not free'd, that is done by \ref JIP_Net_FreeNode. A pointer to the node can be
 *  returned in ppsNode for passing to this.
 *  \param psJIP_Context        Pointer to the JIP Context
 *  \param psAddress            Pointer to IPv6 Address structure
 *  \param ppsNode              [out]Pointer to location to store the node that has been removed.
 *  \return E_JIP_OK if successfully removed
 */
teJIP_Status eJIP_NetRemoveNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress, tsNode **ppsNode);


/** Free all storage associated with a node
 *  All Mibs are free'd.
 *  All traps are unregistered (packets are sent out, which may fail if the node has already left the network.
 *  \param psJIP_Context        Pointer to the JIP Context
 *  \param psNode               Pointer to Node to free
 * \return E_JIP_OK if successfully free'd
 */
teJIP_Status eJIP_NetFreeNode(tsJIP_Context *psJIP_Context, tsNode *psNode);


/** Free all storage associated with a Mib
 *  All variables that have been read are free'd.
 *  All traps are unregistered (packets are sent out, which may fail if the node has already left the network.
 *  Remember to take a copy of the psNext mib pointer before calling this, as afterwards psMib is a dangling pointer.
 *  \param psJIP_Context        Pointer to the JIP Context
 *  \param psMib                Pointer to Mib to free
 * \return E_JIP_OK if successfully free'd
 */
teJIP_Status eJIP_FreeMib(tsJIP_Context *psJIP_Context, tsMib *psMib);

tsMib *psJIP_NodeAddMib(tsNode *psNode, uint32_t u32MibId, uint8_t u8Index, const char *pcName);

tsVar *psJIP_MibAddVar(tsMib *psMib, uint8_t u8Index, const char *pcName, teJIP_VarType eVarType, 
                       teJIP_AccessType eAccessType, teJIP_Security eSecurity);



/** Utility function to add a node stucture to a linked list of nodes.
 *  \param ppsNodeListHead      Pointer to head of liked list
 *  \param psNode               Pointer to node to add to list
 *  \return Pointer to the node in the list
 */
tsNode *psJIP_NodeListAdd(tsNode **ppsNodeListHead, tsNode *psNode);


/** Utility function to remove a node stucture from a linked list of nodes.
 *  \param ppsNodeListHead      Pointer to head of liked list
 *  \param psNode               Pointer to node to remove from list
 *  \return Pointer to the node now removed from the list
 */
tsNode *psJIP_NodeListRemove(tsNode **ppsNodeListHead, tsNode *psNode);



teJIP_Status eJIPserver_HandlePacket(tsJIP_Context *psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress, tsJIPAddress *psDstAddress,
                                     teJIP_Command eReceiveCommand, uint8_t *pcReceiveData, unsigned int iReceiveDataLength,
                                     teJIP_Command *peSendCommand,  uint8_t *pcSendData, unsigned int *piSendDataLength);


teJIP_Status eGroups_Init(tsNode *psNode);

teJIP_Status eGroups_GroupsGet(tsVar *psVar);
teJIP_Status eGroups_GroupAddSet(tsVar *psVar, tsJIPAddress *psDstAddress);
teJIP_Status eGroups_GroupRemoveSet(tsVar *psVar, tsJIPAddress *psDstAddress);
teJIP_Status eGroups_GroupClearSet(tsVar *psVar, tsJIPAddress *psDstAddress);


#endif /* __JIPPRIVATE_H__ */