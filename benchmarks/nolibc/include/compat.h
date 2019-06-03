// functions exposed to ocaml-freestanding
// these need to not be name mangled

#ifdef __cplusplus
extern "C" {
#endif

    void riscv_poweroff(int status) __attribute__((noreturn));
    void riscv_write(const char* s, unsigned int length);
    unsigned long long riscv_clock_monotonic();

#ifdef __cplusplus
}
#endif


