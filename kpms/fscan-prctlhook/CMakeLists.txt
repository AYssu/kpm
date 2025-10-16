cmake_minimum_required(VERSION 3.15)
project(fscan-prctlhook C)

# ============================
# 默认编译器配置
# ============================

# 默认使用 Clang/OLLVM
if(NOT DEFINED USE_CLANG AND NOT DEFINED TARGET_COMPILE)
    set(USE_CLANG ON)
endif()

# 默认编译器路径
if(NOT DEFINED CLANG_PATH)
    set(CLANG_PATH "/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang")
endif()

if(NOT DEFINED CLANG_LD)
    set(CLANG_LD "/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld")
endif()

# ============================
# 编译器配置
# ============================

# 检查是否使用 CLANG
if(USE_CLANG)
    set(CMAKE_C_COMPILER ${CLANG_PATH})
    set(CMAKE_LINKER ${CLANG_LD})
    
    message(STATUS "Using Clang compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "Using Linker: ${CMAKE_LINKER}")
    
elseif(DEFINED TARGET_COMPILE)
    set(CMAKE_C_COMPILER ${TARGET_COMPILE}gcc)
    set(CMAKE_LINKER ${TARGET_COMPILE}ld)
    message(STATUS "Using GCC compiler: ${CMAKE_C_COMPILER}")
    
else()
    message(FATAL_ERROR "Neither USE_CLANG nor TARGET_COMPILE is set")
endif()

# ============================
# KP_DIR 设置
# ============================

if(NOT DEFINED KP_DIR)
    set(KP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif()
message(STATUS "KP_DIR: ${KP_DIR}")

# ============================
# 编译标志
# ============================

set(CMAKE_C_FLAGS "-O2 -Wall -std=gnu11")

# Clang bare-metal 所需的额外标志
if(USE_CLANG)
    # 目标架构标志
    set(CLANG_TARGET_FLAGS
        "--target=aarch64-elf"
        "-march=armv8-a"
        "-ffreestanding"
        "-nostdlib"
        "-nostdinc"
        "-fno-builtin"
        "-fno-stack-protector"
        "-fno-pic"
        "-fno-pie"
        "-fno-function-sections"
        "-fno-data-sections"
        "-fno-common"              # 关键！禁用 COMMON section
        "-fno-exceptions"
        "-fno-asynchronous-unwind-tables"
    )
    
    # Ollvm 混淆选项
    # 注意：不能使用 -sobf (字符串混淆)，会导致 kallsyms_lookup_name 查找失败
    # -sub: 指令替换  -split: 基本块分割
    # -fla: 控制流平坦化 (会大幅增加代码体积，慎用)
    # -bcf: 虚假控制流 (同样会增加体积)
    set(CLANG_OBFUSCATE_FLAGS
        "-mllvm" "-sub"
        "-mllvm" "-split"
    )
    
    # 符号混淆选项
    set(CLANG_SYMBOL_FLAGS
        "-fvisibility=hidden"
        "-fno-dwarf2-cfi-asm"
    )
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CLANG_TARGET_FLAGS} ${CLANG_OBFUSCATE_FLAGS} ${CLANG_SYMBOL_FLAGS}")
    string(REPLACE ";" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()

# ============================
# 头文件目录
# ============================

set(INCLUDE_DIRS
    .
    include
    patch/include
    linux/include
    linux/arch/arm64/include
    linux/tools/arch/arm64/include
)

foreach(dir ${INCLUDE_DIRS})
    include_directories(${KP_DIR}/kernel/${dir})
endforeach()

# 当前目录的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ============================
# 源文件
# ============================

set(SOURCES
    fscan.c
)

# ============================
# 构建目标
# ============================

add_library(fscan_prctl_obj OBJECT ${SOURCES})

# 自定义命令：链接生成 fscan-prctl.kpm
if(USE_CLANG)
    # 获取 llvm-strip 路径
    string(REGEX REPLACE "clang$" "llvm-strip" LLVM_STRIP ${CLANG_PATH})
    
    add_custom_command(
        OUTPUT fscan-prctl.kpm
        COMMAND ${CMAKE_LINKER} -r -o fscan-prctl.kpm $<TARGET_OBJECTS:fscan_prctl_obj>
        COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols for obfuscation..."
        COMMAND ${LLVM_STRIP} --strip-unneeded fscan-prctl.kpm || true
        DEPENDS fscan_prctl_obj
        COMMENT "Linking and stripping fscan-prctl.kpm with Clang/LLVM"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT fscan-prctl.kpm
        COMMAND ${CMAKE_C_COMPILER} -r -o fscan-prctl.kpm $<TARGET_OBJECTS:fscan_prctl_obj>
        DEPENDS fscan_prctl_obj
        COMMENT "Linking fscan-prctl.kpm with GCC"
        VERBATIM
    )
endif()

# 添加自定义目标
add_custom_target(fscan-prctl ALL DEPENDS fscan-prctl.kpm)

# ============================
# 清理规则
# ============================

set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "fscan-prctl.kpm")

# ============================
# 安装规则（可选）
# ============================

# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fscan-prctl.kpm
#         DESTINATION lib/kpm)

# ============================
# 显示配置信息
# ============================

message(STATUS "==================================")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Linker: ${CMAKE_LINKER}")
message(STATUS "  USE_CLANG: ${USE_CLANG}")
message(STATUS "  CFLAGS: ${CMAKE_C_FLAGS}")
message(STATUS "==================================")

