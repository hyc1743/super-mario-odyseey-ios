// 装载器核心（平台无关部分）—— 派生自参考项目 d2r-mobile 的 dm/host/loader.c（MIT）。
//
// 与参考项目逐条对应的部分：
//   读改写器表头（read_layout）       ← fixups.tsv 的 # 头行（段边界、键页、槽号）
//   装键页（install_key_page）        ← 把「实际键号 × 8」写进产物里的键页，再设只读
//   登记宿主门（register_gates）      ← gates.tsv 的每一行（返回点、种类、参数、原指令地址）
//   解析动态段（parse_dynamic）       ← MOD0 + DT_SYMTAB/STRTAB/INIT/INIT_ARRAY
//   符号哈希表 + 线性对照（DM_SYMCHECK 的做法我们保留）
//   重定位（relocate）                ← RELATIVE / ABS64 / GLOB_DAT / JUMP_SLOT 四类
//
// 本文件刻意不碰任何 Apple 头：平台相关的三件事（分配映像、设段权限、取键号）都在 dm.h 里声明，
// 于是同一份代码既能编进 iOS App，也能在本机当自检跑（loader_selftest.c）。
#include "dm.h"
#include <fcntl.h>

module_t g_mod[MAX_MOD];
int g_nmod;

static unsigned g_gates;
static uint32_t g_symchk_n, g_symchk_bad;

// ------------------------------------------------------------------ 读文件

static void build_symhash(module_t *m);

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    if (out_size) *out_size = (size_t)n;
    return d;
}

// ------------------------------------------------------------------ 改写器表头

// fixups.tsv 的头行形如 `# PATCH_OFF 0x25b0000`；**表头以外不允许有行**（旧版产物会在装载期改代码，直接拒绝）
void read_layout(module_t *m, const char *dir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.fixups.tsv", dir, m->name);
    FILE *f = fopen(path, "r");
    if (!f) die("打不开 %s", path);
    static const char *kn[KEY_N] = {"K18", "KTLS", "KTLR", "KGATE", "KRES0", "KRES1", "KASPECT", "KASPECTUI"};
    char line[256], key[64];
    unsigned long v;
    unsigned seen = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "# %63s %lx", key, &v) != 2)
            die("%s: 产物表头有无法解析的行（旧改写器产物？）：%.60s", path, line);
        if (!strcmp(key, "PATCH_OFF")) m->patch_off = v;
        else if (!strcmp(key, "KEY_PAGE_OFF")) m->key_page_off = v;
        else if (!strcmp(key, "KEY_PAGE_SIZE")) m->key_page_size = v;
        else if (!strcmp(key, "TEXT_END")) m->text_end = v;
        else if (!strcmp(key, "RO_OFF")) m->ro_off = v;
        else if (!strcmp(key, "DATA_OFF")) m->data_off = v;
        else if (!strncmp(key, "KEY_", 4)) {
            for (int k = 0; k < KEY_N; k++) {
                size_t n = strlen(kn[k]);
                if (!strncmp(key + 4, kn[k], n) && !strcmp(key + 4 + n, "_IDX")) {
                    // 槽号写在行尾的制表符之后
                    char *tab = strrchr(line, '\t');
                    if (!tab) die("%s: 键页行缺槽号：%s", path, line);
                    m->key_idx[k] = (unsigned)strtoul(tab + 1, NULL, 10);
                    seen |= 1u << k;
                }
            }
        }
    }
    fclose(f);
    if (!m->patch_off || (m->key_page_off % 0x4000) || m->key_page_size != 0x4000 || seen != 0xff)
        die("%s: 表头不完整（patch=%#llx keypage=%#llx/%#llx seen=%#x）", path,
            (unsigned long long)m->patch_off, (unsigned long long)m->key_page_off,
            (unsigned long long)m->key_page_size, seen);
}

void install_key_page(module_t *m) {
    if (m->key_page_off + m->key_page_size > m->size) die("%s: 键页越界", m->name);
    uint64_t *page = (uint64_t *)(m->base + m->key_page_off);
    memset(page, 0, m->key_page_size);
    for (unsigned i = 0; i < KEY_N; i++) page[m->key_idx[i]] = dm_slot_key(i) * 8;
    // 判断标准是「键建没建」，不是「槽值非 0」：客体完全可能把某个槽写成 0（合法状态），
    // 用槽值反推会把「没建键」和「槽值是 0」混在一起。macOS 上第一次真跑就撞上了这个坑
    if (!dm_slot_keys_ready())
        die("%s: 宿主线程键没建（host_thread_keys_create 没调？）", m->name);
    dm_ro_protect(m->base + m->key_page_off, m->key_page_size);
}

// ------------------------------------------------------------------ 门

// 门种类名（索引 = dm.h 的 DM_GK_*；与 gates.tsv 的字符串一致）
const char *const dm_gate_kind_name[DM_GK_N] = { "svc", "cntpct" };

void register_gates(module_t *m, const char *dir) {
    // 内嵌表：门表直接来自生成物（iOS 上没有 gates.tsv）
    if (m->embed) {
        const dm_embed_mod *e = m->embed;
        for (unsigned i = 0; i < e->n_gates; i++) {
            const dm_embed_gate *g = &e->gates[i];
            if (g->kind >= DM_GK_N) die("%s: 内嵌门种类越界 %u", m->name, g->kind);
            gate_register((uint64_t)(uintptr_t)(m->base + g->ret), dm_gate_kind_name[g->kind],
                          (int)g->arg, g->orig);
        }
        g_gates += e->n_gates;
        LOG("%s: 门 %u 个（内嵌）", m->name, e->n_gates);
        return;
    }
    char path[1024], line[256];
    snprintf(path, sizeof path, "%s/%s.gates.tsv", dir, m->name);
    FILE *f = fopen(path, "r");
    if (!f) { LOG("%s: 没有 gates.tsv（0 个门）", m->name); return; }
    unsigned long ret, orig;
    char kind[16];
    int arg, n = 0;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%lx %15s %d %lx", &ret, kind, &arg, &orig) == 4) {
            gate_register((uint64_t)(uintptr_t)(m->base + ret), kind, arg, orig);
            n++;
        }
    fclose(f);
    g_gates += (unsigned)n;
    LOG("%s: 门 %d 个", m->name, n);
}

unsigned dm_gate_count(void) { return g_gates; }

// ------------------------------------------------------------------ 动态段与符号

void parse_dynamic(module_t *m) {
    uint32_t mod0 = *(uint32_t *)(m->base + 4);
    if (memcmp(m->base + mod0, "MOD0", 4)) die("%s: 找不到 MOD0", m->name);
    m->dyn = (const Elf64_Dyn *)(m->base + mod0 + *(int32_t *)(m->base + mod0 + 4));
    for (const Elf64_Dyn *d = m->dyn; d->d_tag; d++)
        switch (d->d_tag) {
        case DT_SYMTAB: m->symtab = (const Elf64_Sym *)(m->base + d->d_un.d_ptr); break;
        case DT_STRTAB: m->strtab = (const char *)(m->base + d->d_un.d_ptr); break;
        case DT_INIT: m->init = d->d_un.d_ptr; break;
        case DT_INIT_ARRAY: m->init_array = d->d_un.d_ptr; break;
        case DT_INIT_ARRAYSZ: m->init_arraysz = d->d_un.d_val; break;
        }
    // 符号表大小：参考项目用「STRTAB − SYMTAB」推（NSO 的动态段没给 DT_SYMENT 计数）
    m->nsym = (unsigned)(((const uint8_t *)m->strtab - (const uint8_t *)m->symtab) / sizeof(Elf64_Sym));
    build_symhash(m);
}

static void *module_sym_linear(module_t *m, const char *name) {
    for (unsigned i = 1; i < m->nsym; i++) {
        const Elf64_Sym *s = &m->symtab[i];
        if (s->st_shndx && !strcmp(m->strtab + s->st_name, name)) return m->base + s->st_value;
    }
    return NULL;
}

// 符号名哈希表：按索引升序插入、同名已存在则不覆盖 ⇒ 查询结果与线性扫描（首个同名已定义符号）逐条相同
static uint32_t sym_hash(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static void build_symhash(module_t *m) {
    unsigned cap = 16;
    while (cap < m->nsym * 2) cap <<= 1;
    m->symh = calloc(cap, sizeof *m->symh);
    m->symh_mask = cap - 1;
    for (unsigned i = 1; i < m->nsym; i++) {
        const Elf64_Sym *s = &m->symtab[i];
        if (!s->st_shndx) continue;
        const char *nm = m->strtab + s->st_name;
        for (unsigned j = sym_hash(nm) & m->symh_mask;; j = (j + 1) & m->symh_mask) {
            if (!m->symh[j]) { m->symh[j] = i; break; }
            if (!strcmp(m->strtab + m->symtab[m->symh[j]].st_name, nm)) break;
        }
    }
}

void *module_sym(module_t *m, const char *name) {
    if (!m->symh) return module_sym_linear(m, name);
    void *r = NULL;
    for (unsigned j = sym_hash(name) & m->symh_mask; m->symh[j]; j = (j + 1) & m->symh_mask) {
        const Elf64_Sym *s = &m->symtab[m->symh[j]];
        if (!strcmp(m->strtab + s->st_name, name)) { r = m->base + s->st_value; break; }
    }
    // 与线性扫描逐条对照（参考项目 DM_SYMCHECK 的做法）：哈希表写错时这里会立刻报出来
    void *l = module_sym_linear(m, name);
    g_symchk_n++;
    if (l != r) { g_symchk_bad++; LOG("【红】符号哈希与线性扫描不一致：%s 在 %s", name, m->name); }
    return r;
}

void *guest_sym(const char *name) {
    for (int i = 0; i < g_nmod; i++) {
        void *p = module_sym(&g_mod[i], name);
        if (p) return p;
    }
    return NULL;
}

module_t *module_of(const void *p) {
    for (int i = 0; i < g_nmod; i++)
        if ((const uint8_t *)p >= g_mod[i].base && (const uint8_t *)p < g_mod[i].base + g_mod[i].size)
            return &g_mod[i];
    return NULL;
}

// ------------------------------------------------------------------ 重定位

void relocate(module_t *m) {
    uint64_t rela = 0, relasz = 0, jmprel = 0, pltsz = 0;
    for (const Elf64_Dyn *d = m->dyn; d->d_tag; d++)
        switch (d->d_tag) {
        case DT_RELA: rela = d->d_un.d_ptr; break;
        case DT_RELASZ: relasz = d->d_un.d_val; break;
        case DT_JMPREL: jmprel = d->d_un.d_ptr; break;
        case DT_PLTRELSZ: pltsz = d->d_un.d_val; break;
        }
    const Elf64_Rela *tabs[2] = {(const Elf64_Rela *)(m->base + rela), (const Elf64_Rela *)(m->base + jmprel)};
    uint64_t sizes[2] = {relasz, pltsz};
    int cnt[3] = {0}, hle = 0;
    const uint64_t x0_end = (m->text_end + 0x3FFF) & ~(uint64_t)0x3FFF;
    for (int t = 0; t < 2; t++)
        for (uint64_t i = 0; i < sizes[t] / sizeof(Elf64_Rela); i++) {
            const Elf64_Rela *r = &tabs[t][i];
            uint64_t *where = (uint64_t *)(m->base + r->r_offset);
            unsigned type = ELF64_R_TYPE(r->r_info), si = ELF64_R_SYM(r->r_info);
            // 代码段的终点是 text_end **向上**取整到 16K（边界页归代码段、只读+执行，见 loader_apple.c）。
            // 落在这里面的重定位写不进去：macOS 上写只读页当场 SIGBUS，iOS 上同样。所以在写之前
            // 就报清楚 —— 出包时 x0_layout.py 已经拒过一次，这里是装载时的第二道。
            // 这条检查对 RELATIVE 也要做（原来只对符号类做，RELATIVE 先 continue 了，rtld 的 13 条
            // RELATIVE 正好全在边界页里，就这样漏过去）。
            if (r->r_offset < x0_end) die("%s: 重定位落在代码段（%#llx < %#llx）——这个模块不能按三段装（rtld 就是）",
                                          m->name, (unsigned long long)r->r_offset, (unsigned long long)x0_end);
            if (type == R_AARCH64_RELATIVE) { *where = (uint64_t)(uintptr_t)m->base + (uint64_t)r->r_addend; cnt[0]++; continue; }
            const Elf64_Sym *s = &m->symtab[si];
            const char *nm = m->strtab + s->st_name;
            // 解析顺序与参考项目一致：
            //   ① 服务层强制接管（HLE_FORCE）——即使模块内有定义也覆盖，服务客户端就走这条
            //   ② 本模块定义
            //   ③ 别的模块导出（跨模块重定位）
            //   ④ 最后才交给宿主的 import 解析（HLE / 桩）
            uint64_t v = dm_hle_force(nm);
            if (v) hle++;
            else if (s->st_shndx) v = (uint64_t)(uintptr_t)(m->base + s->st_value);
            else {
                void *x = guest_sym(nm);
                if (x) v = (uint64_t)(uintptr_t)x;
                else { v = resolve_import(m, nm); hle++; }
            }
            if (type == R_AARCH64_JUMP_SLOT) { *where = v; cnt[2]++; }
            else { *where = v + (uint64_t)r->r_addend; cnt[1]++; }
        }
    LOG("%s: 重定位 RELATIVE=%d ABS/GLOB=%d JUMP_SLOT=%d，其中交宿主解析 %d", m->name, cnt[0], cnt[1], cnt[2], hle);
}

// ------------------------------------------------------------------ 装载

void load_modules(const char *dir, const char *const *names, int n) {
    if (n > MAX_MOD) die("模块太多");
    for (int i = 0; i < n; i++) {
        module_t *m = &g_mod[g_nmod];
        memset(m, 0, sizeof *m);
        snprintf(m->name, sizeof m->name, "%s", names[i]);

        // ① 优先看内嵌表（iOS 真机路线）：映像与布局都来自内嵌，不读文件、不读 tsv。
        const dm_embed_mod *emb = dm_embed_find(m->name);
        if (emb) {
            m->base = dm_embed_map(emb);
            if (!m->base) die("%s: 内嵌映像映射失败（签名段/mach_vm_remap？）", m->name);
            m->embed = emb;
            m->size = emb->image_size;
            m->text_end = emb->text_end;
            m->ro_off = emb->ro_off;
            m->data_off = emb->data_off;
            m->patch_off = emb->patch_off;
            m->key_page_off = emb->key_page_off;
            m->key_page_size = emb->key_page_size;
            for (unsigned k = 0; k < KEY_N; k++) m->key_idx[k] = emb->key_idx[k];
            // 代码段已是 RX（来自签名页），不再 mprotect；只把键页填好并设只读。
            // 三步各打一行（stderr 不缓冲）：内嵌路径在 CI 上出过崩溃，这样能直接定位到哪一步。
            LOG("%s: 内嵌 ① 布局就位 text_end=%#llx patch=%#llx keypage=%#llx", m->name,
                (unsigned long long)m->text_end, (unsigned long long)m->patch_off,
                (unsigned long long)m->key_page_off);
            parse_dynamic(m);
            LOG("%s: 内嵌 ② 动态段解析完成（符号 %u，dyn %p）", m->name, m->nsym, (void *)m->dyn);
            install_key_page(m);
            LOG("%s: 内嵌 ③ 键页装好", m->name);
            g_nmod++;
            LOG("%s: 内嵌映像 %#zx 字节（三段签名段），text_end=%#llx patch=%#llx 门 %u",
                m->name, m->size, (unsigned long long)m->text_end,
                (unsigned long long)m->patch_off, emb->n_gates);
            continue;
        }

        // ② macOS 开发/CI 从文件装载；iOS 壳模式先试 Guest 动态库（映像不来自文件）
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.patched.bin", dir, m->name);
        size_t n_bytes = 0;
        m->base = dm_image_from_dylib(m->name, &n_bytes);
        if (!m->base) {
            uint8_t *raw = read_file(path, &n_bytes);
            if (!raw) die("打不开 %s（且内嵌表里也没有）", path);
            m->base = dm_image_alloc(n_bytes);
            if (!m->base) die("分配映像失败 %s", path);
            memcpy(m->base, raw, n_bytes);
            free(raw);
        }
        m->size = n_bytes;
        read_layout(m, dir);
        // 保护两段代码区：text（向上取整到整页）与补丁区。中间的 ro 余部 + data（含键页）
        // 保持可写，装载器还要往里面写重定位与键页。没有 RWX 页。
        dm_code_protect(m->base, m->text_end);
        dm_code_protect(m->base + m->patch_off, m->size - m->patch_off);
        parse_dynamic(m);
        install_key_page(m);
        g_nmod++;
        LOG("%s: 映像 %#zx 字节，text_end=%#llx patch=%#llx 符号 %u", m->name, m->size,
            (unsigned long long)m->text_end, (unsigned long long)m->patch_off, m->nsym);
    }
    // 全部模块映射完再重定位与登门（跨模块解析要求所有模块都已在册）
    for (int i = 0; i < g_nmod; i++) {
        relocate(&g_mod[i]);
        register_gates(&g_mod[i], dir);
    }
    LOG("装载完成：%d 个模块，%u 个宿主门；符号查询 %u 次，与线性扫描不一致 %u 次",
        g_nmod, g_gates, g_symchk_n, g_symchk_bad);
    if (g_symchk_bad) die("符号哈希表与线性扫描不一致（实现有 bug）");
}

// 扮演 Horizon rtld 的 InitModules：按装载顺序调各模块的 DT_INIT。
// 时机由 SDK 的启动流程决定（nn::init::Start 之后、nnMain 之前）——参考项目如此，
// 本项目目前的 bootstrap 是「装载完就调」，等接上 SDK 初始化顺序后再对齐。
// 引导阶段：由 host_boot()（host_boot.c）逐步置位。为什么放这里而不是直接查线程层：
// loader_core.c 不能依赖线程层（loader_selftest 只链本文件），但这个不变量必须有人看着 ——
// 真机前两轮都是它没被满足而崩（先是主线程没登记，后是 TLS/TLR 没建）。
static int g_boot_stage;
void dm_boot_mark(int stage) { if (stage > g_boot_stage) g_boot_stage = stage; }

void run_module_inits(void) {
    // 客体构造函数的改写代码第一时间就读线程环境（tpidrro_el0 / tpidr_el0 → 线程槽）。
    // stage < 2 表示「客体线程环境还没建」：那时 TLR 是空，构造函数读 +0x1f8 就是访问 0x1f8。
    if (g_boot_stage < 2)
        LOG("【红】客体线程环境没就绪就调 run_module_inits（当前 stage=%d，需要 2：guest_thread_init）—— 必崩，顺序见 host_boot", g_boot_stage);
    for (int i = 0; i < g_nmod; i++) {
        module_t *m = &g_mod[i];
        if (!m->init) continue;
        LOG("%s: DT_INIT +%#llx（init_array %llu 项）", m->name,
            (unsigned long long)m->init, (unsigned long long)(m->init_arraysz / 8));
        ((void (*)(void))(m->base + m->init))();
    }
}
