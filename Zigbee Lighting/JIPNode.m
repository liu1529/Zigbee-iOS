//
//  JIPNode.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-5.
//
//

#import "JIPNode.h"
#import <arpa/inet.h>

#pragma mark JIPVar

@interface JIPVar ()

@property (nonatomic) tsVar *var;
@property (nonatomic) NSString *name;
@property (nonatomic) BOOL enable;
@property (nonatomic) JIPVarAccessType accessType;

@end

@implementation JIPVar

- (instancetype)initWithVar:(tsVar *)var
{
    self = [super init];
    if (self) {
        self.var = var;
        self.enable = YES;
        
        //update data
        NSDate *unuseData = self.data;
        
    }
    return self;
}

- (NSData *)data
{
    if (self.enable) {
        tsJIP_Context *context = self.var->psOwnerMib->psOwnerNode->psOwnerNetwork->psOwnerContext;
        eJIP_GetVar(context, self.var);
        self.name = [NSString stringWithUTF8String:self.var->pcName];
        self.enable = (self.var->eEnable == E_JIP_VAR_ENABLED) ? YES : NO;
        self.accessType = (JIPVarAccessType)self.var->eAccessType;
        
        return [NSData dataWithBytes:self.var->pvData length:self.var->u8Size];
    }
    return nil;
    
}

- (void) writeData:(NSData *)data
{
    if (self.enable &&
        self.accessType == JIPVarAccessTypeReadWrite) {
        tsJIP_Context *context = self.var->psOwnerMib->psOwnerNode->psOwnerNetwork->psOwnerContext;
        eJIP_SetVar(context, self.var, data.bytes, self.var->u8Size);
    }
}



@end

#pragma mark JIPMIB

@interface JIPMIB ()

@property (nonatomic) tsMib *mib;
@property (nonatomic) NSString *name;
@property (nonatomic) uint32_t ID;
@property (nonatomic, strong) NSMutableArray *varArray;

@end

@implementation JIPMIB

- (instancetype)initWithMib:(tsMib *)mib
{
    self = [super init];
    if (self) {
        self.mib = mib;
        self.name = [NSString stringWithUTF8String:mib->pcName];
        self.ID = mib->u32MibId;
        
        self.varArray = [NSMutableArray new];
        tsVar *psVar = mib->psVars;
        while (psVar) {
            [self.varArray insertObject:[[JIPVar alloc] initWithVar:psVar] atIndex:psVar->u8Index];
            psVar = psVar->psNext;
        }
    }
    return self;
}

- (NSArray *)vars
{
    return self.varArray;
}

@end

#pragma mark JIPNode

@interface JIPNode ()

@property (nonatomic) tsNode *node;
@property (nonatomic) uint32_t deviceID;
@property (nonatomic, strong) NSString *address;
@property (nonatomic, strong) NSMutableArray *MIBArray;

@end

@implementation JIPNode

- (instancetype)initWithTsNode:(tsNode *)node
{
    self = [super init];
    if (self) {
        self.node = node;
        self.deviceID = node->u32DeviceId;
        char s[128];
        inet_ntop(PF_INET6, &node->sNode_Address.sin6_addr, s, sizeof(s));
        self.address = [NSString stringWithUTF8String:s];
        
        self.MIBArray = [NSMutableArray new];
        tsMib *psMib = node->psMibs;
        while (psMib) {
            [self.MIBArray insertObject:[[JIPMIB alloc] initWithMib:psMib] atIndex:psMib->u8Index];
            psMib = psMib->psNext;
        }
    }
    return self;
}

- (NSArray *)MIBs
{
    return self.MIBArray;
}

@end



