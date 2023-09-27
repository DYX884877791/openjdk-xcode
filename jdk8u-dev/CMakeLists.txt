cmake_minimum_required(VERSION 3.15)
project(jvm)
 
enable_language(C ASM)
 
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)

add_compile_options(-fpermissive)
# 用到了操作系统线程，编译时需要加参数-pthread
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# 将汇编文件和C++源代码一起编译
SET(ASM_OPTIONS "-x assembler-with-cpp")
SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS}")
 
# 针对操作系统和CPU架构定义了一些宏
add_definitions(-DHOTSPOT_RELEASE_VERSION="1.8-b132"
        -DJRE_RELEASE_VERSION="1.8.0"
        -DHOTSPOT_BUILD_TARGET="debug"
        -DHOTSPOT_VM_DISTRO="openJDK"
        -DHOTSPOT_LIB_ARCH="amd64"
        -DMACOSX
        -D_GNU_SOURCE
        -D_ALLBSD_SOURCE
        -D_DARWIN_C_SOURCE
        -DXOPEN_SOURCE
        -DAMD64
        -DASSERT
        -DTARGET_OS_FAMILY_bsd
        -DTARGET_ARCH_x86
        -DTARGET_ARCH_MODEL_x86_64
        -DTARGET_OS_ARCH_bsd_x86
        -DTARGET_OS_ARCH_MODEL_bsd_x86_64
        -DTARGET_COMPILER_gcc -D_LP64
        -DCOMPILER1
        -DCOMPILER2
        -DINCLUDE_ALL_GCS
        -DVM_LITTLE_ENDIAN
        -DINCLUDE_JVMTI=1)
 
# 将编译出的动态链接库libjvm.so替换之前编译出的libjvm.so动态链接库
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /home/iHotSpot-8-b132)


include_directories(./src)
include_directories(./src/precompiled)
include_directories(./src/prims)
include_directories(./src/adlc)
include_directories(./src/x86/vm)
include_directories(./src/linux/vm)
include_directories(./src/linux_x86/vm)
include_directories(./src/posix/vm)
include_directories(./src/adfiles)
include_directories(./src/tracefiles)
include_directories(./src/jvmtifiles)

aux_source_directory(./src/asm SOURCE_FILES)
aux_source_directory(./src/c1 SOURCE_FILES)
aux_source_directory(./src/ci SOURCE_FILES)
aux_source_directory(./src/classfile SOURCE_FILES)
aux_source_directory(./src/code SOURCE_FILES)
aux_source_directory(./src/compiler SOURCE_FILES)
aux_source_directory(./src/gc_implementation SOURCE_FILES)
aux_source_directory(./src/gc_implementation/concurrentMarkSweep SOURCE_FILES)
aux_source_directory(./src/gc_implementation/g1 SOURCE_FILES)
aux_source_directory(./src/gc_implementation/parallelScavenge SOURCE_FILES)
aux_source_directory(./src/gc_implementation/parNew SOURCE_FILES)
aux_source_directory(./src/gc_implementation/shared SOURCE_FILES)
aux_source_directory(./src/gc_interface SOURCE_FILES)
aux_source_directory(./src/interpreter SOURCE_FILES)
aux_source_directory(./src/libadt SOURCE_FILES)
aux_source_directory(./src/linux SOURCE_FILES)
aux_source_directory(./src/linux_x86 SOURCE_FILES)
aux_source_directory(./src/memory SOURCE_FILES)
aux_source_directory(./src/oops SOURCE_FILES)
aux_source_directory(./src/opto SOURCE_FILES)
aux_source_directory(./src/posix SOURCE_FILES)
aux_source_directory(./src/precompiled SOURCE_FILES)
aux_source_directory(./src/prims SOURCE_FILES)
aux_source_directory(./src/prims/wbtestmethods SOURCE_FILES)
aux_source_directory(./src/runtime SOURCE_FILES)
aux_source_directory(./src/services SOURCE_FILES)
aux_source_directory(./src/trace SOURCE_FILES)
aux_source_directory(./src/utilities SOURCE_FILES)
aux_source_directory(./src/x86 SOURCE_FILES)
aux_source_directory(./src/tracefiles SOURCE_FILES)
aux_source_directory(./src/adfiles SOURCE_FILES)
 
add_library(${PROJECT_NAME} SHARED  ${SOURCE_FILES} ./src/linux_x86/vm/linux_x86_64.s)

