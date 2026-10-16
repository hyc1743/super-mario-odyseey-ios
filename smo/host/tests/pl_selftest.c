#include "../hle.h"
#include <sys/stat.h>
#ifdef __MINGW32__
#include <direct.h>
#include <process.h>
#define make_dir(p) _mkdir(p)
#define drop_dir(p) _rmdir(p)
#define getpid _getpid
#define setenv(k,v,o) _putenv_s(k,v)
#else
#include <unistd.h>
#define make_dir(p) mkdir(p, 0700)
#define drop_dir(p) rmdir(p)
#endif

void unimpl_stubs(void) {}
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static void write_font(const char *dir, const char *name, const void *data, size_t size) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) { CHECK(fwrite(data, 1, size, f) == size); fclose(f); }
}
int main(int argc, char **argv) {
    int (*state)(int) = (void *)dm_hle_force_lookup("_ZN2nn2pl22GetSharedFontLoadStateENS0_14SharedFontTypeE");
    const void *(*address)(int) = (void *)dm_hle_force_lookup("_ZN2nn2pl20GetSharedFontAddressENS0_14SharedFontTypeE");
    uint64_t (*size)(int) = (void *)dm_hle_force_lookup("_ZN2nn2pl17GetSharedFontSizeENS0_14SharedFontTypeE");
    CHECK(state && address && size);
    CHECK(!state(-1) && !state(6) && !address(-1) && !size(6));
    if (argc > 1) {
        setenv("SMO_FONT_DIR", argv[1], 1);
        for (int i = 0; i < 6; i++) {
            CHECK(state(i) == 1 && address(i) != NULL && size(i) > 1000);
            CHECK(address(i) == address(i));
        }
    } else {
        const char *tmp = getenv("TMP");
        if (!tmp) tmp = "/tmp";
        char dir[1024];
        snprintf(dir, sizeof dir, "%s/smo-font-test-%u", tmp, (unsigned)getpid());
        if (make_dir(dir)) return 2;
        setenv("SMO_FONT_DIR", dir, 1);
        uint8_t font[32] = {0,1,0,0, 0,1,0,0, 0,0,0,0, 'c','m','a','p', 0,0,0,0, 0,0,0,28, 0,0,0,4, 1,2,3,4};
        write_font(dir, "standard.ttf", font, sizeof font);
        write_font(dir, "simplified.ttf", "invalid font", 12);
        font[27] = 5; // table crosses EOF
        write_font(dir, "traditional.ttf", font, sizeof font);
        CHECK(state(0) == 1 && size(0) == sizeof font);
        const uint8_t *p = address(0);
        CHECK(p && p[28] == 1 && p[31] == 4);
        CHECK(!state(1) && !address(1) && !size(1));
        CHECK(!state(2) && !address(2) && !size(2));
        CHECK(!state(3) && !address(3) && !size(3));
        const char *names[] = {"standard.ttf", "simplified.ttf", "traditional.ttf"};
        for (unsigned i = 0; i < 3; i++) {
            char path[2048]; snprintf(path, sizeof path, "%s/%s", dir, names[i]); remove(path);
        }
        CHECK(address(0) == p && state(0) == 1 && p[31] == 4); // immutable process lifetime storage
        CHECK(!drop_dir(dir));
    }
    printf("shared-font service: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
