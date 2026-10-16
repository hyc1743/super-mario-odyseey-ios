// Actual ARM64 assembly regression: guest 4 KiB stack -> native host stack,
// >16 KiB host work, nested gate reentry, guest register updates and SP return.
#include "../dm.h"
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

extern int gate_stack_probe(void *top);
extern void dm_gate(void);
static int calls, depth, fails;
static uintptr_t guest_low, guest_high;
void *dm_host_stack_top(void) { return pthread_get_stackaddr_np(pthread_self()); }
size_t dm_host_stack_size(void) { return pthread_get_stacksize_np(pthread_self()); }
void unimpl_c(unsigned index, const uint64_t *regs) { (void)index; (void)regs; abort(); }
void dm_thread_returned(void) { abort(); }
uint64_t gate_dispatch(uint64_t ret, uint64_t *regs) {
    (void)ret;
    volatile uint8_t large_frame[16384];
    uintptr_t local = (uintptr_t)&large_frame;
    if (local >= guest_low && local < guest_high) fails++;
    if (dm_gate_stack((const void *)local) != NULL) fails++;
    for (size_t i = 0; i < sizeof large_frame; i++) large_frame[i] = (uint8_t)i;
    calls++;
    if (!depth) { depth++; dm_gate(); depth--; }
    if (large_frame[12345] != (uint8_t)12345) fails++;
    regs[0] = 0x42;
    if ((uintptr_t)regs >= guest_low && (uintptr_t)regs < guest_high) regs[21] = 0x2121;
    return 0;
}
int main(void) {
    host_thread_keys_create();
    guest_thread_init();
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    uint8_t *mapping = mmap(NULL, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED || mprotect(mapping + page, page, PROT_READ | PROT_WRITE)) return 2;
    // Keep the bottom guard immediately below an actual 4 KiB usable stack.
    guest_low = (uintptr_t)(mapping + page);
    guest_high = guest_low + 4096;
    if (!gate_stack_probe((void *)guest_high)) fails++;
    if (calls != 2) fails++;
    // Calling a gate from a native guest entry must keep the active SP too.
    dm_gate();
    if (calls != 4) fails++;
    munmap(mapping, page * 3);
    printf("native/guest/nested gate stack: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
