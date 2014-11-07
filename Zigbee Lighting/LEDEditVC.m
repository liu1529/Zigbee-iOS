//
//  LEDEditVC.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-6.
//
//

#import "LEDEditVC.h"

@interface LEDEditVC ()


@property (weak, nonatomic) IBOutlet UIImageView *imageView;

@property (weak, nonatomic) IBOutlet UITextField *nameTextFiled;
@property (weak, nonatomic) IBOutlet UISlider *lumSlider;

@property (weak, nonatomic) IBOutlet UISlider *tempSlider;
- (IBAction)lumChange:(UISlider *)sender;
- (IBAction)tempChange:(UISlider *)sender;

@end

@implementation LEDEditVC

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    self.nameTextFiled.text = self.editLed.name;
    self.imageView.image = self.editLed.image;
    
    [self.lumSlider setValue:self.editLed.lum animated:YES];
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

- (IBAction)lumChange:(UISlider *)sender {
    self.editLed.lum = sender.value;
}

- (IBAction)tempChange:(UISlider *)sender {
}
@end
