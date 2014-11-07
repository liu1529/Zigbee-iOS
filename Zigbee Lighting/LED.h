//
//  LED.h
//  Zigbee Lighting
//
//  Created by lxl on 14-11-7.
//
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "JIPNode.h"

@interface LED : NSObject

@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) UIImage *image;
@property (nonatomic) uint8_t lum;
@property (nonatomic) uint8_t cct;

+ (instancetype) LEDWithJIPNode:(JIPNode *)node;

@end
