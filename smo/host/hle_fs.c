// nn::fs：真机经 fsp-srv 服务；宿主直接接宿主文件系统。
// 派生自参考项目 d2r-mobile 的 dm/host/hle_fs.c（MIT），保留语义、去掉其诊断/注入点部分。
//
// 挂载名 → 宿主目录（SMO 版）：
//   rom    → $SMO_ROMFS（我们从 NCA 解出来的 RomFS 目录；只读）
//   save   → $SMO_SAVE（存档目录；事务式：挂载时把已提交版拷成工作目录，Commit 时换名）
//   temp   → $SMO_TEMP（临时区）
// 路径形如 "rom:/data/x"。FileHandle / DirectoryHandle 按值传 8 字节，这里存宿主 fd / DIR*。
// 结构布局按公开 Horizon ABI：
//   DirectoryEntry = {char name[0x301]; pad[3]; s8 type; pad[3]; s64 size}  —— 0x310 字节
//   type: 0 = 目录, 1 = 文件
#include "dm.h"
#include "hle.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define RES_PATH_NOT_FOUND 0x202u      // fs 模块 2 / 描述 1
#define RES_PATH_EXISTS    0x402u      // fs 模块 2 / 描述 2
enum { MODE_READ = 1, MODE_WRITE = 2, MODE_APPEND = 4 };
enum { ENT_DIR = 0, ENT_FILE = 1 };
enum { DIRMODE_DIR = 1, DIRMODE_FILE = 2 };

typedef struct { char name[0x301]; uint8_t pad0[3]; int8_t type; uint8_t pad1[3]; int64_t size; } DirectoryEntry;
_Static_assert(sizeof(DirectoryEntry) == 0x310, "DirectoryEntry 布局必须与 Horizon ABI 一致");

#define MAX_MOUNT 16
typedef struct { char name[32]; char dir[1024]; char commit[1024]; int ro; int txn; } mount_t;
static mount_t g_mnt[MAX_MOUNT];
static int g_nmnt;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_fmode[65536];                 // fd → 打开方式
static uint8_t g_fro[65536];                   // fd → 是否只读挂载上的文件

static uint32_t err_res(const char *op, const char *path) {
    if (errno == ENOENT || errno == ENOTDIR) return RES_PATH_NOT_FOUND;
    if (errno == EEXIST) return RES_PATH_EXISTS;
    LOG("fs %s 失败：%s（errno=%d）", op, path, errno);
    return 0x2A2u;                             // 通用失败
}

// ---------------------------------------------------------------- 平台小工具
#ifdef __MINGW32__
#include <direct.h>                            // _mkdir / _chsize
#define SMO_MKDIR(p) _mkdir(p)
#define SMO_PREAD(fd, b, n, o) (lseek(fd, o, SEEK_SET) < 0 ? -1 : read(fd, b, n))
#define SMO_PWRITE(fd, b, n, o) (lseek(fd, o, SEEK_SET) < 0 ? -1 : write(fd, b, n))
#define SMO_FTRUNCATE(fd, sz) _chsize(fd, sz)
#define SMO_FSYNC(fd) _commit(fd)
#else
#define SMO_MKDIR(p) mkdir(p, 0755)
#define SMO_PREAD(fd, b, n, o) pread(fd, b, n, o)
#define SMO_PWRITE(fd, b, n, o) pwrite(fd, b, n, o)
#define SMO_FTRUNCATE(fd, sz) ftruncate(fd, sz)
#define SMO_FSYNC(fd) fsync(fd)
#endif

// 路径拼接：**超长就响亮地停**，不静默截断（-Wformat-truncation 指的就是这种地方）
static void path_join(char *out, size_t n, const char *dir, const char *name) {
    int k = snprintf(out, n, "%s/%s", dir, name);
    if (k < 0 || (size_t)k >= n) die("路径太长（%zu 字节上限）：%s/%s", n, dir, name);
}

// 目录树拷贝（存档事务在非 Apple 平台上的退化实现；Apple 上走 clonefile）
static int copy_tree(const char *src, const char *dst) {
    if (SMO_MKDIR(dst) && errno != EEXIST) return -1;
    DIR *d = opendir(src);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char a[2048], b[2048];
        path_join(a, sizeof a, src, e->d_name);
        path_join(b, sizeof b, dst, e->d_name);
        struct stat st;
        if (stat(a, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (copy_tree(a, b)) { closedir(d); return -1; } }
        else {
            FILE *fi = fopen(a, "rb");
            if (!fi) continue;
            FILE *fo = fopen(b, "wb");
            if (!fo) { fclose(fi); continue; }
            char buf[65536];
            size_t n;
            while ((n = fread(buf, 1, sizeof buf, fi)) > 0) fwrite(buf, 1, n, fo);
            fclose(fi);
            fclose(fo);
        }
    }
    closedir(d);
    return 0;
}
static void rm_tree(const char *p) {
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char a[2048];
            path_join(a, sizeof a, p, e->d_name);
            rm_tree(a);
        }
        closedir(d);
        rmdir(p);
    } else {
        unlink(p);
    }
}

// ---------------------------------------------------------------- 挂载与路径

static int mount_find(const char *name) {
    for (int i = 0; i < g_nmnt; i++) if (!strcmp(g_mnt[i].name, name)) return i;
    return -1;
}
static int mount_add(const char *name, const char *dir, int ro, int txn) {
    pthread_mutex_lock(&g_lock);
    if (mount_find(name) >= 0) { pthread_mutex_unlock(&g_lock); return 0; }
    if (g_nmnt >= MAX_MOUNT) die("挂载表满");
    mount_t *m = &g_mnt[g_nmnt++];
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->dir, sizeof m->dir, "%s", dir);
    m->ro = ro;
    m->txn = txn;
    pthread_mutex_unlock(&g_lock);
    return 1;
}

// "rom:/data/x" → 宿主路径；返回 0 = 挂载不存在
static int host_path_ex(const char *path, char *out, size_t n, int *ro, int create_parent) {
    const char *colon = strchr(path, ':');
    if (!colon) { LOG("fs 路径没有挂载名：%s", path); return 0; }
    char name[32];
    size_t nl = (size_t)(colon - path);
    if (nl >= sizeof name) return 0;
    memcpy(name, path, nl);
    name[nl] = 0;
    pthread_mutex_lock(&g_lock);
    int i = mount_find(name);
    if (i < 0) { pthread_mutex_unlock(&g_lock); LOG("fs 挂载不存在：%s", name); return 0; }
    const char *rest = colon + 1;
    while (*rest == '/') rest++;
    snprintf(out, n, "%s/%s", g_mnt[i].dir, rest);
    *ro = g_mnt[i].ro;
    pthread_mutex_unlock(&g_lock);
    if (create_parent) {                       // 建父目录（写路径常用）
        for (char *p = out + 1; *p; p++)
            if (*p == '/') { *p = 0; SMO_MKDIR(out); *p = '/'; }
    }
    return 1;
}
#define HP_EX(path, create)                     \
    char hp[2048];                              \
    int ro = 0;                                 \
    if (!host_path_ex((path), hp, sizeof hp, &ro, (create))) return err_res("path", (path))
#define RW() if (ro) { LOG("fs 对只读挂载写入：%s", hp); return 0x2A2u; }

// ---------------------------------------------------------------- 实现（按名接管）

static uint32_t h_QueryRomCache(size_t *out) { *out = 0x1000; return 0; }

static uint32_t h_MountRom(const char *name, void *cache, size_t sz) {
    (void)cache; (void)sz;
    const char *dir = getenv("SMO_ROMFS");
    if (!dir) { LOG("【红】没设 SMO_ROMFS（游戏资源目录）"); return 0x2A2u; }
    mount_add(name, dir, 1, 0);
    LOG("挂载 %s → %s（只读）", name, dir);
    return 0;
}
static uint32_t h_MountTemp(const char *name) {
    const char *dir = getenv("SMO_TEMP");
    if (!dir) dir = "smo_temp";
    SMO_MKDIR(dir);
    mount_add(name, dir, 0, 0);
    return 0;
}
static uint32_t h_EnsureSave(const void *uid) { (void)uid; return 0; }

// 存档：事务式。挂载时把「已提交版」整树拷成工作目录（Apple 上 clonefile 近似零成本），
// 游戏的读写都落在工作目录；Commit 时把工作目录换成已提交版（Apple 上原子换名）。
static uint32_t h_MountSave(const char *name, const void *uid) {
    (void)uid;
    const char *dir = getenv("SMO_SAVE");
    if (!dir) { LOG("【红】没设 SMO_SAVE（存档目录）"); return 0x2A2u; }
    pthread_mutex_lock(&g_lock);
    int i = mount_find(name);
    pthread_mutex_unlock(&g_lock);
    if (i < 0) {
        char commit[1024], work[1100];
        snprintf(commit, sizeof commit, "%s", dir);
        snprintf(work, sizeof work, "%s.work", dir);
        SMO_MKDIR(commit);
        rm_tree(work);                          // 丢弃上次残留
        if (copy_tree(commit, work)) LOG("【红】存档工作目录准备失败：%s", work);
        pthread_mutex_lock(&g_lock);
        if (g_nmnt >= MAX_MOUNT) die("挂载表满");
        mount_t *m = &g_mnt[g_nmnt++];
        snprintf(m->name, sizeof m->name, "%s", name);
        path_join(m->dir, sizeof m->dir, work, ".");   // 仅仅是复制字符串；work 是目录，拼 "." 只为复用检查
        m->dir[strlen(m->dir) - 2] = 0;                // 去掉 "/."

        snprintf(m->commit, sizeof m->commit, "%s", commit);
        m->ro = 0;
        m->txn = 1;
        pthread_mutex_unlock(&g_lock);
        LOG("挂载存档 %s → 工作目录 %s（已提交版 %s）", name, work, commit);
    }
    return 0;
}

static uint32_t h_Commit(const char *name) {
    pthread_mutex_lock(&g_lock);
    int i = mount_find(name);
    if (i < 0 || !g_mnt[i].txn) { pthread_mutex_unlock(&g_lock); return 0; }
    char work[1024], commit[1024], neu[1100], old[1100];
    path_join(work, sizeof work, g_mnt[i].dir, ".");
    work[strlen(work) - 2] = 0;
    path_join(commit, sizeof commit, g_mnt[i].commit, ".");
    commit[strlen(commit) - 2] = 0;
    pthread_mutex_unlock(&g_lock);
    snprintf(neu, sizeof neu, "%s.new", commit);
    snprintf(old, sizeof old, "%s.old", commit);
    rm_tree(neu);
    if (copy_tree(work, neu)) { LOG("【红】存档提交：准备新版本失败（已提交版不变）"); rm_tree(neu); return 0x2A2u; }
#ifdef __APPLE__
    if (renamex_np(neu, commit, RENAME_SWAP)) { LOG("【红】存档提交：原子换名失败 errno=%d", errno); rm_tree(neu); return 0x2A2u; }
    rm_tree(neu);                               // 换下来的是旧版
#else
    // 非 Apple：没有原子换名。先把旧版挪开再换上新版（两步之间被杀会看到中间态，
    // 但下一次挂载会用 .commit 里存在的那个版本，不会新旧混合 ⇒ 退化为「可能丢一个事务」）
    rm_tree(old);
    if (rename(commit, old)) { LOG("【红】存档提交：挪开旧版失败 errno=%d", errno); rm_tree(neu); return 0x2A2u; }
    if (rename(neu, commit)) { LOG("【红】存档提交：换上新版失败 errno=%d", errno); rename(old, commit); return 0x2A2u; }
    rm_tree(old);
#endif
    LOG("存档已提交（%s）", name);
    return 0;
}

static void h_Unmount(const char *name) { (void)name; }   // 挂载表常驻，卸载为空操作

static uint32_t h_OpenFile(uint64_t *out, const char *path, int mode) {
    HP_EX(path, 0);
    int flags;
    if (mode & MODE_WRITE) { RW(); flags = O_RDWR; }
    else flags = O_RDONLY;
    int fd = open(hp, flags);
    if (fd < 0) return err_res("open", hp);
    int slot = fd & 0xFFFF;
    g_fmode[slot] = (uint8_t)mode;
    g_fro[slot] = (uint8_t)ro;
    *out = (uint64_t)fd;
    return 0;
}
static uint32_t h_ReadFile(size_t *out, uint64_t h, int64_t off, void *buf, size_t sz) {
    if (off < 0) { *out = 0; return 0; }
    long r = SMO_PREAD((int)h, buf, sz, (long)off);
    if (r < 0) return err_res("read", "handle");
    *out = (size_t)r;
    return 0;
}
static uint32_t h_WriteFile(uint64_t h, int64_t off, const void *buf, size_t sz, const void *opt) {
    (void)opt;
    if (g_fro[h & 0xFFFF]) return 0x2A2u;
    long r = SMO_PWRITE((int)h, buf, sz, (long)off);
    return r < 0 ? err_res("write", "handle") : 0;
}
static uint32_t h_GetFileSize(int64_t *out, uint64_t h) {
    struct stat st;
    if (fstat((int)h, &st)) return err_res("fstat", "handle");
    *out = st.st_size;
    return 0;
}
static uint32_t h_SetFileSize(uint64_t h, int64_t sz) {
    if (g_fro[h & 0xFFFF]) return 0x2A2u;
    return SMO_FTRUNCATE((int)h, (long)sz) ? err_res("truncate", "handle") : 0;
}
static uint32_t h_FlushFile(uint64_t h) { SMO_FSYNC((int)h); return 0; }
static void h_CloseFile(uint64_t h) {
    g_fmode[h & 0xFFFF] = 0;
    g_fro[h & 0xFFFF] = 0;
    close((int)h);
}
static int h_GetFileOpenMode(uint64_t h) { return g_fmode[h & 0xFFFF]; }

static uint32_t h_CreateFile(const char *path, int64_t sz) {
    HP_EX(path, 1);
    RW();
    int fd = open(hp, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0) return err_res("create", hp);
    if (sz > 0 && SMO_FTRUNCATE(fd, (long)sz)) {
        uint32_t result = err_res("truncate", hp);
        close(fd);
        unlink(hp);
        return result;
    }
    close(fd);
    return 0;
}
static uint32_t h_DeleteFile(const char *path) {
    HP_EX(path, 0);
    RW();
    return unlink(hp) ? err_res("unlink", hp) : 0;
}
static uint32_t h_CreateDirectory(const char *path) {
    HP_EX(path, 1);
    RW();
    if (SMO_MKDIR(hp) == 0 || errno == EEXIST) return 0;
    return err_res("mkdir", hp);
}
static uint32_t h_DeleteDirRec(const char *path) {
    HP_EX(path, 0);
    RW();
    rm_tree(hp);
    return 0;
}
static uint32_t h_RenameFile(const char *from, const char *to) {
    HP_EX(from, 0);
    char hp2[2048];
    int ro2 = 0;
    if (!host_path_ex(to, hp2, sizeof hp2, &ro2, 1)) return err_res("path", to);
    if (ro || ro2) return 0x2A2u;
    return rename(hp, hp2) ? err_res("rename", hp) : 0;
}
static uint32_t h_GetEntryType(int32_t *out, const char *path) {
    HP_EX(path, 0);
    struct stat st;
    if (stat(hp, &st)) return err_res("stat", hp);
    *out = S_ISDIR(st.st_mode) ? ENT_DIR : ENT_FILE;
    return 0;
}

// 目录句柄：这里把 readdir 的结果在打开时一次读出（Horizon 的 ReadDirectory 是按需取）
typedef struct { char **names; int8_t *types; int64_t *sizes; int n, pos; } dir_t;

static uint32_t h_OpenDirectory(uint64_t *out, const char *path, int mode) {
    (void)mode;
    HP_EX(path, 0);
    DIR *d = opendir(hp);
    if (!d) return err_res("opendir", hp);
    dir_t *dt = calloc(1, sizeof *dt);
    int cap = 16;
    dt->names = malloc(cap * sizeof *dt->names);
    dt->types = malloc(cap);
    dt->sizes = malloc(cap * sizeof *dt->sizes);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (dt->n == cap) {
            cap *= 2;
            dt->names = realloc(dt->names, cap * sizeof *dt->names);
            dt->types = realloc(dt->types, cap);
            dt->sizes = realloc(dt->sizes, cap * sizeof *dt->sizes);
        }
        char a[2048];
        struct stat st;
        path_join(a, sizeof a, hp, e->d_name);
        int isdir = (stat(a, &st) == 0) ? S_ISDIR(st.st_mode) : 0;
        dt->names[dt->n] = strdup(e->d_name);
        dt->types[dt->n] = (int8_t)(isdir ? ENT_DIR : ENT_FILE);
        dt->sizes[dt->n] = isdir ? 0 : (int64_t)st.st_size;
        dt->n++;
    }
    closedir(d);
    dt->pos = 0;
    *out = (uint64_t)(uintptr_t)dt;
    return 0;
}
static uint32_t h_ReadDirectory(int64_t *count, DirectoryEntry *ents, uint64_t hh, int64_t max) {
    dir_t *dt = (dir_t *)(uintptr_t)hh;
    if (!dt) return 0xE401u;
    int n = 0;
    while (n < max && dt->pos < dt->n) {
        DirectoryEntry *de = &ents[n];
        memset(de, 0, sizeof *de);
        snprintf(de->name, sizeof de->name, "%s", dt->names[dt->pos]);
        de->type = dt->types[dt->pos];
        de->size = dt->sizes[dt->pos];
        dt->pos++;
        n++;
    }
    *count = n;
    return 0;
}
static void h_CloseDirectory(uint64_t hh) {
    dir_t *dt = (dir_t *)(uintptr_t)hh;
    if (!dt) return;
    for (int i = 0; i < dt->n; i++) free(dt->names[i]);
    free(dt->names); free(dt->types); free(dt->sizes); free(dt);
}

// ---------------------------------------------------------------- 登记表
// 名字取自 smo/data/hle_imports.tsv（改清单后这里要对得上，hle.c 装载时会校验）
static const hle_impl_ent g_fs_impl[] = {
    {"_ZN2nn2fs22QueryMountRomCacheSizeEPm", (void *)h_QueryRomCache, "fsp-srv；rom 直读宿主目录，缓存给最小值"},
    {"_ZN2nn2fs8MountRomEPKcPvm", (void *)h_MountRom, "fsp-srv；rom → 宿主 RomFS 目录（只读）"},
    {"_ZN2nn2fs21MountTemporaryStorageEPKc", (void *)h_MountTemp, "fsp-srv；临时区 → 宿主目录"},
    {"_ZN2nn2fs14EnsureSaveDataERKNS_7account3UidE", (void *)h_EnsureSave, "fsp-srv；存档目录在挂载时建"},
    {"_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE", (void *)h_MountSave, "fsp-srv；存档 → 工作目录（事务式）"},
    {"_ZN2nn2fs6CommitEPKc", (void *)h_Commit, "fsp-srv；事务式提交：工作目录换成已提交版"},
    {"_ZN2nn2fs14CommitSaveDataEPKc", (void *)h_Commit, "fsp-srv；同上（SMO 用的是这个名字）"},
    {"_ZN2nn2fs7UnmountEPKc", (void *)h_Unmount, "fsp-srv；挂载表常驻，卸载为空操作"},
    {"_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", (void *)h_OpenFile, "fsp-srv → open(2)"},
    {"_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", (void *)h_ReadFile, "fsp-srv → pread(2)"},
    {"_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvmRKNS0_10ReadOptionE", (void *)h_ReadFile, "fsp-srv → pread(2)（带 ReadOption，选项忽略）"},
    {"_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE", (void *)h_WriteFile, "fsp-srv → pwrite(2)"},
    {"_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE", (void *)h_GetFileSize, "fsp-srv → fstat(2)"},
    {"_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl", (void *)h_SetFileSize, "fsp-srv → ftruncate(2)"},
    {"_ZN2nn2fs9FlushFileENS0_10FileHandleE", (void *)h_FlushFile, "fsp-srv → fsync(2)"},
    {"_ZN2nn2fs9CloseFileENS0_10FileHandleE", (void *)h_CloseFile, "fsp-srv → close(2)"},
    {"_ZN2nn2fs15GetFileOpenModeENS0_10FileHandleE", (void *)h_GetFileOpenMode, "fsp-srv；打开方式由宿主记录"},
    {"_ZN2nn2fs10CreateFileEPKcl", (void *)h_CreateFile, "fsp-srv → open(O_CREAT|O_EXCL)"},
    {"_ZN2nn2fs10DeleteFileEPKc", (void *)h_DeleteFile, "fsp-srv → unlink(2)"},
    {"_ZN2nn2fs15CreateDirectoryEPKc", (void *)h_CreateDirectory, "fsp-srv → mkdir(2)"},
    {"_ZN2nn2fs26DeleteDirectoryRecursivelyEPKc", (void *)h_DeleteDirRec, "fsp-srv → 递归删除（只在可写挂载内）"},
    {"_ZN2nn2fs10RenameFileEPKcS2_", (void *)h_RenameFile, "fsp-srv → rename(2)"},
    {"_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", (void *)h_GetEntryType, "fsp-srv → stat(2)"},
    {"_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci", (void *)h_OpenDirectory, "fsp-srv → opendir(3)"},
    {"_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl", (void *)h_ReadDirectory, "fsp-srv → readdir(3)（打开时一次读完）"},
    {"_ZN2nn2fs14CloseDirectoryENS0_15DirectoryHandleE", (void *)h_CloseDirectory, "fsp-srv → closedir(3)"},
};

static void __attribute__((constructor)) fs_reg(void) { hle_impl_register(g_fs_impl, SMO_ARRAY_LEN(g_fs_impl)); }
