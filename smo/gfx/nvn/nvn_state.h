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
    uint64_t v[12];
} nvn_st_ent;

nvn_st_ent *nvn_st_new(void *obj, uint32_t type);
nvn_st_ent *nvn_st_get(const void *obj, uint32_t type);
void        nvn_st_del(void *obj, uint32_t type);
unsigned    nvn_st_count(void);
unsigned    nvn_st_reused(void);
void        nvn_st_reset(void);
void       *nvn_st_iter(unsigned i, uint32_t *type);

#endif
