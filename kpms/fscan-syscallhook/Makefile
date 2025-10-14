ifndef TARGET_COMPILE
    # 如果没设置 TARGET_COMPILE，检查是否使用 CLANG
    ifdef USE_CLANG
        CC = $(CLANG_PATH)
        LD = /root/android-ndk-r23c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld
    else
        $(error TARGET_COMPILE not set)
    endif
else
    CC = $(TARGET_COMPILE)gcc
    LD = $(TARGET_COMPILE)ld
endif

ifndef KP_DIR
    KP_DIR = ../..
endif

# Clang bare-metal 所需的额外标志
ifdef USE_CLANG
    CLANG_TARGET_FLAGS = --target=aarch64-elf
    CLANG_TARGET_FLAGS += -march=armv8-a
    CLANG_TARGET_FLAGS += -ffreestanding -nostdlib -nostdinc -fno-builtin
    CLANG_TARGET_FLAGS += -fno-stack-protector -fno-pic -fno-pie
    CLANG_TARGET_FLAGS += -fno-function-sections -fno-data-sections
    CLANG_TARGET_FLAGS += -fno-common              # ← 关键！禁用 COMMON section
    CLANG_TARGET_FLAGS += -fno-exceptions
    CLANG_TARGET_FLAGS += -fno-asynchronous-unwind-tables
    
    # Ollvm 混淆选项
    # 注意：不能使用 -sobf (字符串混淆)，会导致 kallsyms_lookup_name 查找失败
    # -sub: 指令替换  -split: 基本块分割
    # -fla: 控制流平坦化 (会大幅增加代码体积，慎用)
    # -bcf: 虚假控制流 (同样会增加体积)
    CLANG_OBFUSCATE_FLAGS = -mllvm -sub -mllvm -split
    
    # 符号混淆选项
    CLANG_SYMBOL_FLAGS = -fvisibility=hidden -fno-dwarf2-cfi-asm
    
    CFLAGS += $(CLANG_TARGET_FLAGS) $(CLANG_OBFUSCATE_FLAGS) $(CLANG_SYMBOL_FLAGS)
    CFLAGS += -Wall -std=gnu11
endif

INCLUDE_DIRS := . include patch/include linux/include linux/arch/arm64/include linux/tools/arch/arm64/include
INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

objs := fscan.o

all: fscan.kpm

fscan.kpm: ${objs}
ifdef USE_CLANG
	${LD} -r -o $@ $^
	@echo "Stripping symbols for obfuscation..."
	$(CLANG_PATH:clang=llvm-strip) --strip-unneeded $@ 2>/dev/null || true
else
	${CC} -r -o $@ $^
endif

%.o: %.c
	${CC} $(CFLAGS) $(INCLUDE_FLAGS) -c -O2 -o $@ $<

.PHONY: clean
clean:
	rm -rf *.kpm
	find . -name "*.o" | xargs rm -f




# 用 Ollvm 编译
# export USE_CLANG=1
# export CLANG_PATH="/root/android-ndk-r23c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang"
# export CLANG_LD="/root/android-ndk-r23c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
