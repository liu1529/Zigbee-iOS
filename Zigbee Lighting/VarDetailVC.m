//
//  VarDetailVC.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-6.
//
//

#import "VarDetailVC.h"

@interface VarDetailVC () <UITextFieldDelegate>



@end

@implementation VarDetailVC

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    self.title = self.editVar.name;
    switch (self.editVar.var->eVarType) {
        case E_JIP_VAR_TYPE_UINT8:
        case E_JIP_VAR_TYPE_INT8:
        case E_JIP_VAR_TYPE_UINT16:
        case E_JIP_VAR_TYPE_INT16:
        case E_JIP_VAR_TYPE_INT32:
        case E_JIP_VAR_TYPE_UINT32:
        case E_JIP_VAR_TYPE_UINT64:
        case E_JIP_VAR_TYPE_INT64:
        case E_JIP_VAR_TYPE_FLT:
        case E_JIP_VAR_TYPE_DBL:
        {
            UISlider *slider = [[UISlider alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
            slider.center = self.view.center;
            [self.view addSubview:slider];
            break;
        }
        case E_JIP_VAR_TYPE_STR:
        {
            UITextField *textField = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
            textField.center = self.view.center;
            [self.view addSubview:textField];
            break;
        }
        default:
            break;
    }
    
    
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}



- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    uint8_t buf[self.editVar.data.length];
    [textField.text getBytes:buf maxLength:sizeof(buf) usedLength:nil encoding:NSUTF8StringEncoding options:NSStringEncodingConversionExternalRepresentation range:NSMakeRange(0, sizeof(buf)) remainingRange:nil];
    
    

    
    
    
    return YES;
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/


@end
