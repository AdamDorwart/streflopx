/*
    streflop: STandalone REproducible FLOating-Point32_t eXtended
    Nicolas Brodu, 2006 (original streflop author)
    Adam Dorwart, 2024 (streflopx author)
    Code released according to the GNU Lesser General Public License

    Heavily relies on GNU Libm, itself depending on netlib fplibm, GNU MP, and IBM MP lib.
    Uses SoftFloat too.

    Please read the history and copyright information in the documentation provided with the source code
*/

/*
    x86/x64
	For reference, the layout of the MXCSR register:
	FZ:RC:RC:PM:UM:OM:ZM:DM:IM:Rsvd:PE:UE:OE:ZE:DE:IE
	15 14 13 12 11 10  9  8  7   6   5  4  3  2  1  0

	And the layout of the 387 FPU control word register:
	Rsvd:Rsvd:Rsvd:X:RC:RC:PC:PC:Rsvd:Rsvd:PM:UM:OM:ZM:DM:IM
	 15   14   13 12 11 10  9  8   7    6   5  4  3  2  1  0

	Where:
		Rsvd - Reserved
		FZ   - Flush to Zero
		RC   - Rounding Control
		PM   - Precision Mask
		UM   - Underflow Mask
		OM   - Overflow Mask
		ZM   - Zerodivide Mask
		DM   - Denormal Mask
		IM   - Invalid Mask
		PE   - Precision Exception
		UE   - Underflow Exception
		OE   - Overflow Exception
		ZE   - Zerodivide Exception
		DE   - Denormal Exception
		IE   - Invalid Exception
		X    - Infinity control (unused on 387 and higher)
		PC   - Precision Control

	Source: Intel Architecture Software Development Manual, Volume 1, Basic Architecture
    
    Aarch64
	For reference, the layout of the FPCR register:
     Rsvd:AHP:DN:FZ:RMode:Stride:FZ16: Len :IDE:Rsvd:EBF:IXE:UFE:OFE:DZE:IOE:Rsvd:NEP:AH:FIZ
    63-27  26 25 24 23-22  21-20   19 18-16  15  14   13  12  11  10  9   8   7-3  2   1  0

    Where:
        Rsvd   - Reserved
        AHP    - Alternative Half-Precision
        DN     - Default NaN mode
        FZ     - Flush-to-zero mode
        RMode  - Rounding Mode (2 bits)
        Stride - AArch32 Stride (2 bits)
        FZ16   - Flush-to-zero mode for 16-bit floating-point32_t numbers
        Len    - AArch32 Length (3 bits)
        IDE    - Input Denormal exception trap enable
        EBF    - Extended Bfloat16 behaviors
        IXE    - Inexact exception trap enable
        UFE    - Underflow exception trap enable
        OFE    - Overflow exception trap enable
        DZE    - Division by Zero exception trap enable
        IOE    - Invalid Operation exception trap enable
        NEP    - Output element control for SIMD scalar instructions
        AH     - Alternative Handling of floating-point32_t numbers
        FIZ    - Flush inputs to zero mode
        
	Source: https://developer.arm.com/documentation/ddi0601/2024-06/AArch64-Registers/FPCR--Floating-point-Control-Register
*/

// Included by the main streflop include file
// module broken apart for logical code separation
#ifndef STREFLOP_FPU_H
#define STREFLOP_FPU_H

#include "System.h"

// Can safely make the symbols from softfloat visible to user program, protected in namespace
#if defined(STREFLOP_SOFT)
#include "softfloat/softfloat.h"
#endif

namespace streflop {

// We do not use libm, so let's copy a few flags and C99 functions
// Give warning in case these flags would be defined already, this is indication
// of potential confusion!

#if defined(FE_INVALID) || defined(FE_DENORMAL) || defined(FE_DIVBYZERO) || defined(FE_OVERFLOW) || defined(FE_UNDERFLOW) || defined(FE_INEXACT) || defined(FE_DOWNWARD) || defined(FE_TONEAREST) || defined(FE_TOWARDZERO) || defined(FE_UPWARD)

#warning STREFLOP: FE_XXX flags were already defined and will be redefined! Check you do not use the system libm.
#undef FE_INVALID
#undef FE_DENORMAL
#undef FE_DIVBYZERO
#undef FE_OVERFLOW
#undef FE_UNDERFLOW
#undef FE_INEXACT
#undef FE_INEXACT
#undef FE_ALL_EXCEPT
#undef FE_DOWNWARD
#undef FE_TONEAREST
#undef FE_TOWARDZERO
#undef FE_UPWARD
#undef FE_ROUND_MASK
#endif

#if defined(STREFLOP_X87) || defined(STREFLOP_SSE) || defined(STREFLOP_SOFT)
// Flags for FPU exceptions
enum FPU_Exceptions {

    // Invalid operation. If not signaling, gives NaN instead
    FE_INVALID = 0x0001,
    #define FE_INVALID FE_INVALID

    // Extension: for x86 and SSE
    // Denormal operand. If not signaling, use denormal arithmetic as usual
    FE_DENORMAL = 0x0002,
    #define FE_DENORMAL FE_DENORMAL

    // Division by zero. If not signaling, uses +/- infinity
    FE_DIVBYZERO = 0x0004,
    #define FE_DIVBYZERO FE_DIVBYZERO

    // Overflow. If not signaling, round to nearest (including infinity) according to rounding mode
    FE_OVERFLOW = 0x0008,
    #define FE_OVERFLOW FE_OVERFLOW

    // Underflow. If not signaling, use 0 instead
    FE_UNDERFLOW = 0x0010,
    #define FE_UNDERFLOW FE_UNDERFLOW

    // Rounding was not exact (ex: sqrt(2) is never exact) or when overflow causes rounding
    FE_INEXACT = 0x0020,
    #define FE_INEXACT FE_INEXACT

    // Combination of all the above
    FE_ALL_EXCEPT  = 0x003F
    #define FE_ALL_EXCEPT FE_ALL_EXCEPT
};

// Flags for FPU rounding modes
enum FPU_RoundMode {
    FE_TONEAREST  = 0x0000,
    #define FE_TONEAREST FE_TONEAREST

    FE_DOWNWARD   = 0x0400,
    #define FE_DOWNWARD FE_DOWNWARD

    FE_UPWARD     = 0x0800,
    #define FE_UPWARD FE_UPWARD

    FE_TOWARDZERO = 0x0C00
    #define FE_TOWARDZERO FE_TOWARDZERO
};

#elif defined(STREFLOP_NEON)
// Flags for FPU exceptions
enum FPU_Exceptions {
    FE_INVALID   = 1 << 8,
    #define FE_INVALID FE_INVALID
    
    FE_DIVBYZERO = 1 << 9,
    #define FE_DIVBYZERO FE_DIVBYZERO
    
    FE_OVERFLOW  = 1 << 10,
    #define FE_OVERFLOW FE_OVERFLOW
    
    FE_UNDERFLOW = 1 << 11,
    #define FE_UNDERFLOW FE_UNDERFLOW
    
    FE_INEXACT   = 1 << 12,
    #define FE_INEXACT FE_INEXACT
    
    FE_DENORMAL  = 1 << 15,
    #define FE_DENORMAL FE_DENORMAL
    
    FE_ALL_EXCEPT = 0b1001111100000000
    #define FE_ALL_EXCEPT FE_ALL_EXCEPT
};

// Flags for FPU rounding modes
enum FPU_RoundMode {
    FE_TONEAREST  = 0b00 << 22,
    #define FE_TONEAREST FE_TONEAREST
    
    FE_UPWARD     = 0b01 << 22,
    #define FE_UPWARD FE_UPWARD
    
    FE_DOWNWARD   = 0b10 << 22,
    #define FE_DOWNWARD FE_DOWNWARD
    
    FE_TOWARDZERO = 0b11 << 22,
    #define FE_TOWARDZERO FE_TOWARDZERO
    
    FE_ROUND_MASK = 0b11 << 22
    #define FE_ROUND_MASK FE_ROUND_MASK
};
#endif

/* Note: SSE control word, bits 0..15
0->5: Run-time status flags
6: DAZ (denormals are zero, i.e. don't use denormals if bit is 1)
7->12: Exception flags, same meaning as for the x87 ones
13,14: Rounding flags, same meaning as for the x87 ones
15: Flush to zero (FTZ) for automatic handling of underflow (default is NO)
*/

// plan for portability
#ifdef _MSC_VER
#define STREFLOP_FSTCW(cw) do { short tmp; __asm { fstcw tmp }; (cw) = tmp; } while (0)
#define STREFLOP_FLDCW(cw) do { short tmp = (cw); __asm { fclex }; __asm { fldcw tmp }; } while (0)
#define STREFLOP_STMXCSR(cw) do { int32_t tmp; __asm { stmxcsr tmp }; (cw) = tmp; } while (0)
#define STREFLOP_LDMXCSR(cw) do { int32_t tmp = (cw); __asm { ldmxcsr tmp }; } while (0)
#else
#define STREFLOP_FSTCW(cw) do { asm volatile ("fstcw %0" : "=m" (cw) : ); } while (0)
#define STREFLOP_FLDCW(cw) do { asm volatile ("fclex \n fldcw %0" : : "m" (cw)); } while (0)
#define STREFLOP_STMXCSR(cw) do { asm volatile ("stmxcsr %0" : "=m" (cw) : ); } while (0)
#define STREFLOP_LDMXCSR(cw) do { asm volatile ("ldmxcsr %0" : : "m" (cw) ); } while (0)
#endif

// Subset of all C99 functions

#if defined(STREFLOP_X87)

/// Raise exception for these flags
inline int32_t feraiseexcept(FPU_Exceptions excepts) {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode &= ~( excepts ); // generate error for selection
    STREFLOP_FLDCW(fpu_mode);
    return 0;
}

/// Clear exceptions for these flags
inline int32_t feclearexcept(int32_t excepts) {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode |= excepts;
    STREFLOP_FLDCW(fpu_mode);
    return 0;
}

/// Get current rounding mode
inline int32_t fegetround() {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    return fpu_mode & 0x0C00;
}

/// Set a new rounding mode
inline int32_t fesetround(FPU_RoundMode roundMode) {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode &= 0xF3FF; // clear current mode
    fpu_mode |= roundMode; // sets new mode
    STREFLOP_FLDCW(fpu_mode);
    return 0;
}

typedef int16_t fenv_t;

/// Default env. Defined in Math.cpp to be 0, and initalized on first use to the permanent holder
extern fenv_t FE_DFL_ENV;

/// Get FP env into the given structure
inline int32_t fegetenv(fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (!FE_DFL_ENV) STREFLOP_FSTCW(FE_DFL_ENV);
    // Now store env into argument
    STREFLOP_FSTCW(*envp);
    return 0;
}

/// Sets FP env from the given structure
inline int32_t fesetenv(const fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (!FE_DFL_ENV) STREFLOP_FSTCW(FE_DFL_ENV);
    // Now overwrite current env by argument
    STREFLOP_FLDCW(*envp);
    return 0;
}

/// get env and clear exceptions
inline int32_t feholdexcept(fenv_t *envp) {
    fegetenv(envp);
    feclearexcept(FE_ALL_EXCEPT);
    return 0;
}


template<typename T> inline void streflop_init() {
    struct X {};
    X Unknown_numeric_type;
    // unknown types do not compile
    T error = Unknown_numeric_type;
}

/// Initialize the FPU for the different types
/// this may also be called to switch between code sections using
/// different precisions
template<> inline void streflop_init<Simple>() {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode &= 0xFCFF; // 32 bits internal operations
    STREFLOP_FLDCW(fpu_mode);
}

template<> inline void streflop_init<Double>() {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode &= 0xFCFF;
    fpu_mode |= 0x0200; // 64 bits internal operations
    STREFLOP_FLDCW(fpu_mode);
}

#ifdef Extended
template<> inline void streflop_init<Extended>() {
    uint16_t fpu_mode;
    STREFLOP_FSTCW(fpu_mode);
    fpu_mode &= 0xFCFF;
    fpu_mode |= 0x0300; // 80 bits internal operations
    STREFLOP_FLDCW(fpu_mode);
}
#endif

#elif defined(STREFLOP_SSE)

/// Raise exception for these flags
inline int32_t feraiseexcept(FPU_Exceptions excepts) {
    // Just in case the compiler would store a value on the st(x) registers
    uint16_t x87_mode;
    STREFLOP_FSTCW(x87_mode);
    x87_mode &= ~( excepts ); // generate error for selection
    STREFLOP_FLDCW(x87_mode);

    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
    sse_mode &= ~( excepts << 7 ); // generate error for selection
    STREFLOP_LDMXCSR(sse_mode);

    return 0;
}

/// Clear exceptions for these flags
inline int32_t feclearexcept(int32_t excepts) {
    // Just in case the compiler would store a value on the st(x) registers
    uint16_t x87_mode;
    STREFLOP_FSTCW(x87_mode);
    x87_mode |= excepts;
    STREFLOP_FLDCW(x87_mode);

    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
    sse_mode |= excepts << 7;
    STREFLOP_LDMXCSR(sse_mode);

    return 0;
}

/// Get current rounding mode
inline int32_t fegetround() {
    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
    return (sse_mode>>3) & 0x00000C00;
}

/// Set a new rounding mode
inline int32_t fesetround(FPU_RoundMode roundMode) {
    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
    sse_mode &= 0xFFFF9FFF; // clear current mode
    sse_mode |= roundMode<<3; // sets new mode
    STREFLOP_LDMXCSR(sse_mode);
    return 0;
}

/// stores both x87 and SSE words
struct fenv_t {
    int32_t sse_mode;
    int16_t x87_mode;
};

/// Default env. Defined in Math.cpp, structs are initialized to 0
extern fenv_t FE_DFL_ENV;

/// Get FP env into the given structure
inline int32_t fegetenv(fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (!FE_DFL_ENV.x87_mode) STREFLOP_FSTCW(FE_DFL_ENV.x87_mode);
    // Now store env into argument
    STREFLOP_FSTCW(envp->x87_mode);

    // For SSE
    if (!FE_DFL_ENV.sse_mode) STREFLOP_STMXCSR(FE_DFL_ENV.sse_mode);
    // Now store env into argument
    STREFLOP_STMXCSR(envp->sse_mode);
    return 0;
}

/// Sets FP env from the given structure
inline int32_t fesetenv(const fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (!FE_DFL_ENV.x87_mode) STREFLOP_FSTCW(FE_DFL_ENV.x87_mode);
    // Now overwrite current env by argument
    STREFLOP_FLDCW(envp->x87_mode);

    // For SSE
    if (!FE_DFL_ENV.sse_mode) STREFLOP_STMXCSR(FE_DFL_ENV.sse_mode);
    // Now overwrite current env by argument
    STREFLOP_LDMXCSR(envp->sse_mode);
    return 0;
}

/// get env and clear exceptions
inline int32_t feholdexcept(fenv_t *envp) {
    fegetenv(envp);
    feclearexcept(FE_ALL_EXCEPT);
    return 0;
}


template<typename T> inline void streflop_init() {
    // Do nothing by default, or for unknown types
}

/// Initialize the FPU for the different types
/// this may also be called to switch between code sections using
/// different precisions
template<> inline void streflop_init<Simple>() {
    // Just in case the compiler would store a value on the st(x) registers
    uint16_t x87_mode;
    STREFLOP_FSTCW(x87_mode);
    x87_mode &= 0xFCFF; // 32 bits internal operations
    STREFLOP_FLDCW(x87_mode);

    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
#if defined(STREFLOP_NO_DENORMALS)
    sse_mode |= 0x8040; // set DAZ and FTZ
#else
    sse_mode &= 0xFFFF7FBF; // clear DAZ and FTZ
#endif
    STREFLOP_LDMXCSR(sse_mode);
}

template<> inline void streflop_init<Double>() {
    // Just in case the compiler would store a value on the st(x) registers
    uint16_t x87_mode;
    STREFLOP_FSTCW(x87_mode);
    x87_mode &= 0xFCFF;
    x87_mode |= 0x0200; // 64 bits internal operations
    STREFLOP_FLDCW(x87_mode);

    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
#if defined(STREFLOP_NO_DENORMALS)
    sse_mode |= 0x8040; // set DAZ and FTZ
#else
    sse_mode &= 0xFFFF7FBF; // clear DAZ and FTZ
#endif
    STREFLOP_LDMXCSR(sse_mode);
}

#ifdef Extended
template<> inline void streflop_init<Extended>() {
    // Just in case the compiler would store a value on the st(x) registers
    uint16_t x87_mode;
    STREFLOP_FSTCW(x87_mode);
    x87_mode &= 0xFCFF;
    x87_mode |= 0x0300; // 80 bits internal operations
    STREFLOP_FLDCW(x87_mode);

    int32_t sse_mode;
    STREFLOP_STMXCSR(sse_mode);
#if defined(STREFLOP_NO_DENORMALS)
    sse_mode |= 0x8040; // set DAZ and FTZ
#else
    sse_mode &= 0xFFFF7FBF; // clear DAZ and FTZ
#endif
    STREFLOP_LDMXCSR(sse_mode);
}
#endif


#elif defined(STREFLOP_SOFT)
/// Raise exception for these flags
inline int feraiseexcept(FPU_Exceptions excepts) {
    // Use positive logic
    SoftFloat::float_exception_realtraps |= excepts;
    return 0;
}

/// Clear exceptions for these flags
inline int feclearexcept(int excepts) {
    // Use positive logic
    SoftFloat::float_exception_realtraps &= ~( excepts );
    return 0;
}

/// Get current rounding mode
inline int fegetround() {
    // see softfloat.h for the definition
    switch (SoftFloat::float_rounding_mode) {
        case SoftFloat::float_round_down: return FE_DOWNWARD;
        case SoftFloat::float_round_up: return FE_UPWARD;
        case SoftFloat::float_round_to_zero: return FE_TOWARDZERO;
        default:; // is also initial mode
    }
    // case SoftFloat::float_round_nearest_even:
    return FE_TONEAREST;
}

/// Set a new rounding mode
inline int fesetround(FPU_RoundMode roundMode) {
    // see softfloat.h for the definition
    switch (roundMode) {
        case FE_DOWNWARD: SoftFloat::float_rounding_mode = SoftFloat::float_round_down; return 0;
        case FE_UPWARD: SoftFloat::float_rounding_mode = SoftFloat::float_round_up; return 0;
        case FE_TOWARDZERO: SoftFloat::float_rounding_mode = SoftFloat::float_round_to_zero; return 0;
        case FE_TONEAREST: SoftFloat::float_rounding_mode = SoftFloat::float_round_nearest_even; return 0;
    }
    // Error, invalid mode
    return 1;
}

/// SoftFloat environment comprises non-volatile state variables
struct fenv_t {
    char tininess;
    char rounding_mode;
    int exception_realtraps;
};

/// Default env. Defined in Math.cpp, initialized to some invalid value for detection
extern fenv_t FE_DFL_ENV;

/// Get FP env into the given structure
inline int fegetenv(fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (FE_DFL_ENV.tininess==42) {
        // First use: save default environment now
        FE_DFL_ENV.tininess = SoftFloat::float_detect_tininess;
        FE_DFL_ENV.rounding_mode = SoftFloat::float_rounding_mode;
        FE_DFL_ENV.exception_realtraps = SoftFloat::float_exception_realtraps;
    }
    // Now get the current env in the given argument
    envp->tininess = SoftFloat::float_detect_tininess;
    envp->rounding_mode = SoftFloat::float_rounding_mode;
    envp->exception_realtraps = SoftFloat::float_exception_realtraps;
    return 0;
}

/// Sets FP env from the given structure
inline int fesetenv(const fenv_t *envp) {
    // check that default env exists, otherwise save it now
    if (FE_DFL_ENV.tininess==42) {
        // First use: save default environment now
        FE_DFL_ENV.tininess = SoftFloat::float_detect_tininess;
        FE_DFL_ENV.rounding_mode = SoftFloat::float_rounding_mode;
        FE_DFL_ENV.exception_realtraps = SoftFloat::float_exception_realtraps;
    }
    // Now get the current env in the given argument
    SoftFloat::float_detect_tininess = envp->tininess;
    SoftFloat::float_rounding_mode = envp->rounding_mode;
    SoftFloat::float_exception_realtraps = envp->exception_realtraps;
    return 0;
}

/// get env and clear exceptions
inline int feholdexcept(fenv_t *envp) {
    fegetenv(envp);
    feclearexcept(FE_ALL_EXCEPT);
    return 0;
}

template<typename T> inline void streflop_init() {
    // Do nothing by default, or for unknown types
}

/// Initialize the FPU for the different types
/// this may also be called to switch between code sections using
/// different precisions
template<> inline void streflop_init<Simple>() {
}
template<> inline void streflop_init<Double>() {
}
template<> inline void streflop_init<Extended>() {
}

#elif defined(STREFLOP_NEON)


enum FPU_FlushMode {
    FE_FLUSH_TO_ZERO = 1 << 24
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
    return (fpcr & FE_ROUND_MASK);
}

// Set a new rounding mode
inline int fesetround(FPU_RoundMode roundMode) {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~FE_ROUND_MASK; // Clear rounding mode bits
    fpcr |= roundMode;
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
    fpcr &= ~FE_ROUND_MASK; // Clear rounding mode bits
    fpcr |= FE_TONEAREST;
#if defined(STREFLOP_NO_DENORMALS)
    fpcr |= FE_FLUSH_TO_ZERO;
#else
    fpcr &= ~FE_FLUSH_TO_ZERO;
#endif
    set_fpcr(fpcr);
}

template<> inline void streflop_init<Double>() {
    uint64_t fpcr = get_fpcr();
    fpcr &= ~FE_ROUND_MASK; // Clear rounding mode bits
    fpcr |= FE_TONEAREST;
#if defined(STREFLOP_NO_DENORMALS)
    fpcr |= FE_FLUSH_TO_ZERO;
#else
    fpcr &= ~FE_FLUSH_TO_ZERO;
#endif
    set_fpcr(fpcr);
}

#ifdef Extended
#error "Extended precision not supported on ARM NEON"
#endif

#else
#error STREFLOP: Invalid combination or unknown FPU type.
#endif

}

#endif
