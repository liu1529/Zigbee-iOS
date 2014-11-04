/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Network.h
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

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <JIP.h>
#include <JIP_Private.h>
#include <JIP_Packets.h>
#include <Threads.h>


/** Placeholder for no Flags passed to \ref Network_ExchangeJIP 
 */
#define EXCHANGE_FLAG_NONE          0x00000000


/** Flag passed to \ref Network_ExchangeJIP indicating that more data is expected to follow,
 *  and if the device is sleeping it should attempt to stay awake.
 */
#define EXCHANGE_FLAG_STAY_AWAKE    0x00000001


typedef enum
{
    E_NETWORK_OK,
    E_NETWORK_ERROR_NO_MEM      = 0xfd,
    E_NETWORK_ERROR_TIMEOUT     = 0xfe,
    E_NETWORK_ERROR_FAILED      = 0xff
} teNetworkStatus;

typedef enum
{
    E_NETWORK_PROTO_UNKNOWN,
    E_NETWORK_PROTO_IPV6,
    E_NETWORK_PROTO_IPV4,  
} teNetworkProtocol;


typedef enum
{
    E_NETWORK_LINK_UNKNOWN,
    E_NETWORK_LINK_UDP,
    E_NETWORK_LINK_TCP,
} teLink;

typedef struct
{
    struct in6_addr     sMulticastAddress;      /**< Multicast group address */
    uint32_t            u32NumMembers;          /**< How many nodes are a member of the group */
} tsServerGroups;

typedef struct
{
    int                 iSocket;
    
    teNetworkProtocol   eProtocol;
    
    teLink              eLink;
    
    tsJIP_Context       *psJIP_Context;
    
    uint64_t            u64IPv6Prefix;
    
    struct sockaddr_in6 sBorder_Router_IPv6_Address;
    struct sockaddr_in  sGateway_IPv4_Address;
    
    tsThread            sSocketListener;
    tsQueue             sSocketQueue;
    
    uint32_t            u32NumTrapThreads;      /**< Count of the number of currently spawned trap threads */
    
    uint32_t            u32NumGroups;           /**< How many groups the server is a member of */
    tsServerGroups      *pasServerGroups;       /**< Track which multicast groups the server is a member of */

#ifdef LOCK_NETWORK
    tsLock   sNetworkLock;
#endif /* LOCK_NETWORK */
} tsNetworkContext;

tsJIPAddress   Network_MAC_to_IPv6(tsNetworkContext *psNetworkContext, uint64_t u64MAC_Address);

teNetworkStatus Network_Init(tsNetworkContext *psNetworkContext, tsJIP_Context *psJIP_Context);
teNetworkStatus Network_Destroy(tsNetworkContext *psNetworkContext);

teNetworkStatus Network_Connect(tsNetworkContext *psNetworkContext, const char *pcAddress, const int iPort);
teNetworkStatus Network_Connect4(tsNetworkContext *psNetworkContext, const char *pcIPv4Address, const char *pcIPv6Address, int iPort, int iTCP);

teNetworkStatus Network_Listen(tsNetworkContext *psNetworkContext);

teNetworkStatus Network_ClientGroupJoin(tsNetworkContext *psNetworkContext, const char *pcMulticastAddress);
teNetworkStatus Network_ClientGroupLeave(tsNetworkContext *psNetworkContext, const char *pcMulticastAddress);

teNetworkStatus Network_ServerGroupJoin(tsNetworkContext *psNetworkContext, tsNode *psNode, struct in6_addr *psMulticastAddress);
teNetworkStatus Network_ServerGroupLeave(tsNetworkContext *psNetworkContext, tsNode *psNode, struct in6_addr *psMulticastAddress);


teNetworkStatus Network_Send(tsNetworkContext *psNetworkContext, tsJIPAddress *psAddress, const char *pcData, int iDataLength);
teNetworkStatus Network_Recieve(tsNetworkContext *psNetworkContext, uint32_t u32Timeout, tsJIPAddress *psAddress, char *pcData, unsigned int *iDataLength);

teNetworkStatus Network_ExchangeJIP(tsNetworkContext *psNetworkContext, tsNode *psNode, uint32_t u32Retries, uint32_t u32Flags,
                                    teJIP_Command eSendCommand, const char *pcSendData, int iSendDataLength, 
                                    teJIP_Command eReceiveCommand, char *pcReceiveData, unsigned int *piReceiveDataLength);

teNetworkStatus Network_SendJIP(tsNetworkContext *psNetworkContext, tsJIPAddress *psAddress,
                                teJIP_Command eCommand, const char *pcData, int iDataLength);
#endif /* __NETWORK_H__ */
                            