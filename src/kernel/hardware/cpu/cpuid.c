#include <common.h>
#include <hardware/cpu/cpuid.h>

/*
 * CPUID - info on the CPU
 *
 * http://www.lowlevel.eu/wiki/CPUID
 * http://www.lowlevel.eu/wiki/EFLAGS
 * https://courses.engr.illinois.edu/ece390/books/labmanual/c-prog-mixing.html
 * https://banisterfiend.wordpress.com/2008/08/15/calling-an-asm-function-from-c/
 * https://en.wikipedia.org/wiki/Function_prologue
*/

#define CPUID_VENDOR   0x00000000
#define CPUID_FEATURES 0x00000001
#define CPUID_NAME1    0x80000002
#define CPUID_NAME2    0x80000003
#define CPUID_NAME3    0x80000004

// struct which contains the returned values from cpuid_call
typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_result_t;

typedef struct {
    uint8_t stepping : 4, model : 4, family : 4, type : 2, : 2, model_ext : 4, family_ext : 8, : 4; // EAX
    uint8_t brand_id : 8, clflush_size : 8, processors : 8, apic_id : 8; // EBX
    uint8_t sse3 : 1, : 8, ssse3 : 1, : 8, : 1, sse41 : 1, sse42 : 1, : 8, : 3; // ECX
    uint8_t fpu : 1, vme : 1, : 1, pse : 1, : 2, pae : 1, : 2, apic : 1, : 7, pse36 : 1, : 1,
            clflush : 1, : 2, acpi : 1, mmx : 1, : 1, sse : 1, sse2 : 1, : 1, htt : 1, : 3; // EDX
} __attribute__((packed)) cpuid_features_t;

extern uint32_t cpuid_check();
extern cpuid_result_t* cpuid_call(uint32_t function, cpuid_result_t* res);

#if 0 // prevent Wunused-function
static void cpuid_dump(cpuid_result_t* res) {
    println("EAX=%08x,EBX=%08x,ECX=%08x,EDX=%08x", res->eax, res->ebx, res->ecx, res->edx);
}
#endif

void cpuid_vendor(cpuid_result_t* res) {
    uint32_t buf[4];
    *(uint32_t*) &buf[0] = res->ebx;
    *(uint32_t*) &buf[1] = res->edx;
    *(uint32_t*) &buf[2] = res->ecx;
    buf[3] = '\0';
    print("%s", buf);
}

void cpuid_name(cpuid_result_t* res) {
    uint32_t buf[5];
    *(uint32_t*) &buf[0] = res->eax;
    *(uint32_t*) &buf[1] = res->ebx;
    *(uint32_t*) &buf[2] = res->ecx;
    *(uint32_t*) &buf[3] = res->edx;
    buf[4] = '\0';
    // the following code trims leading and trailing spaces of s
    char* s = (char*) buf;
    while (*s == ' ')
        s++;
    uint8_t spaces = 0;
    for (; *s != '\0'; s++) {
        if (*s == ' ')
            spaces++;
        else {
            for (int i = 0; i < spaces; i++)
                io_putchar(' ');
            spaces = 0;
            io_putchar(*s);
        }
    }
}

// DO NOT make this local, leads to weird behaviour (I have no idea why). Works this way for now.
static cpuid_result_t res;

void cpuid_init() {
    print("CPUID init ... ");
    if (cpuid_check()) {
        print("%2aok%a. ");
        cpuid_name(cpuid_call(CPUID_NAME1, &res));
        cpuid_name(cpuid_call(CPUID_NAME2, &res));
        cpuid_name(cpuid_call(CPUID_NAME3, &res));
        print(" by ");
        cpuid_vendor(cpuid_call(CPUID_VENDOR, &res));
        cpuid_features_t* features = (cpuid_features_t*) cpuid_call(CPUID_FEATURES, &res);
        if (features->sse)     print(", SSE");
        if (features->sse2)    print(", SSE2");
        if (features->sse3)    print(", SSE3");
        if (features->ssse3)   print(", SSSE3");
        if (features->sse41)   print(", SSE4.1");
        if (features->sse42)   print(", SSE4.2");
        if (features->fpu)     print(", FPU");
        if (features->pae)     print(", PAE");
        if (features->mmx)     print(", MMX");
        if (features->vme)     print(", VME");
        if (features->apic)    print(", APIC");
        if (features->acpi)    print(", ACPI");
        if (features->pse)     print(", PSE");
        if (features->pse36)   print(", PSE-36");
        if (features->clflush) print(", CLFLUSH");
        if (features->htt)
            println(", hyperthreading with %d processor(s).", features->processors);
        else
            println(", no hyperthreading.");
    } else
        println("%4afail%a. CPUID not available.");
}