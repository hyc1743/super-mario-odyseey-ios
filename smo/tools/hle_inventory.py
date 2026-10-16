#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HLE 工作清单：SMO 的每个 import 该由谁提供 —— 这就是宿主真正的工作量清单。

参考项目 `dm/tools/hle_inventory.py` 是从**它自己的源码**里抽出 HLE 条目（事后审计）。
本工具做的是**反过来**的事：从事先的游戏文件里算出**必须实现什么**（事前规划）。

判定分三步：
  1. **别的模块导出** → 跨模块重定位，宿主不用管（例如 main 用的 nn::os 由 sdk 提供）
  2. **服务层 nn::** → 宿主强制接管（HLE_FORCE）。理由见下：SDK 的客户端实现要走 IPC，
     而宿主里没有服务进程；接管在 API 层实现比实现 IPC + 服务进程现实得多
  3. **本地库 / libc / C++ 运行时** → 原样跑或走宿主 libc
另外把 `sdk` 里的 svc 与 cntpct 数量列出来 —— 那是内核层要替换的量。

用法：
  hle_inventory.py <ExeFS 目录> [输出 tsv]        # 默认写 smo/data/hle_imports.tsv
  hle_inventory.py <ExeFS 目录> --summary         # 只打印汇总
"""
import collections
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso_info

# ---- 策略表：哪些 nn:: 家族是「服务客户端」（宿主必须接管），哪些可以原样跑 ----
# 依据：这些家族在 Switch 上都是「客户端库 + 系统进程」的形态；宿主里没有那些进程，
# 所以在其 API 层接管。本地库（os/util/diag/mem）是纯计算 + 内核调用，可以原样跑。
SERVICE_FAMILIES = {
    # 文件 / 资源
    "fs", "aoc", "savedata", "content", "ns", "romfs",
    # 输入 / 输出设备
    "hid", "irs", "nfc", "nfp", "mii", "am", "vi", "disp", "lbl", "grcd",
    # 运行环境 / 账号 / 设置
    "oe", "account", "settings", "pctl", "npns", "friends", "prepo", "bcat", "news", "notification",
    # 音频 / 图像 / 字体 / 解码
    "audio", "audren", "codec", "image", "web", "nvjpg", "capsrv",
    # 网络
    "socket", "ssl", "nifm", "ldn", "bsd", "http", "dns",
    # 安全 / 系统信息
    "spl", "crypto", "spsm", "tc", "pm", "fatal", "erpt",
    # 其它服务型
    "time", "tcap", "pdm", "pcm", "htcs", "pl", "pcv", "clkrst", "i2c", "pcie",
    "usb", "sdmmc", "smm", "uart", "gpio", "pinmux", "psc", "pwm", "i2c", "spi", "ts",
    "nvnflinger", "nvdrv", "nvmap", "nvhost", "nvgfx", "gpu", "pwm", "avm",
}
# 这些是**本地库**（纯计算，跑在内核调用之上）：可以原样执行
# lmem 是在调用方提供的内存上管理 ExpHeap / FrameHeap 的本地库。
# v0.6.6 把它误归为服务并强制接到返回 0 的桩，SDK 初始化随即空指针崩溃。
# font/fontll decode and rasterize caller-provided bytes locally. Only pl
# obtains shared font bytes from a system service. Keep the SDK font engine.
LOCAL_LIBRARIES = {"os", "util", "diag", "mem", "lmem", "search", "nnex", "libc", "rtld", "gfx", "ro", "profiler", "font", "fontll"}

# sf 中的缓冲区 getter / 序列化不是远端服务：必须使用 SDK 的实际 TLS 和布局。
# v0.6.7 在 sdk+0x12a2a8 向 GetMessageBufferOnTls 的空桩返回值写消息头而崩溃。
LOCAL_SF_SYMBOLS = {
    "_ZN2nn2sf4hipc21GetMessageBufferOnTlsEv",
    "_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor11SetupHeaderEPvmRKNS2_21HipcMessageHeaderInfoE",
    "_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor6SetTagEt",
    "_ZN2nn2sf4impl6detail22ServiceObjectImplBase211ReleaseImplEv",
    "_ZN2nn2sf4impl6detail22ServiceObjectImplBase216AddReferenceImplEv",
}

# SDK-local flags, also preserved by d2r-mobile's hle_sdkinit.c.
LOCAL_SDK_FLAGS = {
    "_ZN2nn2fs28SetLocalApplicationAccessLogEb",
    "_ZN2nn2fs29SetResultHandledByApplicationEb",
}

# Audited SDK+0x1734d8..0x179c00: caller-owned audio configuration,
# voices, pools, effects and serialization helpers; no renderer service IPC.
# Explicit symbol allowlist keeps unknown/new audio APIs intercepted.
LOCAL_AUDIO_CONFIG = {
    '_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_10SubMixTypeE',
    '_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_12FinalMixTypeE',
    '_ZN2nn5audio12IsVoiceValidEPKNS0_9VoiceTypeE',
    '_ZN2nn5audio12SetDelayTimeEPNS0_9DelayTypeENS_8TimeSpanE',
    '_ZN2nn5audio13AcquireSubMixEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEii',
    '_ZN2nn5audio13AddDeviceSinkEPNS0_19AudioRendererConfigEPNS0_14DeviceSinkTypeEPNS0_12FinalMixTypeEPKaiPKc',
    '_ZN2nn5audio13GetSinkNodeIdEPKNS0_14DeviceSinkTypeE',
    '_ZN2nn5audio13GetSinkNodeIdEPKNS0_22CircularBufferSinkTypeE',
    '_ZN2nn5audio13SetAuxEnabledEPNS0_7AuxTypeEb',
    '_ZN2nn5audio13SetVoicePitchEPNS0_9VoiceTypeEf',
    '_ZN2nn5audio14GetVoiceNodeIdEPKNS0_9VoiceTypeE',
    '_ZN2nn5audio14IsAuxRemovableEPKNS0_7AuxTypeE',
    '_ZN2nn5audio14SetDelayInGainEPNS0_9DelayTypeEf',
    '_ZN2nn5audio14SetVoiceVolumeEPNS0_9VoiceTypeEf',
    '_ZN2nn5audio15AcquireFinalMixEPNS0_19AudioRendererConfigEPNS0_12FinalMixTypeEi',
    '_ZN2nn5audio15GetSubMixNodeIdEPKNS0_10SubMixTypeE',
    '_ZN2nn5audio15PerformanceInfo10GetEntriesEPi',
    '_ZN2nn5audio15PerformanceInfo15MoveToNextFrameEv',
    '_ZN2nn5audio15PerformanceInfo22GetTotalProcessingTimeEv',
    '_ZN2nn5audio15PerformanceInfo9SetBufferEPKvm',
    '_ZN2nn5audio15PerformanceInfoC1Ev',
    '_ZN2nn5audio15PerformanceInfoD1Ev',
    '_ZN2nn5audio15SetDelayDryGainEPNS0_9DelayTypeEf',
    '_ZN2nn5audio15SetDelayEnabledEPNS0_9DelayTypeEb',
    '_ZN2nn5audio16AcquireVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEiiNS0_12SampleFormatEiPKvm',
    '_ZN2nn5audio16AppendWaveBufferEPNS0_9VoiceTypeEPKNS0_10WaveBufferE',
    '_ZN2nn5audio16GetAuxSampleRateEPKNS0_7AuxTypeE',
    '_ZN2nn5audio16IsDelayRemovableEPKNS0_9DelayTypeE',
    '_ZN2nn5audio16ReleaseVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeE',
    '_ZN2nn5audio16SetVoicePriorityEPNS0_9VoiceTypeEi',
    '_ZN2nn5audio17AcquireMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeEPvm',
    '_ZN2nn5audio17GetAuxSampleCountEPKNS0_7AuxTypeE',
    '_ZN2nn5audio17GetSampleByteSizeENS0_12SampleFormatE',
    '_ZN2nn5audio17GetVoicePlayStateEPKNS0_9VoiceTypeE',
    '_ZN2nn5audio17ReadAuxSendBufferEPNS0_7AuxTypeEPii',
    '_ZN2nn5audio17ReleaseMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeE',
    '_ZN2nn5audio17SetAuxInputOutputEPNS0_7AuxTypeEPKaS4_i',
    '_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_10SubMixTypeEfii',
    '_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_12FinalMixTypeEfii',
    '_ZN2nn5audio17SetVoicePlayStateEPNS0_9VoiceTypeENS1_9PlayStateE',
    '_ZN2nn5audio18GetMemoryPoolStateEPKNS0_14MemoryPoolTypeE',
    '_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeEPNS0_12FinalMixTypeEfii',
    '_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeES2_fii',
    '_ZN2nn5audio19SetDelayInputOutputEPNS0_9DelayTypeEPKaS4_i',
    '_ZN2nn5audio19SetDownMixParameterEPNS0_14DeviceSinkTypeEPKNS1_16DownMixParameterE',
    '_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_10SubMixTypeE',
    '_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_12FinalMixTypeE',
    '_ZN2nn5audio20IsMemoryPoolAttachedEPKNS0_14MemoryPoolTypeE',
    '_ZN2nn5audio20IsVoiceDroppedFlagOnEPKNS0_9VoiceTypeE',
    '_ZN2nn5audio20SetDelayFeedbackGainEPNS0_9DelayTypeEf',
    '_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEPNS0_12FinalMixTypeE',
    '_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeES4_',
    '_ZN2nn5audio20WriteAuxReturnBufferEPNS0_7AuxTypeEPKii',
    '_ZN2nn5audio21AddCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeEPKaiPvmNS0_12SampleFormatE',
    '_ZN2nn5audio21GetReleasedWaveBufferEPNS0_9VoiceTypeE',
    '_ZN2nn5audio21SetAudioOutBufferInfoEPNS0_14AudioOutBufferEPvmm',
    '_ZN2nn5audio21SetDelayChannelSpreadEPNS0_9DelayTypeEf',
    '_ZN2nn5audio21SetDelayLowPassAmountEPNS0_9DelayTypeEf',
    '_ZN2nn5audio22ReadCircularBufferSinkEPNS0_22CircularBufferSinkTypeEPvm',
    '_ZN2nn5audio23RequestAttachMemoryPoolEPNS0_14MemoryPoolTypeE',
    '_ZN2nn5audio23RequestDetachMemoryPoolEPNS0_14MemoryPoolTypeE',
    '_ZN2nn5audio24RemoveCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeE',
    '_ZN2nn5audio25GetAudioOutBufferDataSizeEPKNS0_14AudioOutBufferE',
    '_ZN2nn5audio25GetVoicePlayedSampleCountEPKNS0_9VoiceTypeE',
    '_ZN2nn5audio25SetPerformanceFrameBufferEPNS0_19AudioRendererConfigEPvm',
    '_ZN2nn5audio26SetDownMixParameterEnabledEPNS0_14DeviceSinkTypeEb',
    '_ZN2nn5audio27InitializeAudioOutParameterEPNS0_17AudioOutParameterE',
    '_ZN2nn5audio28GetAudioOutBufferDataPointerEPKNS0_14AudioOutBufferE',
    '_ZN2nn5audio29GetRequiredBufferSizeForDelayENS_8TimeSpanEii',
    '_ZN2nn5audio29InitializeAudioRendererConfigEPNS0_19AudioRendererConfigERKNS0_22AudioRendererParameterEPvm',
    '_ZN2nn5audio29IsValidAudioRendererParameterERKNS0_22AudioRendererParameterE',
    '_ZN2nn5audio29SetVoiceBiquadFilterParameterEPNS0_9VoiceTypeEiRKNS0_21BiquadFilterParameterE',
    '_ZN2nn5audio32InitializeAudioRendererParameterEPNS0_22AudioRendererParameterE',
    '_ZN2nn5audio36GetAudioRendererConfigWorkBufferSizeERKNS0_22AudioRendererParameterE',
    '_ZN2nn5audio41GetRequiredBufferSizeForPerformanceFramesERKNS0_22AudioRendererParameterE',
    '_ZN2nn5audio42GetRequiredBufferSizeForCircularBufferSinkEPKNS0_22AudioRendererParameterEiiNS0_12SampleFormatE',
    '_ZN2nn5audio43GetRequiredBufferSizeForAuxSendReturnBufferEPKNS0_22AudioRendererParameterEii',
    '_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeEPvS7_m',
    '_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeEPvS7_m',
    '_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_10SubMixTypeENS_8TimeSpanEi',
    '_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_12FinalMixTypeENS_8TimeSpanEi',
    '_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeE',
    '_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeE',
}


def family_of(sym):
    """抽「顶层命名空间」：改编名第一个嵌套名组件（`_ZN2nn…` → nn、`_ZN2nv…` → nv、
    `_ZN7android…` → android、`_ZNSt3__1…` → std）。

    这里不能用 `startswith("_ZN2nn")` —— 常量成员函数是 `_ZNK2nn…`、虚表是 `_ZTVN2nn…`，
    那样会漏掉一大批（实测漏了 774 条，其中包括 nv(NVN) 与 android(媒体框架)）。
    """
    i = sym.find("2nn")
    if i > 0:
        p = j = i + 3
        while j < len(sym) and sym[j].isdigit():
            j += 1
        if j > p:
            return sym[j:j + int(sym[p:j])]
    # 非 nn：解析 `_ZN<len><name>` / `_ZNK<len><name>` / `_ZTV<len><name>` / `_ZTv0_…`
    if "St3__" in sym[:28] or sym.startswith("_ZSt"):
        return "std(libc++)"            # `St` 是 ABI 里 `std::` 的替代码（_ZNKSt3__1 / _ZTINSt3__1 都要认）
    if "7android" in sym[:20] or sym.startswith("android_"):
        return "android"
    if sym.startswith(("NvRm", "NvOs", "NvError")):
        return "nvidia-driver"
    if sym.startswith("_ZTv") or sym.startswith("_ZTh"):
        return "thunk"                   # 虚/非虚调用 thunk，模块内粘合代码
    m = re.match(r"_Z(?:TV)?N?[KVR]*?[0-9]", sym)
    if not m:
        return None
    k = m.end() - 1
    j = k
    while j < len(sym) and sym[j].isdigit():
        j += 1
    if j == k:
        return None
    n = int(sym[k:j])
    name = sym[j:j + n]
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        return None
    return {"St": "std(libc++)"}.get(name, name)


def load_modules(exefs):
    mods = {}
    for name in ("rtld", "sdk", "subsdk0", "main"):
        p = os.path.join(exefs, name)
        if not os.path.exists(p):
            continue
        img, info = nso_info.load_nso(p)
        nso_info._IMG = img
        dyn, _ = nso_info.parse_dynamic(img)
        syms = nso_info.symbols(img, dyn)
        mods[name] = {
            "info": info,
            "imports": {n for n, v, s, z in syms if s == 0 and n},
            "exports": {n for n, v, s, z in syms if s != 0 and n},
        }
    return mods


def verdict(sym, provided_by):
    fam = family_of(sym)
    if sym in LOCAL_SF_SYMBOLS | LOCAL_SDK_FLAGS | LOCAL_AUDIO_CONFIG and provided_by:
        return "模块内", fam
    # 非 nn 命名空间：这几个是实测查出来的，列出来因为它们不是「服务客户端」那套
    if fam == "nv":
        return "图形(NVN)，宿主接管", fam           # nv::InitializeGraphics / SetGraphicsAllocator
    if fam in ("android", "movie"):
        return "媒体框架(模块内，原样跑)", fam      # SMO 的过场视频走的是 Android 媒体框架
    if fam == "std(libc++)":
        return "C++ 标准库(模块内)", fam
    # C 风格的 C 库 / 运行时 / SDK C API：不是「服务客户端」。不先拦这批的话，
    # 它们会因为「被别的模块导出且不在本地库名单里」而被错归到服务客户端（实测错 275 条）。
    if (not sym.startswith("_Z")) or sym.startswith(("__cxa", "__errno", "__nnmusl", "__nnDetail")):
        if sym.startswith(("nnsocket",)):
            return "网络(宿主:离线桩或BSD socket)", "socket"
        if sym.startswith(("nnssl",)):
            return "网络(宿主:离线桩)", "ssl"
        return "C 库/运行时(模块内或宿主 libc)", fam
    if fam in ("thunk", "nvidia-driver"):
        return ("模块内(粘合/驱动层，通常被更上层绕过)", fam)
    if fam == "android":
        return "媒体框架(模块内，原样跑)", fam
    if sym.startswith("nnsocket"):
        return "网络(宿主:离线桩或BSD socket)", "socket"
    if sym.startswith("nnssl"):
        return "网络(宿主:离线桩)", "ssl"
    if provided_by:
        return ("模块内" if fam in LOCAL_LIBRARIES else "模块内(服务客户端，仍需接管)"), fam
    if fam in SERVICE_FAMILIES:
        return "宿主接管(HLE_FORCE)", fam
    if fam in LOCAL_LIBRARIES:
        return "宿主HLE(库)", fam
    if sym.startswith("_Z") or sym.startswith("__cxa") or sym.startswith("_ZTV") or sym.startswith("_ZSt"):
        return "C++/运行时(模块内或宿主libc++)", fam
    if sym.startswith("__rel") or sym.startswith("__got") or sym.startswith("__EX") or sym.startswith("__tbss") or sym.startswith("__tdata"):
        return "装载器符号(宿主提供)", fam
    if sym in ("memcpy", "memset", "memmove", "strlen", "strcmp", "strncpy", "sqrtf", "cosf", "sinf", "acosf",
               "asinf", "atan2f", "tanf", "powf", "expf", "logf", "atanf", "floorf", "ceilf", "fmodf"):
        return "libc(宿主直通)", fam
    if sym.startswith("glslc") or sym.startswith("nnos") or sym.startswith("__nnDetail"):
        return "SDK 内部/死引用", fam
    return "未分类", fam


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    exefs = sys.argv[1]
    summary_only = "--summary" in sys.argv
    out = next((a for a in sys.argv[2:] if not a.startswith("--")), None) or \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "hle_imports.tsv")
    mods = load_modules(exefs)
    rows = []
    for m in ("main", "subsdk0", "sdk", "rtld"):
        if m not in mods:
            continue
        for sym in sorted(mods[m]["imports"]):
            prov = [o for o in ("main", "subsdk0", "sdk", "rtld")
                    if o != m and o in mods and sym in mods[o]["exports"]]
            v, fam = verdict(sym, prov)
            rows.append((m, sym, fam or "-", ",".join(prov) or "-", v))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    if not summary_only:
        with open(out, "w", encoding="utf-8") as f:
            f.write("# 模块\timport\t家族\t由谁导出\t宿主要做什么\n")
            for r in rows:
                f.write("\t".join(r) + "\n")
        print(f"已写出 {out}（{len(rows)} 条）")

    print("\n== 按「宿主要做什么」统计 ==")
    for k, v in collections.Counter(r[4] for r in rows).most_common():
        print(f"  {v:6d}  {k}")
    print("\n== 需要宿主接管的服务家族（按条目数） ==")
    # 注意：服务客户端**即使被 sdk/subsdk0 导出，也要接管**（SDK 的实现要走 IPC，
    # 而宿主里没有服务进程），所以这里把「模块内(服务客户端，仍需接管)」也算进来 ——
    # 这才是宿主真正的工作清单。
    svc = collections.Counter(r[2] for r in rows
                              if r[4] in ("宿主接管(HLE_FORCE)", "宿主HLE(库)",
                                          "模块内(服务客户端，仍需接管)"))
    for k, v in svc.most_common(40):
        print(f"  {v:6d}  nn::{k}")
    print(f"\n  服务家族合计 {sum(svc.values())} 条，共 {len(svc)} 个家族")
    print("\n== 内核层（改写器在各模块里数出来的） ==")
    for m in ("sdk", "subsdk0", "main", "rtld"):
        p = os.path.join(os.path.dirname(os.path.abspath(out)), "..", "..", "work", "rewrite", f"{m}.gates.tsv")
        if os.path.exists(p):
            kinds = collections.Counter(l.split("\t")[1] for l in open(p, encoding="utf-8") if l.strip())
            print(f"  {m:8s} 宿主门 {dict(kinds)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
