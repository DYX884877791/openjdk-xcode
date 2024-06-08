cmake_minimum_required(VERSION 3.15)
project(openjdk8u)
 
enable_language(C ASM)
 
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 98)

add_compile_options(-fpermissive)
add_compile_options(-fpic)
# 用到了操作系统线程，编译时需要加参数-pthread
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
# 将汇编文件和C++源代码一起编译
SET(ASM_OPTIONS "-x assembler-with-cpp")
SET(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS}")
set(CMAKE_ASM_CREATE_SHARED_LIBRARY "gcc ${CFLAGS} -o *.o")
#
set(CMAKE_VERBOSE_MAKEFILE ON)




# 如果cmake版本在3.22以上，可以直接使用cmake_host_system_information变量
# 如果cmake版本在3.22以上，可以直接使用cmake_host_system_information变量，原型如下：
#
#cmake_host_system_information(RESULT <variable> QUERY <key> ...)
#1
#使用它查询系统信息，可以在key处指定多个待查询的指标，查询结果会存储在variable的list中。
#
#经常使用key如下：
#
#NUMBER_OF_LOGICAL_CORES：逻辑核数量
#NUMBER_OF_PHYSICAL_CORES：物理核数量
#HOSTNAME：主机名
#FQDN：全限定域名，同时带有主机名和域名的名称
#TOTAL_VIRTUAL_MEMORY：总虚拟内存，单位MB
#AVAILABLE_VIRTUAL_MEMORY：可用虚拟内存
#TOTAL_PHYSICAL_MEMORY：总物理内存
#AVAILABLE_PHYSICAL_MEMORY：可用物理内存
#IS_64BIT：如果处理器是64位，值为1
#HAS_FPU：如果处理器支持浮点单元，值为1
#PROCESSOR_NAME：处理器名称，如我的电脑上输出“Unknown P6 family”
#PROCESSOR_DESCRIPTION：处理器描述，如“8 core Intel® Xeon® CPU E5640 @ 2.67GHz”
#OS_NAME：操作系统名称，如“Linux”
#OS_RELEASE：操作系统发行信息，如“4.15.0-161-generic”
#OS_VERSION：操作系统版本，如“#169-Ubuntu SMP Fri Oct 15 13:41:54 UTC 2021”
#OS_PLATFORM：操作系统平台，如“x86_64”
#DISTRIB_INFO：3.22版本提供，根据/etc/os-release文件确定查询的变量
#DISTRIB_：3.22版本提供，如果/etc/os-release文件中存在name变量，则获取它
#除指定3.22版本的特性外，其他特性在3.10版本都已提供。
cmake_host_system_information(RESULT PRETTY_NAME QUERY DISTRIB_PRETTY_NAME)
message(STATUS "${PRETTY_NAME}")

cmake_host_system_information(RESULT DISTRO QUERY DISTRIB_INFO)
foreach(VAR IN LISTS DISTRO)
    message(STATUS "${VAR}=`${${VAR}}`")
endforeach()

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "current platform: Linux ")
    set(PLATFORM linux)
    set(OS linux)
    set(LINUX 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    MESSAGE(STATUS "current platform: Mac OS X")
    set(PLATFORM bsd)
    set(MACOSX 1)
    set(OS macosx)
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

set(LEVEL slowdebug)
set(MAKE_OUTPUT_DIR build)

# 针对操作系统和CPU架构定义了一些宏
add_definitions(-DHOTSPOT_RELEASE_VERSION="25.71-b01")
add_definitions(-DLINK_INTO_LIBJVM)
add_definitions(-DFULL_VERSION="25.71-b01")

add_definitions(-DJRE_RELEASE_VERSION="1.8.0")
add_definitions(-DZIP_DEBUGIFO_FILES=0)
add_definitions(-DDISABLE_HOTSPOT_OS_VERSION_CHECK="OK")

add_definitions(-DHOTSPOT_BUILD_USER="root")
add_definitions(-DHOTSPOT_BUILD_VERSION="internal")
add_definitions(-DHOTSPOT_BUILD_TARGET="debug")
add_definitions(-DHOTSPOT_VM_DISTRO="openJDK")
add_definitions(-DHOTSPOT_LIB_ARCH="amd64")

add_definitions(-DALLOW_OPERATOR_NEW_USAGE)
add_definitions(-DARCH="amd64")

if (MACOSX)
    add_definitions(-DMACOSX)
    add_definitions(-D_ALLBSD_SOURCE)
    add_definitions(-D_XOPEN_SOURCE)
endif (MACOSX)

if (LINUX)
    add_definitions(-DLINUX)
endif (LINUX)

add_definitions(-D_GNU_SOURCE)

add_definitions(-DAMD64)
add_definitions(-DASSERT)

add_definitions(-DTARGET_OS_FAMILY_${PLATFORM})
add_definitions(-DTARGET_ARCH_x86)
add_definitions(-DTARGET_ARCH_MODEL_x86_64)
add_definitions(-DTARGET_OS_ARCH_${PLATFORM}_x86)
add_definitions(-DTARGET_OS_ARCH_MODEL_${PLATFORM}_x86_64)
add_definitions(-DTARGET_COMPILER_gcc)
add_definitions(-D_LP64)
add_definitions(-DCOMPILER1)
add_definitions(-DCOMPILER2)
add_definitions(-DINCLUDE_ALL_GCS)
add_definitions(-DVM_LITTLE_ENDIAN)
add_definitions(-DINCLUDE_JVMTI=1)
add_definitions(-D_NMT_NOINLINE_)

set(CURRENT_DIR ${CMAKE_SOURCE_DIR})
message("当前目录路径：${CURRENT_DIR}")

# 将编译出的动态链接库libjvm.so替换之前编译出的libjvm.so动态链接库
# 设置静态库文件目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../Gammar)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ../Gammar)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../Gammar)

set(MAKE_OUTPUT ${MAKE_OUTPUT_DIR}/${OS}-x86_64-normal-server-${LEVEL})
message("MAKE_OUTPUT: ${MAKE_OUTPUT}")
if(NOT EXISTS ${CURRENT_DIR}/${MAKE_OUTPUT}/images)
    message("请先构建openjdk image...")
else()

    set(HOTSPOT_GENERATED_DIR ${MAKE_OUTPUT}/hotspot/${PLATFORM}_amd64_compiler2/generated)

    include_directories(hotspot/src/share/vm)
    include_directories(${HOTSPOT_GENERATED_DIR})
    include_directories(hotspot/src/share/vm/precompiled)
    include_directories(hotspot/src/share/vm/prims)
    include_directories(hotspot/src/cpu/x86/vm)
    include_directories(hotspot/src/os/${PLATFORM}/vm)
    include_directories(hotspot/src/os/posix/vm)
    include_directories(hotspot/src/os_cpu/${PLATFORM}_x86/vm)


    file(GLOB_RECURSE HOTSPOT_SHARE_SOURCES hotspot/src/share/vm/*.cpp)
    file(GLOB_RECURSE HOTSPOT_CPU_SOURCES hotspot/src/cpu/x86/vm/*.cpp)
    file(GLOB_RECURSE HOTSPOT_OS_SOURCES hotspot/src/os/${PLATFORM}/vm/*.cpp)
    file(GLOB_RECURSE HOTSPOT_POSIX_SOURCES hotspot/src/os/posix/vm/*.cpp)
    file(GLOB_RECURSE HOTSPOT_OS_CPU_SOURCES hotspot/src/os_cpu/${PLATFORM}_x86/vm/*.cpp)
    file(GLOB_RECURSE MAKE_BUILD_SOURCES ${HOTSPOT_GENERATED_DIR}/*.cpp)

    # 将jvmtiEnvRecommended.cpp文件排除掉...
    file(GLOB jvmtiEnvRecommended ${HOTSPOT_GENERATED_DIR}/jvmtifiles/jvmtiEnvRecommended.cpp)
    # message("jvmtiEnvRecommended: ${jvmtiEnvRecommended}")
    list(REMOVE_ITEM MAKE_BUILD_SOURCES ${jvmtiEnvRecommended})

    # 将jvmtiEnvStub.cpp文件排除掉...
    file(GLOB jvmtiEnvStub ${HOTSPOT_GENERATED_DIR}/jvmtifiles/jvmtiEnvStub.cpp)
    # message("jvmtiEnvStub: ${jvmtiEnvStub}")
    list(REMOVE_ITEM MAKE_BUILD_SOURCES ${jvmtiEnvStub})

    # 将hotspot/src/cpu/x86目录下以_32结尾的cpp文件排除掉...
    file(GLOB REMOVE_SOURCES hotspot/src/cpu/x86/vm/*_32.cpp)
    # message("REMOVE_SOURCES: ${REMOVE_SOURCES}")
    list(REMOVE_ITEM HOTSPOT_CPU_SOURCES ${REMOVE_SOURCES})

    # 将hotspot/src/share/vm/adlc目录下的cpp文件排除掉...
    file(GLOB REMOVE_ADLC_SOURCES hotspot/src/share/vm/adlc/*.cpp)
    # message("REMOVE_ADLC_SOURCES: ${REMOVE_ADLC_SOURCES}")
    list(REMOVE_ITEM HOTSPOT_SHARE_SOURCES ${REMOVE_ADLC_SOURCES})

    # 将hotspot/src/share/vm/shark目录下的cpp文件排除掉...
    file(GLOB REMOVE_SHARK_SOURCES hotspot/src/share/vm/shark/*.cpp)
    # message("REMOVE_SHARK_SOURCES: ${REMOVE_SHARK_SOURCES}")
    list(REMOVE_ITEM HOTSPOT_SHARE_SOURCES ${REMOVE_SHARK_SOURCES})


    file(GLOB CPP_SOURCES ${HOTSPOT_SHARE_SOURCES} ${HOTSPOT_CPU_SOURCES} ${HOTSPOT_OS_SOURCES} ${HOTSPOT_POSIX_SOURCES} ${HOTSPOT_OS_CPU_SOURCES} ${MAKE_BUILD_SOURCES})
    # message("SOURCE_FILES: ${CPP_SOURCES}")

    # 添加jvm的动态链接库...
    add_library(jvm SHARED  ${CPP_SOURCES} hotspot/src/os_cpu/${PLATFORM}_x86/vm/${PLATFORM}_x86_64.s)

    target_compile_options(jvm PRIVATE
            -fno-rtti
            -fno-exceptions
            -pipe
            -fno-strict-aliasing
            -fno-omit-frame-pointer
            -Wno-logical-op-parentheses
            -Wno-parentheses-equality
            -Wno-parentheses
            -Wno-switch
            -Wno-tautological-compare
            -Wno-reserved-user-defined-literal
            -Wno-delete-non-virtual-dtor
            -Wno-deprecated
            -Wno-format
            -Wno-error
            -Wno-implicit-function-declaration
            -Wno-dynamic-class-memaccess
            -Wno-empty-body
            -Wpointer-arith
            -Wsign-compare
            -Wformat=2
            -fvisibility=hidden
            -g
            -m64
            -mno-omit-leaf-frame-pointer
    )

    if (MACOSX)
        target_compile_options(jvm PRIVATE
                -flimit-debug-info
                -mstack-alignment=16
        )
    endif ()

#    add_library(jli STATIC
#            jdk/src/share/bin/java.c
#            jdk/src/solaris/bin/java_md_common.c
#            jdk/src/macosx/bin/java_md_macosx.c
#            jdk/src/share/bin/jli_util.c
#            jdk/src/share/bin/parse_manifest.c
#            jdk/src/share/bin/version_comp.c
#            jdk/src/share/bin/wildcard.c
#            jdk/src/share/bin/splashscreen_stubs.c
#    )
#
#    target_include_directories(jli PRIVATE jdk/src/solaris/bin jdk/src/share/bin jdk/src/macosx/bin jdk/src/macosx/javavm/export)
#
#    target_compile_definitions(jli PRIVATE
#            -DGAMMA
#            -DLAUNCHER_TYPE="gamma"
#            -DPACKAGE_PATH="/usr/opt"
#            -DLIBARCHNAME="x86_64"
#            -DJDK_MAJOR_VERSION="1"
#            -DJDK_MINOR_VERSION="8"
#    )
#
#    # https://www.jianshu.com/p/5f57d59feb75
#    target_compile_options(jli PRIVATE
#            -x objective-c
#            -Wno-error
#            -Wno-implicit-function-declaration
#            )
#
#
#    add_executable(java jdk/src/share/bin/main.c)
#    target_include_directories(java PRIVATE jdk/src/solaris/bin jdk/src/share/bin jdk/src/macosx/bin jdk/src/macosx/javavm/export)
#
#    target_compile_definitions(java PUBLIC
#            -DPACKAGE_PATH="/usr/opt"
#            -DLIBARCHNAME="x86_64"
#            -DJDK_MAJOR_VERSION="1"
#            -DJDK_MINOR_VERSION="8"
#            )
#
#    target_link_libraries(java jvm pthread dl z -Xlinker -all_load jli
#            "-framework Cocoa"
#            "-framework Foundation"
#            "-framework AppKit"
#            )
#    set_target_properties(java PROPERTIES
#            LINKER_LANGUAGE "CXX"
#            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
#            XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
#            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
#            )


    add_executable(gamma Gamma_launcher/java.c Gamma_launcher/jli_util.c Gamma_launcher/wildcard.c Gamma_launcher/java_md.c)

    # 设置包含路径
    target_include_directories(gamma PRIVATE Gamma_launcher)
    # 设置链接的库
    target_link_libraries(gamma jvm pthread dl)
    # 设置编译时预定义的宏
    target_compile_definitions(gamma PUBLIC -DGAMMA -DLAUNCHER_TYPE="gamma")
endif()