// SMO Mobile —— iOS App 壳
//
// 现在这一版**故意**什么都不做：它只报告「移植做到哪一步」和「游戏文件在不在」。
// 这样既能让出包链路（GitHub Actions → 未签名 IPA → 你自己签名装机）先跑通并稳定下来，
// 也避免出现「装上去什么都不显示、你以为坏了」的情况。
//
// 宿主（系统服务 HLE）与图形层接进来之后，这里会换成加载改写后的游戏程序、开 Metal 视图。

#import <UIKit/UIKit.h>

// 各子系统状态。写在代码里，改的时候顺手改，别让它和 STATUS.md 对不上。
static NSArray<NSArray<NSString *> *> *kSubsystems(void) {
    return @[
        @[@"游戏文件解包（NSP → ExeFS / RomFS）", @"已完成"],
        @[@"模块侦察（导入 / 图形入口 / 着色器位置）", @"已完成"],
        @[@"程序改写（4 个 NSO）", @"已完成"],
        @[@"着色器解包（Yaz0 → SARC → sharc）", @"已完成"],
        @[@"着色器翻译（Maxwell → SPIR-V → MSL）", @"进行中"],
        @[@"宿主加载器与系统服务 HLE", @"未开始"],
        @[@"图形层（NVN → Metal）", @"未开始"],
        @[@"触屏 / 手柄输入", @"未开始"],
    ];
}

// 在 App 自己的目录里找游戏文件。装包流程与参考项目一致：资源放在 App 的目录里，
// 由「文件」App 或 Finder 拷进去。
static NSString *ScanDocuments(void) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *docs = [fm URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                    appropriateForURL:nil create:NO error:nil];
    if (!docs) return @"读不到自己的目录";
    NSArray<NSString *> *items = [fm contentsOfDirectoryAtPath:docs.path error:nil] ?: @[];
    NSUInteger exefs = 0, romfs = 0, shader = 0;
    for (NSString *it in items) {
        NSString *name = it.lastPathComponent;
        if ([name isEqualToString:@"ExeFS"]) exefs++;
        if ([name isEqualToString:@"RomFS"]) romfs++;
        if ([name isEqualToString:@"ShaderData"]) shader++;
        // 也接受「一层目录里装着 ExeFS/RomFS」的放法
        NSString *sub = [docs.path stringByAppendingPathComponent:it];
        BOOL isDir = NO;
        if ([fm fileExistsAtPath:sub isDirectory:&isDir] && isDir) {
            if ([fm fileExistsAtPath:[sub stringByAppendingPathComponent:@"ExeFS"]]) exefs++;
            if ([fm fileExistsAtPath:[sub stringByAppendingPathComponent:@"RomFS"]]) romfs++;
        }
    }
    NSMutableArray<NSString *> *lines = [NSMutableArray array];
    [lines addObject:[NSString stringWithFormat:@"目录：%@", docs.path]];
    [lines addObject:[NSString stringWithFormat:@"顶层条目 %lu 个", (unsigned long)items.count]];
    [lines addObject:[NSString stringWithFormat:@"ExeFS：%@", exefs ? @"找到" : @"没有"]];
    [lines addObject:[NSString stringWithFormat:@"RomFS：%@", romfs ? @"找到" : @"没有"]];
    [lines addObject:[NSString stringWithFormat:@"ShaderData：%@", shader ? @"找到" : @"没有"]];
    if (!exefs || !romfs) {
        [lines addObject:@""];
        [lines addObject:@"把你自己游戏里的 ExeFS 与 RomFS 目录放到这里（用「文件」App 或 Finder），"];
        [lines addObject:@"放好之后回来看这一页。"];
    }
    [lines addObject:@""];
    [lines addObject:@"注意：即使文件放好了，现在也进不了游戏——宿主与图形层还没接上。"];
    return [lines componentsJoinedByString:@"\n"];
}

@interface SMOViewController : UIViewController
@end

@implementation SMOViewController {
    UITextView *_text;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.04 green:0.06 blue:0.10 alpha:1.0];

    _text = [[UITextView alloc] initWithFrame:CGRectZero];
    _text.translatesAutoresizingMaskIntoConstraints = NO;
    _text.editable = NO;
    _text.backgroundColor = UIColor.clearColor;
    _text.textColor = [UIColor colorWithWhite:0.92 alpha:1.0];
    _text.font = [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];
    _text.textContainerInset = UIEdgeInsetsMake(24, 28, 24, 28);
    [self.view addSubview:_text];

    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [_text.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [_text.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [_text.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [_text.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],
    ]];

    _text.text = [self statusText];
}

- (NSString *)statusText {
    UIDevice *dev = UIDevice.currentDevice;
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSMutableArray<NSString *> *out = [NSMutableArray array];

    [out addObject:@"SMO Mobile — iOS 兼容层"];
    [out addObject:@"（把你自己买的 Switch 版《超级马里奥 奥德赛》跑到 iPhone 上）"];
    [out addObject:@""];
    [out addObject:[NSString stringWithFormat:@"版本 %@ (%@)",
                    info[@"CFBundleShortVersionString"] ?: @"?",
                    info[@"CFBundleVersion"] ?: @"?"]];
    [out addObject:[NSString stringWithFormat:@"设备 %@ / %@ %@", dev.model,
                    dev.systemName, dev.systemVersion]];
    [out addObject:[NSString stringWithFormat:@"本机标识 %@", dev.identifierForVendor.UUIDString]];
    [out addObject:@""];
    [out addObject:@"== 移植进度（详见仓库 STATUS.md）=="];
    for (NSArray<NSString *> *row in kSubsystems()) {
        NSString *mark = [row[1] isEqualToString:@"已完成"] ? @"[✓]"
                       : ([row[1] isEqualToString:@"进行中"] ? @"[~]" : @"[ ]");
        [out addObject:[NSString stringWithFormat:@"%@ %@ —— %@", mark, row[0], row[1]]];
    }
    [out addObject:@""];
    [out addObject:@"== 游戏文件 =="];
    [out addObject:ScanDocuments()];
    [out addObject:@""];
    [out addObject:@"== 这个壳是什么 =="];
    [out addObject:@"不模拟 CPU：游戏自己的 aarch64 程序将来会在本机直接执行。"];
    [out addObject:@"本仓库不含任何游戏内容；游戏文件由你自己提供。"];
    [out addObject:@"本项目与 Nintendo 无关。MIT 许可。"];
    return [out componentsJoinedByString:@"\n"];
}

@end

@interface SMOAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation SMOAppDelegate

- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)opts {
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[SMOViewController alloc] init];
    [self.window makeKeyAndVisible];
    NSLog(@"[SMO] 壳已启动；宿主与图形层尚未接入");
    return YES;
}

@end

int main(int argc, char **argv) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(SMOAppDelegate.class));
    }
}
