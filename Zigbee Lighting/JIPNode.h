//
//  JIPNode.h
//  Zigbee Lighting
//
//  Created by lxl on 14-11-5.
//
//

#import <Foundation/Foundation.h>
#import "JIP.h"

typedef enum {
    JIPVarAccessTypeConst,
    JIPVarAccessTypeReadOnly,
    JIPVarAccessTypeReadWrite,
}JIPVarAccessType;

@interface JIPVar : NSObject

@property (nonatomic, readonly) tsVar *var;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) BOOL enable;
@property (nonatomic, readonly) JIPVarAccessType accessType;
@property (nonatomic, readonly) NSData *data;

- (void) writeData:(NSData *)data;

@end

@interface JIPMIB : NSObject

@property (nonatomic, readonly) tsMib *mib;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) uint32_t ID;
@property (nonatomic, strong, readonly) NSArray *vars;

@end

@interface JIPNode : NSObject

@property (nonatomic, readonly) tsNode *node;
@property (nonatomic, readonly) uint32_t deviceID;
@property (nonatomic, strong, readonly) NSString *address;
@property (nonatomic, strong, readonly) NSArray *MIBs;

- (instancetype)initWithTsNode:(tsNode *)node;

@end
