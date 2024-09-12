#ifndef STREFLOP_FPU_H
#define STREFLOP_FPU_H
#ifndef __GLIBC_USE
#define __GLIBC_USE(X) 0
#endif
#include <arm_neon.h>

namespace streflop {

// FPU exception flags (unchanged)
enum FPU_Exceptions {
    FE_INVALID   = 0x0001,
    FE_DENORMAL  = 0x0002,
    FE_DIVBYZERO = 0x0004,
    FE_OVERFLOW  = 0x0008,
    FE_UNDERFLOW = 0x0010,
    FE_INEXACT   = 0x0020,
    FE_ALL_EXCEPT = 0x003F
};

// FPU rounding modes (unchanged)
enum FPU_RoundMode {
    FE_TONEAREST  = 0x0000,
    FE_DOWNWARD   = 0x0400,
    FE_UPWARD     = 0x0800,
    FE_TOWARDZERO = 0x0C00
};

// ARM NEON specific functions to get/set FPCR (Floating-point Control Register)
inline uint64_t get_fpcr() {
    uint64_t fpcr;
    asm volatile("mrs %0, fpcr" : "=r" (fpcr));
    return fpcr;
}

inline void set_fpcr(uint64_t fpcr) {
    asm volatile("msr fpcr, %0" : : "r" (fpcr));
}

// Raise exception for these flags
inline int feraiseexcept(FPU_Exceptions excepts) {
    uint64_t fpcr = get_fpcr();
    fpcr |= (excepts & FE_ALL_EXCEPT);
    set_fpcr(fpcr);
    return 0;
}

// Clear exceptions for these flags
inline int feclearexcept(int excepts) {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~(excepts & FE_ALL_EXCEPT);
    set_fpcr(fpcr);
    return 0;
}

// Get current rounding mode
inline int fegetround() {
    uint64_t fpcr = get_fpcr();
    return (fpcr & 0xC00000) >> 22;
}

// Set a new rounding mode
inline int fesetround(FPU_RoundMode roundMode) {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~0xC00000;
    fpcr |= (roundMode & 0xC00) << 12;
    set_fpcr(fpcr);
    return 0;
}

// ARM NEON environment structure
struct fenv_t {
    uint64_t fpcr;
};

// Default env. Defined in Math.cpp
extern fenv_t FE_DFL_ENV;

// Get FP env into the given structure
inline int fegetenv(fenv_t *envp) {
    envp->fpcr = get_fpcr();
    return 0;
}

// Sets FP env from the given structure
inline int fesetenv(const fenv_t *envp) {
    set_fpcr(envp->fpcr);
    return 0;
}

// Get env and clear exceptions
inline int feholdexcept(fenv_t *envp) {
    fegetenv(envp);
    feclearexcept(FE_ALL_EXCEPT);
    return 0;
}

template<typename T> inline void streflop_init() {
    // Do nothing by default, or for unknown types
}

// Initialize the FPU for the different types
template<> inline void streflop_init<Simple>() {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~0x00C00000; // Clear rounding mode bits
    fpcr |= 0x00000000;  // Set to round to nearest
#if defined(STREFLOP_NO_DENORMALS)
    fpcr |= (1ULL << 24); // Set Flush-to-zero mode
#else
    fpcr &= ~(1ULL << 24); // Clear Flush-to-zero mode
#endif
    set_fpcr(fpcr);
}

template<> inline void streflop_init<Double>() {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~0x00C00000; // Clear rounding mode bits
    fpcr |= 0x00000000;  // Set to round to nearest
#if defined(STREFLOP_NO_DENORMALS)
    fpcr |= (1ULL << 24); // Set Flush-to-zero mode
#else
    fpcr &= ~(1ULL << 24); // Clear Flush-to-zero mode
#endif
    set_fpcr(fpcr);
}

#ifdef Extended
template<> inline void streflop_init<Extended>() {
    // ARM64 doesn't have an extended precision type, so this is the same as Double
    streflop_init<Double>();
}
#endif

} // namespace streflop

#endif // STREFLOP_FPU_H
