//
//  JIPClient.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-5.
//
//

#import "JIPClient.h"



@interface JIPClient ()
{
    tsJIP_Context _sJIP_Context;
}

@property (nonatomic, strong) NSMutableArray *nodesArray;

@end



@implementation JIPClient

+ (instancetype) sharedJIPClient
{
    static dispatch_once_t  onceToken;
    static JIPClient * sSharedInstance;
    dispatch_once(&onceToken, ^{
        sSharedInstance = [[JIPClient alloc] init];
    });
    
    return sSharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        if (eJIP_Init(&_sJIP_Context, E_JIP_CONTEXT_CLIENT) != E_JIP_OK) {
            printf("JIP startup failed\n");
            return nil;
        }
        self.nodesArray = [NSMutableArray new];
    }
    return self;
}


- (instancetype)initWithDelegate:(id<JIPClientDelegate>)delegate
{
    self = [self init];
    if (self) {
        self.delegate = delegate;
    }
    return self;
}

- (NSArray *)nodes
{
    return self.nodesArray;
}

- (void) connectIPV4:(const char *)pcIPv4Address
                IPV6:(const char *)pcIPv6Address
                port:(const int)port
              useTCP:(bool_t) useTCP
{
    
    if (eJIP_Connect4(&_sJIP_Context, pcIPv4Address, pcIPv6Address, port, useTCP) == E_JIP_OK) {
        if ([self.delegate respondsToSelector:@selector(JIPClientDidConnect:)]) {
            [self.delegate JIPClientDidConnect:self];
        }
    }
    else {
        printf("JIP connect ipv4 failed\n");
        if ([self.delegate respondsToSelector:@selector(JIPClientDidFailToConnect:)]) {
            [self.delegate JIPClientDidFailToConnect:self];
        }
    }
}

- (void) connectIPV6:(const char *) pcIPv6Address
                port:(const int)port
{
    if (eJIP_Connect(&_sJIP_Context, pcIPv6Address, port) == E_JIP_OK) {
        if ([self.delegate respondsToSelector:@selector(JIPClientDidConnect:)]) {
            [self.delegate JIPClientDidConnect:self];
        }
    }
    else {
        printf("JIP connect ipv6 failed\n");
        if ([self.delegate respondsToSelector:@selector(JIPClientDidFailToConnect:)]) {
            [self.delegate JIPClientDidFailToConnect:self];
        }
    }
}

- (void) connectIPV4:(const char *)pcIPv4Address
              useTCP:(bool_t)useTCP
{
    [self connectIPV4:pcIPv4Address IPV6:"::0" port:JIP_DEFAULT_PORT useTCP:useTCP];
}

- (void) connectIPV6:(const char *) pcIPv6Address
{
    [self connectIPV6:pcIPv6Address port:JIP_DEFAULT_PORT];
}

- (void) discoverWithDeviceIDs:(NSArray *)deviceIDs completion:(void (^)(void))completion
{
    if (eJIPService_DiscoverNetwork(&_sJIP_Context) != E_JIP_OK)
    {
        printf("JIP discover network failed\n\r");
        if ([self.delegate respondsToSelector:@selector(JIPClientDidFailToDiscover:)]) {
            [self.delegate JIPClientDidFailToDiscover:self];
        }
        
    }
    if (deviceIDs == nil) {
        uint32_t u32NumAddresses;
        tsJIPAddress *psAddresses;
        
        if (eJIP_GetNodeAddressList(&_sJIP_Context, JIP_DEVICEID_ALL, &psAddresses, &u32NumAddresses) == E_JIP_OK)
        {
            for (int nodeIndex = 0 ; nodeIndex < u32NumAddresses; nodeIndex++) {
                tsNode *psNode = psJIP_LookupNode(&_sJIP_Context, &psAddresses[nodeIndex]);
                if (!psNode)
                {
                    fprintf(stderr, "Node has been removed\n");
                    continue;
                }
                if ([self.delegate respondsToSelector:@selector(JIPClient:didDiscoverNode:)]) {
                    [self.delegate JIPClient:self didDiscoverNode:[[JIPNode alloc] initWithTsNode:psNode]];
                }
                eJIP_UnlockNode(psNode);
                
            }
            free(psAddresses);

        }
        else
        {
            printf("Error getting node list\n\r");
            if ([self.delegate respondsToSelector:@selector(JIPClientDidFailToDiscover:)]) {
                [self.delegate JIPClientDidFailToDiscover:self];
            }
        }
    } else {
        for (NSNumber *idNum in deviceIDs) {
            uint32_t u32NumAddresses;
            tsJIPAddress *psAddresses;
            uint32_t deviceID = [idNum unsignedIntValue];
            if (eJIP_GetNodeAddressList(&_sJIP_Context, deviceID, &psAddresses, &u32NumAddresses) == E_JIP_OK)
            {
                tsNode *psNode = psJIP_LookupNode(&_sJIP_Context, &psAddresses[0]);
                if (!psNode)
                {
                    fprintf(stderr, "Node has been removed\n");
                    continue;
                }
                if ([self.delegate respondsToSelector:@selector(JIPClient:didDiscoverNode:)]) {
                    [self.delegate JIPClient:self didDiscoverNode:[[JIPNode alloc] initWithTsNode:psNode]];
                }
                eJIP_UnlockNode(psNode);
                free(psAddresses);
            }
            else
            {
                printf("Error getting node list\n\r");
                if ([self.delegate respondsToSelector:@selector(JIPClientDidFailToDiscover:)]) {
                    [self.delegate JIPClientDidFailToDiscover:self];
                }
                break;
            }

        }
    }
    if (completion) {
        completion();
    }
    
}





void networkChange(teJIP_NetworkChangeEvent eEvent, struct _tsNode *psNode)
{
    switch (eEvent) {
        case E_JIP_NODE_JOIN:
           
            break;
        case E_JIP_NODE_LEAVE:
            break;
        case E_JIP_NODE_MOVE:
            break;
        default:
            break;
    }
}

- (void) monitorNetwork
{
    
    eJIPService_MonitorNetwork(&_sJIP_Context, networkChange);
    
}

- (void) monitorNetworkStop
{
    eJIPService_MonitorNetworkStop(&_sJIP_Context);
}




@end
