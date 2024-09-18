cmake_minimum_required(VERSION 3.10)
project(streflop VERSION 0.4 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(STREFLOP_SOFT "Use software floating-point" OFF)
option(STREFLOP_NO_DENORMALS "Disable denormals" OFF)

# Detect architecture and set appropriate flags
if(NOT STREFLOP_SOFT)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86_64|AMD64)$")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            message("Using STREFLOP_SSE")
            set(STREFLOP_SSE ON)
        else()
            message("Using STREFLOP_X87")
            set(STREFLOP_X87 ON)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        message("Using STREFLOP_NEON")
        set(STREFLOP_NEON ON)
    else()
        message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}. Defaulting to STREFLOP_SOFT.")
        set(STREFLOP_SOFT ON)
    endif()
else()
    message("Using STREFLOP_SOFT")
endif()

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-pipe -g -frename-registers -Wno-narrowing)
    add_compile_options(-ffp-contract=off -frounding-math -fsignaling-nans -fno-strict-aliasing -Wall -fdiagnostics-color)
  
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64"))
        add_compile_options(-mieee-fp)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        add_compile_options(-mno-ms-bitfields)
    endif()
elseif(MSVC)
    # MSVC-specific flags (to be expanded)
    add_compile_options(/W4)
    add_definitions(-D__STDC__=1)
    
    add_compile_options(/permissive- /Zc:__cplusplus /wd4146 /wd4711 /wd5208 /wd4838 /wd2124)
    
    enable_language(ASM_MASM)
    set(fpu_settings_asm "FPUSettings.asm")
    set_source_files_properties(${fpu_settings_asm} PROPERTIES LANGUAGE ASM_MASM)
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

# Glob libm source files
file(GLOB LIBM_FLT32_SOURCES "libm/flt-32/*.cpp")
file(GLOB LIBM_DBL64_SOURCES "libm/dbl-64/*.cpp")
if(NOT STREFLOP_SSE AND NOT STREFLOP_NEON)
    file(GLOB LIBM_LDBL96_SOURCES "libm/ldbl-96/*.cpp")
endif()

add_library(streflop_libm STATIC
    ${LIBM_FLT32_SOURCES}
    ${LIBM_DBL64_SOURCES}
    ${LIBM_LDBL96_SOURCES}
)

target_include_directories(streflop_libm PRIVATE 
   ${CMAKE_CURRENT_SOURCE_DIR}/libm/headers
   ${CMAKE_CURRENT_SOURCE_DIR}/libm
)

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
    list(APPEND STREFLOP_HEADERS
        softfloat/softfloat.h
        softfloat/milieu.h
        SoftFloatWrapper.h
    )

    # Create object libraries for each variant of SoftFloatWrapper
    add_library(SoftFloatWrapperSimple OBJECT SoftFloatWrapper.cpp)
    target_compile_definitions(SoftFloatWrapperSimple PRIVATE N_SPECIALIZED=32)

    add_library(SoftFloatWrapperDouble OBJECT SoftFloatWrapper.cpp)
    target_compile_definitions(SoftFloatWrapperDouble PRIVATE N_SPECIALIZED=64)

    add_library(SoftFloatWrapperExtended OBJECT SoftFloatWrapper.cpp)
    target_compile_definitions(SoftFloatWrapperExtended PRIVATE N_SPECIALIZED=96)

    # Add softfloat.cpp to the sources
    list(APPEND STREFLOP_SOURCES
        softfloat/softfloat.cpp
    )
endif()

# Create the main streflop library
add_library(streflop STATIC EXCLUDE_FROM_ALL ${STREFLOP_SOURCES} ${fpu_settings_asm})

# If STREFLOP_SOFT is enabled, add the object libraries to the main library
if(STREFLOP_SOFT)
    target_link_libraries(streflop
        PRIVATE
        SoftFloatWrapperSimple
        SoftFloatWrapperDouble
        SoftFloatWrapperExtended
    )
endif()

# Set include directories for the main library
target_include_directories(streflop
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
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
