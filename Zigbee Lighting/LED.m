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
@property (nonatomic, strong) JIPMIB *bulbColour;

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
    if ([node lookupMibWithName:@"BulbControl"])
    {
        LED *led =[LED new];
        led.node = node;
        led.bulbControlMib = [node lookupMibWithName:@"BulbControl"];
        led.bulbColour = [node lookupMibWithName:@"BulbColour"];
        
        return led;
    }
    return nil;
}

- (NSString *)name
{
    return self.node.name;
}

- (uint8_t)lum
{
    JIPVar *lumCurrentVar = [self.bulbControlMib lookupVarWithName:@"LumCurrent"];
    if (!lumCurrentVar.data) {
        return 0;
    }
    uint8_t lumCurrent = *(uint8_t *)(lumCurrentVar.data.bytes);
    return lumCurrent;

}

- (void)setLum:(uint8_t)lum
{
    JIPVar *lumTargetVar = [self.bulbControlMib lookupVarWithName:@"LumTarget"];
    [lumTargetVar writeData:[NSData dataWithBytes:&lum length:1]];
}

- (uint8_t)cct
{
    JIPVar *cctCurrentVar = [self.bulbColour lookupVarWithName:@"ColourTempCurrent"];
    if (!cctCurrentVar.data) {
        return 0;
    }
    uint8_t lumCurrent = *(uint8_t *)(cctCurrentVar.data.bytes);
    return lumCurrent;
}

- (void)setCct:(uint8_t)cct
{
    JIPVar *cctTargetVar = [self.bulbColour lookupVarWithName:@"ColourTempTarget"];
    [cctTargetVar writeData:[NSData dataWithBytes:&cct length:1]];
    
}

- (void)setHue:(uint16_t)hue
{
    JIPVar *hueTargetVar = [self.bulbColour lookupVarWithName:@"HueTarget"];
    [hueTargetVar writeData:[NSData dataWithBytes:&hue length:sizeof(hue)]];
    
}

- (void)setSaturation:(uint8_t)saturation
{
    JIPVar *satTargetVar = [self.bulbColour lookupVarWithName:@"SatTarget"];
    [satTargetVar writeData:[NSData dataWithBytes:&saturation length:sizeof(saturation)]];
    
}



@end
