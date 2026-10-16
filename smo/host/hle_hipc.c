// 启动路径的 HIPC 传输：保留 SDK 的 TLS getter / 消息序列化，只替代远端服务。
// 依据：v0.6.7 真机 sdk+0x12a268 的 OpenLogger 请求（type=4，PID，cmd=0）；
// 布局对照 switchbrew/libnx 的 nx/include/switch/sf/{hipc,cmif}.h。
// lm/ILogService cmd0 → 移交 ILogger 会话；ILogger cmd0/1 → 接收日志/设置目的地。
// 不访问 Switch 的服务进程，也不把未知服务或未知命令伪装成成功。
#include "hle.h"
#include <pthread.h>

#define R_BAD_HANDLE 0xE401u
#define R_BAD_MESSAGE 0xD401u
#define R_NO_SESSION 0xCE01u
#define R_UNSUPPORTED 0xF601u
#define SFCI 0x49434653u
#define SFCO 0x4F434653u
#define SESSION_CAP 128
enum { S_LOG_SERVICE = 1, S_LOGGER };

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { uint32_t handle, kind; } g_sessions[SESSION_CAP];
static uint32_t g_sequence;

// 调用方持 g_lock。句柄不复用，关闭后不能误指向新会话。
static uint32_t session_new(uint32_t kind) {
    if (g_sequence == 0x3fffffffu) return 0;
    for (unsigned i = 0; i < SESSION_CAP; i++) {
        if (g_sessions[i].handle) continue;
        g_sessions[i].handle = 0x40000000u | ++g_sequence;
        g_sessions[i].kind = kind;
        return g_sessions[i].handle;
    }
    return 0;
}

static uint32_t read32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void write32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static void h_resolution(void) { /* API 层直接按名字解析，不需要 sm: 会话。 */ }

static uint32_t h_connect(uint32_t *out, const char *name) {
    if (!out) return R_BAD_MESSAGE;
    *out = 0;
    if (!name || strcmp(name, "lm")) {
        LOG("【红】HIPC 未实现的服务：%.8s", name ? name : "(null)");
        return R_UNSUPPORTED;
    }
    pthread_mutex_lock(&g_lock);
    *out = session_new(S_LOG_SERVICE);
    pthread_mutex_unlock(&g_lock);
    LOG("HIPC 连接 lm：句柄 %#x", *out);
    return *out ? 0 : R_NO_SESSION;
}

static void h_close(uint32_t handle) {
    pthread_mutex_lock(&g_lock);
    for (unsigned i = 0; i < SESSION_CAP; i++) {
        if (g_sessions[i].handle != handle) continue;
        g_sessions[i].handle = 0;
        g_sessions[i].kind = 0;
        pthread_mutex_unlock(&g_lock);
        return;
    }
    pthread_mutex_unlock(&g_lock);
    LOG("HIPC 关闭无效句柄 %#x", handle);
}

// HIPC raw 区之前依次是 special/PID/handles、X/A/B/W 描述符。
// CMIF 头相对于消息基址按 16 字节对齐，不能固定认为在 +0x10。
static int parse(const uint8_t *b, size_t size, size_t *cmif, uint32_t *type) {
    if (!b || size < 8) return 0;
    uint32_t a = read32(b), c = read32(b + 4);
    *type = a & 0xffff;
    size_t pos = 8;
    if (c >> 31) {
        if (size < 12) return 0;
        uint32_t special = read32(b + 8);
        pos += 4 + (special & 1 ? 8 : 0)
               + 4 * (((special >> 1) & 15) + ((special >> 5) & 15));
    }
    pos += 8 * ((a >> 16) & 15)
         + 12 * (((a >> 20) & 15) + ((a >> 24) & 15) + ((a >> 28) & 15));
    size_t raw_bytes = 4 * (c & 0x3ff);
    if (pos > size || raw_bytes > size - pos) return 0;
    *cmif = (pos + 15) & ~(size_t)15;
    if (*cmif < pos || *cmif - pos > raw_bytes || raw_bytes - (*cmif - pos) < 16) return 0;
    return read32(b + *cmif) == SFCI && read32(b + *cmif + 4) <= 1;
}

// 固定上限 0x100：对应 SDK 的本线程消息缓冲，绝不覆盖 TLR 的线程对象指针。
static uint32_t h_send(uint32_t handle, void *buffer, size_t capacity) {
    uint8_t *b = buffer;
    size_t size = capacity < 0x100 ? capacity : 0x100, raw;
    uint32_t type;
    if (!parse(b, size, &raw, &type)) {
        LOG("【红】HIPC 消息无效（句柄 %#x，容量 %zu）", handle, capacity);
        return R_BAD_MESSAGE;
    }
    uint32_t cmd = read32(b + raw + 8), token = read32(b + raw + 12);
    // 最短响应连同 raw 区尾部预留共 0x28 字节；失败不能泄漏会话。
    if (size < 0x28) return R_BAD_MESSAGE;
    pthread_mutex_lock(&g_lock);
    uint32_t kind = 0;
    for (unsigned i = 0; i < SESSION_CAP; i++)
        if (g_sessions[i].handle == handle) { kind = g_sessions[i].kind; break; }
    uint32_t moved = 0, result = 0, payload = 0;
    int has_payload = 0;
    if (!kind) {
        pthread_mutex_unlock(&g_lock);
        return R_BAD_HANDLE;
    }
    if ((type == 4 || type == 6) && kind == S_LOG_SERVICE && cmd == 0) {
        moved = session_new(S_LOGGER);
        if (!moved) result = R_NO_SESSION;
    } else if ((type == 4 || type == 6) && kind == S_LOGGER && (cmd == 0 || cmd == 1)) {
        // Logger 无远端输出：cmd0 的日志包按请求接收并丢弃；宿主自身日志仍由 logbuf 留存。
        // 不解引用包的地址描述符，避免将任意客体地址当宿主指针。
    } else if ((type == 5 || type == 7) && cmd == 3) {
        has_payload = 1;          // QueryPointerBufferSize = 0：客户端改用映射缓冲
    } else if ((type == 5 || type == 7) && (cmd == 2 || cmd == 4)) {
        moved = session_new(kind); // CloneCurrentObject / CloneCurrentObjectEx
        if (!moved) result = R_NO_SESSION;
    } else {
        result = R_UNSUPPORTED;
    }
    pthread_mutex_unlock(&g_lock);
    if (cmd != 0 || kind != S_LOGGER || result)
        LOG("HIPC %s type=%u cmd=%u → Result=%#x，移交句柄 %#x",
            kind == S_LOG_SERVICE ? "lm" : "logger", type, cmd, result, moved);

    // data_words 包含 16 字节的对齐预留 + 16 字节 CMIF 头，再加输出数据。
    // 移交句柄位于 special header 后，CMIF 头仍从 +0x10 起。
    size_t response_size = moved ? 0x30 : (has_payload ? 0x2c : 0x28);
    if (response_size > size) {
        if (moved) h_close(moved);
        return R_BAD_MESSAGE;
    }
    memset(b, 0, response_size);
    write32(b + 4, (has_payload ? 9u : 8u) | (moved ? 0x80000000u : 0));
    if (moved) { write32(b + 8, 1u << 5); write32(b + 12, moved); }
    write32(b + 16, SFCO);
    write32(b + 24, result);
    write32(b + 28, token);
    if (has_payload) write32(b + 32, payload);
    return 0;
}

static const hle_impl_ent g_hipc_impl[] = {
    {"_ZN2nn2sf4hipc31InitializeHipcServiceResolutionEv", (void *)h_resolution, "宿主直接解析支持的服务名"},
    {"_ZN2nn2sf4hipc29FinalizeHipcServiceResolutionEv", (void *)h_resolution, "宿主无 sm: 连接需要释放"},
    {"_ZN2nn2sf4hipc20ConnectToHipcServiceEPNS_3svc6HandleEPKc", (void *)h_connect, "lm → 宿主日志服务会话"},
    {"_ZN2nn2sf4hipc15SendSyncRequestENS_3svc6HandleEPvm", (void *)h_send, "HIPC/CMIF：OpenLogger、日志、目的地、会话控制"},
    {"_ZN2nn2sf4hipc24CloseClientSessionHandleENS_3svc6HandleE", (void *)h_close, "释放宿主 HIPC 会话"},
};

static void __attribute__((constructor)) hipc_reg(void) {
    hle_impl_register(g_hipc_impl, SMO_ARRAY_LEN(g_hipc_impl));
}
