/*
    streflop: STandalone REproducible FLOating-Point
    Nicolas Brodu, 2006
    Code released according to the GNU Lesser General Public License

    Heavily relies on GNU Libm, itself depending on netlib fplibm, GNU MP, and IBM MP lib.
    Uses SoftFloat too.

    Please read the history and copyright information in the documentation provided with the source code
*/

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
using namespace std;
// clock
#include <time.h>

#include "streflop.h"

#pragma pack(push, 1)
struct FileHeader {
    char magic[4];  // Magic number to identify the file type
    uint32_t version;  // Version of the file format
    uint32_t dataType;  // 0 for Simple, 1 for Double, 2 for Extended
    uint32_t dataSize;  // Size of each data element in bytes
    uint32_t elementCount;  // Number of elements in the file
    uint32_t extraFlags;  // For future use (e.g., to indicate if it's basic, nan, or lib data)
};
#pragma pack(pop)

uint32_t getFPCR() {
    uint32_t fpcr;
    #if defined(_MSC_VER) && defined(_M_X64)
        _controlfp_s(reinterpret_cast<unsigned int*>(&fpcr), 0, 0);
    #elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __asm__ __volatile__("fnstcw %0" : "=m" (fpcr));
    #else
        fpcr = 0; // Placeholder for other platforms
    #endif
    return fpcr;
}

void logFPCR(const std::string& location) {
    std::cout << "FPCR at " << location << ": 0x" << std::hex << getFPCR() << std::dec << std::endl;
}

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

uint32_t getMXCSR() {
    uint32_t mxcsr;
    #if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        mxcsr = _mm_getcsr();
    #elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __asm__ __volatile__("stmxcsr %0" : "=m" (mxcsr));
    #else
        mxcsr = 0; // Placeholder for other platforms
    #endif
    return mxcsr;
}

void logMXCSR(const std::string& location) {
    std::cout << "MXCSR at " << location << ": 0x" << std::hex << getMXCSR() << std::dec << std::endl;
}

template<class FloatType>
void writeFileHeader(ofstream& of, uint32_t elementCount, uint32_t extraFlags) {
    FileHeader header;
    memcpy(header.magic, "SREF", 4);  // SREF for STandalone REproducible FLOating-Point
    header.version = 1;
    header.dataType = std::is_same<FloatType, streflop::Simple>::value ? 0 :
                      (std::is_same<FloatType, streflop::Double>::value ? 1 : 2);
    header.dataSize = sizeof(FloatType);
    header.elementCount = elementCount;
    header.extraFlags = extraFlags;

    of.write(reinterpret_cast<char*>(&header), sizeof(FileHeader));
}

template<class FloatType> inline void writeFloat(std::ofstream& of, FloatType f) {
    int nbytes = sizeof(f);
    #ifdef Extended
    if (std::is_same<FloatType, streflop::Extended>::value) {
        nbytes = 10;  // Always use 10 bytes for Extended, regardless of its actual size
    }
    #endif
    const char* thefloat = reinterpret_cast<const char*>(&f);
    long check = 1;
    // big endian OK, reverse little endian
    if (*reinterpret_cast<char*>(&check) == 1) {
        std::vector<char> buffer(nbytes);
        for (int i=0; i<nbytes; ++i) buffer[i] = thefloat[nbytes-1-i];
        of.write(buffer.data(), nbytes);
    } else {
        of.write(thefloat, nbytes);
    }
}

template<class FloatType> void doTest(string s, string name) {

    streflop::streflop_init<FloatType>();

    string basic_filename = s + "_" + name + "_basic.bin";
    ofstream basicfile(basic_filename.c_str());
    if (!basicfile) {
        cout << "Problem creating binary file: " << basic_filename << endl;
        exit(2);
    }
    
    string inf_filename = s + "_" + name + "_nan.bin";
    ofstream infnanfile(inf_filename.c_str());
    if (!infnanfile) {
        cout << "Problem creating binary file: " << inf_filename << endl;
        exit(3);
    }

    string mathlib_filename = s + "_" + name + "_lib.bin";
    ofstream mathlibfile(mathlib_filename.c_str());
    if (!mathlibfile) {
        cout << "Problem creating binary file: " << mathlib_filename << endl;
        exit(4);
    }
    
    FloatType f = 42;

    uint32_t lastFPCR = getFPCR();
    uint32_t lastMXCSR = getMXCSR();
    std::cout << "Initial FPCR: 0x" << std::hex << lastFPCR << std::dec << std::endl;
    std::cout << "Initial MXCSR: 0x" << std::hex << lastMXCSR << std::dec << std::endl;
    // Trap NaNs
    feraiseexcept(streflop::FE_INVALID);
    fesetround(streflop::FE_TONEAREST);

    writeFileHeader<FloatType>(basicfile, 10000, 0);  // 0 for basic operations
    // Generate some random numbers and do some post-processing
    // No math function is called before this loop
    for (int i=0; i<10000; ++i) {
        // f = streflop::RandomIE(f, FloatType(i));
        f = f + FloatType(1.0);
        uint32_t currentFPCR = getFPCR();
        if (currentFPCR != lastFPCR) {
            std::cout << "FPCR changed at iteration " << i << " (before inner loop). New value: 0x" 
                    << std::hex << currentFPCR << std::dec << std::endl;
            lastFPCR = currentFPCR;
        }
        uint32_t currentMXCSR = getMXCSR();
        if (currentMXCSR != lastMXCSR) {
            std::cout << "MXCSR changed at iteration " << i << " (before inner loop). New value: 0x" 
                    << std::hex << currentMXCSR << std::dec << std::endl;
            lastMXCSR = currentMXCSR;
        }
        for (int j=0; j<100; ++j) {
            f += FloatType(0.3) / f + FloatType(1.0);
            
            uint32_t currentFPCR = getFPCR();
            if (currentFPCR != lastFPCR) {
                std::cout << "FPCR changed at iteration " << i << ", sub-iteration " << j 
                        << ". New value: 0x" << std::hex << currentFPCR << std::dec << std::endl;
                lastFPCR = currentFPCR;
            }
            uint32_t currentMXCSR = getMXCSR();
            if (currentMXCSR != lastMXCSR) {
                std::cout << "MXCSR changed at iteration " << i << ", sub-iteration " << j 
                        << ". New value: 0x" << std::hex << currentMXCSR << std::dec << std::endl;
                lastMXCSR = currentMXCSR;
            }
        }
        writeFloat(basicfile, f);
        currentFPCR = getFPCR();
        if (currentFPCR != lastFPCR) {
            std::cout << "FPCR changed at iteration " << i << " (after inner loop). New value: 0x" 
                    << std::hex << currentFPCR << std::dec << std::endl;
            lastFPCR = currentFPCR;
        }
        currentMXCSR = getMXCSR();
        if (currentMXCSR != lastMXCSR) {
            std::cout << "MXCSR changed at iteration " << i << " (after inner loop). New value: 0x" 
                    << std::hex << currentMXCSR << std::dec << std::endl;
            lastMXCSR = currentMXCSR;
        }
    }
    basicfile.close();

    writeFileHeader<FloatType>(infnanfile, 10003, 1);  // 1 for NaN operations (5000 + 5000 + 3)
    // Explicit checks for Inf, Nan, Denormals
    // Minimal number somewhere around 10-4932 for long double
    // Check for denormals at the same time
    f = FloatType(0.1); // 0.1 is not perfectly representable in binary
    for (int i=0; i<5000; ++i) {
        f *= FloatType(0.1);
        writeFloat(infnanfile, f);
    }

    // check for round-up to +Inf
    f = FloatType(10.0001); // not representable exactly
    for (int i=0; i<5000; ++i) {
        f *= FloatType(10.0001);
        writeFloat(infnanfile, f);
    }

    // Explicit +inf check
    f = FloatType(+0.0);
    f = FloatType(1.0)/f;
    writeFloat(infnanfile, f);

    // Explicit -inf check
    f = FloatType(-0.0);
    f = FloatType(1.0)/f;
    writeFloat(infnanfile, f);

    // A few NaN checks
    feclearexcept(streflop::FE_INVALID);
    f *= FloatType(0.0); // inf * 0
    writeFloat(infnanfile, f);
    f = FloatType(+0.0);
    f = FloatType(1.0)/f;
    FloatType g = FloatType(-0.0);
    g = FloatType(1.0)/g;
    f += g; // inf - inf
    writeFloat(infnanfile, f);
    f = FloatType(+0.0);
    g = FloatType(+0.0);
    f /= g; // 0/0
    writeFloat(infnanfile, f);

    infnanfile.close();

    // Trap NaNs again
    feraiseexcept(streflop::FE_INVALID);

    writeFileHeader<FloatType>(mathlibfile, 10000, 2);  // 2 for math library operations
    // Call the Math functions
    for (int i=0; i<10000; ++i) {
        f = streflop::tanh(streflop::cbrt(streflop::fabs(streflop::log2(streflop::sin(FloatType(streflop::RandomII(0,i)))+FloatType(2.0)))+FloatType(1.0)));
        writeFloat(mathlibfile, f);
    }

    mathlibfile.close();

}

template<class FloatType> ostream& displayHex(ostream& out, FloatType f) {
    int nbytes = sizeof(f);
    char* thefloat = reinterpret_cast<char*>(&f);
    long check = 1;
    // big endian OK, reverse little endian
    if (*reinterpret_cast<char*>(&check) == 1) {
        char buffer[nbytes];
        for (int i=0; i<nbytes; ++i) buffer[i] = thefloat[nbytes-1-i];
        for (int i=0; i<nbytes-1; ++i) out << hex << ((int)buffer[i] & 0xFF) << dec << " ";
        out << hex << ((int)buffer[nbytes-1] & 0xFF) << dec << " ";
    } else {
        for (int i=0; i<nbytes-1; ++i) out << hex << ((int)thefloat[i] & 0xFF) << dec << " ";
        out << hex << ((int)thefloat[nbytes-1] & 0xFF) << dec << " ";
    }
    return out;
}


int main(int argc, const char** argv) {

    streflop::RandomInit(42);

    if (argc<2) {
        cout << "You should provide a base file name for the arithmetic test binary results. This base name will be appended the suffix _basic for basic operations not using the math library, _nan for denormals and NaN operations, and _lib for results calling the math library functions (sqrt, sin, etc.). The extension .bin is then finally appended to the file name." << endl;
        cout << "Example: " << argv[0] << " x87_gcc4.1_linux will produce 3 files: x87_gcc4.1_linux_basic.bin, x87_gcc4.1_linux_nan.bin and x87_gcc4.1_linux_lib.bin" << endl;
        return 1;
    }

    doTest<streflop::Simple>(argv[1], "simple");
    doTest<streflop::Double>(argv[1], "double");
#if defined(Extended)
    doTest<streflop::Extended>(argv[1], "extended");
#endif

    return 0;
}

