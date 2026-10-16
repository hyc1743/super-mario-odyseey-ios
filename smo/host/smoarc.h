// 资源容器：把 RomFS 的几千个小文件装进 1~2 个大文件，拷进手机时只有两三个文件。
//
// 为什么这么做（与参考项目的 .data 同理）：iOS 上拷几千个小文件非常慢（每个文件的文件系统开销），
// 大文件则接近磁盘顺序速度。格式刻意做简单（无压缩 —— .szs 本身已经是 Yaz0 压缩过的）：
//
//   卷头:  "SMOA" u8(卷号) u8(卷总数)
//   条目:  u16(名字字节数) u32(数据字节数) u32(crc32) 名字 数据        ← 条目不跨卷
//   结尾:  每个卷尾部一个 "END!" 标记（写完校验用）
//
// 展开器：App 首次启动时把卷解开到 Documents/RomFS，之后游戏照常读目录（fs 层不用改）。
// 平台无关（只用到 stdio/zlib 的 crc32），CI 可测。
#ifndef SMOARC_H
#define SMOARC_H
#include <stddef.h>
#include <stdint.h>

// CRC32（打包脚本同算法校验用）
uint32_t crc_buf(const void *p, size_t n);

// 展开卷到 dir（路径里可含子目录）。返回 0 成功；-1 格式错；-2 IO 错。
int smoarc_extract(const char *dir, const char *const *volumes, unsigned n);

// 带进度的版本：每展开一个文件回调一次（done = 已完成个数，从 1 开始；name = 该文件相对路径）。
// 用途：5.5 GB 的展开要几分钟，App 里得能看到进度（不然看起来像卡死）。cb 传 NULL 等价于 smoarc_extract。
typedef void (*smoarc_progress_fn)(void *user, unsigned done, const char *name);
int smoarc_extract_cb(const char *dir, const char *const *volumes, unsigned n,
                      smoarc_progress_fn cb, void *user);

// 卷组规划：在 docs 下找 RomFS.smoa.0 卷，返回卷数（0 = 没找到）。
// 认三种形状（前两种是历史布局，第三种是 prepare_phone_package.py 打的包的形状）：
//   docs/RomFS.smoa.N                 → target = docs/RomFS
//   docs/<一层目录>/RomFS.smoa.N        → target = docs/<一层目录>/RomFS
//   docs/<一层目录>/RomFS/RomFS.smoa.N → target = docs/<一层目录>/RomFS
// volumes 是调用方给的 [max][1024]；target/base 写展开目标目录与卷所在目录（标记文件放 base）。
// 为什么这段判断写在 C 里：它是「App 会不会展开」的唯一依据，而它曾经写错过（包里的形状不认）——
// 放这里才能进 CI 单测。
unsigned smoarc_plan(const char *docs_dir, char (*volumes)[1024], unsigned max,
                     char *target, size_t tcap, char *base, size_t bcap);

#endif
