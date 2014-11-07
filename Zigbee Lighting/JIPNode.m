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

@end

@implementation JIPMIB

- (instancetype)initWithMib:(tsMib *)mib
{
    self = [super init];
    if (self) {
        self.mib = mib;
        self.name = [NSString stringWithUTF8String:mib->pcName];
        self.ID = mib->u32MibId;
        
    }
    return self;
}

- (NSArray *)vars
{
    static NSMutableArray *varArray = nil;
    if (varArray == nil) {
        tsVar *psVar = self.mib->psVars;
        while (psVar) {
            [varArray insertObject:[[JIPVar alloc] initWithVar:psVar] atIndex:psVar->u8Index];
            psVar = psVar->psNext;
        }

    }
    return varArray;
}

- (JIPVar *) lookupVarWithName:(NSString *)name
{
    eJIP_LockNode(self.mib->psOwnerNode, YES);
    tsVar *tsVar = psJIP_LookupVar(self.mib, NULL, name.UTF8String);
    if (tsVar == NULL) {
        eJIP_UnlockNode(self.mib->psOwnerNode);
        return nil;
    }
    JIPVar *var = [[JIPVar alloc] initWithVar:tsVar];
    eJIP_UnlockNode(self.mib->psOwnerNode);
    return var;
}

- (JIPVar *) lookupVarWithIndex:(uint8_t) VarIndex
{
    eJIP_LockNode(self.mib->psOwnerNode, YES);
    tsVar *tsVar = psJIP_LookupVarIndex(self.mib, VarIndex);
    if (tsVar == NULL) {
        eJIP_UnlockNode(self.mib->psOwnerNode);
        return nil;
    }
    JIPVar *var = [[JIPVar alloc] initWithVar:tsVar];
    eJIP_UnlockNode(self.mib->psOwnerNode);
    return var;
}

@end

#pragma mark JIPNode

@interface JIPNode ()

@property (nonatomic) tsNode *node;
@property (nonatomic) uint32_t deviceID;
@property (nonatomic, strong) NSString *address;

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
    }
    return self;
}

- (NSArray *)MIBs
{
    static NSMutableArray *MIBArray = nil;
    if (MIBArray == nil) {
        MIBArray = [NSMutableArray new];
        tsMib *psMib = self.node->psMibs;
        while (psMib) {
            [MIBArray insertObject:[[JIPMIB alloc] initWithMib:psMib] atIndex:psMib->u8Index];
            psMib = psMib->psNext;
        }
    }
    return MIBArray;
}

- (NSString *)name
{
    JIPMIB *mib = [self lookupMibWithName:@"Node"];
    if (mib) {
        JIPVar *var = [mib lookupVarWithName:@"DescriptiveName"];
        if (var) {
            return [NSString stringWithUTF8String:var.data.bytes];
        }
    }
    return self.address;
}

- (JIPMIB *) lookupMibWithName:(NSString *)name
{
    eJIP_LockNode(self.node, YES);
    tsMib *tsmib = psJIP_LookupMib(self.node, NULL, name.UTF8String);
    if (tsmib == NULL) {
        eJIP_UnlockNode(self.node);
        return nil;
    }
    JIPMIB *mib = [[JIPMIB alloc] initWithMib:tsmib];
    eJIP_UnlockNode(self.node);
    return mib;
}

- (JIPMIB *) lookupMibWithID:(uint32_t) MibId
{
    eJIP_LockNode(self.node, YES);
    tsMib *tsmib = psJIP_LookupMibId(self.node, NULL, MibId);
    if (tsmib == NULL) {
        eJIP_UnlockNode(self.node);
        return nil;
    }
    JIPMIB *mib = [[JIPMIB alloc] initWithMib:tsmib];
    eJIP_UnlockNode(self.node);
    return mib;
}

@end



