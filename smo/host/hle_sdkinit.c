// SDK service-startup policy adapted from d2r-mobile dm/host/hle_sdkinit.c
// (MIT, Copyright (c) 2026 jhaizhou; license in third_party/d2r-mobile.LICENSE).
// Keep SDK-local allocators, virtual memory and flags native. Replace only the
// service connection/registration entrypoints: subsequent APIs are host HLE.
#include "hle.h"

static void h_diag_startup(void) { LOG("SDK 初始化接管：nndiagStartup（宿主日志）"); }
static void h_aoc_startup(void) { LOG("SDK 初始化接管：aoc::Initialize（宿主内容查询）"); }
static void h_pctl_startup(void) { LOG("SDK 初始化接管：pctl::Initialize（宿主环境）"); }
static void h_abort_observer(void) { LOG("SDK 初始化接管：diag::InitializeApplicationAbortObserver（宿主崩溃日志）"); }

// SDK's guest backtrace walker depends on Horizon's mapping metadata. On an
// abort it can itself abort, destroying the useful original call stack. Keep
// the fatal outcome and report at the host boundary before that recursion.
static void __attribute__((noreturn)) h_abort(const char *condition, const char *function,
                                           const char *file, int line) {
    die("GUEST_ABORT condition=%s function=%s file=%s line=%d",
        condition ? condition : "", function ? function : "", file ? file : "", line);
}
static void __attribute__((noreturn)) h_abort_result(const char *condition, const char *function,
                                                  const char *file, int line,
                                                  const uint32_t *result, const char *format) {
    LOG("GUEST_ABORT result=%#x format=%s", result ? *result : 0, format ? format : "");
    h_abort(condition, function, file, line);
}

static const hle_impl_ent g_sdkinit_impl[] = {
    {"nndiagStartup", (void *)h_diag_startup, "诊断日志由宿主提供"},
    {"_ZN2nn3aoc10InitializeEv", (void *)h_aoc_startup, "无需创建 Horizon 内容服务会话"},
    {"_ZN2nn4pctl10InitializeEv", (void *)h_pctl_startup, "无需创建 Horizon 家长控制会话"},
    {"_ZN2nn4diag34InitializeApplicationAbortObserverEv", (void *)h_abort_observer, "宿主记录信号崩溃"},
    {"_ZN2nn4diag6detail9AbortImplEPKcS3_S3_i", (void *)h_abort, "宿主保留原始中止现场，避免客体回溯递归中止"},
    {"_ZN2nn4diag6detail9AbortImplEPKcS3_S3_iPKNS_6ResultES3_z", (void *)h_abort_result, "宿主记录错误码并保留原始中止现场"},
};

static void __attribute__((constructor)) sdkinit_reg(void) {
    hle_impl_register(g_sdkinit_impl, SMO_ARRAY_LEN(g_sdkinit_impl));
}
