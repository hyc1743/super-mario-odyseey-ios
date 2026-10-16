// fs 服务单测：不需要游戏文件（自己造一棵 romfs 与一个存档目录）。
//
// 检查的都是启动/运行时会走的路径：
//   1. 挂载 + 路径映射（"rom:/a/b" → 宿主目录）
//   2. 打开/读/取大小/关闭；不存在 → 0x202（不是通用失败，游戏会按它分支）
//   3. 只读挂载上写入必须被拒
//   4. 目录：GetEntryType、OpenDirectory/ReadDirectory，**DirectoryEntry 布局（0x310）逐字段**
//   5. 存档事务：写到工作目录、Commit 后已提交目录里能看到
//   6. 真实现的名字必须都在生成的自强接管清单里（hle.c 装载时校验，名字打错就红）
#include "../dm.h"
#include "../hle.h"
#include <sys/stat.h>
#ifdef __MINGW32__
#include <direct.h>
#define SMO_MKDIR_TEST(p) _mkdir(p)
// mingw 没有 setenv：用 _putenv_s 顶上（只为自检用）
static void setenv(const char *k, const char *v, int ow) { (void)ow; _putenv_s(k, v); }
#else
#define SMO_MKDIR_TEST(p) mkdir((p), 0755)
#endif

static int fails;
// 注意：条件**只能求值一次**。我第一版写成 printf(...(cond)...); if (!(cond)) ...
// 会把带副作用的断言跑两遍（日志成对出现、目录游标被走到底），是真实踩过的坑。
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

// ---- hle.c / 平台层的桩 ----
void unimpl_stubs(void) { }
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
int svc_thread(int n, uint64_t *r) { (void)n; (void)r; return 0; }
// gate_register / resolve_import 由 hle.c 提供

// ---- 被测函数（hle_fs.c 里是 static，所以按名取）----
typedef uint32_t (*fn_mount)(const char *, void *, size_t);
typedef uint32_t (*fn_mountsave)(const char *, const void *);
typedef uint32_t (*fn_open)(uint64_t *, const char *, int);
typedef uint32_t (*fn_read)(size_t *, uint64_t, int64_t, void *, size_t);
typedef uint32_t (*fn_write)(uint64_t, int64_t, const void *, size_t, const void *);
typedef uint32_t (*fn_size)(int64_t *, uint64_t);
typedef void (*fn_close)(uint64_t);
typedef uint32_t (*fn_type)(int32_t *, const char *);
typedef uint32_t (*fn_opendir)(uint64_t *, const char *, int);
typedef uint32_t (*fn_readdir)(int64_t *, void *, uint64_t, int64_t);
typedef void (*fn_closedir)(uint64_t);
typedef uint32_t (*fn_commit)(const char *);
typedef uint32_t (*fn_create)(const char *, int64_t);

#define GET(cname) ((void *)dm_hle_force_lookup(cname))

// 路径拼接：gcc 的 -Wformat-truncation 对「未知长度的 %s」会警告（-Werror 下就是编译失败，
// Linux 上第一次 CI 就是这么红的），所以拼进 helper 里显式检查。
static void jp(char *out, size_t n, const char *a, const char *b, const char *c) {
    int k = snprintf(out, n, "%s%s%s", a, b, c ? c : "");
    if (k < 0 || (size_t)k >= n) { printf("路径太长"); exit(2); }
}

static void mkfile(const char *p, const char *data) {
    FILE *f = fopen(p, "wb");
    if (!f) { printf("  【红】建文件失败 %s\n", p); fails++; return; }
    fwrite(data, 1, strlen(data), f);
    fclose(f);
}

int main(void) {
    const char *tmp = getenv("TEMP") ? getenv("TEMP") : "/tmp";
    char rom[512], save[512], temp[512], p[600];
    jp(rom, sizeof rom, tmp, "/smo_fs_test/rom", NULL);
    jp(save, sizeof save, tmp, "/smo_fs_test/save", NULL);
    jp(temp, sizeof temp, tmp, "/smo_fs_test/temp", NULL);
    // 先清干净：不清的话上一轮留下的文件会让 CreateFile 报 EEXIST、目录项数也多出来
    //（我第一次跑就是踩了这个：实现是对的，测试不可重入）
    jp(p, sizeof p, tmp, "/smo_fs_test", NULL);
#ifdef __MINGW32__
    { char cmd[700]; snprintf(cmd, sizeof cmd, "rmdir /s /q \"%s\" >NUL 2>NUL", p); system(cmd); }
#else
    { char cmd[700]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", p); system(cmd); }
#endif
    // 顺序要紧：mkdir 不会替你建父目录（Windows 与 Linux 都一样，我第一版就写反了）
    SMO_MKDIR_TEST(p);
    SMO_MKDIR_TEST(rom);
    SMO_MKDIR_TEST(save);
    SMO_MKDIR_TEST(temp);
    jp(p, sizeof p, rom, "/System", NULL);
    SMO_MKDIR_TEST(p);
    char f1[2048], f2[2048];
    jp(f1, sizeof f1, p, "/Version.txt", NULL);
    jp(f2, sizeof f2, p, "/data.bin", NULL);
    mkfile(f1, "SMO-1.0.0");
    mkfile(f2, "0123456789");

    setenv("SMO_ROMFS", rom, 1);
    setenv("SMO_SAVE", save, 1);
    setenv("SMO_TEMP", temp, 1);
    // 注意：hle_fs.c 是在 constructor 里登记的，所以这里必须先触发一次表装载
    (void)dm_hle_force_lookup("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci");
    printf("[smo] 接管清单 %u 条，其中真实现 %u 条\n", g_hle_force_n, hle_impl_count());

    printf("== 1. 挂载与路径映射 ==\n");
    uint32_t r = ((fn_mount)GET("_ZN2nn2fs22QueryMountRomCacheSizeEPm")) ? 0 : 1;
    size_t cache = 0;
    CHK(((uint32_t (*)(size_t *))GET("_ZN2nn2fs22QueryMountRomCacheSizeEPm"))(&cache) == 0 && cache > 0,
        "QueryMountRomCacheSize 返回非 0 缓存");
    CHK(((fn_mount)GET("_ZN2nn2fs8MountRomEPKcPvm"))("rom", NULL, 0) == 0, "挂载 rom（只读）");
    CHK(((fn_mount)GET("_ZN2nn2fs8MountRomEPKcPvm"))("content", NULL, 0) == 0, "挂载游戏实际使用的 content 名称");
    uint64_t content_file = 0;
    CHK(((fn_open)GET("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"))(&content_file, "content:/System/Version.txt", 1) == 0,
        "content 挂载能打开 SMO_ROMFS 中的资源");
    if (content_file) ((fn_close)GET("_ZN2nn2fs9CloseFileENS0_10FileHandleE"))(content_file);
    CHK(((uint32_t (*)(const char *))GET("_ZN2nn2fs21MountTemporaryStorageEPKc"))("temp") == 0, "挂载 temp");
    CHK(((fn_mountsave)GET("_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE"))("save", NULL) == 0, "挂载存档（事务式）");
    (void)r;

    printf("== 2. 打开 / 读 / 取大小 ==\n");
    uint64_t h = 0;
    CHK(((fn_open)GET("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"))(&h, "rom:/System/Version.txt", 1) == 0, "打开 rom:/System/Version.txt");
    int64_t sz = 0;
    CHK(((fn_size)GET("_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE"))(&sz, h) == 0 && sz == 9, "大小 = 9");
    char buf[32] = {0};
    size_t got = 0;
    CHK(((fn_read)GET("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm"))(&got, h, 0, buf, sizeof buf) == 0 && got == 9
        && !memcmp(buf, "SMO-1.0.0", 9), "读出内容一致");
    size_t got2 = 0;
    CHK(((fn_read)GET("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm"))(&got2, h, 4, buf, 3) == 0 && got2 == 3
        && !memcmp(buf, "1.0", 3), "带偏移读一致（pread 语义）");
    ((fn_close)GET("_ZN2nn2fs9CloseFileENS0_10FileHandleE"))(h);

    printf("== 3. 不存在的文件 → 0x202 ==\n");
    uint64_t h2 = 0;
    CHK(((fn_open)GET("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"))(&h2, "rom:/System/Nope.txt", 1) == 0x202,
        "PathNotFound = 0x202（不是通用失败）");
    int32_t ty = -1;
    CHK(((fn_type)GET("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc"))(&ty, "rom:/System/Nope.txt") == 0x202,
        "GetEntryType 同样 0x202");

    printf("== 4. 只读挂载拒绝写入 ==\n");
    uint64_t h3 = 0;
    CHK(((fn_open)GET("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"))(&h3, "rom:/System/Version.txt", 2) != 0,
        "以写方式打开只读挂载上的文件被拒");
    CHK(((fn_create)GET("_ZN2nn2fs10CreateFileEPKcl"))("rom:/System/New.txt", 0) != 0, "在只读挂载上建文件被拒");

    printf("== 5. 目录 ==\n");
    CHK(((fn_type)GET("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc"))(&ty, "rom:/System") == 0 && ty == 0,
        "GetEntryType：目录 = 0");
    CHK(((fn_type)GET("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc"))(&ty, "rom:/System/Version.txt") == 0 && ty == 1,
        "GetEntryType：文件 = 1");
    uint64_t dh = 0;
    CHK(((fn_opendir)GET("_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci"))(&dh, "rom:/System", 1) == 0, "打开目录");
    uint8_t ent[0x310 * 8];
    int64_t cnt = 0;
    CHK(((fn_readdir)GET("_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl"))(&cnt, ent, dh, 8) == 0,
        "读目录");
    CHK(cnt == 2, "目录里有 2 项");
    int seen_file = 0, seen_size_ok = 0;
    for (int i = 0; i < cnt; i++) {
        const uint8_t *e = ent + i * 0x310;
        const char *nm = (const char *)e;
        int8_t t = (int8_t)e[0x304];
        int64_t z = (int64_t)(*(const uint64_t *)(e + 0x308));
        if (t == 1) { seen_file++; if (z == 9 || z == 10) seen_size_ok++; }
        if (!strcmp(nm, "Version.txt")) CHK(z == 9, "目录项里 Version.txt 的 size = 9（布局对齐）");
    }
    CHK(seen_file == 2, "两项都是文件（type = 1，偏移 0x304）");
    CHK(seen_size_ok == 2, "两项的 size 都读到了（偏移 0x308）");
    ((fn_closedir)GET("_ZN2nn2fs14CloseDirectoryENS0_15DirectoryHandleE"))(dh);

    printf("== 6. 存档事务 ==\n");
    uint64_t sh = 0;
    CHK(((fn_create)GET("_ZN2nn2fs10CreateFileEPKcl"))("save:/save.dat", 0) == 0, "在存档里建文件");
    CHK(((fn_open)GET("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci"))(&sh, "save:/save.dat", 2) == 0, "以写方式打开");
    CHK(((fn_write)GET("_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE"))(sh, 0, "SAVEDATA", 8, NULL) == 0, "写入");
    ((fn_close)GET("_ZN2nn2fs9CloseFileENS0_10FileHandleE"))(sh);
    jp(p, sizeof p, save, "/save.dat", NULL);
    FILE *chk = fopen(p, "rb");
    CHK(chk == NULL, "提交之前，已提交目录里看不到新文件（写落在工作目录）");
    if (chk) fclose(chk);
    CHK(((fn_commit)GET("_ZN2nn2fs6CommitEPKc"))("save") == 0, "Commit 成功");
    chk = fopen(p, "rb");
    CHK(chk != NULL, "提交之后已提交目录里有这个文件");
    if (chk) {
        char b[16] = {0};
        size_t n = fread(b, 1, sizeof b, chk);
        fclose(chk);
        CHK(n == 8 && !memcmp(b, "SAVEDATA", 8), "提交后的内容一致");
    }

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
