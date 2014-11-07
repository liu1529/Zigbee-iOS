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

@interface LEDListCollectionVC () <JIPClientDelegate>

@property (nonatomic, strong) JIPClient *client;
@property (nonatomic, strong) NSMutableArray *LEDs;
@property (nonatomic, strong) UIRefreshControl *refreshControl;

@end

@implementation LEDListCollectionVC

static NSString * const reuseIdentifier = @"Cell";

- (void)viewDidLoad {
    [super viewDidLoad];
    
    // Uncomment the following line to preserve selection between presentations
    // self.clearsSelectionOnViewWillAppear = NO;
    
    // Register cell classes
    
    
    // Do any additional setup after loading the view.
    
    self.LEDs = [NSMutableArray new];
    
    self.client = [JIPClient sharedJIPClient];
    self.client.delegate = self;
    [self.client connectIPV4:"192.168.1.1" IPV6:"::0" port:1872 useTCP:YES];
    
    //refresh
    self.refreshControl = [[UIRefreshControl alloc] init];
    self.refreshControl.attributedTitle = [[NSAttributedString alloc] initWithString:@"Pull to refresh"];
    [self.refreshControl addTarget:self action:@selector(refreshList) forControlEvents:UIControlEventValueChanged];
    [self.collectionView addSubview:self.refreshControl];
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)JIPClientDidConnect:(JIPClient *)client
{
    [client discoverWithDeviceIDs:nil completion:nil];
}

- (void)JIPClientDidFailToConnect:(JIPClient *)client
{
    
}

- (void)JIPClient:(JIPClient *)client didDiscoverNode:(JIPNode *)node
{
    if ([node lookupMibWithName:@"BulbControl"]) {
        LED *led = [LED LEDWithJIPNode:node];
        [self.LEDs addObject:led];
        [self.collectionView reloadData];
    }
    
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
    
}

@end
