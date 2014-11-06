//
//  LEDListVC.m
//  Zigbee Lighting
//
//  Created by lxl on 14-11-4.
//
//

#import "LEDListVC.h"
#import "JIPClient.h"
#import "MIBListTableVC.h"


@interface LEDListVC () <JIPClientDelegate,
                        UITableViewDataSource>

@property (nonatomic, strong) JIPClient *aJIPClient;
@property (nonatomic, strong) NSMutableArray *JIPNodes;

@property (nonatomic, weak) IBOutlet UITableView *tableView;
@property (nonatomic, strong) UIRefreshControl *refreshControl;

@end

@implementation LEDListVC

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
  
    self.JIPNodes = [NSMutableArray new];
    
    self.aJIPClient = [[JIPClient alloc] initWithDelegate:self];
    [self.aJIPClient connectIPV4:"192.168.11.1" IPV6:"::0" port:1872 useTCP:YES];
    
    
    
    self.refreshControl = [[UIRefreshControl alloc] init];
    self.refreshControl.attributedTitle = [[NSAttributedString alloc] initWithString:@"Pull to refresh"];
    [self.refreshControl addTarget:self action:@selector(refresh) forControlEvents:UIControlEventValueChanged];
    [self.tableView addSubview:self.refreshControl];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}



#pragma mark - JIPClientDelegate

- (void)JIPClientDidConnect:(JIPClient *)client
{
    [client discover];
}

- (void)JIPClientDidFailToConnect:(JIPClient *)client
{

}

- (void)JIPClientDidDiscover:(JIPClient *)client
{
    [self.refreshControl endRefreshing];
    for (int i = 0; i < client.nodes.count; i++) {
        [self.JIPNodes addObject:client.nodes[i]];
        [self.tableView insertRowsAtIndexPaths:@[[NSIndexPath indexPathForRow:i inSection:0]]
                              withRowAnimation:UITableViewRowAnimationAutomatic];
        
        
    }
    
}

- (void)JIPClientDidFailToDiscover:(JIPClient *)client
{
    [self.refreshControl endRefreshing];
}

#pragma mark - TableViewDataSource

- (void) refresh
{
    NSMutableArray *indexPaths = [NSMutableArray new];
    for (int i = 0; i < self.JIPNodes.count; i++) {
        [indexPaths addObject:[NSIndexPath indexPathForRow:i inSection:0]];
    }
    [self.JIPNodes removeAllObjects];
    [self.tableView deleteRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationAutomatic];
    [self.aJIPClient discover];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return self.JIPNodes.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell" forIndexPath:indexPath];
    
    JIPNode *node = self.JIPNodes[indexPath.row];
    cell.textLabel.text = node.address;
    cell.detailTextLabel.text = [NSString stringWithFormat:@"ID:0X%x",node.deviceID];
    
    
    
    return cell;
}




#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
    MIBListTableVC *vc = [segue destinationViewController];
    NSIndexPath *index = [self.tableView indexPathsForSelectedRows][0];
    vc.editNode = self.JIPNodes[index.row];
}


@end
