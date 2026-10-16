// Exact offline shader index. GPU-data lookup requires V2 span metadata and
// validates accesses against tracked CPU-backed pools. Archive layout is not
// assumed to describe the runtime control pointer; that pointer is never read.
#ifndef SMO_NVN_SHADER_H
#define SMO_NVN_SHADER_H

#include <stddef.h>
#include <stdint.h>

#define NVN_SHADER_MSL_MAX 64

typedef struct {
    uint64_t key;
    uint32_t bytes;               /* Exact offline SPH+code span, zero for legacy rows. */
    uint64_t prefix, code_key, code_prefix;
    uint16_t stage;                 // 0=compute 1=vertex 5=fragment（与 Ryujinx ShaderStage 同号）
    uint8_t ambiguous;
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

/* Only accesses tracked CPU-backed GPU pool ranges. Never reads control pointers.
 * Offsets 0 and 0x30 are candidates, not assumed runtime layouts. A result
 * requires a unique exact span hash match, not merely a prefix match. */
typedef struct {
    unsigned candidates, matches, offset, code_only;
    uint32_t bytes;
} nvn_shader_match;
const nvn_shader_ent *nvn_shader_find_gpu(uint64_t address, nvn_shader_match *match);

unsigned nvn_shader_count(void);
unsigned nvn_shader_hits(void);
void     nvn_shader_reset(void);

#endif
