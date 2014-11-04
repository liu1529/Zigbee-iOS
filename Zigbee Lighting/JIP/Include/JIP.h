/****************************************************************************
 *
 * MODULE:             libJIP
 *
 * COMPONENT:          JIP.h
 *
 * REVISION:           $Revision: 56155 $
 *
 * DATED:              $Date: 2013-08-22 11:18:50 +0100 (Thu, 22 Aug 2013) $
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

/** \mainpage libJIP Documentation
 *
 *  \section intro_sec Introduction
 *
 *  libJIP provides a C API for accessing the JIP service of JenNet-IP wireless nodes
 *  in a IEEE802.15.4 network.
 *  JIP is a UDP protocol on IPv6. IPv6 is used due to the huge address space that is
 *  available for the wireless nodes.
 *  JIP is a self-describing protocol, supporting automatic discovery both of the nodes
 *  that exist in a network, and of the services that they support, via their MiBs and Variables.
 *
 *  \section usage_sec Usage
 * 
 *  This section shows the stages necessary to discover a JIP network of nodes, read every 
 *  variable in the network and then print the results.
 *
 *  \subsection step1 Step 1: Initialise libJIP and JIP context
 *  \code
    tsJIP_Context sJIP_Context;
    if (eJIP_Init(&sJIP_Context, E_JIP_CONTEXT_CLIENT) != E_JIP_OK)
    {
        printf("JIP startup failed\n");
        exit (-1);
    }
    \endcode
 * 
 *  \subsection step2 Step 2: Connect to IPv6 address of network gateway.
 *  \code
    if (eJIP_Connect(&sJIP_Context, pcBorderRouterIPv6Address, JIP_DEFAULT_PORT) != E_JIP_OK)
    {
        printf("JIP connect failed\n");
        exit (-1);
    }
    \endcode
 *
 *  \subsection step3 Step 3: Discover network contents.
 *  \code
    if (eJIP_DiscoverNetwork(&sJIP_Context) != E_JIP_OK)
    {
        printf("JIP discover network failed\n");
        exit (-1);
    }
    \endcode
 *
 * \subsection step4 Step 4: Iterate accross the nodes in the network.
 * First the network context must be locked to prevent any other thread modifying it.
 *  \code
    tsNode *psNode;
    tsJIPAddress   *NodeAddressList = NULL;
    uint32_t        u32NumNodes = 0;
    
    printf("Reading Network: \n");
    
    if (eJIP_GetNodeAddressList(&sJIP_Context, &NodeAddressList, &u32NumNodes) != E_JIP_OK)
    {
        printf("Error reading node list\n");
        exit(0);
    }
    
    for (NodeIndex = 0; NodeIndex < u32NumNodes; NodeIndex++)
    {
        char buffer[INET6_ADDRSTRLEN] = "Could not determine address\n";
        
        psNode = psJIP_LookupNode(&sJIP_Context, &NodeAddressList[NodeIndex]);
        if (!psNode)
        {
            printf("Node has been removed\n");
            continue;
        }
        
        inet_ntop(AF_INET6, &psNode->sNode_Address.sin6_addr, addr_buffer, INET6_ADDRSTRLEN);
        
        printf("  Reading Node: %s", addr_buffer);
        
        ...
        
        eJIP_UnlockNode(psNode);
    }
    free(NodeAddressList);
    \endcode
 *
 * \subsection step5 Step 5: Iterate accross the MiBs of the node
 *  \code
        tsMib *psMib;

        psMib = psNode->psMibs;
        while (psMib)
        {
            printf("    Reading Mib: %s\n", psMib->pcName);
            ...
            psMib = psMib->psNext;
        }
    \endcode
 *
 * \subsection step6 Step 6: Iterate accross the variables of the MiB
 *  \code
                psVar = psMib->psVars;
                while (psVar)
                {
                    printf("      Reading Var: %s\n", psVar->pcName);
                    ...
                    psVar = psVar->psNext;
                }
    \endcode
 *
 * \subsection step7 Step 7: Read the variable
 *  \code
                    if (eJIP_GetVar(&sJIP_Context, psVar) != E_JIP_OK)
                    {
                        printf("      Error reading Var: %s\n", psVar->pcName);
                    }
    \endcode
 *
 * \subsection step8 Step 8: Print the network context.
 *  \code
    if (eJIP_PrintNetworkContent(&sJIP_Context) != E_JIP_OK)
    {
        printf("Error displaying network contents.\n");
    }
    \endcode
 * 
 * 
 * Copyright Jennic Ltd 2011. All rights reserved
 */



#include <netinet/in.h>


#ifndef __JIP_H__
#define __JIP_H__

/** Default port number for JIP service */
#define JIP_DEFAULT_PORT 1873

/* Define some useful convenience macros. */
#define STRING(a) stringize(a)
#define stringize(s) #s

#define PACK __attribute__((packed))


/** \ingroup Convenience 
 * Special device ID for all devices.
 * When passed to \ref eJIP_GetNodeAddressList,
 * all devices in the network are returned.
 */
#define JIP_DEVICEID_ALL          (0xFFFFFFFF)


/** Typedef for boolean values */
typedef enum 
{
    False = 0,
    True  = 1
} bool_t;


/** Typedef for different liJIP context types */
typedef enum _eJIP_Context
{
    E_JIP_CONTEXT_CLIENT,                       /**< libJIP operating in client mode */
    E_JIP_CONTEXT_SERVER,                       /**< libJIP operating in server mode */
} PACK teJIP_ContextType;


/** Enumerated return type for libJIP.
 *  These are also the return codes sent by the nodes
 */
typedef enum _eJIP_Status
{
    /* These statuses are used in the UDP protocol */
    E_JIP_OK                        = 0x00,     /**< Success */
    E_JIP_ERROR_TIMEOUT             = 0x7f,     /**< Request timed out */
    E_JIP_ERROR_BAD_MIB_INDEX       = 0x8f,     /**< MiB index does not exist */
    E_JIP_ERROR_BAD_VAR_INDEX       = 0x9f,     /**< Variable index does not exist */
    E_JIP_ERROR_NO_ACCESS           = 0xaf,     /**< Security failure */
    E_JIP_ERROR_BAD_BUFFER_SIZE     = 0xbf,     /**< Buffer is not big enough */
    E_JIP_ERROR_WRONG_TYPE          = 0xcf,     /**< Type mismatch */
    E_JIP_ERROR_BAD_VALUE           = 0xdf,     /**< Value not appropriate for type */
    E_JIP_ERROR_DISABLED            = 0xef,     /**< Access is disabled */
    E_JIP_ERROR_FAILED              = 0xff,     /**< Generic failure */
    
    /* These statuses are used by libJIP */
    E_JIP_ERROR_BAD_DEVICE_ID       = 0x11,     /**< Bad device ID was passed */
    E_JIP_ERROR_NETWORK             = 0x12,     /**< Network error */
    E_JIP_ERROR_WOULD_BLOCK         = 0x13,     /**< The operation would block the current thread */
    E_JIP_ERROR_NO_MEM              = 0x14,     /**< Memory allocation failed */
    E_JIP_ERROR_WRONG_CONTEXT       = 0x15,     /**< A function was called that is inappropriate for the context type */
} PACK teJIP_Status;


/** Enumerated type of variable types supported by JIP */
typedef enum _eJIP_VarType
{
#define E_JIP_VAR_TYPE_TABLE 64                 /**< Offset of table types */
    E_JIP_VAR_TYPE_INT8,                        /**< Signed 8 bit integer */
    E_JIP_VAR_TYPE_INT16,                       /**< Signed 16 bit integer */
    E_JIP_VAR_TYPE_INT32,                       /**< Signed 32 bit integer */
    E_JIP_VAR_TYPE_INT64,                       /**< Signed 64 bit integer */
    E_JIP_VAR_TYPE_UINT8,                       /**< Unsigned 8 bit integer */
    E_JIP_VAR_TYPE_UINT16,                      /**< Unsigned 16 bit integer */
    E_JIP_VAR_TYPE_UINT32,                      /**< Unsigned 32 bit integer */
    E_JIP_VAR_TYPE_UINT64,                      /**< Unsigned 64 bit integer */
    E_JIP_VAR_TYPE_FLT,                         /**< 32 bit float (IEEE standard) */
    E_JIP_VAR_TYPE_DBL,                         /**< 64 bit double (IEEE standard) */
    E_JIP_VAR_TYPE_STR,                         /**< String */
    E_JIP_VAR_TYPE_BLOB,                        /**< Binary object */

    E_JIP_VAR_TYPE_TABLE_BLOB = E_JIP_VAR_TYPE_TABLE + E_JIP_VAR_TYPE_BLOB,
                                                /**< Table of Binary objects */
#undef E_JIP_VAR_TYPE_TABLE
} PACK teJIP_VarType;


/** Enumerates type of access types to variables */
typedef enum _eJIP_AccessType
{
    E_JIP_ACCESS_TYPE_CONST,                    /**< Variable contains constant data */
    E_JIP_ACCESS_TYPE_READ_ONLY,                /**< Variable contains read only data */
    E_JIP_ACCESS_TYPE_READ_WRITE,               /**< Variable contains read/write data */
} PACK teJIP_AccessType;


/** Enumerated types of variable level security */
typedef enum _eJIP_Security
{
    E_JIP_SECURITY_NONE,                        /**< Variable has no security */
} PACK teJIP_Security;


/** Enumerated types of variable enabled/disabled */
typedef enum _eJIP_VarEnable
{
    E_JIP_VAR_DISABLED,                         /**< Variable is disabled */
    E_JIP_VAR_ENABLED,                          /**< Variable is enabled */
} PACK teJIP_VarEnable;


/** Enumerated type of network change events */
typedef enum
{
    E_JIP_NODE_JOIN    = 0,                     /**< Node joined network */
    E_JIP_NODE_LEAVE   = 1,                     /**< Node left network */
    E_JIP_NODE_MOVE    = 2,                     /**< Node moved in network */
} teJIP_NetworkChangeEvent;


/** Typedef a JIP address to be a IPv6 structure */
typedef struct sockaddr_in6 tsJIPAddress;


/** Structure representing a table row */
typedef struct _tsTableRow
{
    uint32_t u32Length;                         /**< Length of the row (number of bytes) */
    union
    {
        uint8_t* pbData;                        /**< Row data for \ref E_JIP_VAR_TYPE_TABLE_BLOB variables */
        void*    pvData;                        /**< Universal row data pointer. It should be cast to the right type */
    };                                          /**< Pointer to the row */
} tsTableRow;


/** Structure representing a table's data.
 *  The pvData pointer of a \ref tsVar points to this structure.
 */
typedef struct
{
    uint32_t u32NumRows;                        /**< Number of rows in the table */
    tsTableRow *psRows;                         /**< Array of Pointers to each row */
} tsTable;


/** Structure to represent a lock(mutex) used on data structures 
 *  \ingroup Locks
 */
typedef struct
{
    void *pvPriv;                               /**< Implementation specific private structure */
} tsLock;


/* Forward declarations of structures */
struct _tsVar;
struct _tsMib;
struct _tsNode;
struct _tsNetwork;
struct _tsJIP_Context;


/** Server callback function for when a variable is being read by a client.
 *  \param psVar        Pointer to the variable being read
 *  \return E_JIP_OK if request was successful
 */
typedef teJIP_Status (*tprCbVarGet)(struct _tsVar *psVar);

/** Server callback function for when a variable is being set by a client.
 *  \param psVar        Pointer to the variable being updated
 *  \param psMulticastAddress    If this was a multicast set request, this contains a 
 *                      pointer to IPv6 address that was the destination of the request.
 *                      Otherwise, this is NULL.
 *  \return E_JIP_OK if update was successful
 */
typedef teJIP_Status (*tprCbVarSet)(struct _tsVar *psVar, tsJIPAddress *psMulticastAddress);


/** Function prototype for a trap callback
 *  \ingroup Traps
 *  The application provides functions with this prototype to be called
 *  when the status of network node variables change.
 *  The function is called in the context of a "Trap" thread. The application must
 *  ensure that this callback function is thread safe from the main application thread.
 *  Every trap notification is handled in its own thread context.
 *  The callback function is called with the node structure relating to this variable already locked with 
 *  \ref eJIP_LockNode.
 *  A single trap callback function can be registered for every variable
 *  They are registered using \ref  eJIP_TrapVar
 *  \param psVar        Pointer to variable that has changed.
 *  \return none
 */
typedef void (*tprCbVarTrap)(struct _tsVar *psVar);


/** Function prototype for network monitoring.
 *  \ingroup NetworkDiscovery
 *  The application provides a function with this prototype to be called
 *  when a node joins, leaves or moves in the network.
 *  The function is called in the context of a "Network monitor" thread. The application must
 *  ensure that this callback function is thread safe from the main application thread.
 *  The callback function is called with the JIP context locked with \ref eJIP_Lock. 
 *  The callback function is called with the Node locked with \ref eJIP_LockNode. 
 *  The application may lock the context again, but must not unlock the context more times that it locks it.
 *  A single application callback function for the instance of libJIP can be registered
 *  using \ref eJIPService_MonitorNetwork
 *  \param eEvent           What event has occured
 *  \param psNode           The Node that this event refers to
 *  \return none
 */
typedef void (*tprCbNetworkChange)(teJIP_NetworkChangeEvent eEvent, struct _tsNode *psNode);


/** Structure representing a JIP variable 
 *  The variables are held as a linked list from a \ref tsMib structure.
 */
typedef struct _tsVar
{
    char*                   pcName;             /**< Name of the variable */
    uint8_t                 u8Index;            /**< Index of the variable within it's MiB */
    uint8_t                 u8Size;             /**< Used for Blobs - Number of bytes of data */
    
    teJIP_VarEnable         eEnable;            /**< Determines if the variable is disabled */
    
    teJIP_VarType           eVarType;           /**< Type of variable */
    teJIP_AccessType        eAccessType;        /**< Access type of the variable (const/read/write/etc) */
    teJIP_Security          eSecurity;          /**< Security type of the variable (none/etc) */
    
    union
    {
        int8_t*             pi8Data;            /**< Data for \ref E_JIP_VAR_TYPE_INT8 variables */
        int16_t*            pi16Data;           /**< Data for \ref E_JIP_VAR_TYPE_INT16 variables */
        int32_t*            pi32Data;           /**< Data for \ref E_JIP_VAR_TYPE_INT32 variables */
        int64_t*            pi64Data;           /**< Data for \ref E_JIP_VAR_TYPE_INT64 variables */
        uint8_t*            pu8Data;            /**< Data for \ref E_JIP_VAR_TYPE_UINT8 variables */
        uint16_t*           pu16Data;           /**< Data for \ref E_JIP_VAR_TYPE_UINT16 variables */
        uint32_t*           pu32Data;           /**< Data for \ref E_JIP_VAR_TYPE_UINT32 variables */
        uint64_t*           pu64Data;           /**< Data for \ref E_JIP_VAR_TYPE_UINT64 variables */
        float*              pfData;             /**< Data for \ref E_JIP_VAR_TYPE_FLT variables */
        double*             pdData;             /**< Data for \ref E_JIP_VAR_TYPE_DBL variables */
        char*               pcData;             /**< Data for \ref E_JIP_VAR_TYPE_STR variables */
        uint8_t*            pbData;             /**< Data for \ref E_JIP_VAR_TYPE_BLOB variables */
        tsTable*            ptData;             /**< Data for \ref E_JIP_VAR_TYPE_TABLE_BLOB variables */
        void*               pvData;             /**< Universal data pointer. It should be cast to the right type */
    };                                          /**< Pointer to the actual data. 
                                                 * The member accessed is dependant upon \ref eVarType
                                                 *
                                                 * When operating in CLIENT mode,
                                                 * This is NULL until the variable is read using \ref eJIP_GetVar
                                                 * 
                                                 * When operating in SERVER mode, 
                                                 * this may be set to point at the contents of the variable. 
                                                 * Optionally, the \ref prCbVarGet and \ref prCbVarSet callbacks 
                                                 * may be used to populate the variable with data on request.
                                                 */
    
    tprCbVarGet             prCbVarGet;         /**< Function to be called upon a get. The function should set the pvData
                                                 * pointer in the \ref tsVar stucture with the latest data. This data will
                                                 * be returned to the client.
                                                 * The callback may be left as NULL if the application will supply data in the
                                                 * pvData pointer. If both are NULL, the variable will be set to DISABLED.
                                                 */
    tprCbVarSet             prCbVarSet;         /**< Function to be called upon a set. The data from the client has been set
                                                 * in pvData in the \ref tsVar structure.
                                                 * The callback may be left as NULL if the application does not wish to be 
                                                 * notified of sets on the variable.
                                                 */
    
    uint8_t                 u8TrapHandle;       /**< A handle value associated with traps from this variable */
    tprCbVarTrap            prCbVarTrap;        /**< Registered Trap callback function. 
                                                 * Traps are registered using \ref eJIP_TrapVar
                                                 */
    
    struct _tsMib*          psOwnerMib;         /**< Pointer to the owner MiB of this variable */
    struct _tsVar*          psNext;             /**< Pointer to the next variable in the linked list */
} tsVar;


/** Structure representing a JIP MiB 
 *  The MiBs are held as a linked list from a \ref tsNode structure.
 */
typedef struct _tsMib
{
    char*                   pcName;             /**< Name of the MiB */
    uint32_t                u32MibId;           /**< Identifier of this MiB type */
    uint8_t                 u8Index;            /**< Index of this MiB */

    uint32_t                u32NumVars;         /**< The number of variables that this MiB has */
    tsVar*                  psVars;             /**< Pointer to linked list of \ref tsVar variables */
    
    struct _tsNode*         psOwnerNode;        /**< Pointer to the owner node of this MiB */
    struct _tsMib*          psNext;             /**< Pointer to the next MiB in the linked list */
} tsMib;


/** Structure representing a JIP Node 
 *  The Nodes are held as a linked list from a \ref tsNetwork structure.
 */
typedef struct _tsNode
{
    tsJIPAddress            sNode_Address;      /**< JIP Address (IPv6 address and port number of the JIP service on this node) */
    
    uint32_t                u32DeviceId;        /**< 32 Bit device ID of the node. Can be used to identify Nodes of the same type,
                                                 * so that member MiBs and Variables do not have to be queried for every node.
                                                 */
                                                 
    uint32_t                u32NumMibs;         /**< The number of MiBs that this node has */
    tsMib*                  psMibs;             /**< Pointer to linked list of \ref tsMib MiBs */
    
    tsLock                  sLock;              /**< Mutex to protect this node */
    
    void*                   pvPriv;             /**< Pointer to private data */
    
    struct _tsNetwork*      psOwnerNetwork;     /**< Pointer to the owner network of this node */
    struct _tsNode*         psNext;             /**< Pointer to the next node in the linked list */
} tsNode;


/** Structure representing a JIP Network */
typedef struct _tsNetwork
{
    uint32_t                u32NumNodes;        /**< The number of nodes that this network has */
    
    struct _tsJIP_Context*  psOwnerContext;     /**< Pointer to the owner JIP context for this network */
    tsNode*                 psNodes;            /**< Pointer to linked list of \ref tsNode nodes */
} tsNetwork;


/** JIP Context structure. This is passed to most JIP functions */
typedef struct _tsJIP_Context
{
    tsNetwork               sNetwork;           /**< The JIP Contexts network */
    void *pvPriv;                               /**< Pointer to internal private data */
    
    /* User configurable parameters */
    int                     iMulticastInterface;/**< Index of the interface from which to send multicasts.
                                                     This may be changed for each call to \ref eJIP_MulticastSetVar.
                                                     The socket option IPV6_MULTICAST_IF is set with this value before
                                                     Each multicast set request is sent. Changing this value each call
                                                     Allows an application to multicast on multiple interfaces if required.
                                                     The default value is the index of the "tun0" interface, or 0 for the 
                                                     default interface. */
    int                     iMulticastSendCount;/**< The number of times to send each multicast set request.
                                                     The default is 2 to send each request twice. */
    
    
} tsJIP_Context;


/** Version string for libJIP */
extern const char *JIP_Version;


/** \defgroup libJIPManagement libJIP Management
 *  These are functions for managing the library, including initialising the library (\ref eJIP_Init)
 *  and freeing up resources (\ref eJIP_Destroy).
 *  Once the library has been initialised, it must be connected to the network gateway. libJIP supports
 *  a direct IPv6 connection, via \ref eJIP_Connect, or an IPv4 connection to a JIPv4 proxy which then converts
 *  the IPv4 traffic to IPv6 datagrams. This is established using \ref eJIP_Connect4.
 * @{ */


/** Initialise the JIP library.
 *  Sets up a new context ready for use
 *  \param psJIP_Context        Pointer to JIP Context
 *  \param eJIP_ContextType     Type of context to initialise (client or server)
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_Init(tsJIP_Context *psJIP_Context, teJIP_ContextType eJIP_ContextType);


/** Finish with the JIP library.
 *  Closes all connections
 *  Free's all memory associated with the context
 *  Sets up a new context ready for use
 *  \param psJIP_Context        Pointer to JIP Context
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_Destroy(tsJIP_Context *psJIP_Context);


/** Function to return a string representing a libJIP status
 *  \param eStatus              Status to convert
 *  \return String constant
 */
const char *pcJIP_strerror(teJIP_Status eStatus);


/** @} */


/** \defgroup client Client Context functions
 *  These are functions for operating the library in client mode.
 *  The library should first be initialised using \ref eJIP_Init and \ref E_JIP_CONTEXT_CLIENT.
 *  Once the library has been initialised, it must be connected to the network gateway. libJIP supports
 *  a direct IPv6 connection, via \ref eJIP_Connect, or an IPv4 connection to a JIPv4 proxy which then converts
 *  the IPv4 traffic to IPv6 datagrams. This is established using \ref eJIP_Connect4.
 * @{ */


/** Connect libJIP to a network gateway.
 *  Sets up a connected socket from which to issue requests to a network of JIP nodes.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param pcAddress            String representing the hostname / IPv6 address to connect to. This will be the IPv6 address of network gateway.
 *  \param iPort                Port number to conenct to. Usually \ref JIP_DEFAULT_PORT
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_Connect(tsJIP_Context *psJIP_Context, const char *pcAddress, const int iPort);


/** Connect libJIP to a network gateway using IPv4.
 *  This requires a listening JIP4 proxy at the remote end of the connection to pass on the IPv6 packets.
 *  The IPv6 addresses of Nodes are still used, but passed via an IPv4 tunnel to the remote JIP4 proxy.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param pcIPv4Address        String representing the hostname / IPv4 address to connect to. This will be the IPv4 address of the JIP4 gateway.
 *  \param pcIPv6Address        String representing the hostname / IPv6 address to connect to. This will be the IPv6 address of network gateway.
 *  \param iPort                Port number to conenct to. Usually \ref JIP_DEFAULT_PORT
 *  \param bTCP                 If True, connect using a TCP stream. Otherwise use UDP datagrams as normal.
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_Connect4(tsJIP_Context *psJIP_Context, const char *pcIPv4Address, const char *pcIPv6Address, const int iPort, const bool_t bTCP);



/** Join an IPv6 multicast group so that we can receive multicast trap notifies from a node sending to that group.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param pcAddress            String representing IPv6 multicast address.
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_GroupJoin(tsJIP_Context *psJIP_Context, const char *pcAddress);


/** Leave an IPv6 multicast group so that we no longer receive multicast trap notifies from a node sending to that group.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param pcAddress            String representing IPv6 multicast address.
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_GroupLeave(tsJIP_Context *psJIP_Context, const char *pcAddress);


/** @} */


/** \defgroup NetworkDiscovery Network Discovery
 *  JIP is a self-describing protocol. In order to take advantage of the interfaces offered by nodes
 *  in the network, first the list of nodes must be discovered, and then the interfaces (exposed via
 *  MiBs and variables) that each node supports can be discovered. This is done via a call to 
 *  \ref eJIPService_DiscoverNetwork.
 *  Discovering the network populates the \ref tsJIP_Context with a snapshot in time of the network
 *  at the time this was done. libJIP also supports background monitoring of the network, via
 *  \ref eJIPService_MonitorNetwork. This function creates a new thread which monitors the network, and
 *  calls the user supplied callback function \ref tprCbNetworkChange when a change is detected.
 * @{ */


/** Discover the network that is attached to the network gateway.
 *  The network gateway IPV6 address has already been set up using \ref eJIP_Connect.
 *  This function then populates the \ref tsNetwork of psJIP_Context with 
 *  all of the nodes in the network. It also populates each of the \ref tsNode structures 
 *  with the \ref tsMib and \ref tsVar structures. After calling this function, psJIP_Context
 *  has a full description of the network, it's nodes and services.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIPService_DiscoverNetwork(tsJIP_Context *psJIP_Context);


/** Request that libJIP begin monitoring the network. It will spawn a new thread, the "Network monitor" thread.
 *  This thread will notify the application of changes in the network by calling prCbNetworkChange. The function
 *  is called in this threads context.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param prCbNetworkChange    Callback function to call on network change
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIPService_MonitorNetwork(tsJIP_Context *psJIP_Context, tprCbNetworkChange prCbNetworkChange);


/** Request that libJIP stop monitoring the network. It will destory the "Network monitor" thread.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIPService_MonitorNetworkStop(tsJIP_Context *psJIP_Context);

/* @} */


/** \defgroup ManipulatingVariables Manipulating variables
 *  libJIP allows the application to read the variables in the network
 *  using \ref eJIP_GetVar. Before calling this function for the first time, the pvData pointer
 *  of a \ref tsVar is NULL. After reading, it is allocated to a size appropriate for the data
 *  returned from the node.
 *  If the variable has a \ref teJIP_AccessType which allows the data to be updated, then this can 
 *  be done via unicast to an individual node using \ref eJIP_SetVar. If the application wishes
 *  to update the same variable on many nodes simultaneously, this can be done with an IPv6 multicast
 *  using \ref eJIP_MulticastSetVar.
 * @{ */


/** Read a variable. A request is made to the node for the data content of this variable.
 *  The pvData member of psVar is allocated and filled with the data.
 *  This is only supported in CLIENT mode.
 *  \param psJIP_Context        Pointer to the JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param psVar                Pointer to the variable to read
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_GetVar(tsJIP_Context *psJIP_Context, tsVar *psVar);


/** Sets a variable. In CLIENT mode, a request is made to the node to update the data content of this variable.
 *  If the request succeeds, the pvData member of psVar is allocated and filled with the request data. This
 *  means that the local data is kept in sync with the remote node data.
 *  In SERVER mode, this function just updates the local data of the variable.
 *  Sets of E_JIP_VAR_TYPE_TABLE_BLOB are only suppoted in SERVER mode, when pvNewData should point to a tsTable
 *  structure that can be copied to the variables pvData.
 *  \param psJIP_Context        Pointer to the JIP Context
 *  \param psVar                Pointer to the variable to set
 *  \param pvNewData            Pointer to the data to set the variable with
 *  \param u32Size              Size of the variable. This should be set correctly for variable sized variable
 *                              types such as strings (strlen) and BLOBS.
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_SetVar(tsJIP_Context *psJIP_Context, tsVar *psVar, void *pvNewData, uint32_t u32Size);


/** Sets a variable using a IPv6 multicast. A request is made to the IPv6 multicast address to update the data content of this variable.
 *  The psVar parameter can be the relevant variable on any node in, or out of, the multicast group. It is used for
 *  all information except the destination IPv6 address, which is contained in psAddress.
 *  All nodes that are members of the IPv6 multicast group will update the data content of this variable. Note that 
 *  multicast messages are by definition non-guaranteed and will not generate any return traffic, including Traps.
 *  Unlike unicast set of a variable with \ref eJIP_SetVar The pvData member of psVar will NOT be updated, 
 *  as there is no guarantee that every node in the group received and acted upon the command.
 *  Sets of E_JIP_VAR_TYPE_TABLE_BLOB are not supported.
 *  \param psJIP_Context        Pointer to the JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param psVar                Pointer to the variable to set
 *  \param pvData               Pointer to the data to set the variable with
 *  \param u32Size              Size of the variable. This should be set correctly for variable sized variable
 *                              types such as strings and BLOBS.
 *  \param psAddress            IPv6 Multicast address to send the request to. Includes IPv6 address and port number.
 *  \param iMaxHops             Sets the maximum number of hops to the network gateway on the IPv6 multicast datagram.
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_MulticastSetVar(tsJIP_Context *psJIP_Context, tsVar *psVar, void *pvData, uint32_t u32Size, tsJIPAddress *psAddress, int iMaxHops);

/* @} */


/** \defgroup Traps Traps
 *  JIP supports an ad-hoc notification system called "Traps". A client application may request 
 *  to be notified when a variable is changed on a node in the network, using \ref eJIP_TrapVar.
 *  The node will then send unsolicited notification messages to the IPv6 address and port of libJIP.
 *  The callback function (\ref tprCbVarTrap) registered along with this call will be called, in a new
 *  thread context that only exists for the duration of handling the trap.
 *  If the application no longer wishes to be notified of updates to a variable, it can request this via
 *  \ref eJIP_UntrapVar.
 * @{ */


/** Request setting up a trap on a variable.
 *  Sends a packet to the node asking it to start sending trap notifications.
 *  If this succeeds, the node will start sending unsolicited messages whenever the variable psVar changes.
 *  libJIP will the call the prCbVarTrap callback function with the new value
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param psVar                Pointer to the variable
 *  \param u8NotificationHandle Handle to add.
 *  \param prCbVarTrap          Callabck function to call.
 *  \return E_JIP_OK trap was set up OK.
 */
teJIP_Status eJIP_TrapVar(tsJIP_Context *psJIP_Context, tsVar *psVar, uint8_t u8NotificationHandle, tprCbVarTrap prCbVarTrap);


/** Request removal of trap on a variable.
 *  Sends a packet to the node asking it to stop sending trap requests.
 *  If this succeeds, it removes the trap function pointer from the variable.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_CLIENT context)
 *  \param psVar                Pointer to the variable
 *  \param u8NotificationHandle Handle to remove.
 *  \return E_JIP_OK trap was removed OK.
 */
teJIP_Status eJIP_UntrapVar(tsJIP_Context *psJIP_Context, tsVar *psVar, uint8_t u8NotificationHandle);

/** @} Traps */


/** \defgroup Persistance Persisting the discovered network
 *  libJIP supports persisting the discovered network accross uses. This avoids carrying out the 
 *  relatively lengthy procedure of discovering the network each time the library is used.
 *  The discovered network comprises two sections, the list of nodes and the definitions of those nodes.
 *  The node definitions should be saved and loaded first using \ref eJIPService_PersistXMLSaveDefinitions and
 *  \ref eJIPService_PersistXMLLoadDefinitions respectively.
 *  Optionally, the list of nodes can be saved and reinstated using \ref eJIPService_PersistXMLSaveNetwork and
 *  \ref eJIPService_PersistXMLLoadNetwork respectively.
 * @{ */

/** Save the current definitions of nodes in the \ref tsNetwork to an xml file. This file can then be reloaded later
 *  using \ref eJIPService_PersistXMLLoadDefinitions to avoid having to rediscover the definitions of nodes in the network.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param pcFileName           Filename of the xml file to save to
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPService_PersistXMLSaveDefinitions(tsJIP_Context *psJIP_Context, const char *pcFileName);


/** Load an xml file previously saved using \ref eJIPService_PersistXMLSaveDefinitions into the \ref tsNetwork member of psJIP_Context
 *  This avoids having to rediscover the network, loading just the member MiB and Variables of DeviceIds so that they are
 *  known when the list of nodes is loaded using \ref eJIPService_PersistXMLLoadNetwork of discovered using
 *  \ref eJIPService_DiscoverNetwork.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param pcFileName           Filename of the xml file to load from
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPService_PersistXMLLoadDefinitions(tsJIP_Context *psJIP_Context, const char *pcFileName);


/** Save the current content of the \ref tsNetwork to an xml file. This file can then be reloaded later
 *  using \ref eJIPService_PersistXMLLoadNetwork to avoid having to rediscover the list of nodes in the network.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param pcFileName           Filename of the xml file to save to
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPService_PersistXMLSaveNetwork(tsJIP_Context *psJIP_Context, const char *pcFileName);


/** Load an xml file previously saved using \ref eJIPService_PersistXMLSaveNetwork into the \ref tsNetwork member of psJIP_Context
 *  This loads just the list of nodes in the network.
 *  Bear in mind that the nodes in the network may have changed in the meantime, so the list may no longer be accurate.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param pcFileName           Filename of the xml file to load from
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPService_PersistXMLLoadNetwork(tsJIP_Context *psJIP_Context, const char *pcFileName);


/** @} */


/** \defgroup Locks libJIP Thread locking
 *  libJIP makes use of several threads. A thread is spawned to monitor the network socket, so that
 *  asynchronous trap notifications can be passed on. Each trap notification is itself handles by a new thread
 *  which only exists for the purpose of handling the trap.
 *  In order to protect the data structures of the library, \ref tsLock stuctures are used.
 *  The JIP context has a single  mutex to proctect the librarys internal structures. It can be locked/unlocked using 
 *  \ref eJIP_Lock / \ref eJIP_Unlock respectively.
 *  Some functions automatically lock the JIP context, to prevent modifications. These include,
 *  \ref tprCbVarTrap and \ref tprCbNetworkChange
 * @{ */


/** Locks the JIP context. A mutex then protects from other threads changing the internal structures.
 *  This function is used internally by some API functions, with \ref eJIP_Unlock to unlock the context.
 *  If locking the context in the application, for example to iterate over the node list, it should be
 *  unlocked again using \ref eJIP_Unlock.
 *  \param psJIP_Context        Pointer to JIP Context
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_Lock(tsJIP_Context *psJIP_Context);


/** Unlocks the JIP context. This allows other threads to change the internal structures.
 *  This function is used internally by some API functions, with \ref eJIP_Lock to lock the context.
 *  The application should only unlock the context if it has previously locked it using \ref eJIP_Lock.
 *  \param psJIP_Context        Pointer to JIP Context
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_Unlock(tsJIP_Context *psJIP_Context);


/** Attempts to lock the node structure. A mutex then prevents other threads from changing any data associated with 
 *  this node. Once the node has been locked ot the thread, it must be unlocked using \ref eJIP_UnlockNode.
 *  If bWait is specified as TRUE, the calling thread is suspended until the lock becomes free.
 *  If bWait is specified as FALSE, and the mutex cannot be locked immediately, the function returns immediately
 *  with status E_JIP_ERROR_WOULD_BLOCK.
 *  \param psNode               Pointer to the node stucture to lock.
 *  \param bWait                TRUE to suspend the thread until the lock can be acquired.
 *  \return E_JIP_OK on success, or E_JIP_ERROR_WOULD_BLOCK,
 */
teJIP_Status eJIP_LockNode(tsNode *psNode, bool_t bWait);


/** Unlocks the node structure. The node structure must have been previously locked to this thread 
 *  using \ref eJIP_LockNode
 *  \param psNode               Pointer to the node structure to lock.
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_UnlockNode(tsNode *psNode);


/** @} */


/** \defgroup Convenience Convenience functions for accessing Nodes/MiBs/Variables
 *  libJIP has some convenience functions to allow an application easy access to
 *  data structures for a Node, MiB and Variable by looking them up by address or name.
 *  There is also a function to dump out the network contents via printf, \ref eJIP_PrintNetworkContent
 * @{ */


/** Get a list of addresses of nodes in the network.
 *  This function returns a snapshot of the addresses of nodes in the network.
 *  The list of nodes is filtered by u32DeviceIdFilter. If this is specified as \ref JIP_DEVICEID_ALL, then
 *  the returned list contains all known devices in the network. If it is specified as a known device type,
 *  then only devices of this type are returned in the list.
 *  ppsAddresses is malloc'd by libJIP to contain as many node addresses as there are nodes in the network.
 *  This pointer should be free'd when the application is done with the list.
 *  pu32NumAddresses is set to the number of nodes in the network.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param u32DeviceIdFilter    Device ID to filter list of nodes with
 *  \param ppsAddresses[out]    Pointer to a pointer that can be malloc'd to contain the node address list
 *  \param pu32NumAddresses     Pointer to a location in which to store the number of nodes in the network
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_GetNodeAddressList(tsJIP_Context *psJIP_Context, const uint32_t u32DeviceIdFilter, tsJIPAddress **ppsAddresses, uint32_t *pu32NumAddresses);


/** Get a pointer to a node, if it exists in the network.
 *  This function internally locks the JIP context. If the requested node is found, it is locked by \ref eJIP_LockNode.
 *  The JIP context is then unlocked and the locked node structure returned. Once the thread is finished with the node, 
 *  it must unlock it via \ref eJIP_UnlockNode.
 *  \param psJIP_Context        Pointer to JIP Context 
 *  \param psAddress            Pointer to IPv6 Address structure
 *  \return NULL if node is unknown, otherwise pointer to it's structure
 */
tsNode *psJIP_LookupNode(tsJIP_Context *psJIP_Context, tsJIPAddress *psAddress);


/** Determine if a node has a MiB with the given name. If it does, a pointer to the MiB is returned.
 *  Otherwise, NULL. The calling thread must hold a lock on the parent node structure via \ref eJIP_LockNode
 *  or \ref psJIP_LookupNode.
 *  \param psNode               Pointer to the node to inspect
 *  \param psStartMib           Pointer to MiB to start at. NULL to start at beginning.
 *                              This can be used to return multiple MiBs with the same name from a node.
 *  \param pcName               Name of the MiB to look for.
 *  \return NULL if psNode has no MiB with the given name, otherwise a pointer to the MiB.
 */
tsMib *psJIP_LookupMib(tsNode *psNode, tsMib *psStartMib, const char *pcName);


/** Determine if a node has a MiB with the given ID. If it does, a pointer to the MiB is returned.
 *  Otherwise, NULL. The calling thread must hold a lock on the parent node structure via \ref eJIP_LockNode
 *  or \ref psJIP_LookupNode.
 *  \param psNode               Pointer to the node to inspect
 *  \param psStartMib           Pointer to MiB to start at. NULL to start at beginning.
 *                              This can be used to return multiple MiBs with the same name from a node.
 *  \param u32MibId             Identifier of the MiB type to look for.
 *  \return NULL if psNode has no MiB with the given name, otherwise a pointer to the MiB.
 */
tsMib *psJIP_LookupMibId(tsNode *psNode, tsMib *psStartMib, uint32_t u32MibId);


/** Determine if a MiB has a variable with the given name. If it does, a pointer to the variable is returned.
 *  Otherwise, NULL. The calling thread must hold a lock on the parent node structure via \ref eJIP_LockNode
 *  or \ref psJIP_LookupNode.
 *  \param psMib                Pointer to the MiB to inspect
 *  \param psStartVar           Pointer to variable to start at. NULL to start at beginning.
 *                              This can be used to return multiple variables with the same name from a MiB.
 *  \param pcName               Name of the variable to look for.
 *  \return NULL if psMib has no variable with the given name, otherwise a pointer to the variable.
 */
tsVar *psJIP_LookupVar(tsMib *psMib, tsVar *psStartVar, const char *pcName);


/** Determine if a MiB has a variable with the given index. If it does, a pointer to the variable is returned.
 *  Otherwise, NULL. The calling thread must hold a lock on the parent node structure via \ref eJIP_LockNode
 *  or \ref psJIP_LookupNode.
 *  \param psMib                Pointer to the MiB to inspect
 *  \param u8Index              Index of the variable within it's MiB.
 *  \return NULL if psMib has no variable with the given index, otherwise a pointer to the variable.
 */
tsVar *psJIP_LookupVarIndex(tsMib *psMib, uint8_t u8Index);


/** Print out the network context. This function dumps the list of nodes, their MiBs, variables, and
 *  variable values, out through stdout.
 *  \param psJIP_Context        Pointer to JIP Context
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIP_PrintNetworkContent(tsJIP_Context *psJIP_Context);


/** @} */


/** \defgroup server Server Context functions
 *  These are functions for operating the library in server mode.
 *  The library should first be initialised using \ref eJIP_Init and \ref E_JIP_CONTEXT_SERVER.
 *  Once the library has been initialised, node definitions must be loaded using
 *  \ref eJIPService_PersistXMLLoadDefinitions. The loaded XML file must contain XML definitions
 *  of each required Device ID. At least one node with a device ID contained in teh XML definitions
 *  file may then be added to the network using \ref eJIPserver_AddNode.
 *  The library should then be instructed to begin listening on the required network interfaces
 *  using \ref eJIPserver_Listen.
 * @{ */


/** This function adds a node to the JIP context. The node that is created has a device ID of u32DeviceId,
 *  which determines the MIBs and variables thay the node will have.
 *  The standard Node MIB is populated with information passed in to this function.
 *  The library will create a socket bound to the IPv6 address pcAddress, port iPort for this node.
 *  This address must exist on a local interaface for the bind to suceed, otherwise E_JIP_ERROR_NETWORK
 *  will be returned.
 *  This function internally locks the JIP context in order to add the node, then releases it again. 
 *  If a pointer to the new node is requested with ppsNode, then the node is left locked by \ref eJIP_LockNode.
 *  when it is returned, otherwise if ppsNode is NULL, then the new node is unlocked and no pointer is returned.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_SERVER context)
 *  \param pcAddress            IPv6 address of the node (Must exist as a local interface address)
 *  \param iPort                Port number to bind the service to
 *  \param u32DeviceId          Device ID of the node to create
 *  \param pcName               String representing the name of the device
 *  \param pcVersion            String representing the version of the device
 *  \param ppsNode[out]         Pointer to a location in which to store a pointer to the new node.
 *                              If NULL, no pointer is returned. Otherwise a pointer to the node will be
 *                              stored in this location. The Node is locked to the thread and must be
 *                              unlocked with \ref eJIP_UnlockNode.
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_NodeAdd(tsJIP_Context *psJIP_Context, 
                                const char *pcAddress, const int iPort, 
                                uint32_t u32DeviceId, char *pcName, const char *pcVersion, 
                                tsNode **ppsNode);


/** This function removes a node from the JIP context. The node will be removed from the network and all 
 *  memory associated with it free'd. After calling this function the psNode pointer will no longer be 
 *  valid and must not be used again. 
 *  Before calling this function, the node must be  locked using  \ref eJIP_LockNode. This function 
 *  internally locks the JIP context in order to remove the node, then releases it again. 
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_SERVER context)
 *  \param psNode               Pointer to the node to remove from the network, locked using 
 *                              \ref eJIP_LockNode
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_NodeRemove(tsJIP_Context *psJIP_Context, tsNode *psNode);


/** Begin listening for incoming packets to the nodes that exist in the network. Nodes
 *  may still be created/destroyed after this function has been called, they will be 
 *  added to (\ref eJIPserver_AddNode)/removed from (\ref eJIPserver_NodeRemove)
 * the listening server.
 *  \param psJIP_Context        Pointer to JIP Context (Must be an E_JIP_CONTEXT_SERVER context)
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_Listen(tsJIP_Context *psJIP_Context);


/** Join a node to a multicast group.
 *  This function causes the node psNode to join the IPv6 multicast address given 
 *  by pcMulticastAddress. The node should be locked via \ref eJIP_LockNode.
 *  The socket will join the group, and the node will show membership of the group
 *  in the Groups mib.
 *  \param psNode           Pointer to the node structure
 *  \param pcMulticastAddress String containing the IPv6 address of the group to join
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_NodeGroupJoin(tsNode *psNode, const char *pcMulticastAddress);


/** Remove a node from a multicast group.
 *  This function causes the node psNode to leave the IPv6 multicast address given 
 *  by pcMulticastAddress. The node should be locked via \ref eJIP_LockNode.
 *  The socket will leave the group, and the node will no longer show membership of the group
 *  in the Groups mib.
 *  \param psNode           Pointer to the node structure
 *  \param pcMulticastAddress String containing the IPv6 address of the group to leave
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_NodeGroupLeave(tsNode *psNode, const char *pcMulticastAddress);


/** Get the number of groups that a node is a member of, and list of the group addresses.
 *  \param psNode           Pointer to the node structure
 *  \param pu32NumAddresses[out] Pointer to location to store the number of groups
 *  \param pasGroupAddresses[out] Pointer to store a malloc'd array of group addresses. The application should free()
 *                          this once it has finished with it.
 *  \return E_JIP_OK on success.
 */
teJIP_Status eJIPserver_NodeGroupMembership(tsNode *psNode, uint32_t *pu32NumGroups, struct in6_addr **pasGroupAddresses);


/** Utility function for the groups mib. Convert the compressed form used for group addresses
 *  into a full IPv6 multicast address.
 *  \param psAddress[out]   Pointer to location for the IPv6 multicast address to be stored
 *  \param pau8Buffer       Compressed data buffer. Usually this will be psVar->pvData.
 *  \param u8BufferLength   Length of data buffer. Usually this will be psVar->u8Size.
 *  \return None. But store output into psAddress.
 */
void vJIPserver_GroupMibCompressedAddressToIn6(struct in6_addr *psAddress, uint8_t *pau8Buffer, uint8_t u8BufferLength);


/** Function to update a local \ref tsVar structure with new data.
 *  This function free's any allocated storage for the variable, then
 *  mallocs u32Size new bytes and copies the passed data into it.
 *  The psVar must belong to a \ref tsNode than has been locked using \ref eJIP_LockNode.
 *  \param psVar            Pointer to variable to update
 *  \param pvData           Pointer to location containing new data. This will be copied into the variable
 *  \param u32Size          Size (in bytes) of new data
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_SetVarValue(tsVar *psVar, void *pvData, uint32_t u32Size);


/** Function to update a table row data
 *  In much the same way as \ref eJIP_SetVarValue, this function updates an individual row of
 *  a table. It free's any existing storage for the row at Index u32Index inthe table, then 
 *  mallocs u32Length new bytes and copies the passed data into it.
 *  The psVar must belong to a \ref tsNode than has been locked using \ref eJIP_LockNode.
 *  The \ref tsVar must be a table type.
 *  \param psVar            Pointer to table variable to update
 *  \param u32Index         Index of row of the table that should be updated.     
 *  \param pvData           Pointer to location containing new data. This will be copied into the table row
 *  \param u32Size          Size (in bytes) of new data
 *  \return E_JIP_OK on success
 */
teJIP_Status eJIP_Table_UpdateRow(tsVar *psVar, uint32_t u32Index, void *pvData, uint32_t u32Size);


/** @} */


#endif /* __JIP_H__ */



