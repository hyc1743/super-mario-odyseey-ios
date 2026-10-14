// 着色器索引：运行时把「控制段里的着色器 blob」对到离线译好的 MSL。
//
// 键的算法：**FNV-1a 64**，对整个 blob（SPH 头 0x50 字节 + 机器码）算。离线侧
// （smo/gfx/shader_index.py）用同一个算法，所以键一致。
// 为什么不用 sha256：离线那轮用的是 sha256 前 16 位做目录名（那是给文件系统用的），
// 运行时只需要一个能在 ~2 万条目里不撞的 64 位键，FNV-1a 足够，而且宿主侧不需要加密码库。
//
// 索引文件是**文本**（便于人眼核对与 diff）：
//     # 注释行
//     <fnv64 十六进制>\t<stage>\t<msl 文件名>
//
// 注意：**控制段的确切布局还没有真机证据**。已确认的是游戏归档里每个着色器槽的布局
// （+0x00 marker 0x12345678 / +0x30 SPH 0x50 / +0x80 机器码，见 STATUS.md 第七轮），
// 而 AGL 是把归档里的字节拷进控制段的，所以先按同一布局取（nvn_shader_find_control），
// 并在尺寸不符时打印实际头 16 字节 —— 真机日志足以定下布局。
#ifndef SMO_NVN_SHADER_H
#define SMO_NVN_SHADER_H

#include <stddef.h>
#include <stdint.h>

#define NVN_SHADER_MSL_MAX 64

typedef struct {
    uint64_t key;
    uint16_t stage;                 // 0=compute 1=vertex 5=fragment（与 Ryujinx ShaderStage 同号）
    char     msl[NVN_SHADER_MSL_MAX];
} nvn_shader_ent;

// FNV-1a 64（索引与查找共用；离线侧同算法）
uint64_t nvn_shader_key(const uint8_t *p, size_t n);

// 载入索引文件。返回载入条数，-1 表示打不开。
int nvn_shader_load(const char *path);

// 查：p/n 必须是**完整的** sph+code（0x50 + 机器码）
const nvn_shader_ent *nvn_shader_find(const uint8_t *p, size_t n);

// 控制段里找（见头文件开头的说明）：先试游戏归档的槽布局，失败再按 8 字节对齐滑动扫描
// （滑动扫描只在控制段不大时做：上限 64 KiB，避免退化成 O(n²)）。
const nvn_shader_ent *nvn_shader_find_control(const uint8_t *ctrl, size_t n);

unsigned nvn_shader_count(void);
unsigned nvn_shader_hits(void);
void     nvn_shader_reset(void);

#endif
