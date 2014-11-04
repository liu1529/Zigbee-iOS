/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          Network.c
 *
 * REVISION:           $Revision: 56878 $
 *
 * DATED:              $Date: 2013-10-01 11:33:13 +0100 (Tue, 01 Oct 2013) $
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <errno.h>

#include <JIP.h>
#include <JIP_Private.h>
#include <JIP_Packets.h>
#include <Network.h>
#include <Trace.h>
#include <Threads.h>

#define IPV6_RECVPKTINFO    IPV6_PKTINFO

#define DBG_FUNCTION_CALLS 0
#define DBG_NETWORK 0

/** Timeout period (ms) for powered devices */
#define JIP_CLIENT_TIMEOUT_POWERED      500 

/** Timeout period (ms) for sleeping devices */
#define JIP_CLIENT_TIMEOUT_SLEEPING     8000 


#define MAX_SERVER_EVENTS 100


/** If this is defined, then the source port of UDP datagrams from libJIP will be
 *  bound to JIP_DEFAULT_PORT.
 *  This has the advantage that any registered traps will make it to this process
 *  if it has been restarted.
 */
//#define FIXED_SOURCE_PORT


static void *pvClientSocketListenerThread(void *psThreadInfoVoid);
static void *pvServerSocketListenerThread(void *psThreadInfoVoid);
static void *pvTrapHandlerThread(void *psThreadInfoVoid);


static teNetworkStatus Network_ServerExchange(tsJIP_Context* psJIP_Context, tsNode *psNode, tsJIPAddress *psAddress, tsJIPAddress *psDstAddress,
                                        char *pcReceiveData, unsigned int iReceiveDataLength,
                                        const char *pcSendData, unsigned int *piSendDataLength);

/** This function determines the index of the netwrok interface
 *  that has the address on which the node is bound.
 *  This allows us to set the multicast listen address to the
 *  same interface as that listening for unicasts.
 *  \param psNode       Pointer to node structure
 *  \return > 0 interface number or < 0 error.
 */
static int iNetwork_NodeInterface(tsNode *psNode);


tsJIPAddress   Network_MAC_to_IPv6(tsNetworkContext *psNetworkContext, uint64_t u64MAC_Address)
{
    tsJIPAddress sJIPAddress;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    memset (&sJIPAddress, 0, sizeof(struct sockaddr_in6));
    sJIPAddress.sin6_family  = AF_INET6;
    sJIPAddress.sin6_port    = htons(JIP_DEFAULT_PORT);
    
    u64MAC_Address = htobe64(u64MAC_Address);
    
    memcpy(&sJIPAddress.sin6_addr.s6_addr[0], &psNetworkContext->u64IPv6Prefix, sizeof(uint64_t));
    memcpy(&sJIPAddress.sin6_addr.s6_addr[8], &u64MAC_Address, sizeof(uint64_t));
    
    DBG_vPrintf_IPv6Address(DBG_NETWORK, sJIPAddress.sin6_addr);
    
    return sJIPAddress;
}


teNetworkStatus Network_Init(tsNetworkContext *psNetworkContext, tsJIP_Context *psJIP_Context)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    // Clear down the context first
    memset(psNetworkContext, 0, sizeof(tsNetworkContext));
    
    psNetworkContext->iSocket = -1;
    
    psNetworkContext->psJIP_Context = psJIP_Context;

    psNetworkContext->eProtocol = E_NETWORK_PROTO_UNKNOWN;
#ifdef LOCK_NETWORK
    /* Create the mutext for the network context */
    if (eLockCreate(&psNetworkContext->sNetworkLock) != E_LOCK_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to create lock\n");
        return E_NETWORK_ERROR_FAILED;
    }
#endif /* LOCK_NETWORK */

    /* Initialise number of trap threads */
    psNetworkContext->u32NumTrapThreads = 0;
    
    return E_NETWORK_OK;
}


teNetworkStatus Network_Destroy(tsNetworkContext *psNetworkContext)
{  
    eThreadStop(&psNetworkContext->sSocketListener);
    eQueueDestroy(&psNetworkContext->sSocketQueue);
    
    free(psNetworkContext->pasServerGroups);
    
    while (u32AtomicGet(&psNetworkContext->u32NumTrapThreads) > 0)
    {
        /* Yield to any trap threads so that they can clean themselves up. */
        eThreadYield();
    }
    
    if (psNetworkContext->iSocket >= 0)
    {
        close(psNetworkContext->iSocket);
    }
    
    return E_NETWORK_OK;
}


teNetworkStatus Network_ServerGroupJoin(tsNetworkContext *psNetworkContext, tsNode *psNode, struct in6_addr *psMulticastAddress)
{
    tsServerGroups      *psNewGroups;
    int i;

    for (i = 0; i < psNetworkContext->u32NumGroups; i++)
    {
        if (memcmp(&psNetworkContext->pasServerGroups[i].sMulticastAddress, psMulticastAddress, sizeof(struct in6_addr)) == 0)
        {
            psNetworkContext->pasServerGroups[i].u32NumMembers++;
            DBG_vPrintf(DBG_NETWORK, "Server is already in group - now %d members.\n", psNetworkContext->pasServerGroups[i].u32NumMembers);
            return E_NETWORK_OK;
        }
    }
    
    /* New group */
    DBG_vPrintf(DBG_NETWORK, "Add server to new group - now %d groups.\n", psNetworkContext->u32NumGroups + 1);
    
    psNewGroups = realloc(psNetworkContext->pasServerGroups, sizeof(tsServerGroups) * (psNetworkContext->u32NumGroups + 1));
    if (!psNewGroups)
    {
        return E_NETWORK_ERROR_NO_MEM;
    }
    psNetworkContext->pasServerGroups = psNewGroups;
    
    {
        struct ipv6_mreq    sRequest;
        struct ifaddrs *ifp, *ifs;
        
        if (getifaddrs(&ifs) < 0) 
        {
            DBG_vPrintf(DBG_NETWORK, "%s: getifaddrs failed (%s)\n", __FUNCTION__, strerror(errno));
            return E_NETWORK_ERROR_FAILED;
        }
        
        sRequest.ipv6mr_multiaddr = *psMulticastAddress;

        for (ifp = ifs; ifp; ifp = ifp->ifa_next)
        {
            if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_INET6)
            {
                char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
                DBG_vPrintf(DBG_NETWORK, "Interface %5s: [%s]\n", ifp->ifa_name,
                    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)(ifp->ifa_addr))->sin6_addr, acAddr, INET6_ADDRSTRLEN));
                
                sRequest.ipv6mr_interface = if_nametoindex(ifp->ifa_name);
                DBG_vPrintf(DBG_NETWORK, "Join group on interface %d\n", sRequest.ipv6mr_interface);
                
                // Now we join the group
                if (setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, &sRequest, sizeof(struct ipv6_mreq)) < 0)
                {
                    /* If the socket is already a member of the group, that is ok. */
                    if (errno != EADDRINUSE)
                    {
                        DBG_vPrintf(DBG_NETWORK, "%s: setsockopt failed (%s)\n", __FUNCTION__, strerror(errno));
                        return E_NETWORK_ERROR_FAILED;
                    }
                }
            }
        }
        freeifaddrs(ifs);
    }

    psNewGroups = &psNetworkContext->pasServerGroups[psNetworkContext->u32NumGroups];
    psNetworkContext->u32NumGroups++;
    
    psNewGroups->sMulticastAddress = *psMulticastAddress;
    psNewGroups->u32NumMembers = 1;
    
    return E_NETWORK_OK;
}


teNetworkStatus Network_ServerGroupLeave(tsNetworkContext *psNetworkContext, tsNode *psNode, struct in6_addr *psMulticastAddress)
{
    int i;
    int iGroupAddressSlot = 0;
    
    for (i = 0; i < psNetworkContext->u32NumGroups; i++)
    {
        if (memcmp(&psNetworkContext->pasServerGroups[i].sMulticastAddress, psMulticastAddress, sizeof(struct in6_addr)) == 0)
        {
            psNetworkContext->pasServerGroups[i].u32NumMembers--;
            DBG_vPrintf(DBG_NETWORK, "Server is in group - now %d members.\n", psNetworkContext->pasServerGroups[i].u32NumMembers);
            
            if (psNetworkContext->pasServerGroups[i].u32NumMembers == 0)
            {
                /* break from the loop if we are going to remove the group */
                iGroupAddressSlot = i;
                break;
            }
            
            return E_NETWORK_OK;
        }
    }
    
    if (i == psNetworkContext->u32NumGroups)
    {
        /* If we get to the end of the table then the group did not exist */
        DBG_vPrintf(DBG_NETWORK, "Server is not in group\n");
        return E_NETWORK_ERROR_FAILED;
    }
    
    {
        struct ipv6_mreq    sRequest;
        struct ifaddrs *ifp, *ifs;
        
        if (getifaddrs(&ifs) < 0) 
        {
            DBG_vPrintf(DBG_NETWORK, "%s: getifaddrs failed (%s)\n", __FUNCTION__, strerror(errno));
            return E_NETWORK_ERROR_FAILED;
        }
        
        sRequest.ipv6mr_multiaddr = *psMulticastAddress;

        for (ifp = ifs; ifp; ifp = ifp->ifa_next)
        {
            if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_INET6)
            {
                char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
                DBG_vPrintf(DBG_NETWORK, "Interface %5s: [%s]\n", ifp->ifa_name,
                    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)(ifp->ifa_addr))->sin6_addr, acAddr, INET6_ADDRSTRLEN));
                
                sRequest.ipv6mr_interface = if_nametoindex(ifp->ifa_name);
                DBG_vPrintf(DBG_NETWORK, "Leave group on interface %d\n", sRequest.ipv6mr_interface);
                
                // Now we leave the group
                if (setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &sRequest, sizeof(struct ipv6_mreq)) < 0)
                {
                    /* If the socket was not a member of the group, that is ok. */
                    if (errno != EADDRNOTAVAIL)
                    {
                        DBG_vPrintf(DBG_NETWORK, "%s: setsockopt failed (%s)\n", __FUNCTION__, strerror(errno));
                        return E_NETWORK_ERROR_FAILED;
                    }
                }
            }
        }
        freeifaddrs(ifs);
    }

    // Now remove the entry from the server groups table.
    psNetworkContext->u32NumGroups--;
    DBG_vPrintf(DBG_NETWORK, "Remove server from group - now %d groups.\n", psNetworkContext->u32NumGroups);
    
    // Shuffle remaining groups down the table.
    memmove(&psNetworkContext->pasServerGroups[iGroupAddressSlot], 
            &psNetworkContext->pasServerGroups[iGroupAddressSlot + 1],
            sizeof(tsServerGroups) * (psNetworkContext->u32NumGroups - iGroupAddressSlot));
    
    return E_NETWORK_OK;
}


teNetworkStatus Network_ClientGroupJoin(tsNetworkContext *psNetworkContext, const char *pcMulticastAddress)
{
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    struct ipv6_mreq    sRequest;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    memset(&sRequest, 0, sizeof(struct ipv6_mreq));

    /* Set IPv6 multicast address */
    if (inet_pton(AF_INET6, pcMulticastAddress, &sRequest.ipv6mr_multiaddr) <= 0)
    {
        DBG_vPrintf(DBG_NETWORK, "Could not determine network address from [%s]\n", pcMulticastAddress);
        return E_NETWORK_ERROR_FAILED;
    }
    
    // If we have a tun0, join on this interface, otherwise join on all interfaces (if_nametoindex returns 0 = default)
    sRequest.ipv6mr_interface = if_nametoindex("tun0");
    
    // Now we join the group
    if (setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, &sRequest, sizeof(struct ipv6_mreq)) < 0)
    {
        DBG_vPrintf(DBG_NETWORK, "%s: setsockopt failed (%s)\n", __FUNCTION__, strerror(errno));
        return E_NETWORK_ERROR_FAILED;
    }
    
    DBG_vPrintf(DBG_NETWORK, "Node added to group [%s] (interface %d)\n", 
                inet_ntop(AF_INET6, &sRequest.ipv6mr_multiaddr, acAddr, INET6_ADDRSTRLEN), sRequest.ipv6mr_interface);
 
    return E_NETWORK_OK;
}


teNetworkStatus Network_ClientGroupLeave(tsNetworkContext *psNetworkContext, const char *pcMulticastAddress)
{
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    struct ipv6_mreq    sRequest;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    memset(&sRequest, 0, sizeof(struct ipv6_mreq));

    /* Set IPv6 multicast address */
    if (inet_pton(AF_INET6, pcMulticastAddress, &sRequest.ipv6mr_multiaddr) <= 0)
    {
        DBG_vPrintf(DBG_NETWORK, "Could not determine network address from [%s]\n", pcMulticastAddress);
        return E_NETWORK_ERROR_FAILED;
    }
    
    // If we have a tun0, join on this interface, otherwise join on all interfaces (if_nametoindex returns 0 = default)
    sRequest.ipv6mr_interface = if_nametoindex("tun0");
    
    // Now we join the group
    if (setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &sRequest, sizeof(struct ipv6_mreq)) < 0)
    {
        DBG_vPrintf(DBG_NETWORK, "%s: setsockopt failed (%s)\n", __FUNCTION__, strerror(errno));
        return E_NETWORK_ERROR_FAILED;
    }
    
    DBG_vPrintf(DBG_NETWORK, "Node added to group [%s] (interface %d)\n", 
                inet_ntop(AF_INET6, &sRequest.ipv6mr_multiaddr, acAddr, INET6_ADDRSTRLEN), sRequest.ipv6mr_interface);
 
    return E_NETWORK_OK;
}


teNetworkStatus Network_Connect(tsNetworkContext *psNetworkContext, const char *pcAddress, int iPort)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    struct addrinfo hints, *res;
    int s;
    char acBuffer[6];
    
    // first, load up address structs with getaddrinfo():

    DBG_vPrintf(DBG_NETWORK, "Connecting to %s\n", pcAddress);
    
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    sprintf(acBuffer, "%d", iPort);
    s = getaddrinfo(pcAddress, acBuffer, &hints, &res);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return E_NETWORK_ERROR_FAILED;
    }

    psNetworkContext->iSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    if (psNetworkContext->iSocket < 0)
    {
        perror("Could not create socket");
        return E_NETWORK_ERROR_FAILED;
    }
    
    memcpy(&psNetworkContext->u64IPv6Prefix, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, sizeof(uint64_t));
    memcpy(&psNetworkContext->sBorder_Router_IPv6_Address, res->ai_addr, sizeof(struct sockaddr_in6));

#ifdef FIXED_SOURCE_PORT
    memset(&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, 0, sizeof(struct in6_addr));
    /* Bind to the local address, port JIP_DEFAULT_PORT */
    if (bind(psNetworkContext->iSocket, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Error binding socket");
        return E_NETWORK_ERROR_FAILED;
    }
#endif /* FIXED_SOURCE_PORT */
    
    freeaddrinfo(res);
    
    DBG_vPrintf_IPv6Address(DBG_NETWORK, psNetworkContext->sBorder_Router_IPv6_Address.sin6_addr);

    psNetworkContext->eProtocol = E_NETWORK_PROTO_IPV6;
    psNetworkContext->eLink = E_NETWORK_LINK_UDP;
    
    /* Set up socket listener thread and queue */
    if (eQueueCreate(&psNetworkContext->sSocketQueue, 3) != E_QUEUE_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to set up connect socket queue\n");
        return E_NETWORK_ERROR_FAILED;
    }
    
    psNetworkContext->sSocketListener.pvThreadData = psNetworkContext;

    if (eThreadStart(pvClientSocketListenerThread, &psNetworkContext->sSocketListener, E_THREAD_JOINABLE) != E_THREAD_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to start connect socket listener thread\n");
        return E_NETWORK_ERROR_FAILED;
    }

    DBG_vPrintf(DBG_NETWORK, "Connect socket set up ok\n");

    return E_NETWORK_OK;
}



teNetworkStatus Network_Connect4(tsNetworkContext *psNetworkContext, const char *pcIPv4Address, const char *pcIPv6Address, int iPort, int iTCP)
{
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    struct addrinfo hints, *res;
    int s;
    char acBuffer[6];

    // first, load up address structs with getaddrinfo():

    DBG_vPrintf(DBG_NETWORK, "Connecting to %s (via %s)\n", pcIPv6Address, pcIPv4Address);
    
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    if (iTCP == 0)
    {
        hints.ai_socktype = SOCK_DGRAM;
    }
    else
    {
        hints.ai_socktype = SOCK_STREAM;
    }
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    sprintf(acBuffer, "%d", iPort);
    s = getaddrinfo(pcIPv4Address, acBuffer, &hints, &res);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return E_NETWORK_ERROR_FAILED;
    }

    psNetworkContext->iSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    if (psNetworkContext->iSocket < 0)
    {
        perror("Could not create socket");
        return E_NETWORK_ERROR_FAILED;
    }
    
    memcpy(&psNetworkContext->sGateway_IPv4_Address, res->ai_addr, sizeof(struct sockaddr_in));
    
#ifdef FIXED_SOURCE_PORT
    memset(&((struct sockaddr_in *)res->ai_addr)->sin_addr, 0, sizeof(struct in_addr));
    /* Bind to the local address, port JIP_DEFAULT_PORT */
    if (bind(psNetworkContext->iSocket, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Error binding socket");
        return E_NETWORK_ERROR_FAILED;
    }
#endif /* FIXED_SOURCE_PORT */

    freeaddrinfo(res);
    
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
           
    s = getaddrinfo(pcIPv6Address, acBuffer, &hints, &res);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return E_NETWORK_ERROR_FAILED;
    }

    memcpy(&psNetworkContext->u64IPv6Prefix, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, sizeof(uint64_t));
    memcpy(&psNetworkContext->sBorder_Router_IPv6_Address, res->ai_addr, sizeof(struct sockaddr_in6));
    freeaddrinfo(res);
    
    DBG_vPrintf_IPv6Address(DBG_NETWORK, psNetworkContext->sBorder_Router_IPv6_Address.sin6_addr);

    psNetworkContext->eProtocol = E_NETWORK_PROTO_IPV4;
    
    if (iTCP)
    {
        psNetworkContext->eLink = E_NETWORK_LINK_TCP;
        
        if (connect(psNetworkContext->iSocket, (struct sockaddr*)&psNetworkContext->sGateway_IPv4_Address, sizeof(struct sockaddr_in)) != 0)
        {
            perror("Connect");
            DBG_vPrintf(DBG_NETWORK, "Could not connect to gateway\n");
            return E_NETWORK_ERROR_FAILED;
        }
    }
    else
    {
        psNetworkContext->eLink = E_NETWORK_LINK_UDP;
    }
    
    if (eQueueCreate(&psNetworkContext->sSocketQueue, 3) != E_QUEUE_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to set up connect socket queue\n");
        return E_NETWORK_ERROR_FAILED;
    }
    
    psNetworkContext->sSocketListener.pvThreadData = psNetworkContext;

    if (eThreadStart(pvClientSocketListenerThread, &psNetworkContext->sSocketListener, E_THREAD_JOINABLE) != E_THREAD_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to start connect socket listener thread\n");
        return E_NETWORK_ERROR_FAILED;
    }

    DBG_vPrintf(DBG_NETWORK, "Connect socket set up ok\n");

    return E_NETWORK_OK;
}


teNetworkStatus Network_Listen(tsNetworkContext *psNetworkContext)
{
    struct addrinfo hints, *res;
    int s;
    teNetworkStatus eStatus = E_NETWORK_OK;
    char acPort[10];
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    /* Get the port back into a string for getaddrinfo */
    snprintf(acPort, 10, "%d", JIP_DEFAULT_PORT);
    
    // load up address structs with getaddrinfo() for the socket:
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo("::", acPort, &hints, &res);
    if (s != 0)
    {
        DBG_vPrintf(DBG_NETWORK, "getaddrinfo failed: %s\n", gai_strerror(s));
        return E_NETWORK_ERROR_FAILED;
    }

    psNetworkContext->iSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (psNetworkContext->iSocket < 0)
    {
        DBG_vPrintf(DBG_NETWORK, "Failed to create listening socket (%s)\n", strerror(errno));
        eStatus = E_NETWORK_ERROR_FAILED;
    }
    else
    {
        const int on = 1, off = 0;

        setsockopt(psNetworkContext->iSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        setsockopt(psNetworkContext->iSocket, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
        setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        setsockopt(psNetworkContext->iSocket, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
        
        if (bind(psNetworkContext->iSocket, res->ai_addr, sizeof(struct sockaddr_in6)) < 0)
        {
            DBG_vPrintf(DBG_NETWORK, "Failed to bind listening socket\n");
            eStatus = E_NETWORK_ERROR_FAILED;
        }
        else
        {
            DBG_vPrintf(DBG_NETWORK, "Network bound to address [%s]:%d\n", 
                        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, acAddr, INET6_ADDRSTRLEN), 
                        ntohs(((struct sockaddr_in6 *)res->ai_addr)->sin6_port));
        }
    }
    
    freeaddrinfo(res);  
    
    if (eStatus == E_NETWORK_OK)
    {
        // Start the server listening threads
        psNetworkContext->sSocketListener.pvThreadData = psNetworkContext;

        if (eThreadStart(pvServerSocketListenerThread, &psNetworkContext->sSocketListener, E_THREAD_JOINABLE) != E_THREAD_OK)
        {
            DBG_vPrintf(DBG_NETWORK, "Failed to start server socket listener thread\n");
            eStatus = E_NETWORK_ERROR_FAILED;
        }
    }
    return eStatus;
}


teNetworkStatus Network_Send(tsNetworkContext *psNetworkContext, tsJIPAddress *psAddress, const char *pcData, int iDataLength)
{
    ssize_t iBytesSent;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    DBG_vPrintf_IPv6Address(DBG_NETWORK, psAddress->sin6_addr);
    
    DBG_vPrintf(DBG_NETWORK, "Data length: %d, %p\n", iDataLength, pcData);
    
    if (psNetworkContext->eProtocol == E_NETWORK_PROTO_IPV6)
    {
        DBG_vPrintf(DBG_NETWORK, "Sending direct IPv6 packet\n");
        iBytesSent = sendto(psNetworkContext->iSocket, pcData, iDataLength, 0, 
                        (struct sockaddr*)psAddress, sizeof(struct sockaddr_in6));
    }
    else if (psNetworkContext->eProtocol == E_NETWORK_PROTO_IPV4)
    {
        const uint32_t u32HeaderSize = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(struct in6_addr);
        DBG_vPrintf(DBG_NETWORK, "Sending IPv6 in IPv4 packet\n");
        char *NewPacket = alloca(iDataLength + u32HeaderSize);
        uint16_t u16Length;
        
        u16Length = htons(iDataLength + sizeof(struct in6_addr));

        NewPacket[0] = 1;       /* JIPv4 version */
        memcpy(&NewPacket[1], &u16Length, sizeof(uint16_t));
        memcpy(&NewPacket[3], &psAddress->sin6_addr, sizeof(struct in6_addr));
        memcpy(&NewPacket[u32HeaderSize], pcData, iDataLength);
        
        if (psNetworkContext->eLink == E_NETWORK_LINK_UDP)
        {
            iBytesSent = sendto(psNetworkContext->iSocket, NewPacket, u32HeaderSize + iDataLength, 0, 
                (struct sockaddr*)&psNetworkContext->sGateway_IPv4_Address, sizeof(struct sockaddr_in));
        }
        else if (psNetworkContext->eLink == E_NETWORK_LINK_TCP)
        {
            iBytesSent = send(psNetworkContext->iSocket, NewPacket, u32HeaderSize + iDataLength, 0);
        }
        else
        {
            DBG_vPrintf(DBG_NETWORK, "Unknown Link layer\n");
            return E_NETWORK_ERROR_FAILED;
        }
        DBG_vPrintf(DBG_NETWORK, "Sent %d bytes (of %d)\n", (int)iBytesSent, iDataLength);
        iBytesSent -= u32HeaderSize;
    }
    else
    {
        DBG_vPrintf(DBG_NETWORK, "Unknown protocol\n");
        return E_NETWORK_ERROR_FAILED;
    }
    
    if (iBytesSent != iDataLength)
    {
        perror("Failed to send");
        return E_NETWORK_ERROR_FAILED;
    }
    return E_NETWORK_OK;
}

typedef struct
{
    ssize_t             iBytesRecieved;
    struct sockaddr_in6 sRecv_addr;
#define PACKET_BUFFER_SIZE 1024
    char                acBuffer[PACKET_BUFFER_SIZE];
} tsReceivedPacket;


/** Structure of data passed to a trap handler thread */
typedef struct
{
    tsNetworkContext *psNetworkContext;     /**< Pointer to the network context */
    tsReceivedPacket *psReceivedPacket;     /**< Pointer to the packet that has been received */
} tsTrapHandlerTheadInfo;


static void *pvClientSocketListenerThread(void *psThreadInfoVoid)
{
    tsThread *psThreadInfo = (tsThread *)psThreadInfoVoid;
    tsNetworkContext *psNetworkContext = (tsNetworkContext *)psThreadInfo->pvThreadData;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    psThreadInfo->eState = E_THREAD_RUNNING;
    
    while (psThreadInfo->eState == E_THREAD_RUNNING)
    {        
        unsigned int AddressSize = sizeof(struct sockaddr_in6);
        tsReceivedPacket *psReceivedPacket = malloc(sizeof(tsReceivedPacket));
        
        if (!psReceivedPacket)
        {
            printf("Could not malloc buffer to receive data\n");
            sleep(1);
            continue;
        }
        
        DBG_vPrintf(DBG_NETWORK, "Ready to get packet\n");
        
        if (psNetworkContext->eLink == E_NETWORK_LINK_TCP)
        {
            psReceivedPacket->iBytesRecieved = recv(psNetworkContext->iSocket, &psReceivedPacket->acBuffer, 
                                                    PACKET_BUFFER_SIZE, 0);
            
            if (psReceivedPacket->iBytesRecieved < 0)
            {
                perror("recv");
            }
            else if (psReceivedPacket->iBytesRecieved == 0)
            {
                printf("Client disconnected\n");
                close(psNetworkContext->iSocket);
                break;
            }
        }
        else if (psNetworkContext->eLink == E_NETWORK_LINK_UDP)
        {
            DBG_vPrintf(DBG_NETWORK, "recv()\n");
            psReceivedPacket->iBytesRecieved = recvfrom(psNetworkContext->iSocket, &psReceivedPacket->acBuffer, 
                                                    PACKET_BUFFER_SIZE, 0,
                                                    (struct sockaddr*)&psReceivedPacket->sRecv_addr, &AddressSize);
            DBG_vPrintf(DBG_NETWORK, "Got %d bytes\n", (int)psReceivedPacket->iBytesRecieved);
        }
        else
        {
            DBG_vPrintf(DBG_NETWORK, "Unknown link layer\n");
        }
        
        if (psReceivedPacket->iBytesRecieved > 0)
        {
            if (psNetworkContext->eProtocol == E_NETWORK_PROTO_IPV4)
            {
                const uint32_t u32HeaderSize = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(struct in6_addr);
                psReceivedPacket->iBytesRecieved -= u32HeaderSize;
                
                memset (&psReceivedPacket->sRecv_addr, 0, sizeof(struct sockaddr_in6));
                psReceivedPacket->sRecv_addr.sin6_family  = AF_INET6;
                psReceivedPacket->sRecv_addr.sin6_port    = htons(JIP_DEFAULT_PORT);
                
                memcpy(&psReceivedPacket->sRecv_addr.sin6_addr, &psReceivedPacket->acBuffer[sizeof(uint8_t) + sizeof(uint16_t)], sizeof(struct in6_addr));
                memmove(&psReceivedPacket->acBuffer[0], &psReceivedPacket->acBuffer[u32HeaderSize], psReceivedPacket->iBytesRecieved);
            }
            tsJIP_MsgHeader *psReceiveHeader;
            
            DBG_vPrintf(DBG_NETWORK, "%s: Packet from node: ", __FUNCTION__);
            DBG_vPrintf_IPv6Address(DBG_NETWORK, psReceivedPacket->sRecv_addr.sin6_addr);
            
            psReceiveHeader = (tsJIP_MsgHeader *)psReceivedPacket->acBuffer; 
            
            if (psReceiveHeader->u8Version != JIP_VERSION)
            {
                /* Check version */
                DBG_vPrintf(DBG_NETWORK, "Version mismatch!\n");
                free(psReceivedPacket);
                continue;
            }
            
            /* Here we check if this is an ad-hoc packet, that we should pass up up for handling,
             * or one we miy have been expecting as part of a challenge-response */            
            switch (psReceiveHeader->eCommand)
            {
                case (E_JIP_COMMAND_TRAP_NOTIFY):
                    {
                        tsThread               *psTrapHandlerThread;
                        tsTrapHandlerTheadInfo *psTrapHandlerTheadInfo;
                        
                        psTrapHandlerThread = malloc(sizeof(tsThread));
                        
                        if (psTrapHandlerThread == NULL)
                        {
                            DBG_vPrintf(DBG_NETWORK, "Could not allocate space for incoming trap\n");
                            free(psReceivedPacket);
                        }
                        else
                        {
                            psTrapHandlerTheadInfo = malloc(sizeof(tsTrapHandlerTheadInfo));
                            
                            if (psTrapHandlerTheadInfo == NULL)
                            {
                                DBG_vPrintf(DBG_NETWORK, "Could not allocate space for incoming trap\n");
                                free(psTrapHandlerThread);
                                free(psReceivedPacket);
                            }
                            else
                            {
                                psTrapHandlerTheadInfo->psNetworkContext = psNetworkContext;
                                psTrapHandlerTheadInfo->psReceivedPacket = psReceivedPacket;
                                psTrapHandlerThread->pvThreadData = psTrapHandlerTheadInfo;

                                if (eThreadStart(pvTrapHandlerThread, psTrapHandlerThread, E_THREAD_DETACHED) != E_THREAD_OK)
                                {
                                    DBG_vPrintf(DBG_NETWORK, "Failed to start trap handler thread\n");

                                    free(psTrapHandlerTheadInfo);
                                    free(psTrapHandlerThread);
                                    free(psReceivedPacket);
                                }
                                else
                                {
                                    /* Increment number of running threads */
                                    u32AtomicAdd(&psNetworkContext->u32NumTrapThreads, 1);
                                }
                            }
                        }
                    }
                    break;
                
                default:
                    if (eQueueQueue(&psNetworkContext->sSocketQueue, psReceivedPacket) != E_QUEUE_OK)
                    {
                        DBG_vPrintf(DBG_NETWORK, "Failed to equeue response packet.\n");
                        free(psReceivedPacket);
                    }
                    else
                    {
                        DBG_vPrintf(DBG_NETWORK, "Response enqueued.\n");
                    }
                    break;
            }
        }
        else
        {
            DBG_vPrintf(DBG_NETWORK, "Error during recv %d: %s.\n",  (int)psReceivedPacket->iBytesRecieved, psReceivedPacket->iBytesRecieved == -1 ? strerror(errno) : "socket closed");
            free(psReceivedPacket);
        }
    }
    
    DBG_vPrintf(DBG_NETWORK, "%s: exit\n", __FUNCTION__);
    
    /* Return from thread clearing resources */
    eThreadFinish(psThreadInfo);
    return NULL;
}


static void *pvTrapHandlerThread(void *psThreadInfoVoid)
{
    tsThread *psThreadInfo = (tsThread *)psThreadInfoVoid;
    tsTrapHandlerTheadInfo *psTrapHandlerTheadInfo = (tsTrapHandlerTheadInfo *)psThreadInfo->pvThreadData;
    tsNetworkContext *psNetworkContext = psTrapHandlerTheadInfo->psNetworkContext;
    tsJIP_Context *psJIP_Context = psNetworkContext->psJIP_Context;
    tsReceivedPacket *psReceivedPacket = psTrapHandlerTheadInfo->psReceivedPacket;
    //PRIVATE_CONTEXT(psJIP_Context);
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    DBG_vPrintf(DBG_NETWORK, "%s Trap from ", __FUNCTION__);
    DBG_vPrintf_IPv6Address(DBG_NETWORK, psReceivedPacket->sRecv_addr.sin6_addr);
    
    /* Call handler, in this threads context. */
    eJIP_TrapEvent(psJIP_Context, &psReceivedPacket->sRecv_addr, psReceivedPacket->acBuffer);
    
    DBG_vPrintf(DBG_NETWORK, "%s Trap handled ", __FUNCTION__);
    DBG_vPrintf_IPv6Address(DBG_NETWORK, psReceivedPacket->sRecv_addr.sin6_addr);
    
    /* Free up storage and exit this thread */
    free(psReceivedPacket);
    free(psTrapHandlerTheadInfo);
    free(psThreadInfo->pvPriv);
    free(psThreadInfo);
    
    DBG_vPrintf(DBG_NETWORK, "Trap handler thread exit\n");
    
    /* Decrement number of running threads */
    u32AtomicAdd(&psNetworkContext->u32NumTrapThreads, -1);
    return NULL;    
}


static void *pvServerSocketListenerThread(void *psThreadInfoVoid)
{
    tsThread *psThreadInfo = (tsThread *)psThreadInfoVoid;
    tsNetworkContext *psNetworkContext = (tsNetworkContext *)psThreadInfo->pvThreadData;
    tsJIP_Context *psJIP_Context = psNetworkContext->psJIP_Context;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    psThreadInfo->eState = E_THREAD_RUNNING;

    while (psThreadInfo->eState == E_THREAD_RUNNING)
    {
        uint32_t u32Attempts = 0;
        int iInLen = 0;
        unsigned int iOutLen = 0;
        struct msghdr           sMsgInfo;
        struct iovec            sIO;
        struct sockaddr_in6     sSrcAddress;
        struct in6_pktinfo*     psInPacketInfo = NULL;
        struct cmsghdr*         psControlMessage;
        
        char acInMsgControl[1024];
        char acOutMsgControl[1024];
        char acInBuf[PACKET_BUFFER_SIZE];
        char acOutBuf[PACKET_BUFFER_SIZE];
        
        bool_t bIsMulticast = False;
        
        memset(&sMsgInfo, 0, sizeof(struct msghdr));
        memset(&sIO, 0, sizeof(struct iovec));
        memset(acInMsgControl, 0, sizeof(acInMsgControl));
        
        sIO.iov_base = acInBuf;
        sIO.iov_len  = PACKET_BUFFER_SIZE;
        
        sMsgInfo.msg_name = &sSrcAddress;
        sMsgInfo.msg_namelen = sizeof(sSrcAddress);
        sMsgInfo.msg_iov = &sIO;
        sMsgInfo.msg_iovlen = 1;
        sMsgInfo.msg_control = acInMsgControl;
        sMsgInfo.msg_controllen = sizeof(acInMsgControl);

        iInLen = recvmsg(psNetworkContext->iSocket, &sMsgInfo, 0);
        
        if (iInLen <= 0)
        {
            DBG_vPrintf(DBG_NETWORK, "%s: Error in recvmsg (%s)\n", __FUNCTION__, strerror(errno));
            continue;
        }   
            
        DBG_vPrintf(DBG_NETWORK, "%s: Got %d bytes from: ", __FUNCTION__, iInLen);
        DBG_vPrintf_IPv6Address(DBG_NETWORK, sSrcAddress.sin6_addr);

        for (psControlMessage = CMSG_FIRSTHDR(&sMsgInfo); 
             psControlMessage != 0; 
             psControlMessage = CMSG_NXTHDR(&sMsgInfo, psControlMessage))
        {
            if (psControlMessage->cmsg_level == IPPROTO_IPV6 && psControlMessage->cmsg_type == IPV6_PKTINFO)
            {
                psInPacketInfo = (struct in6_pktinfo*)CMSG_DATA(psControlMessage);
            }
        }
        
        if (!psInPacketInfo)
        {
            // Without the packet info we have no idea of the destination address - discard it.
            continue;
        }

        DBG_vPrintf(DBG_NETWORK, "%s: Packet destination: ", __FUNCTION__);
        DBG_vPrintf_IPv6Address(DBG_NETWORK, psInPacketInfo->ipi6_addr);

        if (psInPacketInfo->ipi6_addr.s6_addr[0] == 0xFF)
        {
            DBG_vPrintf(DBG_NETWORK, "Multicast packet\n");
            bIsMulticast = True;
        }

        // Look up which node(s) have that unicast address / are members of the multicast group
start_lock:
        eJIP_Lock(psJIP_Context);
        {
            tsNode *psNode = psJIP_Context->sNetwork.psNodes;
            tsJIPAddress sDstAddress;
            
            memset(&sDstAddress, 0, sizeof(tsJIPAddress));
            
            memcpy(&sDstAddress.sin6_addr, &psInPacketInfo->ipi6_addr, sizeof(struct in6_addr));
            
            while (psNode)
            {
                if (bIsMulticast)
                {
                    /* This was a multicast packet so look through each nodes goup membership */
                    int iGroupAddressSlot;
                    tsNode_Private *psNode_Private;
                    
                    if (eJIP_LockNode(psNode, False) == E_JIP_ERROR_WOULD_BLOCK)
                    {
                        DBG_vPrintf(DBG_NETWORK, "Locking node %p would block\n", psNode);
                        eJIP_Unlock(psJIP_Context);
                        if (++u32Attempts > 10)
                        {
                            DBG_vPrintf(DBG_NETWORK, "Error locking node:");
                            DBG_vPrintf_IPv6Address(DBG_NETWORK, psNode->sNode_Address.sin6_addr);
                            
                            /* Give up on this node */
                            u32Attempts = 0;
                            continue;
                        }
                        eThreadYield();
                        goto start_lock;
                    }
                    eJIP_Unlock(psJIP_Context);

                    // We've got a lock on the node at this point
                    psNode_Private = (tsNode_Private *)psNode->pvPriv;
                    
                    /* Check if the node is already in the group */
                    for (iGroupAddressSlot = 0; 
                        iGroupAddressSlot < JIP_DEVICE_MAX_GROUPS; 
                        iGroupAddressSlot++)
                    {
                        if (memcmp(&psNode_Private->asGroupAddresses[iGroupAddressSlot], &psInPacketInfo->ipi6_addr, sizeof(struct in6_addr)) == 0)
                        {
                            DBG_vPrintf(DBG_NETWORK, "%s: Node is in the multicast group\n", __FUNCTION__);
                            
                            if (Network_ServerExchange(psJIP_Context, psNode, &sSrcAddress, &sDstAddress,
                                    acInBuf, iInLen,
                                    acOutBuf, &iOutLen) == E_NETWORK_OK)
                            {
                                /* Command handled */
                            }
                            
                            // Discard the response - we don't reply to multicasts.
                            iOutLen = 0;
                        }
                    }
                    
                    // Unlock the node again
                    eJIP_UnlockNode(psNode);
                }
                else
                {
                    if (memcmp(&psInPacketInfo->ipi6_addr, &psNode->sNode_Address.sin6_addr, sizeof(struct in6_addr)) == 0)
                    {
                        DBG_vPrintf(DBG_NETWORK, "Found node ");
                        DBG_vPrintf_IPv6Address(DBG_NETWORK, psNode->sNode_Address.sin6_addr);
                        
                        if (eJIP_LockNode(psNode, False) == E_JIP_ERROR_WOULD_BLOCK)
                        {
                            DBG_vPrintf(DBG_NETWORK, "Locking node %p would block\n", psNode);
                            eJIP_Unlock(psJIP_Context);
                            if (++u32Attempts > 10)
                            {
                                DBG_vPrintf(DBG_NETWORK, "Error locking node:");
                                DBG_vPrintf_IPv6Address(DBG_NETWORK, psNode->sNode_Address.sin6_addr);
                                sleep(1);
                                u32Attempts = 0;
                            }
                            eThreadYield();
                            goto start_lock;
                        }
                        eJIP_Unlock(psJIP_Context);
                        
                        if (Network_ServerExchange(psJIP_Context, psNode, &sSrcAddress, &sDstAddress,
                                        acInBuf, iInLen,
                                        acOutBuf, &iOutLen) == E_NETWORK_OK)
                        {
                            /* Send response */
                            DBG_vPrintf(DBG_NETWORK, "%s: send %d bytes to ", __FUNCTION__, iOutLen);
                            DBG_vPrintf_IPv6Address(DBG_NETWORK, (sSrcAddress.sin6_addr));
                        }
                        else
                        {
                            iOutLen = 0;
                        }
                    }
                    // Unlock the node again
                    eJIP_UnlockNode(psNode);
                }
                psNode = psNode->psNext;
            }
        }
        eJIP_Unlock(psJIP_Context);
        
        if (iOutLen && !bIsMulticast)
        {
            // Send the response packet (but not if this was a multicast
            memset(&sMsgInfo, 0, sizeof(struct msghdr));
            memset(&sIO, 0, sizeof(struct iovec));
            memset(acOutMsgControl, 0, sizeof(acOutMsgControl));
            
            sIO.iov_base = acOutBuf;
            sIO.iov_len  = iOutLen;

            sMsgInfo.msg_name = &sSrcAddress;
            sMsgInfo.msg_namelen = sizeof(sSrcAddress);
            sMsgInfo.msg_iov = &sIO;
            sMsgInfo.msg_iovlen = 1;
            sMsgInfo.msg_control = acOutMsgControl;
            sMsgInfo.msg_controllen = sizeof(acOutMsgControl);

            psControlMessage = CMSG_FIRSTHDR(&sMsgInfo);

            psControlMessage->cmsg_level = IPPROTO_IPV6;
            psControlMessage->cmsg_type = IPV6_PKTINFO;
            memcpy(CMSG_DATA(psControlMessage), psInPacketInfo, sizeof(struct in6_pktinfo));
            psControlMessage->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
            sMsgInfo.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

            if (sendmsg(psNetworkContext->iSocket, &sMsgInfo, 0) != iOutLen)
            {
                DBG_vPrintf(DBG_NETWORK, "%s: Could not send response message (%s) ", __FUNCTION__, strerror(errno));
            }
        }
    }
    
    DBG_vPrintf(DBG_NETWORK, "%s: exit\n", __FUNCTION__);

    /* Return from thread clearing resources */
    eThreadFinish(psThreadInfo);
    return NULL;
}


static teNetworkStatus Network_ServerExchange(tsJIP_Context* psJIP_Context, tsNode *psNode, tsJIPAddress *psSrcAddress, tsJIPAddress *psDstAddress,
                                        char *pcReceiveData, unsigned int iReceiveDataLength,
                                        const char *pcSendData, unsigned int *piSendDataLength)
{
    teJIP_Status    eStatus;
    tsJIP_MsgHeader *psReceiveHeader;
    tsJIP_MsgHeader *psSendHeader;

    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    /* We got a packet - see if it matches what we expect */
    psReceiveHeader = (tsJIP_MsgHeader *)pcReceiveData; 
    psSendHeader    = (tsJIP_MsgHeader *)pcSendData; 

    if (psReceiveHeader->u8Version != JIP_VERSION)
    {
        /* Check version */
        DBG_vPrintf(DBG_NETWORK, "Version mismatch!\n");
        return E_NETWORK_ERROR_FAILED;
    }

    DBG_vPrintf(DBG_NETWORK, "Packet OK\n");
    
    
    eStatus = eJIPserver_HandlePacket(psJIP_Context, psNode, psSrcAddress, psDstAddress,
                                     psReceiveHeader->eCommand, (uint8_t *)pcReceiveData, iReceiveDataLength,
                                     &psSendHeader->eCommand,  (uint8_t *)pcSendData, piSendDataLength);
    
    if (eStatus == E_JIP_OK)
    {
        /* Send a response */
        
        /* Set up outgoing message header */
        psSendHeader->u8Version = JIP_VERSION;
        psSendHeader->u8Handle = psReceiveHeader->u8Handle;
        
        return E_NETWORK_OK;
    }
    
    /* No data to send back */
    *piSendDataLength = 0;

    return E_NETWORK_ERROR_FAILED;
}



teNetworkStatus Network_Recieve(tsNetworkContext *psNetworkContext, uint32_t u32Timeout, tsJIPAddress *psAddress, char *pcData, unsigned int *iDataLength)
{
    tsReceivedPacket *psReceivedPacket;
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    
    if (eQueueDequeueTimed(&psNetworkContext->sSocketQueue, u32Timeout, (void **)&psReceivedPacket) != E_QUEUE_OK)
    {
        DBG_vPrintf(DBG_NETWORK, "Packet not received\n");
        return E_NETWORK_ERROR_TIMEOUT;
    }
    else
    {
        const char acDefaultAddress[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        if (memcmp(acDefaultAddress, &psAddress->sin6_addr, sizeof(struct in6_addr)) != 0)
        {
            if (memcmp(&psAddress->sin6_addr, &psReceivedPacket->sRecv_addr.sin6_addr, sizeof(struct in6_addr)) != 0)
            {
                DBG_vPrintf(DBG_NETWORK, "  Packet from wrong source, expected: ");
                
                DBG_vPrintf_IPv6Address(DBG_NETWORK, psAddress->sin6_addr);
                
                DBG_vPrintf(DBG_NETWORK, "  got: ");
                DBG_vPrintf_IPv6Address(DBG_NETWORK, psReceivedPacket->sRecv_addr.sin6_addr);
                
                /* Not from the right place */
                return E_NETWORK_ERROR_FAILED;
            }
        }
        else
        {
            /* Change the address to the real one */
            memcpy(&psAddress->sin6_addr, &psReceivedPacket->sRecv_addr.sin6_addr, sizeof(struct in6_addr));
            memcpy(&psNetworkContext->sBorder_Router_IPv6_Address.sin6_addr, &psReceivedPacket->sRecv_addr.sin6_addr, sizeof(struct in6_addr));
            memcpy(&psNetworkContext->u64IPv6Prefix, &psReceivedPacket->sRecv_addr.sin6_addr, sizeof(uint64_t));
        }
        
        DBG_vPrintf(DBG_NETWORK, "  Packet OK");
        
        /* Copy the data from the queue, and free the structure */
        *iDataLength = psReceivedPacket->iBytesRecieved;
        memcpy(pcData, psReceivedPacket->acBuffer, psReceivedPacket->iBytesRecieved);
        free(psReceivedPacket);
    
        return E_NETWORK_OK;
    }
}


teNetworkStatus Network_ExchangeJIP(tsNetworkContext *psNetworkContext, tsNode *psNode, uint32_t u32Retries, uint32_t u32Flags,
                                     teJIP_Command eSendCommand, const char *pcSendData, int iSendDataLength, 
                                     teJIP_Command eReceiveCommand, char *pcReceiveData, unsigned int *piReceiveDataLength)
{
    teNetworkStatus eStatus;
    uint32_t i;
    tsJIP_MsgHeader *psSendHeader;
    tsJIP_MsgHeader *psReceiveHeader;
    static uint8_t u8Handle = 0;
    uint8_t u8MatchHandle = 0;
    
    uint32_t u32Timeout;
        
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    psSendHeader = (tsJIP_MsgHeader *)pcSendData;
    
    psSendHeader->u8Version = JIP_VERSION;
    psSendHeader->eCommand  = eSendCommand;
    
    u8Handle = (u8Handle + 1) & 0x7f;
    psSendHeader->u8Handle  = u8Handle;

    if (u32Flags & EXCHANGE_FLAG_STAY_AWAKE)
    {
        DBG_vPrintf(DBG_NETWORK, "Setting stay awake bit\n");
        psSendHeader->u8Handle |= 0x80;
    }
    
    /* Get a copy of the handle to match responses against */
    u8MatchHandle = psSendHeader->u8Handle;
    
    // Most significant bit of device ID marks a node as a sleeping device
    if (psNode->u32DeviceId & 0x80000000)
    {
        u32Timeout = JIP_CLIENT_TIMEOUT_SLEEPING;
    }
    else
    {
        u32Timeout = JIP_CLIENT_TIMEOUT_POWERED;
    }
    DBG_vPrintf(DBG_NETWORK, "Timeout set to %d\n", u32Timeout);
    
    for (i = 0; i < u32Retries; i++)
    {
        if((eStatus = Network_Send(psNetworkContext, &psNode->sNode_Address, pcSendData, 
            iSendDataLength)) != E_NETWORK_OK)
        {
            DBG_vPrintf(DBG_NETWORK, "Error sending data (%d) on attempt %d\n", eStatus, i);
            continue;
        }
        
        while (eStatus == E_NETWORK_OK)
        {
            eStatus = Network_Recieve(psNetworkContext, u32Timeout, &psNode->sNode_Address, pcReceiveData, piReceiveDataLength);
            if (eStatus == E_NETWORK_OK)
            {
                /* We got a packet - see if it matches what we expect */
                psReceiveHeader = (tsJIP_MsgHeader *)pcReceiveData; 
            
                if (psReceiveHeader->u8Version != JIP_VERSION)
                {
                    /* Check version */
                    DBG_vPrintf(DBG_NETWORK, "Version mismatch!\n");
                    continue;
                }
                
                if (psReceiveHeader->eCommand != eReceiveCommand)
                {
                    DBG_vPrintf(DBG_NETWORK, "Unexpected response!\n");
                    continue;
                }
                
                if (psReceiveHeader->u8Handle != u8MatchHandle)
                {
                    /* Check handle matches */
                    DBG_vPrintf(DBG_NETWORK, "Handle mismatch!\n");
                    continue;
                }

                DBG_vPrintf(DBG_NETWORK, "Packet OK\n");
                /* Return the packet */
                return E_NETWORK_OK;
            }
            else
            {
                /* No packet received,or an error occured - retransmit */
                break; /* From the while loop */
            }
        }
    }

    return E_NETWORK_ERROR_TIMEOUT;
}

teNetworkStatus Network_SendJIP(tsNetworkContext *psNetworkContext, tsJIPAddress *psAddress,
                                     teJIP_Command eCommand, const char *pcSendData, int iDataLength)
{
    tsJIP_MsgHeader *psSendHeader;
        
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);
    
    psSendHeader = (tsJIP_MsgHeader *)pcSendData;
    
    psSendHeader->u8Version = JIP_VERSION;
    psSendHeader->eCommand  = eCommand;
    psSendHeader->u8Handle  = rand() * 255; /* Probably not the best way of generating a handle ever */
    
    return Network_Send(psNetworkContext, psAddress, pcSendData, iDataLength);
}



#if defined USE_INTERNAL_BYTESWAP_64

uint64_t bswap_64(uint64_t x)
{
    uint64_t y;
    
    y = ((((uint64_t)__bswap_32(x) & 0x00000000FFFFFFFFL) << 32) | (((uint64_t)__bswap_32(x >> 32) & 0x00000000FFFFFFFFL)));
    
    return y;
}

#endif /* _UCLIBC_ */


static int iNetwork_NodeInterface(tsNode *psNode)
{
    struct ifaddrs *ifp, *ifs;
    int iInterfaceIndex = -1;
    char acAddr[INET6_ADDRSTRLEN] = "Could not determine address\n";
    
    DBG_vPrintf(DBG_FUNCTION_CALLS, "%s\n", __FUNCTION__);

    if (getifaddrs(&ifs) < 0) 
    {
        DBG_vPrintf(DBG_NETWORK, "%s: getifaddrs failed (%s)\n", __FUNCTION__, strerror(errno));
        return -1;
    }

    for (ifp = ifs; ifp; ifp = ifp->ifa_next)
    {
        if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_INET6)
        {
            DBG_vPrintf(DBG_NETWORK, "Interface %5s: [%s]\n", ifp->ifa_name,
                inet_ntop(AF_INET6, &((struct sockaddr_in6 *)(ifp->ifa_addr))->sin6_addr, acAddr, INET6_ADDRSTRLEN));
            
            if (memcmp(&psNode->sNode_Address.sin6_addr,
                       &((struct sockaddr_in6 *)(ifp->ifa_addr))->sin6_addr,
                       sizeof(struct in6_addr)) == 0)
            {
                DBG_vPrintf(DBG_NETWORK, "%s: Found matching interface: %s\n", __FUNCTION__, ifp->ifa_name);
                iInterfaceIndex = if_nametoindex(ifp->ifa_name);
                break;
            }
        }
    }

    freeifaddrs(ifs);
    
    return iInterfaceIndex;
}

