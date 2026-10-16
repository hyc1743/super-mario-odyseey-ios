// SMO Mobile —— iOS App 壳。
//
// 现状：报告「移植做到哪一步」+「游戏文件在不在」（用与宿主同一份的 romfs_resolve 逻辑），
// 并把宿主的**日志环形缓冲**显示在屏幕上（未接电脑也能看）。宿主启动在下一轮接入。

#import <UIKit/UIKit.h>
#include "logbuf.h"
#include "romfs_resolve.h"
#include "smoarc.h"
#include "dm.h"
#include "hle.h"
#include "hle_thread.h"
extern void dm_jump_guest(void *arg, void *fn);

// ---- 宿主引导（后台线程跑；日志进环形缓冲，前台定时刷新显示） ----
static void RunHost(void) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *docs = [fm URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                    appropriateForURL:nil create:NO error:nil];
    char ep[1024];
    if (!docs || !dm_exefs_path(ep, sizeof ep, docs.path.UTF8String)) {
        LOG("App：Documents 里没有 ExeFS —— 只显示状态页（把改写产物放进「文件」App 的 SMOMobile/ExeFS）");
        return;
    }
    // 首次运行：Documents 里没有 RomFS 目录、但有 RomFS.smoa.0 卷时，先展开（几千个小文件装在两个大文件里）
    NSFileManager *fm2 = NSFileManager.defaultManager;
    NSURL *docs2 = [fm2 URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                      appropriateForURL:nil create:NO error:nil];
    if (docs2) {
        // 卷可能在 Documents 根，也可能在一层包装目录（用户拷的是整个 SMO Resource 文件夹）。
        // 依次找：哪个目录里有 RomFS.smoa.0 且没有 RomFS 目录，就在哪里展开。
        NSMutableArray<NSString *> *cands = [NSMutableArray arrayWithObject:docs2.path];
        for (NSString *sub in [fm2 contentsOfDirectoryAtPath:docs2.path error:nil] ?: @[]) {
            BOOL sd = NO;
            NSString *sp = [docs2.path stringByAppendingPathComponent:sub];
            if ([fm2 fileExistsAtPath:sp isDirectory:&sd] && sd) [cands addObject:sp];
        }
        for (NSString *base in cands) {
            NSString *romfsDir = [base stringByAppendingPathComponent:@"RomFS"];
            BOOL isDir = NO;
            if ([fm2 fileExistsAtPath:romfsDir isDirectory:&isDir] && isDir) continue;
            const char *vols[8];
            unsigned nvols = 0;
            for (int i = 0; i < 8; i++) {
                NSString *v = [base stringByAppendingPathComponent:
                               [NSString stringWithFormat:@"RomFS.smoa.%d", i]];
                if (![fm2 fileExistsAtPath:v]) break;
                vols[nvols++] = v.UTF8String;
            }
            if (nvols) {
                LOG("首次运行：在 %s 展开 RomFS 容器（%u 卷，几分钟）…", base.UTF8String, nvols);
                int rc = smoarc_extract(base.UTF8String, vols, nvols);
                LOG("展开结果：%d（0 = 成功）", rc);
            }
        }
    }
    LOG("== SMO 宿主（App 内）：从 %s 装载 ==", ep);
    host_thread_keys_create();
    // 不装 rtld：宿主自己扮演 rtld（映射/重定位/DT_INIT 都在 loader_core.c），而 rtld 的 32 条重定位
    // 落在代码段边界页里，按三段装不了（smo/ios/tools/x0_layout.py）。参考项目同样只装游戏模块。
    static const char *names[] = { "sdk", "subsdk0", "main" };
    load_modules(ep, names, 3);
    run_module_inits();
    uint32_t h = svc_thread_main_init();
    LOG("主线程句柄 %#x", h);
    void *fn = guest_sym("nnMain");
    if (!fn) { LOG("【红】找不到入口符号 nnMain"); return; }
    module_t *m = module_of(fn);
    LOG("== 进入客体入口 nnMain（在 %s，偏移 +%#zx）==", m ? m->name : "?",
        m ? (size_t)((const uint8_t *)fn - m->base) : 0);
    guest_thread_init();
    dm_jump_guest(NULL, fn);
    LOG("== 客体入口返回了（正常游戏里 nnMain 不会返回）==");
}

// 各子系统状态。写在代码里，改的时候顺手改，别让它和 STATUS.md 对不上。
static NSArray<NSArray<NSString *> *> *kSubsystems(void) {
    return @[
        @[@"游戏文件解包（NSP → ExeFS / RomFS）", @"已完成"],
        @[@"模块侦察（导入 / 图形入口 / 着色器位置）", @"已完成"],
        @[@"程序改写（4 个 NSO，含动态等价性）", @"已完成"],
        @[@"着色器翻译（18470 个 → SPIR-V → MSL）", @"已完成"],
        @[@"宿主加载器（文件与三段内嵌两条路径）", @"已完成"],
        @[@"内核 svc + fs/oe/hid 服务", @"部分"],
        @[@"图形层（NVN → Metal，离屏验证中）", @"进行中"],
        @[@"触屏 / 手柄输入", @"未开始"],
    ];
}

// 最近日志（宿主的环形缓冲 logbuf.c；与宿主共用同一份代码，逻辑有 CI 自检）
static NSString *RecentLog(void) {
    char buf[16384];
    size_t n = dm_log_dump(buf, sizeof buf);
    if (!n) return @"（暂无宿主日志）";
    return [NSString stringWithUTF8String:buf];
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
    char rp[1024], ep[1024];
    if (dm_romfs_path(rp, sizeof rp, docs.path.UTF8String))
        [lines addObject:[NSString stringWithFormat:@"RomFS 路径（宿主同款解析）：%s", rp]];
    if (dm_exefs_path(ep, sizeof ep, docs.path.UTF8String))
        [lines addObject:[NSString stringWithFormat:@"ExeFS 路径（宿主同款解析）：%s", ep]];
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

    // 导出日志按钮：一键把「状态 + 宿主日志」存成文件并拉起分享（AirDrop/邮件/存文件）
    UIButton *exportBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    [exportBtn setTitle:@"导出日志" forState:UIControlStateNormal];
    [exportBtn setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    exportBtn.backgroundColor = [UIColor colorWithWhite:0.18 alpha:0.92];
    exportBtn.layer.cornerRadius = 9;
    exportBtn.titleLabel.font = [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
    exportBtn.translatesAutoresizingMaskIntoConstraints = NO;
    [exportBtn addTarget:self action:@selector(exportLog) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:exportBtn];
    [NSLayoutConstraint activateConstraints:@[
        [exportBtn.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
        [exportBtn.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-16],
        [exportBtn.widthAnchor constraintEqualToConstant:96],
        [exportBtn.heightAnchor constraintEqualToConstant:36],
    ]];

    // 每秒刷新：宿主日志是活的（后台引导 + 客体初始化都会往环形缓冲里写）
    [NSTimer scheduledTimerWithTimeInterval:1.0 repeats:YES block:^(NSTimer *t) {
        (void)t;
        _text.text = [self statusText];
    }];
}

- (void)exportLog {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *docs = [fm URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                    appropriateForURL:nil create:YES error:nil];
    NSString *stamp = [NSDateFormatter localizedStringFromDate:NSDate.date
                                                     dateStyle:NSDateFormatterShortStyle
                                                     timeStyle:NSDateFormatterMediumStyle];
    stamp = [stamp stringByReplacingOccurrencesOfString:@"/" withString:@"-"];
    stamp = [stamp stringByReplacingOccurrencesOfString:@":" withString:@"."];
    NSString *name = [NSString stringWithFormat:@"SMO-log-%@.txt", stamp];
    NSURL *url = [docs URLByAppendingPathComponent:name];
    NSString *text = [self statusText];            // 状态 + 游戏文件 + 宿主日志全在里面
    NSError *err = nil;
    if (![text writeToURL:url atomically:YES encoding:NSUTF8StringEncoding error:&err]) {
        UIAlertController *ac = [UIAlertController alertControllerWithTitle:@"导出失败"
                                    message:err.localizedDescription ?: @"?"
                                    preferredStyle:UIAlertControllerStyleAlert];
        [ac addAction:[UIAlertAction actionWithTitle:@"好" style:UIAlertActionStyleDefault handler:nil]];
        [self presentViewController:ac animated:YES completion:nil];
        return;
    }
    // 同时留一份固定名的最新日志（方便你在「文件」App 里直接翻）
    [text writeToURL:[docs URLByAppendingPathComponent:@"SMO-log-latest.txt"]
          atomically:YES encoding:NSUTF8StringEncoding error:nil];
    UIActivityViewController *avc = [[UIActivityViewController alloc]
        initWithActivityItems:@[url] applicationActivities:nil];
    [self presentViewController:avc animated:YES completion:nil];
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
    [out addObject:@"== 宿主日志 =="];
    [out addObject:RecentLog()];
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
    // 宿主引导放后台：不得阻塞主线程（引导 + 客体入口会一直跑）
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ RunHost(); });
    return YES;
}

@end

int main(int argc, char **argv) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(SMOAppDelegate.class));
    }
}
