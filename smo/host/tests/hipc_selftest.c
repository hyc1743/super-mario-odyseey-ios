// 模拟 v0.6.7 的 OpenLogger：PID 描述符 + SFCI cmd0 → 移交 ILogger 句柄。
// 从强制接管表按 ABI 调真实现；不共享被测解析/编码 helper。
#include "../hle.h"
#include <pthread.h>

void unimpl_stubs(void) { }
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
static int fails;
#define CHK(cond, msg) do { int ok = (cond); printf("  %s %s\n", ok ? "PASS" : "FAIL", msg); if (!ok) fails++; } while (0)

typedef uint32_t (*connect_fn)(uint32_t *, const char *);
typedef uint32_t (*send_fn)(uint32_t, void *, size_t);
typedef void (*close_fn)(uint32_t);
static connect_fn connect_service;
static send_fn send_request;
static close_fn close_session;

// 请求头的期望字由本次真实汇编记录直接给出，不用被测实现造消息。
static void open_logger_request(uint32_t *b) {
    memset(b, 0, 0x100);
    b[0] = 4; b[1] = 0x8000000a; b[2] = 1;
    b[8] = 0x49434653;              // PID 占用 +0x0c..+0x13，CMIF 对齐到 +0x20
    b[11] = 0x12345678;             // token
}
static void logger_request(uint32_t *b, uint32_t cmd) {
    memset(b, 0, 0x100);
    b[0] = 4; b[1] = 8;
    b[4] = 0x49434653; b[6] = cmd;
}

static void *thread_roundtrip(void *arg) {
    (void)arg;
    uint32_t service = 0, b[64];
    if (connect_service(&service, "lm")) return (void *)1;
    open_logger_request(b);
    if (send_request(service, b, sizeof b) || b[6] || !b[3]) {
        close_session(service);
        return (void *)1;
    }
    close_session(b[3]); close_session(service);
    return NULL;
}

int main(void) {
    // These SDK-internal relocations must select host implementations too.
    // Native audio initialization reaches FlushDataCache through this route.
    void (*flush)(const void *, size_t) = (void *)dm_hle_force_lookup("_ZN2nn2os14FlushDataCacheEPKvm");
    void (*flush_all)(void) = (void *)dm_hle_force_lookup("_ZN2nn2os20FlushEntireDataCacheEv");
    void (*clear_code)(void *, void *) = (void *)dm_hle_force_lookup("__clear_cache");
    CHK(flush && flush_all && clear_code, "SDK cache boundaries select host implementations");
    if (flush && flush_all && clear_code) {
        uint8_t buffer[64];
        memset(buffer, 0x5a, sizeof buffer);
        flush(buffer + 1, sizeof buffer - 2);
        flush_all();
        clear_code(buffer, buffer + sizeof buffer);
        clear_code(buffer, buffer); // empty range
        CHK(buffer[0] == 0x5a && buffer[63] == 0x5a, "cache boundaries preserve caller bytes");
    }
    const char *startup[] = {
        "nndiagStartup", "_ZN2nn3aoc10InitializeEv", "_ZN2nn4pctl10InitializeEv",
        "_ZN2nn4diag34InitializeApplicationAbortObserverEv",
    };
    for (unsigned i = 0; i < SMO_ARRAY_LEN(startup); i++) {
        uint64_t fn = dm_hle_force_lookup(startup[i]);
        int registered = 0;
        for (unsigned j = 0; j < g_hle_force_n; j++)
            if (!strcmp(startup[i], g_hle_force[j].name)) registered = fn != (uint64_t)(uintptr_t)g_hle_force[j].fn;
        CHK(fn && registered, "SDK service startup resolves to host implementation, including internal SDK names");
        if (fn && registered) ((void (*)(void))(uintptr_t)fn)();
    }
    connect_service = (connect_fn)(uintptr_t)dm_hle_force_lookup("_ZN2nn2sf4hipc20ConnectToHipcServiceEPNS_3svc6HandleEPKc");
    send_request = (send_fn)(uintptr_t)dm_hle_force_lookup("_ZN2nn2sf4hipc15SendSyncRequestENS_3svc6HandleEPvm");
    close_session = (close_fn)(uintptr_t)dm_hle_force_lookup("_ZN2nn2sf4hipc24CloseClientSessionHandleENS_3svc6HandleE");
    CHK(connect_service && send_request && close_session, "传输 API 均已接到真实现");
    CHK(!dm_hle_force_lookup("_ZN2nn2sf4hipc21GetMessageBufferOnTlsEv"), "TLS 缓冲区 getter 留给 SDK，不能接到空桩");
    CHK(!dm_hle_force_lookup("_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor11SetupHeaderEPvmRKNS2_21HipcMessageHeaderInfoE"), "消息头序列化留给 SDK");
    uint32_t service = 0, b[64];
    CHK(connect_service(&service, "lm") == 0 && service, "连接 lm 写出有效句柄");
    open_logger_request(b);
    CHK(send_request(service, b, sizeof b) == 0, "真机形状的 OpenLogger 请求成功");
    CHK(b[0] == 0 && b[1] == 0x80000008 && b[2] == 0x20, "响应携带一个 move handle 和正确 data_words");
    CHK(b[4] == 0x4f434653 && b[6] == 0 && b[7] == 0x12345678, "SFCO/Result/token 位于 SDK 解析所需偏移");
    uint32_t logger = b[3];
    CHK(logger != 0 && logger != service, "独立的 ILogger 会话");
    logger_request(b, 1);
    CHK(send_request(logger, b, sizeof b) == 0 && b[6] == 0, "ILogger SetDestination 成功");
    logger_request(b, 0);
    b[0] |= 1u << 20; b[1] = 8;    // 一个 A 描述符，CMIF 从 +0x10 移到 +0x20
    b[4] = 0; b[8] = 0x49434653; b[10] = 0;
    CHK(send_request(logger, b, sizeof b) == 0 && b[6] == 0, "含映射缓冲描述符的 Log 请求成功");
    logger_request(b, 3); b[0] = 5;
    CHK(send_request(logger, b, sizeof b) == 0 && b[6] == 0 && b[8] == 0, "控制命令查询 pointer buffer 回报 0");
    logger_request(b, 2); b[0] = 5;
    CHK(send_request(logger, b, sizeof b) == 0 && b[3] && b[3] != logger, "Clone 返回独立可关闭句柄");
    close_session(b[3]);
    logger_request(b, 99);
    CHK(send_request(logger, b, sizeof b) == 0 && b[4] == 0x4f434653 && b[6] != 0, "未知命令回 CMIF 错误，不能伪装成功");
    uint32_t absent = 99;
    CHK(connect_service(&absent, "missing") != 0 && absent == 0, "未知服务失败且清空输出句柄");
    close_session(logger);
    logger_request(b, 0);
    CHK(send_request(logger, b, sizeof b) == 0xe401, "已关闭的句柄不可继续请求");
    close_session(service);

    CHK(send_request(0, NULL, 0x100) != 0, "空消息被拒");
    open_logger_request(b);
    CHK(send_request(0, b, 0x10) != 0, "被截断的 PID/CMIF 消息被拒");
    open_logger_request(b); b[1] = 0x800003ff;
    CHK(send_request(0, b, sizeof b) != 0, "越过缓冲区的 data_words 被拒");

    // 重复创建/关闭超过表容量，应复用槽但不能复用旧句柄。
    uint32_t previous = service;
    for (unsigned i = 0; i < 300; i++) {
        uint32_t h = 0;
        if (connect_service(&h, "lm") || h == previous) { fails++; break; }
        previous = h; close_session(h);
    }
    uint32_t handles[128];
    unsigned n = 0;
    while (n < 128 && connect_service(&handles[n], "lm") == 0) n++;
    absent = 99;
    CHK(n == 128 && connect_service(&absent, "lm") == 0xce01 && !absent, "会话耗尽回报错误，不返回虚假句柄");
    for (unsigned i = 0; i < n; i++) close_session(handles[i]);
    pthread_t threads[8];
    unsigned started = 0;
    for (; started < 8; started++) if (pthread_create(&threads[started], NULL, thread_roundtrip, NULL)) break;
    CHK(started == 8, "并发请求线程创建成功");
    for (unsigned i = 0; i < started; i++) {
        void *result = (void *)1;
        pthread_join(threads[i], &result);
        CHK(result == NULL, "并发 OpenLogger 各自取得并释放会话");
    }
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails != 0;
}
