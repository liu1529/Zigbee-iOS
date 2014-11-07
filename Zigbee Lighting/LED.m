//
//  LED.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-7.
//
//

#import "LED.h"

@interface LED ()

@property (nonatomic, strong) JIPNode *node;
@property (nonatomic, strong) JIPMIB *bulbControlMib;

@end

@implementation LED

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.image = [UIImage imageNamed:@"LED.png"];
        
    }
    return self;
}

+ (instancetype) LEDWithJIPNode:(JIPNode *)node
{
    LED *led =[LED new];
    led.node = node;
    led.bulbControlMib = [node lookupMibWithName:@"BulbControl"];
    
    return led;
}

- (NSString *)name
{
    return self.node.name;
}

- (uint8_t)lum
{
    static JIPVar *lumCurrentVar = nil;
    if (!lumCurrentVar) {
        lumCurrentVar = [self.bulbControlMib lookupVarWithName:@"LumCurrent"];
    }
    uint8_t lumCurrent = *(uint8_t *)(lumCurrentVar.data.bytes);
    return lumCurrent;

}

- (void)setLum:(uint8_t)lum
{
    static JIPVar *lumTargetVar = nil;
    if (!lumTargetVar) {
        lumTargetVar = [self.bulbControlMib lookupVarWithName:@"LumTarget"];
    }
    [lumTargetVar writeData:[NSData dataWithBytes:&lum length:1]];
}

@end
