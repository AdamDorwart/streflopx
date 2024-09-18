cmake_minimum_required(VERSION 3.10)
project(streflop VERSION 0.4 LANGUAGES CXX C)

# Set C++ standard
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(STREFLOP_SOFT "Use software floating-point" OFF)
option(STREFLOP_NO_DENORMALS "Disable denormals" OFF)

# Detect architecture and set appropriate flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86_64)$")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(STREFLOP_SSE ON)
    else()
        set(STREFLOP_X87 ON)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(STREFLOP_NEON ON)
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}. Defaulting to STREFLOP_SOFT.")
    set(STREFLOP_SOFT ON)
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-pipe -g -frename-registers -Wno-narrowing)
    add_compile_options(-ffp-contract=off -frounding-math -fsignaling-nans -fno-strict-aliasing -Wall -fdiagnostics-color)
	add_compile_options(-v) 
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64"))
        add_compile_options(-mieee-fp)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        add_compile_options(-mno-ms-bitfields)
    endif()
elseif(MSVC)
    # MSVC-specific flags (to be expanded)
    add_compile_options(/W4)
endif()

# Platform-specific linker flags
if(APPLE)
    add_link_options(-Wl,-dead_strip)
elseif(UNIX AND NOT APPLE)
    add_link_options(-Wl,--as-needed)
endif()

# Set compile definitions based on architecture
if(STREFLOP_X87)
    add_compile_definitions(STREFLOP_X87=1)
elseif(STREFLOP_SSE)
    add_compile_definitions(STREFLOP_SSE=1)
elseif(STREFLOP_NEON)
    add_compile_definitions(STREFLOP_NEON=1)
elseif(STREFLOP_SOFT)
    add_compile_definitions(STREFLOP_SOFT=1)
endif()

if(STREFLOP_NO_DENORMALS)
    add_compile_definitions(STREFLOP_NO_DENORMALS=1)
endif()

SET(LIBM_FLT32_SOURCES
	"libm/flt-32/e_acosf.cpp"
	"libm/flt-32/e_acoshf.cpp"
	"libm/flt-32/e_asinf.cpp"
	"libm/flt-32/e_atan2f.cpp"
	"libm/flt-32/e_atanhf.cpp"
	"libm/flt-32/e_coshf.cpp"
	"libm/flt-32/e_exp2f.cpp"
	"libm/flt-32/e_expf.cpp"
	"libm/flt-32/e_fmodf.cpp"
	"libm/flt-32/e_gammaf_r.cpp"
	"libm/flt-32/e_hypotf.cpp"
	"libm/flt-32/e_j0f.cpp"
	"libm/flt-32/e_j1f.cpp"
	"libm/flt-32/e_jnf.cpp"
	"libm/flt-32/e_lgammaf_r.cpp"
	"libm/flt-32/e_log10f.cpp"
	"libm/flt-32/e_log2f.cpp"
	"libm/flt-32/e_logf.cpp"
	"libm/flt-32/e_powf.cpp"
	"libm/flt-32/e_rem_pio2f.cpp"
	"libm/flt-32/e_remainderf.cpp"
	"libm/flt-32/e_sinhf.cpp"
	"libm/flt-32/e_sqrtf.cpp"
	"libm/flt-32/k_cosf.cpp"
	"libm/flt-32/k_rem_pio2f.cpp"
	"libm/flt-32/k_sinf.cpp"
	"libm/flt-32/k_tanf.cpp"
	"libm/flt-32/s_asinhf.cpp"
	"libm/flt-32/s_atanf.cpp"
	"libm/flt-32/s_cbrtf.cpp"
	"libm/flt-32/s_ceilf.cpp"
	"libm/flt-32/s_copysignf.cpp"
	"libm/flt-32/s_cosf.cpp"
	"libm/flt-32/s_erff.cpp"
	"libm/flt-32/s_expm1f.cpp"
	"libm/flt-32/s_fabsf.cpp"
	"libm/flt-32/s_finitef.cpp"
	"libm/flt-32/s_floorf.cpp"
	"libm/flt-32/s_fpclassifyf.cpp"
	"libm/flt-32/s_frexpf.cpp"
	"libm/flt-32/s_ilogbf.cpp"
	"libm/flt-32/s_isinff.cpp"
	"libm/flt-32/s_isnanf.cpp"
	"libm/flt-32/s_ldexpf.cpp"
	"libm/flt-32/s_llrintf.cpp"
	"libm/flt-32/s_llroundf.cpp"
	"libm/flt-32/s_log1pf.cpp"
	"libm/flt-32/s_logbf.cpp"
	"libm/flt-32/s_lrintf.cpp"
	"libm/flt-32/s_lroundf.cpp"
	"libm/flt-32/s_modff.cpp"
	"libm/flt-32/s_nearbyintf.cpp"
	"libm/flt-32/s_nextafterf.cpp"
	"libm/flt-32/s_remquof.cpp"
	"libm/flt-32/s_rintf.cpp"
	"libm/flt-32/s_roundf.cpp"
	"libm/flt-32/s_scalblnf.cpp"
	"libm/flt-32/s_scalbnf.cpp"
	"libm/flt-32/s_signbitf.cpp"
	"libm/flt-32/s_sincosf.cpp"
	"libm/flt-32/s_sinf.cpp"
	"libm/flt-32/s_tanf.cpp"
	"libm/flt-32/s_tanhf.cpp"
	"libm/flt-32/s_truncf.cpp"
	"libm/flt-32/w_expf.cpp"
	)
# Glob libm source files
#file(GLOB LIBM_FLT32_SOURCES "libm/flt-32/*.cpp")
#file(GLOB LIBM_DBL64_SOURCES "libm/dbl-64/*.cpp")
if(NOT STREFLOP_SSE AND NOT STREFLOP_NEON)
    file(GLOB LIBM_LDBL96_SOURCES "libm/ldbl-96/*.cpp")
endif()

add_library(streflop_libm STATIC
    ${LIBM_FLT32_SOURCES}
    ${LIBM_DBL64_SOURCES}
    ${LIBM_LDBL96_SOURCES}
)

#target_include_directories(streflop_libm PRIVATE 
#    ${CMAKE_CURRENT_SOURCE_DIR}/libm/headers
#    ${CMAKE_CURRENT_SOURCE_DIR}/libm
#)

target_compile_definitions(streflop_libm PRIVATE LIBM_COMPILING)

set_target_properties(streflop_libm PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

SET(cxxflags "-I\"${CMAKE_CURRENT_SOURCE_DIR}/libm/headers\"")
set_source_files_properties(${LIBM_FLT32_SOURCES} PROPERTIES COMPILE_FLAGS "-DLIBM_COMPILING_FLT32 ${cxxflags}")
set_source_files_properties(${LIBM_DBL64_SOURCES} PROPERTIES COMPILE_FLAGS "-DLIBM_COMPILING_DBL64")
if(NOT STREFLOP_SSE AND NOT STREFLOP_NEON)
    set_source_files_properties(${LIBM_LDBL96_SOURCES} PROPERTIES COMPILE_FLAGS "-DLIBM_COMPILING_LDBL96")
endif()

# Include the streflop_libm_bridge.h for libm
#target_compile_options(streflop_libm PRIVATE
#    -include ${CMAKE_CURRENT_SOURCE_DIR}/libm/streflop_libm_bridge.h
#)

# Define streflop header files
set(STREFLOP_HEADERS
    Math.h
    System.h
    FPUSettings.h
    IntegerTypes.h
    Random.h
    streflop.h
    X87DenormalSquasher.h
)

# Main streflop sources
set(STREFLOP_SOURCES
    Math.cpp
    Random.cpp
)

if(STREFLOP_SOFT)
    list(APPEND STREFLOP_SOURCES
        SoftFloatWrapper.cpp
        softfloat/softfloat.cpp
    )
    list(APPEND STREFLOP_HEADERS
        SoftFloatWrapper.h
    )
endif()

add_library(streflop STATIC EXCLUDE_FROM_ALL ${STREFLOP_SOURCES})

target_include_directories(streflop PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(streflop PRIVATE streflop_libm)

# Ensure streflop doesn't use libm headers
#target_compile_definitions(streflop PRIVATE STREFLOP_NO_LIBM)
# Executables
add_executable(arithmeticTest arithmeticTest.cpp)
target_link_libraries(arithmeticTest PRIVATE streflop)

add_executable(randomTest randomTest.cpp)
target_link_libraries(randomTest PRIVATE streflop)

add_executable(compareFloats compareFloats.cpp)

# Installation
install(TARGETS streflop streflop_libm
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${STREFLOP_HEADERS}
    DESTINATION include/streflop
)
