//
//  LEDListCollectionVC.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-6.
//
//

#import "LEDListCollectionVC.h"
#import "LEDCollectionViewCell.h"
#import "JIPClient.h"
#import "LEDEditVC.h"
#import "LED.h"
#import "IndicatorView.h"
#import "UIImageEffects.h"


@interface LEDListCollectionVC () <JIPClientDelegate>

@property (nonatomic, strong) JIPClient *client;
@property (nonatomic, strong) NSMutableArray *LEDs;
@property (nonatomic, strong) UIRefreshControl *refreshControl;
@property (nonatomic, strong) IndicatorView *indicatorView;

@end

@implementation LEDListCollectionVC

static NSString * const reuseIdentifier = @"Cell";

- (void)viewDidLoad {
    [super viewDidLoad];
    
    // Uncomment the following line to preserve selection between presentations
    // self.clearsSelectionOnViewWillAppear = NO;
    
    // Register cell classes
    
    
    // Do any additional setup after loading the view.
    
    self.client = [JIPClient sharedJIPClient];
    self.client.delegate = self;

    
    [self findGetWay];
    
    self.LEDs = [NSMutableArray new];
    
    
    //refresh
//    self.refreshControl = [[UIRefreshControl alloc] init];
//    self.refreshControl.attributedTitle = [[NSAttributedString alloc] initWithString:@"Pull to refresh"];
//    [self.refreshControl addTarget:self action:@selector(refreshList) forControlEvents:UIControlEventValueChanged];
//    [self.collectionView addSubview:self.refreshControl];
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)findGetWay
{
    
    self.indicatorView = [[[NSBundle mainBundle] loadNibNamed:@"IndicatorView" owner:nil options:nil] lastObject];
    self.indicatorView.label.text = @"Finding Getway...";
    [self.view insertSubview:self.indicatorView aboveSubview:self.collectionView];

    [self.client findGetway];
}

- (void)JIPClient:(JIPClient *)client didFoundGetwayWithIPV4:(NSString *)ipv4 withIPV6:(NSString *)ipv6
{
    self.indicatorView.label.text = @"Connecting...";
    if (ipv4)
    {
        [self.client connectIPV4:ipv4.UTF8String IPV6:"::0" port:1872 useTCP:YES];
    }
    else if (ipv6)
    {
        [self.client connectIPV6:ipv6.UTF8String port:1872];
    }
}

#pragma mark - JIPClientDelegate

- (void)JIPClientDidConnect:(JIPClient *)client
{
    self.indicatorView.label.text = @"Discovering...";
    [client discoverWithDeviceIDs:nil completion:nil];
    [client startMonitorNetwork];
}

- (void)JIPClientDidFailToConnect:(JIPClient *)client
{
    if (self.indicatorView) {
        [self.indicatorView removeFromSuperview];
        self.indicatorView = nil;
        
    }
   
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Connect Error"
                                                    message:@"Connent Getway Failed"
                                                   delegate:nil
                                          cancelButtonTitle:@"OK"
                                          otherButtonTitles: nil];
    [alert show];
    
}

- (void)JIPClient:(JIPClient *)client didDiscoverNode:(JIPNode *)node
{
    if (self.indicatorView) {
         self.indicatorView.hidden = YES;
        [self.indicatorView removeFromSuperview];
        self.indicatorView = nil;
        [self.view setNeedsDisplay];
    }
    
    LED *led = [LED LEDWithJIPNode:node];
    if (led) {
        [self.LEDs addObject:led];
        [self.collectionView reloadData];
    }
    
}

- (void)JIPClientDidFailToDiscover:(JIPClient *)client
{
    if (self.indicatorView) {
        [self.indicatorView removeFromSuperview];
        self.indicatorView = nil;
    }
    [self.refreshControl endRefreshing];
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Discover Error"
                                                    message:@"Discover Failed"
                                                   delegate:nil
                                          cancelButtonTitle:@"OK"
                                          otherButtonTitles: nil];
    [alert show];
}

- (void)JIPClient:(JIPClient *)client nodeDidJoin:(JIPNode *)node
{
    LED *led = [LED LEDWithJIPNode:node];
    if (led) {
        [self.LEDs addObject:led];
        [self.collectionView reloadData];
    }

}

- (void)JIPClient:(JIPClient *)client nodeDidLeave:(JIPNode *)node
{
    for (LED *led in self.LEDs) {
        if (led.node == node) {
            [self.LEDs removeObject:led];
            [self.collectionView reloadData];
            break;
        }
    }

}

- (void)JIPClient:(JIPClient *)client nodeDidMove:(JIPNode *)node
{
    
}

- (void) refreshList
{
    [self.LEDs removeAllObjects];
    [self.client discoverWithDeviceIDs:nil completion:^{
        [self.refreshControl endRefreshing];
    }];
    
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

#pragma mark <UICollectionViewDataSource>

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView {
    return 1;
}


- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    return self.LEDs.count;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    LEDCollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:reuseIdentifier forIndexPath:indexPath];
    
    // Configure the cell
    LED *led = self.LEDs[indexPath.row];
    
    cell.imageView.image = led.image;
    cell.imageView.alpha = 0;
    [UIView animateWithDuration:1 animations:^{
        cell.imageView.alpha = 1;
    }];
    
    cell.label.text = led.name;
    
    return cell;
}

#pragma mark <UICollectionViewDelegate>

/*
// Uncomment this method to specify if the specified item should be highlighted during tracking
- (BOOL)collectionView:(UICollectionView *)collectionView shouldHighlightItemAtIndexPath:(NSIndexPath *)indexPath {
	return YES;
}
*/

/*
// Uncomment this method to specify if the specified item should be selected
- (BOOL)collectionView:(UICollectionView *)collectionView shouldSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    return YES;
}
*/

/*
// Uncomment these methods to specify if an action menu should be displayed for the specified item, and react to actions performed on the item
- (BOOL)collectionView:(UICollectionView *)collectionView shouldShowMenuForItemAtIndexPath:(NSIndexPath *)indexPath {
	return NO;
}

- (BOOL)collectionView:(UICollectionView *)collectionView canPerformAction:(SEL)action forItemAtIndexPath:(NSIndexPath *)indexPath withSender:(id)sender {
	return NO;
}

- (void)collectionView:(UICollectionView *)collectionView performAction:(SEL)action forItemAtIndexPath:(NSIndexPath *)indexPath withSender:(id)sender {
	
}
*/

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    LEDEditVC *vc = segue.destinationViewController;
    NSIndexPath *index = [self.collectionView indexPathForCell:sender];
    vc.editLed = self.LEDs[index.row];
    
    UIGraphicsBeginImageContextWithOptions(self.view.frame.size, YES, 0);
    [self.view drawViewHierarchyInRect:self.view.frame afterScreenUpdates:NO];
    UIImage *im = UIGraphicsGetImageFromCurrentImageContext();
    if ([[[UIDevice currentDevice] systemVersion] floatValue] < 7.1)
    {
        // There was a bug in iOS versions 7.0.x which caused vImage buffers
        // created using vImageBuffer_InitWithCGImage to be initialized with data
        // that had the reverse channel ordering (RGBA) if BOTH of the following
        // conditions were met:
        //      1) The vImage_CGImageFormat structure passed to
        //         vImageBuffer_InitWithCGImage was configured with
        //         (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little)
        //         for the bitmapInfo member.  That is, if you wanted a BGRA
        //         vImage buffer.
        //      2) The CGImage object passed to vImageBuffer_InitWithCGImage
        //         was loaded from an asset catalog.
        //
        // To reiterate, this bug only affected images loaded from asset
        // catalogs.
        //
        // The workaround is to setup a bitmap context, draw the image, and
        // capture the contents of the bitmap context in a new image.
        
//        UIGraphicsBeginImageContextWithOptions(im.size, NO, im.scale);
//        [im drawAtPoint:CGPointZero];
//        im = UIGraphicsGetImageFromCurrentImageContext();
//        UIGraphicsEndImageContext();
    }

    UIGraphicsEndImageContext();
    
    vc.bgImage = [UIImageEffects imageByApplyingLightEffectToImage:im];
    
}

@end
