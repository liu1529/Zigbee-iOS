//
//  JIPClient.h
//  Zigbee Lighting
//
//  Created by lxl on 14-11-5.
//
//

#import <Foundation/Foundation.h>
#import "JIP.h"
#import "JIPNode.h"

@class JIPClient;
@protocol JIPClientDelegate <NSObject>

- (void) JIPClientDidConnect:(JIPClient *)client;
- (void) JIPClientDidFailToConnect:(JIPClient *)client;

- (void) JIPClient:(JIPClient *)client didDiscoverNode:(JIPNode *)node;
- (void) JIPClientDidFailToDiscover:(JIPClient *)client;

- (void) JIPClient:(JIPClient *)client didFoundGetwayWithIPV4:(NSString *)ipv4 withIPV6:(NSString *)ipv6;

@optional
- (void) JIPClient:(JIPClient *)client nodeDidJoin:(JIPNode *)node;
- (void) JIPClient:(JIPClient *)client nodeDidLeave:(JIPNode *)node;
- (void) JIPClient:(JIPClient *)client nodeDidMove:(JIPNode *)node;



@end

@interface JIPClient : NSObject

+ (instancetype) sharedJIPClient;

- (instancetype)initWithDelegate:(id<JIPClientDelegate>)delegate;

- (void) findGetway;

- (void) connectIPV4:(const char *)pcIPv4Address
                IPV6:(const char *)pcIPv6Address
                port:(const int)port
              useTCP:(bool_t) useTCP;
- (void) connectIPV6:(const char *) pcIPv6Address
                port:(const int)port;
- (void) connectIPV4:(const char *)pcIPv4Address
              useTCP:(bool_t)useTCP;
- (void) connectIPV6:(const char *) pcIPv6Address;

- (void) discoverWithDeviceIDs:(NSArray *)deviceIDs completion:(void (^)(void))completion;

@property (nonatomic, strong) id<JIPClientDelegate> delegate;
@property (nonatomic, strong, readonly) NSArray *nodes;

@end
