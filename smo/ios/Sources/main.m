// SMO Mobile —— iOS App 壳。
//
// 现状：报告「移植做到哪一步」+「游戏文件在不在」（用与宿主同一份的 romfs_resolve 逻辑），
// 并把宿主的**日志环形缓冲**显示在屏幕上（未接电脑也能看）。宿主启动在下一轮接入。

#import <UIKit/UIKit.h>
#include <stdio.h>
#include <string.h>
#include "logbuf.h"
#include "romfs_resolve.h"
#include "smoarc.h"
#include "dm.h"
#include "hle.h"
#include "hle_thread.h"
extern void dm_jump_guest(void *arg, void *fn);
void dm_crash_install(void);        // crashlog.c：信号 → PC/LR/帧链写进日志

// ---- 宿主引导（后台线程跑；日志进环形缓冲，前台定时刷新显示） ----

// 数「真实条目」：容器卷（RomFS.smoa.N）与展开标记不算 —— 只有卷不算「已展开」
static unsigned CountRealEntries(NSFileManager *fm, NSString *dir) {
    unsigned n = 0;
    for (NSString *e in [fm contentsOfDirectoryAtPath:dir error:nil] ?: @[]) {
        if ([e hasPrefix:@"RomFS.smoa."] || [e isEqualToString:@".smoa-done"]) continue;
        n++;
    }
    return n;
}

// 5.5 GB 展开要几分钟：每 100 个文件一行，屏幕上能看见在动
static void PackProgress(void *user, unsigned done, const char *name) {
    (void)user;
    if (done % 100) return;
    LOG("  容器展开：%u 个文件（%s）", done, name ? name : "");
}

// 文件头若干字节的十六进制（拷坏时靠它对照：正确的大小与头字节写在 STATUS/说明里）
static NSString *HexHead(NSString *path, NSUInteger n) {
    NSError *err = nil;
    NSFileHandle *h = [NSFileHandle fileHandleForReadingAtPath:path];
    if (!h) {
        NSDictionary *at = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&err];
        (void)at;
        return [NSString stringWithFormat:@"（读不了：%@）", err.localizedDescription ?: @"原因不明"];
    }
    NSData *d = [h readDataOfLength:n];
    [h closeFile];
    if (d.length == 0) return @"（读到 0 字节 —— 文件像是占位符/还没落盘）";
    NSMutableString *s = [NSMutableString string];
    const uint8_t *b = d.bytes;
    for (NSUInteger i = 0; i < d.length; i++) [s appendFormat:@"%02x ", b[i]];
    return s;
}

static void RunHost(void) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *docs = [fm URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                    appropriateForURL:nil create:NO error:nil];
    char ep[1024];
    if (!docs || !dm_exefs_path(ep, sizeof ep, docs.path.UTF8String)) {
        LOG("App：Documents 里没有 ExeFS —— 只显示状态页（把改写产物放进「文件」App 的 SMOMobile/ExeFS）");
        return;
    }
    // ---- 资源容器：把 RomFS.smoa.N 卷展开成真正的目录树 ----
    // 卷在哪、要展开到哪 —— 判断在 smoarc_plan()（C，能进 CI 单测）。它认三种形状，第三种
    // （Documents/<包装目录>/RomFS/RomFS.smoa.N）就是我们自己打的包的形状：上一版不认它，
    // 手机上根本没展开，RomFS 目录里只有两个卷，游戏一读文件就崩。
    // 标记文件 .smoa-done 放在卷旁边（不放 RomFS 目录里，免得被游戏的目录列举撞上）。
    NSFileManager *fm2 = NSFileManager.defaultManager;
    NSURL *docs2 = [fm2 URLForDirectory:NSDocumentDirectory inDomain:NSUserDomainMask
                      appropriateForURL:nil create:NO error:nil];
    if (docs2) {
        char vols[24][1024], target[1024], base[1024];   // 卷上限 24：小卷（512 MB/1 GB）也够用
        unsigned nvols = smoarc_plan(docs2.path.UTF8String, vols, 24, target, sizeof target,
                                     base, sizeof base);
        if (nvols) {
            const char *volp[24];
            for (unsigned i = 0; i < nvols; i++) volp[i] = vols[i];
            NSString *targetS = [NSString stringWithUTF8String:target];
            // 标记放在 RomFS 的**父目录**：三种形状下 target 都是「.../RomFS」，父目录即包装目录。
            // 不放进 RomFS 里面 —— 游戏会列举目录，树里多一个点文件是没必要的风险。
            NSString *marker = [[targetS stringByDeletingLastPathComponent]
                                 stringByAppendingPathComponent:@".smoa-done"];
            if ([fm2 fileExistsAtPath:marker]) {
                LOG("容器：%s 已展开过（.smoa-done），跳过", target);
            } else {
                // 走到这里就必定是「卷在、树没展开」：smoarc_plan 只在确实找到卷时才返回非 0。
                // 上一版把「目录里有东西」当成「已展开」，用户手机上顶层有一个不相干的条目，
                // 于是展开被跳过 —— RomFS 里一个游戏文件都没有。判据不能这么松。
                // 先报每个卷的大小与头字节：拷坏（截断 / 传输过程改了字节 / 拷的是旧文件）时，
                // 这两项立刻就能对照出来（源：卷 0 = 3218914442 字节，头 53 4d 4f 41 00 02；
                // 卷 1 = 2286390220 字节，头 53 4d 4f 41 01 02）。
                for (unsigned i = 0; i < nvols; i++) {
                    NSString *vp = [NSString stringWithUTF8String:volp[i]];
                    NSError *err = nil;
                    NSDictionary *at = [fm2 attributesOfItemAtPath:vp error:&err];
                    LOG("  卷 %u：%llu 字节（源应为 %s），改于 %@，头 %@", i,
                        [at fileSize], i == 0 ? "3218914442" : "2286390220",
                        [at fileModificationDate], HexHead(vp, 8));
                    (void)err;
                }
                [fm2 createDirectoryAtPath:targetS withIntermediateDirectories:YES attributes:nil error:nil];
                LOG("首次运行：展开 RomFS 容器 %u 卷 → %s（5.5 GB，几分钟；每 100 个文件一行）",
                    nvols, target);
                int rc = smoarc_extract_cb(target, volp, nvols, PackProgress, NULL);
                LOG("展开结果：%d（0 = 成功；-1 格式错；-2 读写错）—— %s", rc, smoarc_error());
                if (rc == 0) {
                    [[NSString stringWithFormat:@"vols=%u\n", nvols] writeToFile:marker atomically:YES
                                                                         encoding:NSUTF8StringEncoding error:nil];
                } else {
                    LOG("【红】展开没成功（见上一行的原因）—— 先别进游戏：重拷一遍卷，或看看手机剩余空间");
                }
            }
        }
    }
    // RomFS 里到底有没有东西：只有卷没展开时，进了游戏就是一路读文件失败，最后以莫名其妙的崩溃收场。
    // 这里直接拦下并把原因写清楚（比让用户拿个崩溃报告猜省事得多）。
    {
        char rp[1024];
        if (!dm_romfs_path(rp, sizeof rp, docs.path.UTF8String)) {
            LOG("【红】Documents 里没有 RomFS（放好 RomFS 目录，或 RomFS.smoa.N 卷）");
            return;
        }
        unsigned n = CountRealEntries(fm, [NSString stringWithUTF8String:rp]);
        LOG("RomFS：%s（顶层 %u 个条目）", rp, n);
        if (!n) {
            LOG("【红】RomFS 目录里没有展开的内容（只剩容器卷？）—— 见上面的展开日志");
            return;
        }
        // 完整的 RomFS 只有十几个顶层目录：少的时候把名字列出来，一眼看出是没展开还是拷少了
        if (n <= 16) {
            NSArray *es = [[fm contentsOfDirectoryAtPath:[NSString stringWithUTF8String:rp] error:nil]
                           sortedArrayUsingSelector:@selector(compare:)];
            for (NSString *e in es) LOG("   · %s", e.UTF8String);
        }
    }
    LOG("== SMO 宿主（App 内）：从 %s 装载 ==", ep);
    // 不装 rtld：宿主自己扮演 rtld（映射/重定位/DT_INIT 都在 loader_core.c），而 rtld 的 32 条重定位
    // 落在代码段边界页里，按三段装不了（smo/ios/tools/x0_layout.py）。参考项目同样只装游戏模块。
    static const char *names[] = { "sdk", "subsdk0", "main" };
    // 引导顺序只有一处：host_boot()（与 CLI 宿主同一份）。上一版这里把 run_module_inits 写在了
    // svc_thread_main_init 前面 —— 客体第一个构造函数就去读线程/TLS 环境，真机上一跑就崩。
    void *fn = host_boot(ep, names, 3, "nnMain");
    if (!fn) return;
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
    // 注意：日志里若混进非法 UTF-8，stringWithUTF8String: 会返回 nil；而 nil 一旦进
    // NSMutableArray 就抛 NSInvalidArgumentException 直接终止进程（真机第七崩）。
    // 两层兜底：UTF-8 不行就退到 Latin-1（字节一对一映射，永不失败）。
    NSString *s = [[NSString alloc] initWithBytes:buf length:n encoding:NSUTF8StringEncoding];
    if (!s) s = [[NSString alloc] initWithBytes:buf length:n encoding:NSISOLatin1StringEncoding];
    return s ?: @"（日志读不出来）";
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
    // 日志实时落盘到 Documents/SMO-log-live.txt（崩溃也留得下；导出按钮里也会带上）
    NSString *liveLog = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject
                          stringByAppendingPathComponent:@"SMO-log-live.txt"];
    if (liveLog) dm_log_set_file(liveLog.UTF8String);
    // 系统级 stderr 单独落一个文件：libsystem / malloc / musl 的断言消息默认只走 stderr，
    // 真机上没有终端就永远看不到（真机第七崩的 SIGABRT 就是系统库发的，消息丢了）。
    if (liveLog) {
        NSString *errPath = [liveLog.stringByDeletingLastPathComponent
                             stringByAppendingPathComponent:@"SMO-stderr.txt"];
        if (errPath) freopen(errPath.UTF8String, "w", stderr);
    }
    dm_crash_install();             // 崩溃也留现场（信号 + PC/LR + 帧链 → SMO-log-live.txt）

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
