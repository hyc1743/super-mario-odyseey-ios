// 【自动生成，不要手改】由 smo/tools/gen_hle_force.py 从 smo/data/hle_imports.tsv 生成。
//
// 这张表是「宿主必须接管的 import」：服务层 nn:: 的客户端（即使 SDK 里有定义也要覆盖，
// 因为 SDK 的实现要走 IPC 到不存在的服务进程）+ 无人提供的图形入口。
// 每条的理由写在 why 字段里。当前阶段都接到通用桩（打印一次、返回 0）；
// 真实现按家族逐个替换掉对应的桩。
#include "hle.h"

// 通用桩：第一次调用打印一行，返回 0（与参考项目对「没有实现的导入」的处理一致）
static unsigned g_stub_seen[446];
static uint64_t hle_stub_default(const char *what, const char *why, unsigned i) {
    if (!g_stub_seen[i]++) LOG("HLE 桩被调用：%s —— %s", what, why);
    return 0;
}


static uint64_t hle_s0(void);
static uint64_t hle_s1(void);
static uint64_t hle_s2(void);
static uint64_t hle_s3(void);
static uint64_t hle_s4(void);
static uint64_t hle_s5(void);
static uint64_t hle_s6(void);
static uint64_t hle_s7(void);
static uint64_t hle_s8(void);
static uint64_t hle_s9(void);
static uint64_t hle_s10(void);
static uint64_t hle_s11(void);
static uint64_t hle_s12(void);
static uint64_t hle_s13(void);
static uint64_t hle_s14(void);
static uint64_t hle_s15(void);
static uint64_t hle_s16(void);
static uint64_t hle_s17(void);
static uint64_t hle_s18(void);
static uint64_t hle_s19(void);
static uint64_t hle_s20(void);
static uint64_t hle_s21(void);
static uint64_t hle_s22(void);
static uint64_t hle_s23(void);
static uint64_t hle_s24(void);
static uint64_t hle_s25(void);
static uint64_t hle_s26(void);
static uint64_t hle_s27(void);
static uint64_t hle_s28(void);
static uint64_t hle_s29(void);
static uint64_t hle_s30(void);
static uint64_t hle_s31(void);
static uint64_t hle_s32(void);
static uint64_t hle_s33(void);
static uint64_t hle_s34(void);
static uint64_t hle_s35(void);
static uint64_t hle_s36(void);
static uint64_t hle_s37(void);
static uint64_t hle_s38(void);
static uint64_t hle_s39(void);
static uint64_t hle_s40(void);
static uint64_t hle_s41(void);
static uint64_t hle_s42(void);
static uint64_t hle_s43(void);
static uint64_t hle_s44(void);
static uint64_t hle_s45(void);
static uint64_t hle_s46(void);
static uint64_t hle_s47(void);
static uint64_t hle_s48(void);
static uint64_t hle_s49(void);
static uint64_t hle_s50(void);
static uint64_t hle_s51(void);
static uint64_t hle_s52(void);
static uint64_t hle_s53(void);
static uint64_t hle_s54(void);
static uint64_t hle_s55(void);
static uint64_t hle_s56(void);
static uint64_t hle_s57(void);
static uint64_t hle_s58(void);
static uint64_t hle_s59(void);
static uint64_t hle_s60(void);
static uint64_t hle_s61(void);
static uint64_t hle_s62(void);
static uint64_t hle_s63(void);
static uint64_t hle_s64(void);
static uint64_t hle_s65(void);
static uint64_t hle_s66(void);
static uint64_t hle_s67(void);
static uint64_t hle_s68(void);
static uint64_t hle_s69(void);
static uint64_t hle_s70(void);
static uint64_t hle_s71(void);
static uint64_t hle_s72(void);
static uint64_t hle_s73(void);
static uint64_t hle_s74(void);
static uint64_t hle_s75(void);
static uint64_t hle_s76(void);
static uint64_t hle_s77(void);
static uint64_t hle_s78(void);
static uint64_t hle_s79(void);
static uint64_t hle_s80(void);
static uint64_t hle_s81(void);
static uint64_t hle_s82(void);
static uint64_t hle_s83(void);
static uint64_t hle_s84(void);
static uint64_t hle_s85(void);
static uint64_t hle_s86(void);
static uint64_t hle_s87(void);
static uint64_t hle_s88(void);
static uint64_t hle_s89(void);
static uint64_t hle_s90(void);
static uint64_t hle_s91(void);
static uint64_t hle_s92(void);
static uint64_t hle_s93(void);
static uint64_t hle_s94(void);
static uint64_t hle_s95(void);
static uint64_t hle_s96(void);
static uint64_t hle_s97(void);
static uint64_t hle_s98(void);
static uint64_t hle_s99(void);
static uint64_t hle_s100(void);
static uint64_t hle_s101(void);
static uint64_t hle_s102(void);
static uint64_t hle_s103(void);
static uint64_t hle_s104(void);
static uint64_t hle_s105(void);
static uint64_t hle_s106(void);
static uint64_t hle_s107(void);
static uint64_t hle_s108(void);
static uint64_t hle_s109(void);
static uint64_t hle_s110(void);
static uint64_t hle_s111(void);
static uint64_t hle_s112(void);
static uint64_t hle_s113(void);
static uint64_t hle_s114(void);
static uint64_t hle_s115(void);
static uint64_t hle_s116(void);
static uint64_t hle_s117(void);
static uint64_t hle_s118(void);
static uint64_t hle_s119(void);
static uint64_t hle_s120(void);
static uint64_t hle_s121(void);
static uint64_t hle_s122(void);
static uint64_t hle_s123(void);
static uint64_t hle_s124(void);
static uint64_t hle_s125(void);
static uint64_t hle_s126(void);
static uint64_t hle_s127(void);
static uint64_t hle_s128(void);
static uint64_t hle_s129(void);
static uint64_t hle_s130(void);
static uint64_t hle_s131(void);
static uint64_t hle_s132(void);
static uint64_t hle_s133(void);
static uint64_t hle_s134(void);
static uint64_t hle_s135(void);
static uint64_t hle_s136(void);
static uint64_t hle_s137(void);
static uint64_t hle_s138(void);
static uint64_t hle_s139(void);
static uint64_t hle_s140(void);
static uint64_t hle_s141(void);
static uint64_t hle_s142(void);
static uint64_t hle_s143(void);
static uint64_t hle_s144(void);
static uint64_t hle_s145(void);
static uint64_t hle_s146(void);
static uint64_t hle_s147(void);
static uint64_t hle_s148(void);
static uint64_t hle_s149(void);
static uint64_t hle_s150(void);
static uint64_t hle_s151(void);
static uint64_t hle_s152(void);
static uint64_t hle_s153(void);
static uint64_t hle_s154(void);
static uint64_t hle_s155(void);
static uint64_t hle_s156(void);
static uint64_t hle_s157(void);
static uint64_t hle_s158(void);
static uint64_t hle_s159(void);
static uint64_t hle_s160(void);
static uint64_t hle_s161(void);
static uint64_t hle_s162(void);
static uint64_t hle_s163(void);
static uint64_t hle_s164(void);
static uint64_t hle_s165(void);
static uint64_t hle_s166(void);
static uint64_t hle_s167(void);
static uint64_t hle_s168(void);
static uint64_t hle_s169(void);
static uint64_t hle_s170(void);
static uint64_t hle_s171(void);
static uint64_t hle_s172(void);
static uint64_t hle_s173(void);
static uint64_t hle_s174(void);
static uint64_t hle_s175(void);
static uint64_t hle_s176(void);
static uint64_t hle_s177(void);
static uint64_t hle_s178(void);
static uint64_t hle_s179(void);
static uint64_t hle_s180(void);
static uint64_t hle_s181(void);
static uint64_t hle_s182(void);
static uint64_t hle_s183(void);
static uint64_t hle_s184(void);
static uint64_t hle_s185(void);
static uint64_t hle_s186(void);
static uint64_t hle_s187(void);
static uint64_t hle_s188(void);
static uint64_t hle_s189(void);
static uint64_t hle_s190(void);
static uint64_t hle_s191(void);
static uint64_t hle_s192(void);
static uint64_t hle_s193(void);
static uint64_t hle_s194(void);
static uint64_t hle_s195(void);
static uint64_t hle_s196(void);
static uint64_t hle_s197(void);
static uint64_t hle_s198(void);
static uint64_t hle_s199(void);
static uint64_t hle_s200(void);
static uint64_t hle_s201(void);
static uint64_t hle_s202(void);
static uint64_t hle_s203(void);
static uint64_t hle_s204(void);
static uint64_t hle_s205(void);
static uint64_t hle_s206(void);
static uint64_t hle_s207(void);
static uint64_t hle_s208(void);
static uint64_t hle_s209(void);
static uint64_t hle_s210(void);
static uint64_t hle_s211(void);
static uint64_t hle_s212(void);
static uint64_t hle_s213(void);
static uint64_t hle_s214(void);
static uint64_t hle_s215(void);
static uint64_t hle_s216(void);
static uint64_t hle_s217(void);
static uint64_t hle_s218(void);
static uint64_t hle_s219(void);
static uint64_t hle_s220(void);
static uint64_t hle_s221(void);
static uint64_t hle_s222(void);
static uint64_t hle_s223(void);
static uint64_t hle_s224(void);
static uint64_t hle_s225(void);
static uint64_t hle_s226(void);
static uint64_t hle_s227(void);
static uint64_t hle_s228(void);
static uint64_t hle_s229(void);
static uint64_t hle_s230(void);
static uint64_t hle_s231(void);
static uint64_t hle_s232(void);
static uint64_t hle_s233(void);
static uint64_t hle_s234(void);
static uint64_t hle_s235(void);
static uint64_t hle_s236(void);
static uint64_t hle_s237(void);
static uint64_t hle_s238(void);
static uint64_t hle_s239(void);
static uint64_t hle_s240(void);
static uint64_t hle_s241(void);
static uint64_t hle_s242(void);
static uint64_t hle_s243(void);
static uint64_t hle_s244(void);
static uint64_t hle_s245(void);
static uint64_t hle_s246(void);
static uint64_t hle_s247(void);
static uint64_t hle_s248(void);
static uint64_t hle_s249(void);
static uint64_t hle_s250(void);
static uint64_t hle_s251(void);
static uint64_t hle_s252(void);
static uint64_t hle_s253(void);
static uint64_t hle_s254(void);
static uint64_t hle_s255(void);
static uint64_t hle_s256(void);
static uint64_t hle_s257(void);
static uint64_t hle_s258(void);
static uint64_t hle_s259(void);
static uint64_t hle_s260(void);
static uint64_t hle_s261(void);
static uint64_t hle_s262(void);
static uint64_t hle_s263(void);
static uint64_t hle_s264(void);
static uint64_t hle_s265(void);
static uint64_t hle_s266(void);
static uint64_t hle_s267(void);
static uint64_t hle_s268(void);
static uint64_t hle_s269(void);
static uint64_t hle_s270(void);
static uint64_t hle_s271(void);
static uint64_t hle_s272(void);
static uint64_t hle_s273(void);
static uint64_t hle_s274(void);
static uint64_t hle_s275(void);
static uint64_t hle_s276(void);
static uint64_t hle_s277(void);
static uint64_t hle_s278(void);
static uint64_t hle_s279(void);
static uint64_t hle_s280(void);
static uint64_t hle_s281(void);
static uint64_t hle_s282(void);
static uint64_t hle_s283(void);
static uint64_t hle_s284(void);
static uint64_t hle_s285(void);
static uint64_t hle_s286(void);
static uint64_t hle_s287(void);
static uint64_t hle_s288(void);
static uint64_t hle_s289(void);
static uint64_t hle_s290(void);
static uint64_t hle_s291(void);
static uint64_t hle_s292(void);
static uint64_t hle_s293(void);
static uint64_t hle_s294(void);
static uint64_t hle_s295(void);
static uint64_t hle_s296(void);
static uint64_t hle_s297(void);
static uint64_t hle_s298(void);
static uint64_t hle_s299(void);
static uint64_t hle_s300(void);
static uint64_t hle_s301(void);
static uint64_t hle_s302(void);
static uint64_t hle_s303(void);
static uint64_t hle_s304(void);
static uint64_t hle_s305(void);
static uint64_t hle_s306(void);
static uint64_t hle_s307(void);
static uint64_t hle_s308(void);
static uint64_t hle_s309(void);
static uint64_t hle_s310(void);
static uint64_t hle_s311(void);
static uint64_t hle_s312(void);
static uint64_t hle_s313(void);
static uint64_t hle_s314(void);
static uint64_t hle_s315(void);
static uint64_t hle_s316(void);
static uint64_t hle_s317(void);
static uint64_t hle_s318(void);
static uint64_t hle_s319(void);
static uint64_t hle_s320(void);
static uint64_t hle_s321(void);
static uint64_t hle_s322(void);
static uint64_t hle_s323(void);
static uint64_t hle_s324(void);
static uint64_t hle_s325(void);
static uint64_t hle_s326(void);
static uint64_t hle_s327(void);
static uint64_t hle_s328(void);
static uint64_t hle_s329(void);
static uint64_t hle_s330(void);
static uint64_t hle_s331(void);
static uint64_t hle_s332(void);
static uint64_t hle_s333(void);
static uint64_t hle_s334(void);
static uint64_t hle_s335(void);
static uint64_t hle_s336(void);
static uint64_t hle_s337(void);
static uint64_t hle_s338(void);
static uint64_t hle_s339(void);
static uint64_t hle_s340(void);
static uint64_t hle_s341(void);
static uint64_t hle_s342(void);
static uint64_t hle_s343(void);
static uint64_t hle_s344(void);
static uint64_t hle_s345(void);
static uint64_t hle_s346(void);
static uint64_t hle_s347(void);
static uint64_t hle_s348(void);
static uint64_t hle_s349(void);
static uint64_t hle_s350(void);
static uint64_t hle_s351(void);
static uint64_t hle_s352(void);
static uint64_t hle_s353(void);
static uint64_t hle_s354(void);
static uint64_t hle_s355(void);
static uint64_t hle_s356(void);
static uint64_t hle_s357(void);
static uint64_t hle_s358(void);
static uint64_t hle_s359(void);
static uint64_t hle_s360(void);
static uint64_t hle_s361(void);
static uint64_t hle_s362(void);
static uint64_t hle_s363(void);
static uint64_t hle_s364(void);
static uint64_t hle_s365(void);
static uint64_t hle_s366(void);
static uint64_t hle_s367(void);
static uint64_t hle_s368(void);
static uint64_t hle_s369(void);
static uint64_t hle_s370(void);
static uint64_t hle_s371(void);
static uint64_t hle_s372(void);
static uint64_t hle_s373(void);
static uint64_t hle_s374(void);
static uint64_t hle_s375(void);
static uint64_t hle_s376(void);
static uint64_t hle_s377(void);
static uint64_t hle_s378(void);
static uint64_t hle_s379(void);
static uint64_t hle_s380(void);
static uint64_t hle_s381(void);
static uint64_t hle_s382(void);
static uint64_t hle_s383(void);
static uint64_t hle_s384(void);
static uint64_t hle_s385(void);
static uint64_t hle_s386(void);
static uint64_t hle_s387(void);
static uint64_t hle_s388(void);
static uint64_t hle_s389(void);
static uint64_t hle_s390(void);
static uint64_t hle_s391(void);
static uint64_t hle_s392(void);
static uint64_t hle_s393(void);
static uint64_t hle_s394(void);
static uint64_t hle_s395(void);
static uint64_t hle_s396(void);
static uint64_t hle_s397(void);
static uint64_t hle_s398(void);
static uint64_t hle_s399(void);
static uint64_t hle_s400(void);
static uint64_t hle_s401(void);
static uint64_t hle_s402(void);
static uint64_t hle_s403(void);
static uint64_t hle_s404(void);
static uint64_t hle_s405(void);
static uint64_t hle_s406(void);
static uint64_t hle_s407(void);
static uint64_t hle_s408(void);
static uint64_t hle_s409(void);
static uint64_t hle_s410(void);
static uint64_t hle_s411(void);
static uint64_t hle_s412(void);
static uint64_t hle_s413(void);
static uint64_t hle_s414(void);
static uint64_t hle_s415(void);
static uint64_t hle_s416(void);
static uint64_t hle_s417(void);
static uint64_t hle_s418(void);
static uint64_t hle_s419(void);
static uint64_t hle_s420(void);
static uint64_t hle_s421(void);
static uint64_t hle_s422(void);
static uint64_t hle_s423(void);
static uint64_t hle_s424(void);
static uint64_t hle_s425(void);
static uint64_t hle_s426(void);
static uint64_t hle_s427(void);
static uint64_t hle_s428(void);
static uint64_t hle_s429(void);
static uint64_t hle_s430(void);
static uint64_t hle_s431(void);
static uint64_t hle_s432(void);
static uint64_t hle_s433(void);
static uint64_t hle_s434(void);
static uint64_t hle_s435(void);
static uint64_t hle_s436(void);
static uint64_t hle_s437(void);
static uint64_t hle_s438(void);
static uint64_t hle_s439(void);
static uint64_t hle_s440(void);
static uint64_t hle_s441(void);
static uint64_t hle_s442(void);
static uint64_t hle_s443(void);
static uint64_t hle_s444(void);
static uint64_t hle_s445(void);

static uint64_t hle_s0(void) { return hle_stub_default("_ZN2nn2fs10CreateFileEPKcl", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 0); }
static uint64_t hle_s1(void) { return hle_stub_default("_ZN2nn2fs10DeleteFileEPKc", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 1); }
static uint64_t hle_s2(void) { return hle_stub_default("_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 2); }
static uint64_t hle_s3(void) { return hle_stub_default("_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 3); }
static uint64_t hle_s4(void) { return hle_stub_default("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 4); }
static uint64_t hle_s5(void) { return hle_stub_default("_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 5); }
static uint64_t hle_s6(void) { return hle_stub_default("_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 6); }
static uint64_t hle_s7(void) { return hle_stub_default("_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 7); }
static uint64_t hle_s8(void) { return hle_stub_default("_ZN2nn2fs14CloseDirectoryENS0_15DirectoryHandleE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 8); }
static uint64_t hle_s9(void) { return hle_stub_default("_ZN2nn2fs14CommitSaveDataEPKc", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 9); }
static uint64_t hle_s10(void) { return hle_stub_default("_ZN2nn2fs14EnsureSaveDataERKNS_7account3UidE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 10); }
static uint64_t hle_s11(void) { return hle_stub_default("_ZN2nn2fs15CreateDirectoryEPKc", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 11); }
static uint64_t hle_s12(void) { return hle_stub_default("_ZN2nn2fs22QueryMountRomCacheSizeEPm", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 12); }
static uint64_t hle_s13(void) { return hle_stub_default("_ZN2nn2fs7UnmountEPKc", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 13); }
static uint64_t hle_s14(void) { return hle_stub_default("_ZN2nn2fs8MountRomEPKcPvm", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 14); }
static uint64_t hle_s15(void) { return hle_stub_default("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 15); }
static uint64_t hle_s16(void) { return hle_stub_default("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 16); }
static uint64_t hle_s17(void) { return hle_stub_default("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvmRKNS0_10ReadOptionE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 17); }
static uint64_t hle_s18(void) { return hle_stub_default("_ZN2nn2fs9CloseFileENS0_10FileHandleE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 18); }
static uint64_t hle_s19(void) { return hle_stub_default("_ZN2nn2fs9FlushFileENS0_10FileHandleE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 19); }
static uint64_t hle_s20(void) { return hle_stub_default("_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE", "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用", 20); }
static uint64_t hle_s21(void) { return hle_stub_default("_ZN2nn2oe10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 21); }
static uint64_t hle_s22(void) { return hle_stub_default("_ZN2nn2oe16GetOperationModeEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 22); }
static uint64_t hle_s23(void) { return hle_stub_default("_ZN2nn2oe17FinishStartupLogoEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 23); }
static uint64_t hle_s24(void) { return hle_stub_default("_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 24); }
static uint64_t hle_s25(void) { return hle_stub_default("_ZN2nn2oe18GetDesiredLanguageEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 25); }
static uint64_t hle_s26(void) { return hle_stub_default("_ZN2nn2oe18GetPerformanceModeEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 26); }
static uint64_t hle_s27(void) { return hle_stub_default("_ZN2nn2oe20GetCurrentFocusStateEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 27); }
static uint64_t hle_s28(void) { return hle_stub_default("_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 28); }
static uint64_t hle_s29(void) { return hle_stub_default("_ZN2nn2oe23EnableGamePlayRecordingEPvm", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 29); }
static uint64_t hle_s30(void) { return hle_stub_default("_ZN2nn2oe24GetExpectedVolumeBalanceEPfS1_", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 30); }
static uint64_t hle_s31(void) { return hle_stub_default("_ZN2nn2oe24SetExpectedVolumeBalanceEff", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 31); }
static uint64_t hle_s32(void) { return hle_stub_default("_ZN2nn2oe25TryPopNotificationMessageEPj", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 32); }
static uint64_t hle_s33(void) { return hle_stub_default("_ZN2nn2oe27GetPerformanceConfigurationENS0_15PerformanceModeE", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 33); }
static uint64_t hle_s34(void) { return hle_stub_default("_ZN2nn2oe27SetPerformanceConfigurationENS0_15PerformanceModeEi", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 34); }
static uint64_t hle_s35(void) { return hle_stub_default("_ZN2nn2oe28SetResumeNotificationEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 35); }
static uint64_t hle_s36(void) { return hle_stub_default("_ZN2nn2oe29SetScreenShotImageOrientationENS_5album16ImageOrientationE", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 36); }
static uint64_t hle_s37(void) { return hle_stub_default("_ZN2nn2oe37IsUserInactivityDetectionTimeExtendedEv", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 37); }
static uint64_t hle_s38(void) { return hle_stub_default("_ZN2nn2oe38SetUserInactivityDetectionTimeExtendedEb", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 38); }
static uint64_t hle_s39(void) { return hle_stub_default("_ZN2nn2oe42SetOperationModeChangedNotificationEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 39); }
static uint64_t hle_s40(void) { return hle_stub_default("_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用", 40); }
static uint64_t hle_s41(void) { return hle_stub_default("_ZN2nn2pl17GetSharedFontSizeENS0_14SharedFontTypeE", "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用", 41); }
static uint64_t hle_s42(void) { return hle_stub_default("_ZN2nn2pl20GetSharedFontAddressENS0_14SharedFontTypeE", "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用", 42); }
static uint64_t hle_s43(void) { return hle_stub_default("_ZN2nn2pl22GetSharedFontLoadStateENS0_14SharedFontTypeE", "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用", 43); }
static uint64_t hle_s44(void) { return hle_stub_default("_ZN2nn2vi10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 44); }
static uint64_t hle_s45(void) { return hle_stub_default("_ZN2nn2vi11CreateLayerEPPNS0_5LayerEPNS0_7DisplayE", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 45); }
static uint64_t hle_s46(void) { return hle_stub_default("_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 46); }
static uint64_t hle_s47(void) { return hle_stub_default("_ZN2nn2vi18OpenDefaultDisplayEPPNS0_7DisplayE", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 47); }
static uint64_t hle_s48(void) { return hle_stub_default("_ZN2nn2vi19SetLayerScalingModeEPNS0_5LayerENS0_11ScalingModeE", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 48); }
static uint64_t hle_s49(void) { return hle_stub_default("_ZN2nn2vi20GetDisplayVsyncEventEPNS_2os15SystemEventTypeEPNS0_7DisplayE", "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用", 49); }
static uint64_t hle_s50(void) { return hle_stub_default("_ZN2nn3err9ShowErrorENS_6ResultE", "模块内(服务客户端，仍需接管)；家族 nn::err；main 引用", 50); }
static uint64_t hle_s51(void) { return hle_stub_default("_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 51); }
static uint64_t hle_s52(void) { return hle_stub_default("_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 52); }
static uint64_t hle_s53(void) { return hle_stub_default("_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyLeftStateEiRKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 53); }
static uint64_t hle_s54(void) { return hle_stub_default("_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 54); }
static uint64_t hle_s55(void) { return hle_stub_default("_ZN2nn3hid13GetNpadStatesEPNS0_17NpadJoyRightStateEiRKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 55); }
static uint64_t hle_s56(void) { return hle_stub_default("_ZN2nn3hid13VibrationNode15SetModulationToEPKS1_RKNS0_19VibrationModulationE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 56); }
static uint64_t hle_s57(void) { return hle_stub_default("_ZN2nn3hid13VibrationNode6UpdateEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 57); }
static uint64_t hle_s58(void) { return hle_stub_default("_ZN2nn3hid14DisconnectNpadERKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 58); }
static uint64_t hle_s59(void) { return hle_stub_default("_ZN2nn3hid14InitializeNpadEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 59); }
static uint64_t hle_s60(void) { return hle_stub_default("_ZN2nn3hid14VibrationMixer10SetMixModeENS0_16VibrationMixModeE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 60); }
static uint64_t hle_s61(void) { return hle_stub_default("_ZN2nn3hid14VibrationMixerC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 61); }
static uint64_t hle_s62(void) { return hle_stub_default("_ZN2nn3hid15GetNpadStyleSetERKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 62); }
static uint64_t hle_s63(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayer18SetCurrentPositionEi", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 63); }
static uint64_t hle_s64(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayer4LoadEPKvm", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 64); }
static uint64_t hle_s65(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayer4PlayEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 65); }
static uint64_t hle_s66(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayer4StopEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 66); }
static uint64_t hle_s67(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayer7SetLoopEb", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 67); }
static uint64_t hle_s68(void) { return hle_stub_default("_ZN2nn3hid15VibrationPlayerC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 68); }
static uint64_t hle_s69(void) { return hle_stub_default("_ZN2nn3hid15VibrationTarget24SetVibrationDeviceHandleERKNS0_21VibrationDeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 69); }
static uint64_t hle_s70(void) { return hle_stub_default("_ZN2nn3hid15VibrationTarget26UnsetVibrationDeviceHandleEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 70); }
static uint64_t hle_s71(void) { return hle_stub_default("_ZN2nn3hid15VibrationTargetC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 71); }
static uint64_t hle_s72(void) { return hle_stub_default("_ZN2nn3hid15VibrationWriter5WriteERKNS0_14VibrationValueE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 72); }
static uint64_t hle_s73(void) { return hle_stub_default("_ZN2nn3hid15VibrationWriterC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 73); }
static uint64_t hle_s74(void) { return hle_stub_default("_ZN2nn3hid16GetDebugPadStateEPNS0_13DebugPadStateE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 74); }
static uint64_t hle_s75(void) { return hle_stub_default("_ZN2nn3hid18InitializeDebugPadEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 75); }
static uint64_t hle_s76(void) { return hle_stub_default("_ZN2nn3hid18SendVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 76); }
static uint64_t hle_s77(void) { return hle_stub_default("_ZN2nn3hid18SetNpadJoyHoldTypeENS0_15NpadJoyHoldTypeE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 77); }
static uint64_t hle_s78(void) { return hle_stub_default("_ZN2nn3hid18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 78); }
static uint64_t hle_s79(void) { return hle_stub_default("_ZN2nn3hid18SwapNpadAssignmentERKjS2_", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 79); }
static uint64_t hle_s80(void) { return hle_stub_default("_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 80); }
static uint64_t hle_s81(void) { return hle_stub_default("_ZN2nn3hid20GetNpadJoyAssignmentERKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 81); }
static uint64_t hle_s82(void) { return hle_stub_default("_ZN2nn3hid21InitializeTouchScreenEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 82); }
static uint64_t hle_s83(void) { return hle_stub_default("_ZN2nn3hid21ShowControllerSupportEPNS0_27ControllerSupportResultInfoERKNS0_20ControllerSupportArgE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 83); }
static uint64_t hle_s84(void) { return hle_stub_default("_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorES2_RKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 84); }
static uint64_t hle_s85(void) { return hle_stub_default("_ZN2nn3hid22GetSixAxisSensorStatesEPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 85); }
static uint64_t hle_s86(void) { return hle_stub_default("_ZN2nn3hid22SetSupportedNpadIdTypeEPKjm", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 86); }
static uint64_t hle_s87(void) { return hle_stub_default("_ZN2nn3hid23GetSixAxisSensorHandlesEPNS0_19SixAxisSensorHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 87); }
static uint64_t hle_s88(void) { return hle_stub_default("_ZN2nn3hid23MergeSingleJoyAsDualJoyERKjS2_", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 88); }
static uint64_t hle_s89(void) { return hle_stub_default("_ZN2nn3hid23VibrationNodeConnection10DisconnectEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 89); }
static uint64_t hle_s90(void) { return hle_stub_default("_ZN2nn3hid23VibrationNodeConnection7ConnectEPNS0_13VibrationNodeES3_", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 90); }
static uint64_t hle_s91(void) { return hle_stub_default("_ZN2nn3hid23VibrationNodeConnectionC1EPNS0_13VibrationNodeES3_", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 91); }
static uint64_t hle_s92(void) { return hle_stub_default("_ZN2nn3hid23VibrationNodeConnectionC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 92); }
static uint64_t hle_s93(void) { return hle_stub_default("_ZN2nn3hid24SetSupportedNpadStyleSetENS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 93); }
static uint64_t hle_s94(void) { return hle_stub_default("_ZN2nn3hid25GetVibrationDeviceHandlesEPNS0_21VibrationDeviceHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 94); }
static uint64_t hle_s95(void) { return hle_stub_default("_ZN2nn3hid25InitializeVibrationDeviceERKNS0_21VibrationDeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 95); }
static uint64_t hle_s96(void) { return hle_stub_default("_ZN2nn3hid28SetNpadJoyAssignmentModeDualERKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 96); }
static uint64_t hle_s97(void) { return hle_stub_default("_ZN2nn3hid30SetNpadJoyAssignmentModeSingleERKj", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 97); }
static uint64_t hle_s98(void) { return hle_stub_default("_ZN2nn3hid30SetNpadJoyAssignmentModeSingleERKjNS0_17NpadJoyDeviceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 98); }
static uint64_t hle_s99(void) { return hle_stub_default("_ZN2nn3nfp10GetTagInfoEPNS0_7TagInfoERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 99); }
static uint64_t hle_s100(void) { return hle_stub_default("_ZN2nn3nfp10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 100); }
static uint64_t hle_s101(void) { return hle_stub_default("_ZN2nn3nfp11ListDevicesEPNS0_12DeviceHandleEPii", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 101); }
static uint64_t hle_s102(void) { return hle_stub_default("_ZN2nn3nfp12GetModelInfoEPNS0_9ModelInfoERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 102); }
static uint64_t hle_s103(void) { return hle_stub_default("_ZN2nn3nfp13StopDetectionERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 103); }
static uint64_t hle_s104(void) { return hle_stub_default("_ZN2nn3nfp14GetDeviceStateERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 104); }
static uint64_t hle_s105(void) { return hle_stub_default("_ZN2nn3nfp14StartDetectionERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 105); }
static uint64_t hle_s106(void) { return hle_stub_default("_ZN2nn3nfp19AttachActivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 106); }
static uint64_t hle_s107(void) { return hle_stub_default("_ZN2nn3nfp21AttachDeactivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 107); }
static uint64_t hle_s108(void) { return hle_stub_default("_ZN2nn3nfp5MountERKNS0_12DeviceHandleENS0_9ModelTypeENS0_11MountTargetE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 108); }
static uint64_t hle_s109(void) { return hle_stub_default("_ZN2nn3nfp7UnmountERKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 109); }
static uint64_t hle_s110(void) { return hle_stub_default("_ZN2nn3nfp8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 110); }
static uint64_t hle_s111(void) { return hle_stub_default("_ZN2nn3nfp9GetNpadIdEPjRKNS0_12DeviceHandleE", "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用", 111); }
static uint64_t hle_s112(void) { return hle_stub_default("_ZN2nn3nsd9ResolveExEPNS0_4FqdnERKS1_", "模块内(服务客户端，仍需接管)；家族 nn::nsd；main 引用", 112); }
static uint64_t hle_s113(void) { return hle_stub_default("_ZN2nn3ssl10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用", 113); }
static uint64_t hle_s114(void) { return hle_stub_default("_ZN2nn3ssl7Context15ImportServerPkiEPmPKcjNS0_17CertificateFormatE", "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用", 114); }
static uint64_t hle_s115(void) { return hle_stub_default("_ZN2nn3ssl7Context6CreateENS1_10SslVersionE", "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用", 115); }
static uint64_t hle_s116(void) { return hle_stub_default("_ZN2nn3ssl8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用", 116); }
static uint64_t hle_s117(void) { return hle_stub_default("_ZN2nn3web19ShowOfflineHtmlPageEPNS0_26OfflineHtmlPageReturnValueERKNS0_22ShowOfflineHtmlPageArgE", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 117); }
static uint64_t hle_s118(void) { return hle_stub_default("_ZN2nn3web22ShowOfflineHtmlPageArg16SetFooterEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 118); }
static uint64_t hle_s119(void) { return hle_stub_default("_ZN2nn3web22ShowOfflineHtmlPageArg17SetBackgroundKindERKNS0_21OfflineBackgroundKindE", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 119); }
static uint64_t hle_s120(void) { return hle_stub_default("_ZN2nn3web22ShowOfflineHtmlPageArg17SetPointerEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 120); }
static uint64_t hle_s121(void) { return hle_stub_default("_ZN2nn3web22ShowOfflineHtmlPageArg18SetBootDisplayKindENS0_22OfflineBootDisplayKindE", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 121); }
static uint64_t hle_s122(void) { return hle_stub_default("_ZN2nn3web26OfflineHtmlPageReturnValueC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 122); }
static uint64_t hle_s123(void) { return hle_stub_default("_ZN2nn3web36ShowOfflineHtmlPageArgWithPlayReport20SetPlayReportEnabledEb", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 123); }
static uint64_t hle_s124(void) { return hle_stub_default("_ZN2nn3web36ShowOfflineHtmlPageArgWithPlayReportC1ERKNS_13ApplicationIdEPKc", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 124); }
static uint64_t hle_s125(void) { return hle_stub_default("_ZN2nn4lmem15CreateFrameHeapEPvmi", "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用", 125); }
static uint64_t hle_s126(void) { return hle_stub_default("_ZN2nn4lmem16DestroyFrameHeapEPNS0_6detail8HeapHeadE", "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用", 126); }
static uint64_t hle_s127(void) { return hle_stub_default("_ZN2nn4lmem21AllocateFromFrameHeapEPNS0_6detail8HeapHeadEmi", "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用", 127); }
static uint64_t hle_s128(void) { return hle_stub_default("_ZN2nn4nifm10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 128); }
static uint64_t hle_s129(void) { return hle_stub_default("_ZN2nn4nifm18IsNetworkAvailableEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 129); }
static uint64_t hle_s130(void) { return hle_stub_default("_ZN2nn4nifm19SetLocalNetworkModeEb", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 130); }
static uint64_t hle_s131(void) { return hle_stub_default("_ZN2nn4nifm20SubmitNetworkRequestEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 131); }
static uint64_t hle_s132(void) { return hle_stub_default("_ZN2nn4nifm22GetCurrentProxySettingEPNS0_12ProxySettingE", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 132); }
static uint64_t hle_s133(void) { return hle_stub_default("_ZN2nn4nifm22IsNetworkRequestOnHoldEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 133); }
static uint64_t hle_s134(void) { return hle_stub_default("_ZN2nn4nifm26GetCurrentPrimaryIpAddressEP7in_addr", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 134); }
static uint64_t hle_s135(void) { return hle_stub_default("_ZN2nn4nifm26HandleNetworkRequestResultEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 135); }
static uint64_t hle_s136(void) { return hle_stub_default("_ZN2nn4nifm27SubmitNetworkRequestAndWaitEv", "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用", 136); }
static uint64_t hle_s137(void) { return hle_stub_default("_ZN2nn4time10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 137); }
static uint64_t hle_s138(void) { return hle_stub_default("_ZN2nn4time11ToPosixTimeEPiPNS0_9PosixTimeEiRKNS0_12CalendarTimeE", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 138); }
static uint64_t hle_s139(void) { return hle_stub_default("_ZN2nn4time13IsInitializedEv", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 139); }
static uint64_t hle_s140(void) { return hle_stub_default("_ZN2nn4time14ToCalendarTimeEPNS0_12CalendarTimeEPNS0_22CalendarAdditionalInfoERKNS0_9PosixTimeE", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 140); }
static uint64_t hle_s141(void) { return hle_stub_default("_ZN2nn4time18ToPosixTimeFromUtcERKNS0_12CalendarTimeE", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 141); }
static uint64_t hle_s142(void) { return hle_stub_default("_ZN2nn4time19ToCalendarTimeInUtcERKNS0_9PosixTimeE", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 142); }
static uint64_t hle_s143(void) { return hle_stub_default("_ZN2nn4time23StandardUserSystemClock14GetCurrentTimeEPNS0_9PosixTimeE", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 143); }
static uint64_t hle_s144(void) { return hle_stub_default("_ZN2nn4time8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用", 144); }
static uint64_t hle_s145(void) { return hle_stub_default("_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_10SubMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 145); }
static uint64_t hle_s146(void) { return hle_stub_default("_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_12FinalMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 146); }
static uint64_t hle_s147(void) { return hle_stub_default("_ZN2nn5audio12IsVoiceValidEPKNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 147); }
static uint64_t hle_s148(void) { return hle_stub_default("_ZN2nn5audio12SetDelayTimeEPNS0_9DelayTypeENS_8TimeSpanE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 148); }
static uint64_t hle_s149(void) { return hle_stub_default("_ZN2nn5audio13AcquireSubMixEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 149); }
static uint64_t hle_s150(void) { return hle_stub_default("_ZN2nn5audio13AddDeviceSinkEPNS0_19AudioRendererConfigEPNS0_14DeviceSinkTypeEPNS0_12FinalMixTypeEPKaiPKc", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 150); }
static uint64_t hle_s151(void) { return hle_stub_default("_ZN2nn5audio13GetSinkNodeIdEPKNS0_14DeviceSinkTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 151); }
static uint64_t hle_s152(void) { return hle_stub_default("_ZN2nn5audio13GetSinkNodeIdEPKNS0_22CircularBufferSinkTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 152); }
static uint64_t hle_s153(void) { return hle_stub_default("_ZN2nn5audio13SetAuxEnabledEPNS0_7AuxTypeEb", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 153); }
static uint64_t hle_s154(void) { return hle_stub_default("_ZN2nn5audio13SetVoicePitchEPNS0_9VoiceTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 154); }
static uint64_t hle_s155(void) { return hle_stub_default("_ZN2nn5audio14GetVoiceNodeIdEPKNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 155); }
static uint64_t hle_s156(void) { return hle_stub_default("_ZN2nn5audio14IsAuxRemovableEPKNS0_7AuxTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 156); }
static uint64_t hle_s157(void) { return hle_stub_default("_ZN2nn5audio14SetDelayInGainEPNS0_9DelayTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 157); }
static uint64_t hle_s158(void) { return hle_stub_default("_ZN2nn5audio14SetVoiceVolumeEPNS0_9VoiceTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 158); }
static uint64_t hle_s159(void) { return hle_stub_default("_ZN2nn5audio15AcquireFinalMixEPNS0_19AudioRendererConfigEPNS0_12FinalMixTypeEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 159); }
static uint64_t hle_s160(void) { return hle_stub_default("_ZN2nn5audio15GetSubMixNodeIdEPKNS0_10SubMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 160); }
static uint64_t hle_s161(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfo10GetEntriesEPi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 161); }
static uint64_t hle_s162(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfo15MoveToNextFrameEv", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 162); }
static uint64_t hle_s163(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfo22GetTotalProcessingTimeEv", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 163); }
static uint64_t hle_s164(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfo9SetBufferEPKvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 164); }
static uint64_t hle_s165(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfoC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 165); }
static uint64_t hle_s166(void) { return hle_stub_default("_ZN2nn5audio15PerformanceInfoD1Ev", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 166); }
static uint64_t hle_s167(void) { return hle_stub_default("_ZN2nn5audio15SetDelayDryGainEPNS0_9DelayTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 167); }
static uint64_t hle_s168(void) { return hle_stub_default("_ZN2nn5audio15SetDelayEnabledEPNS0_9DelayTypeEb", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 168); }
static uint64_t hle_s169(void) { return hle_stub_default("_ZN2nn5audio16AcquireVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEiiNS0_12SampleFormatEiPKvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 169); }
static uint64_t hle_s170(void) { return hle_stub_default("_ZN2nn5audio16AppendWaveBufferEPNS0_9VoiceTypeEPKNS0_10WaveBufferE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 170); }
static uint64_t hle_s171(void) { return hle_stub_default("_ZN2nn5audio16GetAuxSampleRateEPKNS0_7AuxTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 171); }
static uint64_t hle_s172(void) { return hle_stub_default("_ZN2nn5audio16IsDelayRemovableEPKNS0_9DelayTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 172); }
static uint64_t hle_s173(void) { return hle_stub_default("_ZN2nn5audio16ReleaseVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 173); }
static uint64_t hle_s174(void) { return hle_stub_default("_ZN2nn5audio16SetVoicePriorityEPNS0_9VoiceTypeEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 174); }
static uint64_t hle_s175(void) { return hle_stub_default("_ZN2nn5audio17AcquireMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeEPvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 175); }
static uint64_t hle_s176(void) { return hle_stub_default("_ZN2nn5audio17GetAuxSampleCountEPKNS0_7AuxTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 176); }
static uint64_t hle_s177(void) { return hle_stub_default("_ZN2nn5audio17GetVoicePlayStateEPKNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 177); }
static uint64_t hle_s178(void) { return hle_stub_default("_ZN2nn5audio17OpenAudioRendererEPNS0_19AudioRendererHandleEPNS_2os11SystemEventERKNS0_22AudioRendererParameterEPvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 178); }
static uint64_t hle_s179(void) { return hle_stub_default("_ZN2nn5audio17ReadAuxSendBufferEPNS0_7AuxTypeEPii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 179); }
static uint64_t hle_s180(void) { return hle_stub_default("_ZN2nn5audio17ReleaseMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 180); }
static uint64_t hle_s181(void) { return hle_stub_default("_ZN2nn5audio17SetAuxInputOutputEPNS0_7AuxTypeEPKaS4_i", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 181); }
static uint64_t hle_s182(void) { return hle_stub_default("_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_10SubMixTypeEfii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 182); }
static uint64_t hle_s183(void) { return hle_stub_default("_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_12FinalMixTypeEfii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 183); }
static uint64_t hle_s184(void) { return hle_stub_default("_ZN2nn5audio17SetVoicePlayStateEPNS0_9VoiceTypeENS1_9PlayStateE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 184); }
static uint64_t hle_s185(void) { return hle_stub_default("_ZN2nn5audio17StopAudioRendererENS0_19AudioRendererHandleE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 185); }
static uint64_t hle_s186(void) { return hle_stub_default("_ZN2nn5audio18CloseAudioRendererENS0_19AudioRendererHandleE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 186); }
static uint64_t hle_s187(void) { return hle_stub_default("_ZN2nn5audio18GetMemoryPoolStateEPKNS0_14MemoryPoolTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 187); }
static uint64_t hle_s188(void) { return hle_stub_default("_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeEPNS0_12FinalMixTypeEfii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 188); }
static uint64_t hle_s189(void) { return hle_stub_default("_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeES2_fii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 189); }
static uint64_t hle_s190(void) { return hle_stub_default("_ZN2nn5audio18StartAudioRendererENS0_19AudioRendererHandleE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 190); }
static uint64_t hle_s191(void) { return hle_stub_default("_ZN2nn5audio19ListAudioDeviceNameEPNS0_15AudioDeviceNameEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 191); }
static uint64_t hle_s192(void) { return hle_stub_default("_ZN2nn5audio19SetDelayInputOutputEPNS0_9DelayTypeEPKaS4_i", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 192); }
static uint64_t hle_s193(void) { return hle_stub_default("_ZN2nn5audio19SetDownMixParameterEPNS0_14DeviceSinkTypeEPKNS1_16DownMixParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 193); }
static uint64_t hle_s194(void) { return hle_stub_default("_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_10SubMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 194); }
static uint64_t hle_s195(void) { return hle_stub_default("_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_12FinalMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 195); }
static uint64_t hle_s196(void) { return hle_stub_default("_ZN2nn5audio20IsMemoryPoolAttachedEPKNS0_14MemoryPoolTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 196); }
static uint64_t hle_s197(void) { return hle_stub_default("_ZN2nn5audio20IsVoiceDroppedFlagOnEPKNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 197); }
static uint64_t hle_s198(void) { return hle_stub_default("_ZN2nn5audio20SetDelayFeedbackGainEPNS0_9DelayTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 198); }
static uint64_t hle_s199(void) { return hle_stub_default("_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEPNS0_12FinalMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 199); }
static uint64_t hle_s200(void) { return hle_stub_default("_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeES4_", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 200); }
static uint64_t hle_s201(void) { return hle_stub_default("_ZN2nn5audio20WriteAuxReturnBufferEPNS0_7AuxTypeEPKii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 201); }
static uint64_t hle_s202(void) { return hle_stub_default("_ZN2nn5audio21AddCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeEPKaiPvmNS0_12SampleFormatE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 202); }
static uint64_t hle_s203(void) { return hle_stub_default("_ZN2nn5audio21GetActiveChannelCountEv", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 203); }
static uint64_t hle_s204(void) { return hle_stub_default("_ZN2nn5audio21GetReleasedWaveBufferEPNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 204); }
static uint64_t hle_s205(void) { return hle_stub_default("_ZN2nn5audio21SetDelayChannelSpreadEPNS0_9DelayTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 205); }
static uint64_t hle_s206(void) { return hle_stub_default("_ZN2nn5audio21SetDelayLowPassAmountEPNS0_9DelayTypeEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 206); }
static uint64_t hle_s207(void) { return hle_stub_default("_ZN2nn5audio22ReadCircularBufferSinkEPNS0_22CircularBufferSinkTypeEPvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 207); }
static uint64_t hle_s208(void) { return hle_stub_default("_ZN2nn5audio23RequestAttachMemoryPoolEPNS0_14MemoryPoolTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 208); }
static uint64_t hle_s209(void) { return hle_stub_default("_ZN2nn5audio23RequestDetachMemoryPoolEPNS0_14MemoryPoolTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 209); }
static uint64_t hle_s210(void) { return hle_stub_default("_ZN2nn5audio24GetActiveAudioDeviceNameEPNS0_15AudioDeviceNameE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 210); }
static uint64_t hle_s211(void) { return hle_stub_default("_ZN2nn5audio24RemoveCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 211); }
static uint64_t hle_s212(void) { return hle_stub_default("_ZN2nn5audio25GetVoicePlayedSampleCountEPKNS0_9VoiceTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 212); }
static uint64_t hle_s213(void) { return hle_stub_default("_ZN2nn5audio25SetPerformanceFrameBufferEPNS0_19AudioRendererConfigEPvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 213); }
static uint64_t hle_s214(void) { return hle_stub_default("_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 214); }
static uint64_t hle_s215(void) { return hle_stub_default("_ZN2nn5audio26SetAudioDeviceOutputVolumeEPKNS0_15AudioDeviceNameEf", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 215); }
static uint64_t hle_s216(void) { return hle_stub_default("_ZN2nn5audio26SetDownMixParameterEnabledEPNS0_14DeviceSinkTypeEb", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 216); }
static uint64_t hle_s217(void) { return hle_stub_default("_ZN2nn5audio29ExecuteAudioRendererRenderingENS0_19AudioRendererHandleE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 217); }
static uint64_t hle_s218(void) { return hle_stub_default("_ZN2nn5audio29GetRequiredBufferSizeForDelayENS_8TimeSpanEii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 218); }
static uint64_t hle_s219(void) { return hle_stub_default("_ZN2nn5audio29InitializeAudioRendererConfigEPNS0_19AudioRendererConfigERKNS0_22AudioRendererParameterEPvm", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 219); }
static uint64_t hle_s220(void) { return hle_stub_default("_ZN2nn5audio29IsValidAudioRendererParameterERKNS0_22AudioRendererParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 220); }
static uint64_t hle_s221(void) { return hle_stub_default("_ZN2nn5audio29SetVoiceBiquadFilterParameterEPNS0_9VoiceTypeEiRKNS0_21BiquadFilterParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 221); }
static uint64_t hle_s222(void) { return hle_stub_default("_ZN2nn5audio30GetAudioRendererWorkBufferSizeERKNS0_22AudioRendererParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 222); }
static uint64_t hle_s223(void) { return hle_stub_default("_ZN2nn5audio32InitializeAudioRendererParameterEPNS0_22AudioRendererParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 223); }
static uint64_t hle_s224(void) { return hle_stub_default("_ZN2nn5audio34GetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 224); }
static uint64_t hle_s225(void) { return hle_stub_default("_ZN2nn5audio34SetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 225); }
static uint64_t hle_s226(void) { return hle_stub_default("_ZN2nn5audio36AcquireAudioDeviceSwitchNotificationEPNS_2os11SystemEventE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 226); }
static uint64_t hle_s227(void) { return hle_stub_default("_ZN2nn5audio36GetAudioRendererConfigWorkBufferSizeERKNS0_22AudioRendererParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 227); }
static uint64_t hle_s228(void) { return hle_stub_default("_ZN2nn5audio41GetRequiredBufferSizeForPerformanceFramesERKNS0_22AudioRendererParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 228); }
static uint64_t hle_s229(void) { return hle_stub_default("_ZN2nn5audio42GetRequiredBufferSizeForCircularBufferSinkEPKNS0_22AudioRendererParameterEiiNS0_12SampleFormatE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 229); }
static uint64_t hle_s230(void) { return hle_stub_default("_ZN2nn5audio43GetRequiredBufferSizeForAuxSendReturnBufferEPKNS0_22AudioRendererParameterEii", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 230); }
static uint64_t hle_s231(void) { return hle_stub_default("_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeEPvS7_m", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 231); }
static uint64_t hle_s232(void) { return hle_stub_default("_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeEPvS7_m", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 232); }
static uint64_t hle_s233(void) { return hle_stub_default("_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_10SubMixTypeENS_8TimeSpanEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 233); }
static uint64_t hle_s234(void) { return hle_stub_default("_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_12FinalMixTypeENS_8TimeSpanEi", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 234); }
static uint64_t hle_s235(void) { return hle_stub_default("_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 235); }
static uint64_t hle_s236(void) { return hle_stub_default("_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeE", "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用", 236); }
static uint64_t hle_s237(void) { return hle_stub_default("_ZN2nn5image11JpegDecoder12SetImageDataEPKvm", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 237); }
static uint64_t hle_s238(void) { return hle_stub_default("_ZN2nn5image11JpegDecoder6DecodeEPvmiS2_m", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 238); }
static uint64_t hle_s239(void) { return hle_stub_default("_ZN2nn5image11JpegDecoder7AnalyzeEv", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 239); }
static uint64_t hle_s240(void) { return hle_stub_default("_ZN2nn5image11JpegDecoderC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 240); }
static uint64_t hle_s241(void) { return hle_stub_default("_ZN2nn5image11JpegDecoderD1Ev", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 241); }
static uint64_t hle_s242(void) { return hle_stub_default("_ZN2nn5prepo10PlayReport3AddEPKcd", "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用", 242); }
static uint64_t hle_s243(void) { return hle_stub_default("_ZN2nn5prepo10PlayReport3AddEPKcl", "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用", 243); }
static uint64_t hle_s244(void) { return hle_stub_default("_ZN2nn5prepo10PlayReport4SaveERKNS_7account3UidE", "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用", 244); }
static uint64_t hle_s245(void) { return hle_stub_default("_ZN2nn5prepo10PlayReport9SetBufferEPvm", "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用", 245); }
static uint64_t hle_s246(void) { return hle_stub_default("_ZN2nn5prepo10PlayReportC1EPKc", "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用", 246); }
static uint64_t hle_s247(void) { return hle_stub_default("_ZN2nn6crypto16GenerateSha1HashEPvmPKvm", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 247); }
static uint64_t hle_s248(void) { return hle_stub_default("_ZN2nn6crypto6detail7Md5Impl10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 248); }
static uint64_t hle_s249(void) { return hle_stub_default("_ZN2nn6crypto6detail7Md5Impl6UpdateEPKvm", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 249); }
static uint64_t hle_s250(void) { return hle_stub_default("_ZN2nn6crypto6detail7Md5Impl7GetHashEPvm", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 250); }
static uint64_t hle_s251(void) { return hle_stub_default("_ZN2nn6crypto6detail8Sha1Impl10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 251); }
static uint64_t hle_s252(void) { return hle_stub_default("_ZN2nn6crypto6detail8Sha1Impl6UpdateEPKvm", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 252); }
static uint64_t hle_s253(void) { return hle_stub_default("_ZN2nn6crypto6detail8Sha1Impl7GetHashEPvm", "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用", 253); }
static uint64_t hle_s254(void) { return hle_stub_default("_ZN2nn6detail21UnexpectedDefaultImplEPKcS2_i", "模块内(服务客户端，仍需接管)；家族 nn::detail；main 引用", 254); }
static uint64_t hle_s255(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine10GetAdvanceEPsS2_PiS3_jt", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 255); }
static uint64_t hle_s256(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine10GetKerningEPiS2_jj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 256); }
static uint64_t hle_s257(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine10InitializeEPvj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 257); }
static uint64_t hle_s258(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine11SetAutoHintEb", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 258); }
static uint64_t hle_s259(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine13ReleasesGlyphEPv", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 259); }
static uint64_t hle_s260(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine13SetBoldWeightEi", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 260); }
static uint64_t hle_s261(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine14GetFontMetricsEPNS0_7MetricsE", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 261); }
static uint64_t hle_s262(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine15AcquireGlyphmapEjt", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 262); }
static uint64_t hle_s263(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine15CheckGlyphExistEj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 263); }
static uint64_t hle_s264(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine15SetOutlineWidthEt", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 264); }
static uint64_t hle_s265(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine17AcquireOtfKerningEPKNS0_15OtfKerningTableEjjj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 265); }
static uint64_t hle_s266(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine21AcquireOtfKerningLastEPKNS0_15OtfKerningTableEjj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 266); }
static uint64_t hle_s267(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine22AcquireOtfKerningFirstEPKNS0_15OtfKerningTableEjj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 267); }
static uint64_t hle_s268(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine22GetPointerToWorkBufferEv", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 268); }
static uint64_t hle_s269(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine23FinalizeOtfKerningTableEPNS0_15OtfKerningTableEPFvPvS4_ES4_", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 269); }
static uint64_t hle_s270(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine25InitializeOtfKerningTableEPFPvmmS2_ES2_b", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 270); }
static uint64_t hle_s271(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine7SetFontEPc", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 271); }
static uint64_t hle_s272(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 272); }
static uint64_t hle_s273(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine8GetErrorEv", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 273); }
static uint64_t hle_s274(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine8LoadFontEPcPKvjj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 274); }
static uint64_t hle_s275(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine8SetFlagsENS1_5FlagsE", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 275); }
static uint64_t hle_s276(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngine8SetScaleEiiii", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 276); }
static uint64_t hle_s277(void) { return hle_stub_default("_ZN2nn6fontll18ScalableFontEngineC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 277); }
static uint64_t hle_s278(void) { return hle_stub_default("_ZN2nn6fontll24ScalableFontEngineHelper6DecodeEPKvj", "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用", 278); }
static uint64_t hle_s279(void) { return hle_stub_default("_ZN2nn6socket10InitializeEPvmmi", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 279); }
static uint64_t hle_s280(void) { return hle_stub_default("_ZN2nn6socket10SetSockOptEiiiPKvj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 280); }
static uint64_t hle_s281(void) { return hle_stub_default("_ZN2nn6socket11GetAddrInfoEPKcS2_PK8addrinfoPPS3_", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 281); }
static uint64_t hle_s282(void) { return hle_stub_default("_ZN2nn6socket11GetPeerNameEiP8sockaddrPj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 282); }
static uint64_t hle_s283(void) { return hle_stub_default("_ZN2nn6socket11GetSockNameEiP8sockaddrPj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 283); }
static uint64_t hle_s284(void) { return hle_stub_default("_ZN2nn6socket12FreeAddrInfoEP8addrinfo", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 284); }
static uint64_t hle_s285(void) { return hle_stub_default("_ZN2nn6socket12GetLastErrnoEv", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 285); }
static uint64_t hle_s286(void) { return hle_stub_default("_ZN2nn6socket4BindEiPK8sockaddrj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 286); }
static uint64_t hle_s287(void) { return hle_stub_default("_ZN2nn6socket4PollEP6pollfdmi", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 287); }
static uint64_t hle_s288(void) { return hle_stub_default("_ZN2nn6socket4RecvEiPvmi", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 288); }
static uint64_t hle_s289(void) { return hle_stub_default("_ZN2nn6socket4SendEiPKvmi", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 289); }
static uint64_t hle_s290(void) { return hle_stub_default("_ZN2nn6socket5CloseEi", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 290); }
static uint64_t hle_s291(void) { return hle_stub_default("_ZN2nn6socket5FcntlEiiz", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 291); }
static uint64_t hle_s292(void) { return hle_stub_default("_ZN2nn6socket6AcceptEiP8sockaddrPj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 292); }
static uint64_t hle_s293(void) { return hle_stub_default("_ZN2nn6socket6ListenEii", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 293); }
static uint64_t hle_s294(void) { return hle_stub_default("_ZN2nn6socket6SendToEiPKvmiPK8sockaddrj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 294); }
static uint64_t hle_s295(void) { return hle_stub_default("_ZN2nn6socket6SocketEiii", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 295); }
static uint64_t hle_s296(void) { return hle_stub_default("_ZN2nn6socket7ConnectEiPK8sockaddrj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 296); }
static uint64_t hle_s297(void) { return hle_stub_default("_ZN2nn6socket8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 297); }
static uint64_t hle_s298(void) { return hle_stub_default("_ZN2nn6socket8InetNtoaE7in_addr", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 298); }
static uint64_t hle_s299(void) { return hle_stub_default("_ZN2nn6socket8InetNtopEiPKvPcj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 299); }
static uint64_t hle_s300(void) { return hle_stub_default("_ZN2nn6socket8InetPtonEiPKcPv", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 300); }
static uint64_t hle_s301(void) { return hle_stub_default("_ZN2nn6socket8RecvFromEiPvmiP8sockaddrPj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 301); }
static uint64_t hle_s302(void) { return hle_stub_default("_ZN2nn6socket8ShutdownEii", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 302); }
static uint64_t hle_s303(void) { return hle_stub_default("_ZN2nn6socket9InetHtonlEj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 303); }
static uint64_t hle_s304(void) { return hle_stub_default("_ZN2nn6socket9InetHtonsEt", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 304); }
static uint64_t hle_s305(void) { return hle_stub_default("_ZN2nn6socket9InetNtohlEj", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 305); }
static uint64_t hle_s306(void) { return hle_stub_default("_ZN2nn6socket9InetNtohsEt", "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用", 306); }
static uint64_t hle_s307(void) { return hle_stub_default("_ZN2nn7account10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 307); }
static uint64_t hle_s308(void) { return hle_stub_default("_ZN2nn7account11GetNicknameEPNS0_8NicknameERKNS0_3UidE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 308); }
static uint64_t hle_s309(void) { return hle_stub_default("_ZN2nn7account12AsyncContext14GetSystemEventEPNS_2os11SystemEventE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 309); }
static uint64_t hle_s310(void) { return hle_stub_default("_ZN2nn7account12AsyncContext6CancelEv", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 310); }
static uint64_t hle_s311(void) { return hle_stub_default("_ZN2nn7account12AsyncContext7HasDoneEPb", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 311); }
static uint64_t hle_s312(void) { return hle_stub_default("_ZN2nn7account12AsyncContext9GetResultEv", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 312); }
static uint64_t hle_s313(void) { return hle_stub_default("_ZN2nn7account12AsyncContextC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 313); }
static uint64_t hle_s314(void) { return hle_stub_default("_ZN2nn7account12AsyncContextD1Ev", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 314); }
static uint64_t hle_s315(void) { return hle_stub_default("_ZN2nn7account12ListAllUsersEPiPNS0_3UidEi", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 315); }
static uint64_t hle_s316(void) { return hle_stub_default("_ZN2nn7account17GetLastOpenedUserEPNS0_3UidE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 316); }
static uint64_t hle_s317(void) { return hle_stub_default("_ZN2nn7account19OpenPreselectedUserEPNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 317); }
static uint64_t hle_s318(void) { return hle_stub_default("_ZN2nn7account26GetNetworkServiceAccountIdEPNS0_23NetworkServiceAccountIdERKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 318); }
static uint64_t hle_s319(void) { return hle_stub_default("_ZN2nn7account32IsNetworkServiceAccountAvailableEPbRKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 319); }
static uint64_t hle_s320(void) { return hle_stub_default("_ZN2nn7account36EnsureNetworkServiceAccountAvailableERKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 320); }
static uint64_t hle_s321(void) { return hle_stub_default("_ZN2nn7account37LoadNetworkServiceAccountIdTokenCacheEPmPcmRKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 321); }
static uint64_t hle_s322(void) { return hle_stub_default("_ZN2nn7account44EnsureNetworkServiceAccountIdTokenCacheAsyncEPNS0_12AsyncContextERKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 322); }
static uint64_t hle_s323(void) { return hle_stub_default("_ZN2nn7account8OpenUserEPNS0_10UserHandleERKNS0_3UidE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 323); }
static uint64_t hle_s324(void) { return hle_stub_default("_ZN2nn7account9CloseUserERKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 324); }
static uint64_t hle_s325(void) { return hle_stub_default("_ZN2nn7account9GetUserIdEPNS0_3UidERKNS0_10UserHandleE", "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用", 325); }
static uint64_t hle_s326(void) { return hle_stub_default("_ZN2nn7friends10InitializeEv", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 326); }
static uint64_t hle_s327(void) { return hle_stub_default("_ZN2nn7friends12AsyncContext14GetSystemEventEPNS_2os11SystemEventE", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 327); }
static uint64_t hle_s328(void) { return hle_stub_default("_ZN2nn7friends12AsyncContextC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 328); }
static uint64_t hle_s329(void) { return hle_stub_default("_ZN2nn7friends12AsyncContextD1Ev", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 329); }
static uint64_t hle_s330(void) { return hle_stub_default("_ZN2nn7friends14GetProfileListEPNS0_12AsyncContextEPNS0_7ProfileEPKNS_7account23NetworkServiceAccountIdEi", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 330); }
static uint64_t hle_s331(void) { return hle_stub_default("_ZN2nn7friends7ProfileC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 331); }
static uint64_t hle_s332(void) { return hle_stub_default("_ZN2nn8settings12LanguageCode4MakeENS0_8LanguageE", "模块内(服务客户端，仍需接管)；家族 nn::settings；main 引用", 332); }
static uint64_t hle_s333(void) { return hle_stub_default("_ZN2nn8settingseqERKNS0_12LanguageCodeES3_", "模块内(服务客户端，仍需接管)；家族 nn::settings；main 引用", 333); }
static uint64_t hle_s334(void) { return hle_stub_default("_ZN2nv18InitializeGraphicsEPvm", "图形(NVN)，宿主接管；main 引用", 334); }
static uint64_t hle_s335(void) { return hle_stub_default("_ZN2nv20SetGraphicsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", "图形(NVN)，宿主接管；main 引用", 335); }
static uint64_t hle_s336(void) { return hle_stub_default("_ZN2nv28SetGraphicsDevtoolsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", "图形(NVN)，宿主接管；main 引用", 336); }
static uint64_t hle_s337(void) { return hle_stub_default("_ZNK2nn3hid15VibrationPlayer11GetFileInfoEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 337); }
static uint64_t hle_s338(void) { return hle_stub_default("_ZNK2nn3hid15VibrationPlayer9IsPlayingEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 338); }
static uint64_t hle_s339(void) { return hle_stub_default("_ZNK2nn3hid15VibrationTarget26IsVibrationDeviceHandleSetEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 339); }
static uint64_t hle_s340(void) { return hle_stub_default("_ZNK2nn3hid23VibrationNodeConnection13GetModulationEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 340); }
static uint64_t hle_s341(void) { return hle_stub_default("_ZNK2nn3hid23VibrationNodeConnection14GetDestinationEv", "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用", 341); }
static uint64_t hle_s342(void) { return hle_stub_default("_ZNK2nn3web26OfflineHtmlPageReturnValue10GetLastUrlEv", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 342); }
static uint64_t hle_s343(void) { return hle_stub_default("_ZNK2nn3web26OfflineHtmlPageReturnValue20GetOfflineExitReasonEv", "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用", 343); }
static uint64_t hle_s344(void) { return hle_stub_default("_ZNK2nn5image11JpegDecoder20GetAnalyzedDimensionEv", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 344); }
static uint64_t hle_s345(void) { return hle_stub_default("_ZNK2nn5image11JpegDecoder25GetAnalyzedWorkBufferSizeEv", "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用", 345); }
static uint64_t hle_s346(void) { return hle_stub_default("_ZNK2nn7friends12AsyncContext9GetResultEv", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 346); }
static uint64_t hle_s347(void) { return hle_stub_default("_ZNK2nn7friends7Profile11GetNicknameEv", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 347); }
static uint64_t hle_s348(void) { return hle_stub_default("_ZNK2nn7friends7Profile18GetProfileImageUrlEPNS0_3UrlENS0_9ImageSizeE", "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用", 348); }
static uint64_t hle_s349(void) { return hle_stub_default("_ZTVN10__cxxabiv117__class_type_infoE", "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用", 349); }
static uint64_t hle_s350(void) { return hle_stub_default("_ZTVN10__cxxabiv120__si_class_type_infoE", "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用", 350); }
static uint64_t hle_s351(void) { return hle_stub_default("_ZTVN10__cxxabiv121__vmi_class_type_infoE", "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用", 351); }
static uint64_t hle_s352(void) { return hle_stub_default("nnsocketBind", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 352); }
static uint64_t hle_s353(void) { return hle_stub_default("nnsocketCancel", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 353); }
static uint64_t hle_s354(void) { return hle_stub_default("nnsocketClose", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 354); }
static uint64_t hle_s355(void) { return hle_stub_default("nnsocketConnect", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 355); }
static uint64_t hle_s356(void) { return hle_stub_default("nnsocketFcntl", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 356); }
static uint64_t hle_s357(void) { return hle_stub_default("nnsocketGetHostByNameCancel", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 357); }
static uint64_t hle_s358(void) { return hle_stub_default("nnsocketGetLastErrno", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 358); }
static uint64_t hle_s359(void) { return hle_stub_default("nnsocketGetPeerName", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 359); }
static uint64_t hle_s360(void) { return hle_stub_default("nnsocketGetSockName", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 360); }
static uint64_t hle_s361(void) { return hle_stub_default("nnsocketGetSockOpt", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 361); }
static uint64_t hle_s362(void) { return hle_stub_default("nnsocketInetHtons", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 362); }
static uint64_t hle_s363(void) { return hle_stub_default("nnsocketInetNtohs", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 363); }
static uint64_t hle_s364(void) { return hle_stub_default("nnsocketPoll", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 364); }
static uint64_t hle_s365(void) { return hle_stub_default("nnsocketRecv", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 365); }
static uint64_t hle_s366(void) { return hle_stub_default("nnsocketRequestCancelHandle", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 366); }
static uint64_t hle_s367(void) { return hle_stub_default("nnsocketSend", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 367); }
static uint64_t hle_s368(void) { return hle_stub_default("nnsocketSetLastErrno", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 368); }
static uint64_t hle_s369(void) { return hle_stub_default("nnsocketSetSockOpt", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 369); }
static uint64_t hle_s370(void) { return hle_stub_default("nnsocketSocket", "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用", 370); }
static uint64_t hle_s371(void) { return hle_stub_default("nnsslConnectionCreate", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 371); }
static uint64_t hle_s372(void) { return hle_stub_default("nnsslConnectionDestroy", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 372); }
static uint64_t hle_s373(void) { return hle_stub_default("nnsslConnectionDoHandshakeWithCertBuffer", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 373); }
static uint64_t hle_s374(void) { return hle_stub_default("nnsslConnectionGetConnectionId", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 374); }
static uint64_t hle_s375(void) { return hle_stub_default("nnsslConnectionGetVerifyCertError", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 375); }
static uint64_t hle_s376(void) { return hle_stub_default("nnsslConnectionPeek", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 376); }
static uint64_t hle_s377(void) { return hle_stub_default("nnsslConnectionPending", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 377); }
static uint64_t hle_s378(void) { return hle_stub_default("nnsslConnectionRead", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 378); }
static uint64_t hle_s379(void) { return hle_stub_default("nnsslConnectionSetHostName", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 379); }
static uint64_t hle_s380(void) { return hle_stub_default("nnsslConnectionSetIoMode", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 380); }
static uint64_t hle_s381(void) { return hle_stub_default("nnsslConnectionSetOption", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 381); }
static uint64_t hle_s382(void) { return hle_stub_default("nnsslConnectionSetSessionCacheMode", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 382); }
static uint64_t hle_s383(void) { return hle_stub_default("nnsslConnectionSetSocketDescriptor", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 383); }
static uint64_t hle_s384(void) { return hle_stub_default("nnsslConnectionSetVerifyOption", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 384); }
static uint64_t hle_s385(void) { return hle_stub_default("nnsslConnectionWrite", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 385); }
static uint64_t hle_s386(void) { return hle_stub_default("nnsslContextDestroy", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 386); }
static uint64_t hle_s387(void) { return hle_stub_default("nnsslContextGetContextId", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 387); }
static uint64_t hle_s388(void) { return hle_stub_default("nnsslFinalize", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 388); }
static uint64_t hle_s389(void) { return hle_stub_default("nnsslInitialize", "网络(宿主:离线桩)；家族 nn::ssl；main 引用", 389); }
static uint64_t hle_s390(void) { return hle_stub_default("_ZN2nn2sf4hipc15SendSyncRequestENS_3svc6HandleEPvm", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 390); }
static uint64_t hle_s391(void) { return hle_stub_default("_ZN2nn2sf4hipc20ConnectToHipcServiceEPNS_3svc6HandleEPKc", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 391); }
static uint64_t hle_s392(void) { return hle_stub_default("_ZN2nn2sf4hipc21GetMessageBufferOnTlsEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 392); }
static uint64_t hle_s393(void) { return hle_stub_default("_ZN2nn2sf4hipc24CloseClientSessionHandleENS_3svc6HandleE", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 393); }
static uint64_t hle_s394(void) { return hle_stub_default("_ZN2nn2sf4hipc29FinalizeHipcServiceResolutionEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 394); }
static uint64_t hle_s395(void) { return hle_stub_default("_ZN2nn2sf4hipc31InitializeHipcServiceResolutionEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 395); }
static uint64_t hle_s396(void) { return hle_stub_default("_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor11SetupHeaderEPvmRKNS2_21HipcMessageHeaderInfoE", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 396); }
static uint64_t hle_s397(void) { return hle_stub_default("_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor6SetTagEt", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 397); }
static uint64_t hle_s398(void) { return hle_stub_default("_ZN2nn2sf4hipc6server24HipcServerSessionManager14ProcessMessageEPNS2_21HipcServerSessionBaseEPKNS1_6detail25HipcMessageBufferAccessorEPKNS6_21HipcMessageHeaderInfoEPvm", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 398); }
static uint64_t hle_s399(void) { return hle_stub_default("_ZN2nn2sf4hipc6server24HipcServerSessionManager15ProcessMessage2EPNS2_21HipcServerSessionBaseEPvmS6_m", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 399); }
static uint64_t hle_s400(void) { return hle_stub_default("_ZN2nn2sf4hipc6server28HipcServerSessionManagerBase10AcceptBaseEPNS2_21HipcServerSessionBaseENS_3svc6HandleE", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 400); }
static uint64_t hle_s401(void) { return hle_stub_default("_ZN2nn2sf4hipc6server34HipcServerSessionManagerWithDomain20ProcessManagerInvokeEPNS2_17HipcServerSessionEPKNS1_6detail25HipcMessageBufferAccessorEPKNS6_21HipcMessageHeaderInfoEPvm", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 401); }
static uint64_t hle_s402(void) { return hle_stub_default("_ZN2nn2sf4hipc6server34HipcServerSessionManagerWithDomain21Process2ManagerInvokeEPNS2_17HipcServerSessionEPvmS6_m", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 402); }
static uint64_t hle_s403(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase14InitializePortEiiPKc", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 403); }
static uint64_t hle_s404(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase27RegisterServerSessionToWaitEPNS2_17HipcServerSessionE", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 404); }
static uint64_t hle_s405(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase5StartEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 405); }
static uint64_t hle_s406(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase8LoopAutoEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 406); }
static uint64_t hle_s407(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBaseC2EPNSt3__115aligned_storageILm48ELm8EE4typeEi", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 407); }
static uint64_t hle_s408(void) { return hle_stub_default("_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBaseD2Ev", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 408); }
static uint64_t hle_s409(void) { return hle_stub_default("_ZN2nn2sf4impl6detail22ServiceObjectImplBase211ReleaseImplEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 409); }
static uint64_t hle_s410(void) { return hle_stub_default("_ZN2nn2sf4impl6detail22ServiceObjectImplBase216AddReferenceImplEv", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 410); }
static uint64_t hle_s411(void) { return hle_stub_default("_ZN2nn2sf6detail19DefaultAllocateImplEmmm", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 411); }
static uint64_t hle_s412(void) { return hle_stub_default("_ZN2nn2sf6detail21DefaultDeallocateImplEPvmmm", "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用", 412); }
static uint64_t hle_s413(void) { return hle_stub_default("_ZN2nn3fgm7Request10InitializeENS0_6ModuleEjNS_2os14EventClearModeE", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 413); }
static uint64_t hle_s414(void) { return hle_stub_default("_ZN2nn3fgm7Request10SetAndWaitEjj", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 414); }
static uint64_t hle_s415(void) { return hle_stub_default("_ZN2nn3fgm7Request3GetEPj", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 415); }
static uint64_t hle_s416(void) { return hle_stub_default("_ZN2nn3fgm7Request8FinalizeEv", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 416); }
static uint64_t hle_s417(void) { return hle_stub_default("_ZN2nn3fgm7RequestC1Ev", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 417); }
static uint64_t hle_s418(void) { return hle_stub_default("_ZN2nn3fgm7RequestD1Ev", "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用", 418); }
static uint64_t hle_s419(void) { return hle_stub_default("_ZN2nn4lmem13CreateExpHeapEPvmi", "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用", 419); }
static uint64_t hle_s420(void) { return hle_stub_default("_ZN2nn4lmem13FreeToExpHeapEPNS0_6detail8HeapHeadEPv", "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用", 420); }
static uint64_t hle_s421(void) { return hle_stub_default("_ZN2nn4lmem19AllocateFromExpHeapEPNS0_6detail8HeapHeadEm", "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用", 421); }
static uint64_t hle_s422(void) { return hle_stub_default("_ZN2nn5audio12StopAudioOutEPNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 422); }
static uint64_t hle_s423(void) { return hle_stub_default("_ZN2nn5audio13CloseAudioOutEPNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 423); }
static uint64_t hle_s424(void) { return hle_stub_default("_ZN2nn5audio13StartAudioOutEPNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 424); }
static uint64_t hle_s425(void) { return hle_stub_default("_ZN2nn5audio17GetSampleByteSizeENS0_12SampleFormatE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 425); }
static uint64_t hle_s426(void) { return hle_stub_default("_ZN2nn5audio19OpenDefaultAudioOutEPNS0_8AudioOutERKNS0_17AudioOutParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 426); }
static uint64_t hle_s427(void) { return hle_stub_default("_ZN2nn5audio20AppendAudioOutBufferEPNS0_8AudioOutEPNS0_14AudioOutBufferE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 427); }
static uint64_t hle_s428(void) { return hle_stub_default("_ZN2nn5audio21GetAudioOutSampleRateEPKNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 428); }
static uint64_t hle_s429(void) { return hle_stub_default("_ZN2nn5audio21SetAudioOutBufferInfoEPNS0_14AudioOutBufferEPvmm", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 429); }
static uint64_t hle_s430(void) { return hle_stub_default("_ZN2nn5audio23GetAudioOutChannelCountEPKNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 430); }
static uint64_t hle_s431(void) { return hle_stub_default("_ZN2nn5audio23GetAudioOutSampleFormatEPKNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 431); }
static uint64_t hle_s432(void) { return hle_stub_default("_ZN2nn5audio25GetAudioOutBufferDataSizeEPKNS0_14AudioOutBufferE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 432); }
static uint64_t hle_s433(void) { return hle_stub_default("_ZN2nn5audio25GetReleasedAudioOutBufferEPNS0_8AudioOutE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 433); }
static uint64_t hle_s434(void) { return hle_stub_default("_ZN2nn5audio27InitializeAudioOutParameterEPNS0_17AudioOutParameterE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 434); }
static uint64_t hle_s435(void) { return hle_stub_default("_ZN2nn5audio28GetAudioOutBufferDataPointerEPKNS0_14AudioOutBufferE", "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用", 435); }
static uint64_t hle_s436(void) { return hle_stub_default("_ZNKSt11logic_error4whatEv", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 436); }
static uint64_t hle_s437(void) { return hle_stub_default("_ZNSt11logic_errorC2EPKc", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 437); }
static uint64_t hle_s438(void) { return hle_stub_default("_ZTVSt12length_error", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 438); }
static uint64_t hle_s439(void) { return hle_stub_default("_ZdaPv", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 439); }
static uint64_t hle_s440(void) { return hle_stub_default("_ZdlPv", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 440); }
static uint64_t hle_s441(void) { return hle_stub_default("_Znam", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 441); }
static uint64_t hle_s442(void) { return hle_stub_default("_ZnamRKSt9nothrow_t", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 442); }
static uint64_t hle_s443(void) { return hle_stub_default("_Znwm", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 443); }
static uint64_t hle_s444(void) { return hle_stub_default("_ZnwmRKSt9nothrow_t", "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用", 444); }
static uint64_t hle_s445(void) { return hle_stub_default("_ZN2nn4init5StartEmmPFvvES2_", "模块内(服务客户端，仍需接管)；家族 nn::init；rtld 引用", 445); }

const hle_force_ent g_hle_force[] = {
    {"_ZN2nn2fs10CreateFileEPKcl", (void *)(uintptr_t)hle_s0, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs10DeleteFileEPKc", (void *)(uintptr_t)hle_s1, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE", (void *)(uintptr_t)hle_s2, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl", (void *)(uintptr_t)hle_s3, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", (void *)(uintptr_t)hle_s4, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE", (void *)(uintptr_t)hle_s5, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci", (void *)(uintptr_t)hle_s6, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl", (void *)(uintptr_t)hle_s7, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs14CloseDirectoryENS0_15DirectoryHandleE", (void *)(uintptr_t)hle_s8, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs14CommitSaveDataEPKc", (void *)(uintptr_t)hle_s9, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs14EnsureSaveDataERKNS_7account3UidE", (void *)(uintptr_t)hle_s10, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs15CreateDirectoryEPKc", (void *)(uintptr_t)hle_s11, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs22QueryMountRomCacheSizeEPm", (void *)(uintptr_t)hle_s12, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs7UnmountEPKc", (void *)(uintptr_t)hle_s13, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs8MountRomEPKcPvm", (void *)(uintptr_t)hle_s14, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", (void *)(uintptr_t)hle_s15, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", (void *)(uintptr_t)hle_s16, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvmRKNS0_10ReadOptionE", (void *)(uintptr_t)hle_s17, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs9CloseFileENS0_10FileHandleE", (void *)(uintptr_t)hle_s18, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs9FlushFileENS0_10FileHandleE", (void *)(uintptr_t)hle_s19, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE", (void *)(uintptr_t)hle_s20, "模块内(服务客户端，仍需接管)；家族 nn::fs；main 引用"},
    {"_ZN2nn2oe10InitializeEv", (void *)(uintptr_t)hle_s21, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe16GetOperationModeEv", (void *)(uintptr_t)hle_s22, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe17FinishStartupLogoEv", (void *)(uintptr_t)hle_s23, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE", (void *)(uintptr_t)hle_s24, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe18GetDesiredLanguageEv", (void *)(uintptr_t)hle_s25, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe18GetPerformanceModeEv", (void *)(uintptr_t)hle_s26, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe20GetCurrentFocusStateEv", (void *)(uintptr_t)hle_s27, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", (void *)(uintptr_t)hle_s28, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe23EnableGamePlayRecordingEPvm", (void *)(uintptr_t)hle_s29, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe24GetExpectedVolumeBalanceEPfS1_", (void *)(uintptr_t)hle_s30, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe24SetExpectedVolumeBalanceEff", (void *)(uintptr_t)hle_s31, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe25TryPopNotificationMessageEPj", (void *)(uintptr_t)hle_s32, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe27GetPerformanceConfigurationENS0_15PerformanceModeE", (void *)(uintptr_t)hle_s33, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe27SetPerformanceConfigurationENS0_15PerformanceModeEi", (void *)(uintptr_t)hle_s34, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe28SetResumeNotificationEnabledEb", (void *)(uintptr_t)hle_s35, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe29SetScreenShotImageOrientationENS_5album16ImageOrientationE", (void *)(uintptr_t)hle_s36, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe37IsUserInactivityDetectionTimeExtendedEv", (void *)(uintptr_t)hle_s37, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe38SetUserInactivityDetectionTimeExtendedEb", (void *)(uintptr_t)hle_s38, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe42SetOperationModeChangedNotificationEnabledEb", (void *)(uintptr_t)hle_s39, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", (void *)(uintptr_t)hle_s40, "模块内(服务客户端，仍需接管)；家族 nn::oe；main 引用"},
    {"_ZN2nn2pl17GetSharedFontSizeENS0_14SharedFontTypeE", (void *)(uintptr_t)hle_s41, "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用"},
    {"_ZN2nn2pl20GetSharedFontAddressENS0_14SharedFontTypeE", (void *)(uintptr_t)hle_s42, "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用"},
    {"_ZN2nn2pl22GetSharedFontLoadStateENS0_14SharedFontTypeE", (void *)(uintptr_t)hle_s43, "模块内(服务客户端，仍需接管)；家族 nn::pl；main 引用"},
    {"_ZN2nn2vi10InitializeEv", (void *)(uintptr_t)hle_s44, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn2vi11CreateLayerEPPNS0_5LayerEPNS0_7DisplayE", (void *)(uintptr_t)hle_s45, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE", (void *)(uintptr_t)hle_s46, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn2vi18OpenDefaultDisplayEPPNS0_7DisplayE", (void *)(uintptr_t)hle_s47, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn2vi19SetLayerScalingModeEPNS0_5LayerENS0_11ScalingModeE", (void *)(uintptr_t)hle_s48, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn2vi20GetDisplayVsyncEventEPNS_2os15SystemEventTypeEPNS0_7DisplayE", (void *)(uintptr_t)hle_s49, "模块内(服务客户端，仍需接管)；家族 nn::vi；main 引用"},
    {"_ZN2nn3err9ShowErrorENS_6ResultE", (void *)(uintptr_t)hle_s50, "模块内(服务客户端，仍需接管)；家族 nn::err；main 引用"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj", (void *)(uintptr_t)hle_s51, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj", (void *)(uintptr_t)hle_s52, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyLeftStateEiRKj", (void *)(uintptr_t)hle_s53, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj", (void *)(uintptr_t)hle_s54, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13GetNpadStatesEPNS0_17NpadJoyRightStateEiRKj", (void *)(uintptr_t)hle_s55, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13VibrationNode15SetModulationToEPKS1_RKNS0_19VibrationModulationE", (void *)(uintptr_t)hle_s56, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid13VibrationNode6UpdateEv", (void *)(uintptr_t)hle_s57, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid14DisconnectNpadERKj", (void *)(uintptr_t)hle_s58, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid14InitializeNpadEv", (void *)(uintptr_t)hle_s59, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid14VibrationMixer10SetMixModeENS0_16VibrationMixModeE", (void *)(uintptr_t)hle_s60, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid14VibrationMixerC1Ev", (void *)(uintptr_t)hle_s61, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15GetNpadStyleSetERKj", (void *)(uintptr_t)hle_s62, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayer18SetCurrentPositionEi", (void *)(uintptr_t)hle_s63, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayer4LoadEPKvm", (void *)(uintptr_t)hle_s64, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayer4PlayEv", (void *)(uintptr_t)hle_s65, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayer4StopEv", (void *)(uintptr_t)hle_s66, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayer7SetLoopEb", (void *)(uintptr_t)hle_s67, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationPlayerC1Ev", (void *)(uintptr_t)hle_s68, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationTarget24SetVibrationDeviceHandleERKNS0_21VibrationDeviceHandleE", (void *)(uintptr_t)hle_s69, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationTarget26UnsetVibrationDeviceHandleEv", (void *)(uintptr_t)hle_s70, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationTargetC1Ev", (void *)(uintptr_t)hle_s71, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationWriter5WriteERKNS0_14VibrationValueE", (void *)(uintptr_t)hle_s72, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid15VibrationWriterC1Ev", (void *)(uintptr_t)hle_s73, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid16GetDebugPadStateEPNS0_13DebugPadStateE", (void *)(uintptr_t)hle_s74, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid18InitializeDebugPadEv", (void *)(uintptr_t)hle_s75, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid18SendVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE", (void *)(uintptr_t)hle_s76, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid18SetNpadJoyHoldTypeENS0_15NpadJoyHoldTypeE", (void *)(uintptr_t)hle_s77, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid18StartSixAxisSensorERKNS0_19SixAxisSensorHandleE", (void *)(uintptr_t)hle_s78, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid18SwapNpadAssignmentERKjS2_", (void *)(uintptr_t)hle_s79, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", (void *)(uintptr_t)hle_s80, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid20GetNpadJoyAssignmentERKj", (void *)(uintptr_t)hle_s81, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid21InitializeTouchScreenEv", (void *)(uintptr_t)hle_s82, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid21ShowControllerSupportEPNS0_27ControllerSupportResultInfoERKNS0_20ControllerSupportArgE", (void *)(uintptr_t)hle_s83, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorES2_RKj", (void *)(uintptr_t)hle_s84, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid22GetSixAxisSensorStatesEPNS0_18SixAxisSensorStateEiRKNS0_19SixAxisSensorHandleE", (void *)(uintptr_t)hle_s85, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid22SetSupportedNpadIdTypeEPKjm", (void *)(uintptr_t)hle_s86, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23GetSixAxisSensorHandlesEPNS0_19SixAxisSensorHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", (void *)(uintptr_t)hle_s87, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23MergeSingleJoyAsDualJoyERKjS2_", (void *)(uintptr_t)hle_s88, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23VibrationNodeConnection10DisconnectEv", (void *)(uintptr_t)hle_s89, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23VibrationNodeConnection7ConnectEPNS0_13VibrationNodeES3_", (void *)(uintptr_t)hle_s90, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23VibrationNodeConnectionC1EPNS0_13VibrationNodeES3_", (void *)(uintptr_t)hle_s91, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid23VibrationNodeConnectionC1Ev", (void *)(uintptr_t)hle_s92, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid24SetSupportedNpadStyleSetENS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", (void *)(uintptr_t)hle_s93, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid25GetVibrationDeviceHandlesEPNS0_21VibrationDeviceHandleEiRKjNS_4util10BitFlagSetILi32ENS0_12NpadStyleTagEEE", (void *)(uintptr_t)hle_s94, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid25InitializeVibrationDeviceERKNS0_21VibrationDeviceHandleE", (void *)(uintptr_t)hle_s95, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid28SetNpadJoyAssignmentModeDualERKj", (void *)(uintptr_t)hle_s96, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid30SetNpadJoyAssignmentModeSingleERKj", (void *)(uintptr_t)hle_s97, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3hid30SetNpadJoyAssignmentModeSingleERKjNS0_17NpadJoyDeviceTypeE", (void *)(uintptr_t)hle_s98, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZN2nn3nfp10GetTagInfoEPNS0_7TagInfoERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s99, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp10InitializeEv", (void *)(uintptr_t)hle_s100, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp11ListDevicesEPNS0_12DeviceHandleEPii", (void *)(uintptr_t)hle_s101, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp12GetModelInfoEPNS0_9ModelInfoERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s102, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp13StopDetectionERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s103, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp14GetDeviceStateERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s104, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp14StartDetectionERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s105, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp19AttachActivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s106, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp21AttachDeactivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s107, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp5MountERKNS0_12DeviceHandleENS0_9ModelTypeENS0_11MountTargetE", (void *)(uintptr_t)hle_s108, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp7UnmountERKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s109, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp8FinalizeEv", (void *)(uintptr_t)hle_s110, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nfp9GetNpadIdEPjRKNS0_12DeviceHandleE", (void *)(uintptr_t)hle_s111, "模块内(服务客户端，仍需接管)；家族 nn::nfp；main 引用"},
    {"_ZN2nn3nsd9ResolveExEPNS0_4FqdnERKS1_", (void *)(uintptr_t)hle_s112, "模块内(服务客户端，仍需接管)；家族 nn::nsd；main 引用"},
    {"_ZN2nn3ssl10InitializeEv", (void *)(uintptr_t)hle_s113, "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用"},
    {"_ZN2nn3ssl7Context15ImportServerPkiEPmPKcjNS0_17CertificateFormatE", (void *)(uintptr_t)hle_s114, "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用"},
    {"_ZN2nn3ssl7Context6CreateENS1_10SslVersionE", (void *)(uintptr_t)hle_s115, "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用"},
    {"_ZN2nn3ssl8FinalizeEv", (void *)(uintptr_t)hle_s116, "模块内(服务客户端，仍需接管)；家族 nn::ssl；main 引用"},
    {"_ZN2nn3web19ShowOfflineHtmlPageEPNS0_26OfflineHtmlPageReturnValueERKNS0_22ShowOfflineHtmlPageArgE", (void *)(uintptr_t)hle_s117, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web22ShowOfflineHtmlPageArg16SetFooterEnabledEb", (void *)(uintptr_t)hle_s118, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web22ShowOfflineHtmlPageArg17SetBackgroundKindERKNS0_21OfflineBackgroundKindE", (void *)(uintptr_t)hle_s119, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web22ShowOfflineHtmlPageArg17SetPointerEnabledEb", (void *)(uintptr_t)hle_s120, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web22ShowOfflineHtmlPageArg18SetBootDisplayKindENS0_22OfflineBootDisplayKindE", (void *)(uintptr_t)hle_s121, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web26OfflineHtmlPageReturnValueC1Ev", (void *)(uintptr_t)hle_s122, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web36ShowOfflineHtmlPageArgWithPlayReport20SetPlayReportEnabledEb", (void *)(uintptr_t)hle_s123, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn3web36ShowOfflineHtmlPageArgWithPlayReportC1ERKNS_13ApplicationIdEPKc", (void *)(uintptr_t)hle_s124, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZN2nn4lmem15CreateFrameHeapEPvmi", (void *)(uintptr_t)hle_s125, "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用"},
    {"_ZN2nn4lmem16DestroyFrameHeapEPNS0_6detail8HeapHeadE", (void *)(uintptr_t)hle_s126, "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用"},
    {"_ZN2nn4lmem21AllocateFromFrameHeapEPNS0_6detail8HeapHeadEmi", (void *)(uintptr_t)hle_s127, "模块内(服务客户端，仍需接管)；家族 nn::lmem；main 引用"},
    {"_ZN2nn4nifm10InitializeEv", (void *)(uintptr_t)hle_s128, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm18IsNetworkAvailableEv", (void *)(uintptr_t)hle_s129, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm19SetLocalNetworkModeEb", (void *)(uintptr_t)hle_s130, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm20SubmitNetworkRequestEv", (void *)(uintptr_t)hle_s131, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm22GetCurrentProxySettingEPNS0_12ProxySettingE", (void *)(uintptr_t)hle_s132, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm22IsNetworkRequestOnHoldEv", (void *)(uintptr_t)hle_s133, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm26GetCurrentPrimaryIpAddressEP7in_addr", (void *)(uintptr_t)hle_s134, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm26HandleNetworkRequestResultEv", (void *)(uintptr_t)hle_s135, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4nifm27SubmitNetworkRequestAndWaitEv", (void *)(uintptr_t)hle_s136, "模块内(服务客户端，仍需接管)；家族 nn::nifm；main 引用"},
    {"_ZN2nn4time10InitializeEv", (void *)(uintptr_t)hle_s137, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time11ToPosixTimeEPiPNS0_9PosixTimeEiRKNS0_12CalendarTimeE", (void *)(uintptr_t)hle_s138, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time13IsInitializedEv", (void *)(uintptr_t)hle_s139, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time14ToCalendarTimeEPNS0_12CalendarTimeEPNS0_22CalendarAdditionalInfoERKNS0_9PosixTimeE", (void *)(uintptr_t)hle_s140, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time18ToPosixTimeFromUtcERKNS0_12CalendarTimeE", (void *)(uintptr_t)hle_s141, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time19ToCalendarTimeInUtcERKNS0_9PosixTimeE", (void *)(uintptr_t)hle_s142, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time23StandardUserSystemClock14GetCurrentTimeEPNS0_9PosixTimeE", (void *)(uintptr_t)hle_s143, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn4time8FinalizeEv", (void *)(uintptr_t)hle_s144, "模块内(服务客户端，仍需接管)；家族 nn::time；main 引用"},
    {"_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_10SubMixTypeE", (void *)(uintptr_t)hle_s145, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio11RemoveDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPNS0_12FinalMixTypeE", (void *)(uintptr_t)hle_s146, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio12IsVoiceValidEPKNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s147, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio12SetDelayTimeEPNS0_9DelayTypeENS_8TimeSpanE", (void *)(uintptr_t)hle_s148, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13AcquireSubMixEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEii", (void *)(uintptr_t)hle_s149, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13AddDeviceSinkEPNS0_19AudioRendererConfigEPNS0_14DeviceSinkTypeEPNS0_12FinalMixTypeEPKaiPKc", (void *)(uintptr_t)hle_s150, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13GetSinkNodeIdEPKNS0_14DeviceSinkTypeE", (void *)(uintptr_t)hle_s151, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13GetSinkNodeIdEPKNS0_22CircularBufferSinkTypeE", (void *)(uintptr_t)hle_s152, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13SetAuxEnabledEPNS0_7AuxTypeEb", (void *)(uintptr_t)hle_s153, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio13SetVoicePitchEPNS0_9VoiceTypeEf", (void *)(uintptr_t)hle_s154, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio14GetVoiceNodeIdEPKNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s155, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio14IsAuxRemovableEPKNS0_7AuxTypeE", (void *)(uintptr_t)hle_s156, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio14SetDelayInGainEPNS0_9DelayTypeEf", (void *)(uintptr_t)hle_s157, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio14SetVoiceVolumeEPNS0_9VoiceTypeEf", (void *)(uintptr_t)hle_s158, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15AcquireFinalMixEPNS0_19AudioRendererConfigEPNS0_12FinalMixTypeEi", (void *)(uintptr_t)hle_s159, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15GetSubMixNodeIdEPKNS0_10SubMixTypeE", (void *)(uintptr_t)hle_s160, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfo10GetEntriesEPi", (void *)(uintptr_t)hle_s161, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfo15MoveToNextFrameEv", (void *)(uintptr_t)hle_s162, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfo22GetTotalProcessingTimeEv", (void *)(uintptr_t)hle_s163, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfo9SetBufferEPKvm", (void *)(uintptr_t)hle_s164, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfoC1Ev", (void *)(uintptr_t)hle_s165, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15PerformanceInfoD1Ev", (void *)(uintptr_t)hle_s166, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15SetDelayDryGainEPNS0_9DelayTypeEf", (void *)(uintptr_t)hle_s167, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio15SetDelayEnabledEPNS0_9DelayTypeEb", (void *)(uintptr_t)hle_s168, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16AcquireVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEiiNS0_12SampleFormatEiPKvm", (void *)(uintptr_t)hle_s169, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16AppendWaveBufferEPNS0_9VoiceTypeEPKNS0_10WaveBufferE", (void *)(uintptr_t)hle_s170, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16GetAuxSampleRateEPKNS0_7AuxTypeE", (void *)(uintptr_t)hle_s171, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16IsDelayRemovableEPKNS0_9DelayTypeE", (void *)(uintptr_t)hle_s172, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16ReleaseVoiceSlotEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s173, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio16SetVoicePriorityEPNS0_9VoiceTypeEi", (void *)(uintptr_t)hle_s174, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17AcquireMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeEPvm", (void *)(uintptr_t)hle_s175, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17GetAuxSampleCountEPKNS0_7AuxTypeE", (void *)(uintptr_t)hle_s176, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17GetVoicePlayStateEPKNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s177, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17OpenAudioRendererEPNS0_19AudioRendererHandleEPNS_2os11SystemEventERKNS0_22AudioRendererParameterEPvm", (void *)(uintptr_t)hle_s178, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17ReadAuxSendBufferEPNS0_7AuxTypeEPii", (void *)(uintptr_t)hle_s179, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17ReleaseMemoryPoolEPNS0_19AudioRendererConfigEPNS0_14MemoryPoolTypeE", (void *)(uintptr_t)hle_s180, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17SetAuxInputOutputEPNS0_7AuxTypeEPKaS4_i", (void *)(uintptr_t)hle_s181, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_10SubMixTypeEfii", (void *)(uintptr_t)hle_s182, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17SetVoiceMixVolumeEPNS0_9VoiceTypeEPNS0_12FinalMixTypeEfii", (void *)(uintptr_t)hle_s183, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17SetVoicePlayStateEPNS0_9VoiceTypeENS1_9PlayStateE", (void *)(uintptr_t)hle_s184, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio17StopAudioRendererENS0_19AudioRendererHandleE", (void *)(uintptr_t)hle_s185, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio18CloseAudioRendererENS0_19AudioRendererHandleE", (void *)(uintptr_t)hle_s186, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio18GetMemoryPoolStateEPKNS0_14MemoryPoolTypeE", (void *)(uintptr_t)hle_s187, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeEPNS0_12FinalMixTypeEfii", (void *)(uintptr_t)hle_s188, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio18SetSubMixMixVolumeEPNS0_10SubMixTypeES2_fii", (void *)(uintptr_t)hle_s189, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio18StartAudioRendererENS0_19AudioRendererHandleE", (void *)(uintptr_t)hle_s190, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio19ListAudioDeviceNameEPNS0_15AudioDeviceNameEi", (void *)(uintptr_t)hle_s191, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio19SetDelayInputOutputEPNS0_9DelayTypeEPKaS4_i", (void *)(uintptr_t)hle_s192, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio19SetDownMixParameterEPNS0_14DeviceSinkTypeEPKNS1_16DownMixParameterE", (void *)(uintptr_t)hle_s193, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_10SubMixTypeE", (void *)(uintptr_t)hle_s194, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio19SetVoiceDestinationEPNS0_19AudioRendererConfigEPNS0_9VoiceTypeEPNS0_12FinalMixTypeE", (void *)(uintptr_t)hle_s195, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20IsMemoryPoolAttachedEPKNS0_14MemoryPoolTypeE", (void *)(uintptr_t)hle_s196, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20IsVoiceDroppedFlagOnEPKNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s197, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20SetDelayFeedbackGainEPNS0_9DelayTypeEf", (void *)(uintptr_t)hle_s198, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeEPNS0_12FinalMixTypeE", (void *)(uintptr_t)hle_s199, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20SetSubMixDestinationEPNS0_19AudioRendererConfigEPNS0_10SubMixTypeES4_", (void *)(uintptr_t)hle_s200, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio20WriteAuxReturnBufferEPNS0_7AuxTypeEPKii", (void *)(uintptr_t)hle_s201, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio21AddCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeEPKaiPvmNS0_12SampleFormatE", (void *)(uintptr_t)hle_s202, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio21GetActiveChannelCountEv", (void *)(uintptr_t)hle_s203, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio21GetReleasedWaveBufferEPNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s204, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio21SetDelayChannelSpreadEPNS0_9DelayTypeEf", (void *)(uintptr_t)hle_s205, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio21SetDelayLowPassAmountEPNS0_9DelayTypeEf", (void *)(uintptr_t)hle_s206, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio22ReadCircularBufferSinkEPNS0_22CircularBufferSinkTypeEPvm", (void *)(uintptr_t)hle_s207, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio23RequestAttachMemoryPoolEPNS0_14MemoryPoolTypeE", (void *)(uintptr_t)hle_s208, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio23RequestDetachMemoryPoolEPNS0_14MemoryPoolTypeE", (void *)(uintptr_t)hle_s209, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio24GetActiveAudioDeviceNameEPNS0_15AudioDeviceNameE", (void *)(uintptr_t)hle_s210, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio24RemoveCircularBufferSinkEPNS0_19AudioRendererConfigEPNS0_22CircularBufferSinkTypeEPNS0_12FinalMixTypeE", (void *)(uintptr_t)hle_s211, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio25GetVoicePlayedSampleCountEPKNS0_9VoiceTypeE", (void *)(uintptr_t)hle_s212, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio25SetPerformanceFrameBufferEPNS0_19AudioRendererConfigEPvm", (void *)(uintptr_t)hle_s213, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE", (void *)(uintptr_t)hle_s214, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio26SetAudioDeviceOutputVolumeEPKNS0_15AudioDeviceNameEf", (void *)(uintptr_t)hle_s215, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio26SetDownMixParameterEnabledEPNS0_14DeviceSinkTypeEb", (void *)(uintptr_t)hle_s216, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio29ExecuteAudioRendererRenderingENS0_19AudioRendererHandleE", (void *)(uintptr_t)hle_s217, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio29GetRequiredBufferSizeForDelayENS_8TimeSpanEii", (void *)(uintptr_t)hle_s218, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio29InitializeAudioRendererConfigEPNS0_19AudioRendererConfigERKNS0_22AudioRendererParameterEPvm", (void *)(uintptr_t)hle_s219, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio29IsValidAudioRendererParameterERKNS0_22AudioRendererParameterE", (void *)(uintptr_t)hle_s220, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio29SetVoiceBiquadFilterParameterEPNS0_9VoiceTypeEiRKNS0_21BiquadFilterParameterE", (void *)(uintptr_t)hle_s221, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio30GetAudioRendererWorkBufferSizeERKNS0_22AudioRendererParameterE", (void *)(uintptr_t)hle_s222, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio32InitializeAudioRendererParameterEPNS0_22AudioRendererParameterE", (void *)(uintptr_t)hle_s223, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio34GetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleE", (void *)(uintptr_t)hle_s224, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio34SetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleEi", (void *)(uintptr_t)hle_s225, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio36AcquireAudioDeviceSwitchNotificationEPNS_2os11SystemEventE", (void *)(uintptr_t)hle_s226, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio36GetAudioRendererConfigWorkBufferSizeERKNS0_22AudioRendererParameterE", (void *)(uintptr_t)hle_s227, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio41GetRequiredBufferSizeForPerformanceFramesERKNS0_22AudioRendererParameterE", (void *)(uintptr_t)hle_s228, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio42GetRequiredBufferSizeForCircularBufferSinkEPKNS0_22AudioRendererParameterEiiNS0_12SampleFormatE", (void *)(uintptr_t)hle_s229, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio43GetRequiredBufferSizeForAuxSendReturnBufferEPKNS0_22AudioRendererParameterEii", (void *)(uintptr_t)hle_s230, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeEPvS7_m", (void *)(uintptr_t)hle_s231, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio6AddAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeEPvS7_m", (void *)(uintptr_t)hle_s232, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_10SubMixTypeENS_8TimeSpanEi", (void *)(uintptr_t)hle_s233, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio8AddDelayEPNS0_19AudioRendererConfigEPNS0_9DelayTypeEPvmPNS0_12FinalMixTypeENS_8TimeSpanEi", (void *)(uintptr_t)hle_s234, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_10SubMixTypeE", (void *)(uintptr_t)hle_s235, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5audio9RemoveAuxEPNS0_19AudioRendererConfigEPNS0_7AuxTypeEPNS0_12FinalMixTypeE", (void *)(uintptr_t)hle_s236, "模块内(服务客户端，仍需接管)；家族 nn::audio；main 引用"},
    {"_ZN2nn5image11JpegDecoder12SetImageDataEPKvm", (void *)(uintptr_t)hle_s237, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZN2nn5image11JpegDecoder6DecodeEPvmiS2_m", (void *)(uintptr_t)hle_s238, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZN2nn5image11JpegDecoder7AnalyzeEv", (void *)(uintptr_t)hle_s239, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZN2nn5image11JpegDecoderC1Ev", (void *)(uintptr_t)hle_s240, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZN2nn5image11JpegDecoderD1Ev", (void *)(uintptr_t)hle_s241, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZN2nn5prepo10PlayReport3AddEPKcd", (void *)(uintptr_t)hle_s242, "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用"},
    {"_ZN2nn5prepo10PlayReport3AddEPKcl", (void *)(uintptr_t)hle_s243, "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用"},
    {"_ZN2nn5prepo10PlayReport4SaveERKNS_7account3UidE", (void *)(uintptr_t)hle_s244, "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用"},
    {"_ZN2nn5prepo10PlayReport9SetBufferEPvm", (void *)(uintptr_t)hle_s245, "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用"},
    {"_ZN2nn5prepo10PlayReportC1EPKc", (void *)(uintptr_t)hle_s246, "模块内(服务客户端，仍需接管)；家族 nn::prepo；main 引用"},
    {"_ZN2nn6crypto16GenerateSha1HashEPvmPKvm", (void *)(uintptr_t)hle_s247, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail7Md5Impl10InitializeEv", (void *)(uintptr_t)hle_s248, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail7Md5Impl6UpdateEPKvm", (void *)(uintptr_t)hle_s249, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail7Md5Impl7GetHashEPvm", (void *)(uintptr_t)hle_s250, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail8Sha1Impl10InitializeEv", (void *)(uintptr_t)hle_s251, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail8Sha1Impl6UpdateEPKvm", (void *)(uintptr_t)hle_s252, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6crypto6detail8Sha1Impl7GetHashEPvm", (void *)(uintptr_t)hle_s253, "模块内(服务客户端，仍需接管)；家族 nn::crypto；main 引用"},
    {"_ZN2nn6detail21UnexpectedDefaultImplEPKcS2_i", (void *)(uintptr_t)hle_s254, "模块内(服务客户端，仍需接管)；家族 nn::detail；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine10GetAdvanceEPsS2_PiS3_jt", (void *)(uintptr_t)hle_s255, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine10GetKerningEPiS2_jj", (void *)(uintptr_t)hle_s256, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine10InitializeEPvj", (void *)(uintptr_t)hle_s257, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine11SetAutoHintEb", (void *)(uintptr_t)hle_s258, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine13ReleasesGlyphEPv", (void *)(uintptr_t)hle_s259, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine13SetBoldWeightEi", (void *)(uintptr_t)hle_s260, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine14GetFontMetricsEPNS0_7MetricsE", (void *)(uintptr_t)hle_s261, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine15AcquireGlyphmapEjt", (void *)(uintptr_t)hle_s262, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine15CheckGlyphExistEj", (void *)(uintptr_t)hle_s263, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine15SetOutlineWidthEt", (void *)(uintptr_t)hle_s264, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine17AcquireOtfKerningEPKNS0_15OtfKerningTableEjjj", (void *)(uintptr_t)hle_s265, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine21AcquireOtfKerningLastEPKNS0_15OtfKerningTableEjj", (void *)(uintptr_t)hle_s266, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine22AcquireOtfKerningFirstEPKNS0_15OtfKerningTableEjj", (void *)(uintptr_t)hle_s267, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine22GetPointerToWorkBufferEv", (void *)(uintptr_t)hle_s268, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine23FinalizeOtfKerningTableEPNS0_15OtfKerningTableEPFvPvS4_ES4_", (void *)(uintptr_t)hle_s269, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine25InitializeOtfKerningTableEPFPvmmS2_ES2_b", (void *)(uintptr_t)hle_s270, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine7SetFontEPc", (void *)(uintptr_t)hle_s271, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine8FinalizeEv", (void *)(uintptr_t)hle_s272, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine8GetErrorEv", (void *)(uintptr_t)hle_s273, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine8LoadFontEPcPKvjj", (void *)(uintptr_t)hle_s274, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine8SetFlagsENS1_5FlagsE", (void *)(uintptr_t)hle_s275, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngine8SetScaleEiiii", (void *)(uintptr_t)hle_s276, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll18ScalableFontEngineC1Ev", (void *)(uintptr_t)hle_s277, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6fontll24ScalableFontEngineHelper6DecodeEPKvj", (void *)(uintptr_t)hle_s278, "模块内(服务客户端，仍需接管)；家族 nn::fontll；main 引用"},
    {"_ZN2nn6socket10InitializeEPvmmi", (void *)(uintptr_t)hle_s279, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket10SetSockOptEiiiPKvj", (void *)(uintptr_t)hle_s280, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket11GetAddrInfoEPKcS2_PK8addrinfoPPS3_", (void *)(uintptr_t)hle_s281, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket11GetPeerNameEiP8sockaddrPj", (void *)(uintptr_t)hle_s282, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket11GetSockNameEiP8sockaddrPj", (void *)(uintptr_t)hle_s283, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket12FreeAddrInfoEP8addrinfo", (void *)(uintptr_t)hle_s284, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket12GetLastErrnoEv", (void *)(uintptr_t)hle_s285, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket4BindEiPK8sockaddrj", (void *)(uintptr_t)hle_s286, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket4PollEP6pollfdmi", (void *)(uintptr_t)hle_s287, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket4RecvEiPvmi", (void *)(uintptr_t)hle_s288, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket4SendEiPKvmi", (void *)(uintptr_t)hle_s289, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket5CloseEi", (void *)(uintptr_t)hle_s290, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket5FcntlEiiz", (void *)(uintptr_t)hle_s291, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket6AcceptEiP8sockaddrPj", (void *)(uintptr_t)hle_s292, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket6ListenEii", (void *)(uintptr_t)hle_s293, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket6SendToEiPKvmiPK8sockaddrj", (void *)(uintptr_t)hle_s294, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket6SocketEiii", (void *)(uintptr_t)hle_s295, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket7ConnectEiPK8sockaddrj", (void *)(uintptr_t)hle_s296, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8FinalizeEv", (void *)(uintptr_t)hle_s297, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8InetNtoaE7in_addr", (void *)(uintptr_t)hle_s298, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8InetNtopEiPKvPcj", (void *)(uintptr_t)hle_s299, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8InetPtonEiPKcPv", (void *)(uintptr_t)hle_s300, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8RecvFromEiPvmiP8sockaddrPj", (void *)(uintptr_t)hle_s301, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket8ShutdownEii", (void *)(uintptr_t)hle_s302, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket9InetHtonlEj", (void *)(uintptr_t)hle_s303, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket9InetHtonsEt", (void *)(uintptr_t)hle_s304, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket9InetNtohlEj", (void *)(uintptr_t)hle_s305, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn6socket9InetNtohsEt", (void *)(uintptr_t)hle_s306, "模块内(服务客户端，仍需接管)；家族 nn::socket；main 引用"},
    {"_ZN2nn7account10InitializeEv", (void *)(uintptr_t)hle_s307, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account11GetNicknameEPNS0_8NicknameERKNS0_3UidE", (void *)(uintptr_t)hle_s308, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContext14GetSystemEventEPNS_2os11SystemEventE", (void *)(uintptr_t)hle_s309, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContext6CancelEv", (void *)(uintptr_t)hle_s310, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContext7HasDoneEPb", (void *)(uintptr_t)hle_s311, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContext9GetResultEv", (void *)(uintptr_t)hle_s312, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContextC1Ev", (void *)(uintptr_t)hle_s313, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12AsyncContextD1Ev", (void *)(uintptr_t)hle_s314, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account12ListAllUsersEPiPNS0_3UidEi", (void *)(uintptr_t)hle_s315, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account17GetLastOpenedUserEPNS0_3UidE", (void *)(uintptr_t)hle_s316, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account19OpenPreselectedUserEPNS0_10UserHandleE", (void *)(uintptr_t)hle_s317, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account26GetNetworkServiceAccountIdEPNS0_23NetworkServiceAccountIdERKNS0_10UserHandleE", (void *)(uintptr_t)hle_s318, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account32IsNetworkServiceAccountAvailableEPbRKNS0_10UserHandleE", (void *)(uintptr_t)hle_s319, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account36EnsureNetworkServiceAccountAvailableERKNS0_10UserHandleE", (void *)(uintptr_t)hle_s320, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account37LoadNetworkServiceAccountIdTokenCacheEPmPcmRKNS0_10UserHandleE", (void *)(uintptr_t)hle_s321, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account44EnsureNetworkServiceAccountIdTokenCacheAsyncEPNS0_12AsyncContextERKNS0_10UserHandleE", (void *)(uintptr_t)hle_s322, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account8OpenUserEPNS0_10UserHandleERKNS0_3UidE", (void *)(uintptr_t)hle_s323, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account9CloseUserERKNS0_10UserHandleE", (void *)(uintptr_t)hle_s324, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7account9GetUserIdEPNS0_3UidERKNS0_10UserHandleE", (void *)(uintptr_t)hle_s325, "模块内(服务客户端，仍需接管)；家族 nn::account；main 引用"},
    {"_ZN2nn7friends10InitializeEv", (void *)(uintptr_t)hle_s326, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn7friends12AsyncContext14GetSystemEventEPNS_2os11SystemEventE", (void *)(uintptr_t)hle_s327, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn7friends12AsyncContextC1Ev", (void *)(uintptr_t)hle_s328, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn7friends12AsyncContextD1Ev", (void *)(uintptr_t)hle_s329, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn7friends14GetProfileListEPNS0_12AsyncContextEPNS0_7ProfileEPKNS_7account23NetworkServiceAccountIdEi", (void *)(uintptr_t)hle_s330, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn7friends7ProfileC1Ev", (void *)(uintptr_t)hle_s331, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZN2nn8settings12LanguageCode4MakeENS0_8LanguageE", (void *)(uintptr_t)hle_s332, "模块内(服务客户端，仍需接管)；家族 nn::settings；main 引用"},
    {"_ZN2nn8settingseqERKNS0_12LanguageCodeES3_", (void *)(uintptr_t)hle_s333, "模块内(服务客户端，仍需接管)；家族 nn::settings；main 引用"},
    {"_ZN2nv18InitializeGraphicsEPvm", (void *)(uintptr_t)hle_s334, "图形(NVN)，宿主接管；main 引用"},
    {"_ZN2nv20SetGraphicsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", (void *)(uintptr_t)hle_s335, "图形(NVN)，宿主接管；main 引用"},
    {"_ZN2nv28SetGraphicsDevtoolsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", (void *)(uintptr_t)hle_s336, "图形(NVN)，宿主接管；main 引用"},
    {"_ZNK2nn3hid15VibrationPlayer11GetFileInfoEv", (void *)(uintptr_t)hle_s337, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZNK2nn3hid15VibrationPlayer9IsPlayingEv", (void *)(uintptr_t)hle_s338, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZNK2nn3hid15VibrationTarget26IsVibrationDeviceHandleSetEv", (void *)(uintptr_t)hle_s339, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZNK2nn3hid23VibrationNodeConnection13GetModulationEv", (void *)(uintptr_t)hle_s340, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZNK2nn3hid23VibrationNodeConnection14GetDestinationEv", (void *)(uintptr_t)hle_s341, "模块内(服务客户端，仍需接管)；家族 nn::hid；main 引用"},
    {"_ZNK2nn3web26OfflineHtmlPageReturnValue10GetLastUrlEv", (void *)(uintptr_t)hle_s342, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZNK2nn3web26OfflineHtmlPageReturnValue20GetOfflineExitReasonEv", (void *)(uintptr_t)hle_s343, "模块内(服务客户端，仍需接管)；家族 nn::web；main 引用"},
    {"_ZNK2nn5image11JpegDecoder20GetAnalyzedDimensionEv", (void *)(uintptr_t)hle_s344, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZNK2nn5image11JpegDecoder25GetAnalyzedWorkBufferSizeEv", (void *)(uintptr_t)hle_s345, "模块内(服务客户端，仍需接管)；家族 nn::image；main 引用"},
    {"_ZNK2nn7friends12AsyncContext9GetResultEv", (void *)(uintptr_t)hle_s346, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZNK2nn7friends7Profile11GetNicknameEv", (void *)(uintptr_t)hle_s347, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZNK2nn7friends7Profile18GetProfileImageUrlEPNS0_3UrlENS0_9ImageSizeE", (void *)(uintptr_t)hle_s348, "模块内(服务客户端，仍需接管)；家族 nn::friends；main 引用"},
    {"_ZTVN10__cxxabiv117__class_type_infoE", (void *)(uintptr_t)hle_s349, "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用"},
    {"_ZTVN10__cxxabiv120__si_class_type_infoE", (void *)(uintptr_t)hle_s350, "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用"},
    {"_ZTVN10__cxxabiv121__vmi_class_type_infoE", (void *)(uintptr_t)hle_s351, "模块内(服务客户端，仍需接管)；家族 nn::__cxxabiv1；main 引用"},
    {"nnsocketBind", (void *)(uintptr_t)hle_s352, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketCancel", (void *)(uintptr_t)hle_s353, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketClose", (void *)(uintptr_t)hle_s354, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketConnect", (void *)(uintptr_t)hle_s355, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketFcntl", (void *)(uintptr_t)hle_s356, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketGetHostByNameCancel", (void *)(uintptr_t)hle_s357, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketGetLastErrno", (void *)(uintptr_t)hle_s358, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketGetPeerName", (void *)(uintptr_t)hle_s359, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketGetSockName", (void *)(uintptr_t)hle_s360, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketGetSockOpt", (void *)(uintptr_t)hle_s361, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketInetHtons", (void *)(uintptr_t)hle_s362, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketInetNtohs", (void *)(uintptr_t)hle_s363, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketPoll", (void *)(uintptr_t)hle_s364, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketRecv", (void *)(uintptr_t)hle_s365, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketRequestCancelHandle", (void *)(uintptr_t)hle_s366, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketSend", (void *)(uintptr_t)hle_s367, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketSetLastErrno", (void *)(uintptr_t)hle_s368, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketSetSockOpt", (void *)(uintptr_t)hle_s369, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsocketSocket", (void *)(uintptr_t)hle_s370, "网络(宿主:离线桩或BSD socket)；家族 nn::socket；main 引用"},
    {"nnsslConnectionCreate", (void *)(uintptr_t)hle_s371, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionDestroy", (void *)(uintptr_t)hle_s372, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionDoHandshakeWithCertBuffer", (void *)(uintptr_t)hle_s373, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionGetConnectionId", (void *)(uintptr_t)hle_s374, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionGetVerifyCertError", (void *)(uintptr_t)hle_s375, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionPeek", (void *)(uintptr_t)hle_s376, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionPending", (void *)(uintptr_t)hle_s377, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionRead", (void *)(uintptr_t)hle_s378, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetHostName", (void *)(uintptr_t)hle_s379, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetIoMode", (void *)(uintptr_t)hle_s380, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetOption", (void *)(uintptr_t)hle_s381, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetSessionCacheMode", (void *)(uintptr_t)hle_s382, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetSocketDescriptor", (void *)(uintptr_t)hle_s383, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionSetVerifyOption", (void *)(uintptr_t)hle_s384, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslConnectionWrite", (void *)(uintptr_t)hle_s385, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslContextDestroy", (void *)(uintptr_t)hle_s386, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslContextGetContextId", (void *)(uintptr_t)hle_s387, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslFinalize", (void *)(uintptr_t)hle_s388, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"nnsslInitialize", (void *)(uintptr_t)hle_s389, "网络(宿主:离线桩)；家族 nn::ssl；main 引用"},
    {"_ZN2nn2sf4hipc15SendSyncRequestENS_3svc6HandleEPvm", (void *)(uintptr_t)hle_s390, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc20ConnectToHipcServiceEPNS_3svc6HandleEPKc", (void *)(uintptr_t)hle_s391, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc21GetMessageBufferOnTlsEv", (void *)(uintptr_t)hle_s392, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc24CloseClientSessionHandleENS_3svc6HandleE", (void *)(uintptr_t)hle_s393, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc29FinalizeHipcServiceResolutionEv", (void *)(uintptr_t)hle_s394, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc31InitializeHipcServiceResolutionEv", (void *)(uintptr_t)hle_s395, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor11SetupHeaderEPvmRKNS2_21HipcMessageHeaderInfoE", (void *)(uintptr_t)hle_s396, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6detail25HipcMessageBufferAccessor6SetTagEt", (void *)(uintptr_t)hle_s397, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server24HipcServerSessionManager14ProcessMessageEPNS2_21HipcServerSessionBaseEPKNS1_6detail25HipcMessageBufferAccessorEPKNS6_21HipcMessageHeaderInfoEPvm", (void *)(uintptr_t)hle_s398, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server24HipcServerSessionManager15ProcessMessage2EPNS2_21HipcServerSessionBaseEPvmS6_m", (void *)(uintptr_t)hle_s399, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server28HipcServerSessionManagerBase10AcceptBaseEPNS2_21HipcServerSessionBaseENS_3svc6HandleE", (void *)(uintptr_t)hle_s400, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server34HipcServerSessionManagerWithDomain20ProcessManagerInvokeEPNS2_17HipcServerSessionEPKNS1_6detail25HipcMessageBufferAccessorEPKNS6_21HipcMessageHeaderInfoEPvm", (void *)(uintptr_t)hle_s401, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server34HipcServerSessionManagerWithDomain21Process2ManagerInvokeEPNS2_17HipcServerSessionEPvmS6_m", (void *)(uintptr_t)hle_s402, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase14InitializePortEiiPKc", (void *)(uintptr_t)hle_s403, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase27RegisterServerSessionToWaitEPNS2_17HipcServerSessionE", (void *)(uintptr_t)hle_s404, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase5StartEv", (void *)(uintptr_t)hle_s405, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBase8LoopAutoEv", (void *)(uintptr_t)hle_s406, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBaseC2EPNSt3__115aligned_storageILm48ELm8EE4typeEi", (void *)(uintptr_t)hle_s407, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4hipc6server6detail29HipcAllInOneServerManagerBaseD2Ev", (void *)(uintptr_t)hle_s408, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4impl6detail22ServiceObjectImplBase211ReleaseImplEv", (void *)(uintptr_t)hle_s409, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf4impl6detail22ServiceObjectImplBase216AddReferenceImplEv", (void *)(uintptr_t)hle_s410, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf6detail19DefaultAllocateImplEmmm", (void *)(uintptr_t)hle_s411, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn2sf6detail21DefaultDeallocateImplEPvmmm", (void *)(uintptr_t)hle_s412, "模块内(服务客户端，仍需接管)；家族 nn::sf；subsdk0 引用"},
    {"_ZN2nn3fgm7Request10InitializeENS0_6ModuleEjNS_2os14EventClearModeE", (void *)(uintptr_t)hle_s413, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn3fgm7Request10SetAndWaitEjj", (void *)(uintptr_t)hle_s414, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn3fgm7Request3GetEPj", (void *)(uintptr_t)hle_s415, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn3fgm7Request8FinalizeEv", (void *)(uintptr_t)hle_s416, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn3fgm7RequestC1Ev", (void *)(uintptr_t)hle_s417, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn3fgm7RequestD1Ev", (void *)(uintptr_t)hle_s418, "模块内(服务客户端，仍需接管)；家族 nn::fgm；subsdk0 引用"},
    {"_ZN2nn4lmem13CreateExpHeapEPvmi", (void *)(uintptr_t)hle_s419, "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用"},
    {"_ZN2nn4lmem13FreeToExpHeapEPNS0_6detail8HeapHeadEPv", (void *)(uintptr_t)hle_s420, "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用"},
    {"_ZN2nn4lmem19AllocateFromExpHeapEPNS0_6detail8HeapHeadEm", (void *)(uintptr_t)hle_s421, "模块内(服务客户端，仍需接管)；家族 nn::lmem；subsdk0 引用"},
    {"_ZN2nn5audio12StopAudioOutEPNS0_8AudioOutE", (void *)(uintptr_t)hle_s422, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio13CloseAudioOutEPNS0_8AudioOutE", (void *)(uintptr_t)hle_s423, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio13StartAudioOutEPNS0_8AudioOutE", (void *)(uintptr_t)hle_s424, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio17GetSampleByteSizeENS0_12SampleFormatE", (void *)(uintptr_t)hle_s425, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio19OpenDefaultAudioOutEPNS0_8AudioOutERKNS0_17AudioOutParameterE", (void *)(uintptr_t)hle_s426, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio20AppendAudioOutBufferEPNS0_8AudioOutEPNS0_14AudioOutBufferE", (void *)(uintptr_t)hle_s427, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio21GetAudioOutSampleRateEPKNS0_8AudioOutE", (void *)(uintptr_t)hle_s428, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio21SetAudioOutBufferInfoEPNS0_14AudioOutBufferEPvmm", (void *)(uintptr_t)hle_s429, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio23GetAudioOutChannelCountEPKNS0_8AudioOutE", (void *)(uintptr_t)hle_s430, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio23GetAudioOutSampleFormatEPKNS0_8AudioOutE", (void *)(uintptr_t)hle_s431, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio25GetAudioOutBufferDataSizeEPKNS0_14AudioOutBufferE", (void *)(uintptr_t)hle_s432, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio25GetReleasedAudioOutBufferEPNS0_8AudioOutE", (void *)(uintptr_t)hle_s433, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio27InitializeAudioOutParameterEPNS0_17AudioOutParameterE", (void *)(uintptr_t)hle_s434, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZN2nn5audio28GetAudioOutBufferDataPointerEPKNS0_14AudioOutBufferE", (void *)(uintptr_t)hle_s435, "模块内(服务客户端，仍需接管)；家族 nn::audio；subsdk0 引用"},
    {"_ZNKSt11logic_error4whatEv", (void *)(uintptr_t)hle_s436, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZNSt11logic_errorC2EPKc", (void *)(uintptr_t)hle_s437, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZTVSt12length_error", (void *)(uintptr_t)hle_s438, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZdaPv", (void *)(uintptr_t)hle_s439, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZdlPv", (void *)(uintptr_t)hle_s440, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_Znam", (void *)(uintptr_t)hle_s441, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZnamRKSt9nothrow_t", (void *)(uintptr_t)hle_s442, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_Znwm", (void *)(uintptr_t)hle_s443, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZnwmRKSt9nothrow_t", (void *)(uintptr_t)hle_s444, "模块内(服务客户端，仍需接管)；家族 nn::-；subsdk0 引用"},
    {"_ZN2nn4init5StartEmmPFvvES2_", (void *)(uintptr_t)hle_s445, "模块内(服务客户端，仍需接管)；家族 nn::init；rtld 引用"},
};
const unsigned g_hle_force_n = 446;
