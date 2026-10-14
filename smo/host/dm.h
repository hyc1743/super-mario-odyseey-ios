// SMO 宿主公共头（多模块装载）。
// 派生自参考项目 d2r-mobile 的 dm/host/dm.h（MIT），按本项目的模块集合与产物格式调整。
//
// 装载器扮演 Horizon 的 rtld：映射模块映像、占线程槽、登记宿主门、跨模块重定位、按名解析 import。
// 与参考项目一致的两条硬约定：
//   1. 代码区（原 text + 补丁区）一拷进来就设成只读+执行，此后装载器只写 data
//      —— iOS 上这两块要作为**签名代码段**原样内嵌，运行时不许改代码；
//   2. 线程槽号不写死在改写产物里：产物里的键页只存「槽号 × 8」，装载器把**实际**键号填进去。
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MOD 8
#define LOG(...) do { fprintf(stderr, "[smo] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define die(...) do { LOG("FATAL " __VA_ARGS__); abort(); } while (0)

typedef struct { uint32_t st_name; uint8_t st_info, st_other; uint16_t st_shndx; uint64_t st_value, st_size; } Elf64_Sym;
typedef struct { int64_t d_tag; union { uint64_t d_val, d_ptr; } d_un; } Elf64_Dyn;
typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } Elf64_Rela;
#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

enum { DT_RELA = 7, DT_RELASZ = 8, DT_PLTRELSZ = 2, DT_JMPREL = 23, DT_SYMTAB = 6, DT_STRTAB = 5,
       DT_INIT = 12, DT_INIT_ARRAY = 25, DT_INIT_ARRAYSZ = 27 };
enum { R_AARCH64_ABS64 = 257, R_AARCH64_GLOB_DAT = 1025, R_AARCH64_JUMP_SLOT = 1026, R_AARCH64_RELATIVE = 1027 };

// 键页下标顺序必须与改写器 patch_nso.py 的 KEY_INDEX 一致
enum { KEY_K18, KEY_KTLS, KEY_KTLR, KEY_KGATE, KEY_KRES0, KEY_KRES1, KEY_KASPECT, KEY_KASPECTUI, KEY_N };

// 一个模块的运行时状态
typedef struct {
    char name[16];
    uint8_t *base;                 // 映像（含补丁区）在宿主地址空间的首址
    size_t size;
    uint64_t text_end, ro_off, data_off, patch_off, key_page_off, key_page_size;   // 来自改写器 fixups 头
    unsigned key_idx[KEY_N];
    const Elf64_Dyn *dyn;
    const Elf64_Sym *symtab;
    const char *strtab;
    unsigned nsym;
    uint32_t *symh; unsigned symh_mask;    // 已定义符号的名字哈希表（开放寻址，存符号下标）
    uint64_t init, init_array, init_arraysz;
} module_t;

extern module_t g_mod[MAX_MOD];
extern int g_nmod;

// ---- 平台层：Apple 上在 loader_apple.c 里实现；本机自检用 malloc 版本（loader_selftest.c）----
void *dm_image_alloc(size_t size);           // 为映像分配内存（Apple：按 guest 地址预留区切出）
void dm_code_protect(void *p, size_t n);     // 代码区设为只读+执行（本机自检：空实现）
void dm_ro_protect(void *p, size_t n);       // 键页设为只读

// ---- 宿主提供：import 解析（HLE / 桩）与门登记 ----
uint64_t dm_hle_force(const char *name);     // 服务层强制接管（对应参考项目 hle.h 的 HLE_FORCE）：命中即覆盖模块内定义
uint64_t resolve_import(module_t *m, const char *name);
void gate_register(uint64_t ret_addr, const char *kind, int arg, uint64_t orig);
uint64_t dm_slot_key(unsigned i);            // 第 i 个键的实际槽号（Apple：pthread TSD 键；自检：假槽号）

// ---- 核心（loader_core.c）----
void load_modules(const char *dir, const char *const *names, int n);
void *module_sym(module_t *m, const char *name);
void *guest_sym(const char *name);
module_t *module_of(const void *p);
void install_key_page(module_t *m);
void relocate(module_t *m);
void parse_dynamic(module_t *m);
void read_layout(module_t *m, const char *dir);
void register_gates(module_t *m, const char *dir);
unsigned dm_gate_count(void);
void run_module_inits(void);
// ---- 平台层的页操作（loader_apple.c：mmap/mprotect；自检里用朴素实现）----
void *dm_as_reserve(size_t size);            // 预留一段不可访问的客体地址空间
void dm_as_protect(void *p, size_t n, int rw);   // rw=1 放开读写，0 收回
void *dm_host_stack_top(void);               // 宿主线程栈（QueryMemory 回报栈用）
size_t dm_host_stack_size(void);
// ---- svc 层 ----
void mem_init(void);
int svc_dispatch(unsigned num, uint64_t *regs);   // 返回 0 = 处理了，-1 = 不是这一族
uint64_t dm_app_pool(void);
// 内存族的可观测读出口（诊断/自检用）
uint64_t dm_guest_heap_base(void);
uint64_t dm_guest_heap_cur(void);
int dm_guest_heap_rejects(void);
void host_thread_keys_create(void);
void guest_thread_init(void);
void unimpl_report(void);
