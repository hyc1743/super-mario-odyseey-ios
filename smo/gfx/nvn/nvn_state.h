// NVN 宿主侧状态表（nvn_state.c 的实现细节，nvn_core.c 也要用）。
// 结构体定义只在这里一份：两个 TU 各写一份「相同定义」也能编过，但改一处漏一处是迟早的事。
#ifndef SMO_NVN_STATE_H
#define SMO_NVN_STATE_H

#include <stdint.h>

typedef struct {
    void    *key;
    uint32_t type;
    uint8_t  used;
    uint8_t  pad[3];
    uint64_t v[16];
} nvn_st_ent;

// Program state: device at v[0], count at v[1], six copied data/control pairs.
enum { NVN_PROG_NSTAGES = 1, NVN_PROG_STAGE_BASE = 2 };
typedef struct { uint64_t data; const void *control; } nvn_shader_data;
_Static_assert(sizeof(nvn_shader_data) == 16, "NVNshaderData ABI");

// Entry pointers survive index growth and removal of other objects. Callers
// synchronize use/destruction of the same object; reset invalidates all entries.
nvn_st_ent *nvn_st_new(void *obj, uint32_t type);
nvn_st_ent *nvn_st_get(const void *obj, uint32_t type);
/* 退休表：Finalize 掉的资源仍可能被「录制时引用、提交时已释放」的命令用到。
 * 真机 NVN 按地址访问、不校验生命周期；我们把状态抄一份留在这里，供渲染路径回落查询。
 * 公开 API（Get / Map / Wait 等）仍然按「已 Finalize = 不可用」处理 —— nvn_selftest 有断言盯着。 */
void nvn_st_retire(const nvn_st_ent *entry);
nvn_st_ent *nvn_st_get_retired(const void *obj, uint32_t type);
void        nvn_st_del(void *obj, uint32_t type);
unsigned    nvn_st_count(void);
unsigned    nvn_st_reused(void);
void        nvn_st_reset(void);
void       *nvn_st_iter(unsigned i, uint32_t *type);
/* Find a live memory pool containing a complete GPU address range. The caller
 * must keep the pool alive until GPU completion, as required by NVN. */
int nvn_st_pool_range(uint64_t address, uint64_t size, uint64_t *base, uint64_t *length);
uint64_t nvn_st_resource_serial(void);

#endif
