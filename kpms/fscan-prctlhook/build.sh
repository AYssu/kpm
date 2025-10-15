#!/bin/bash
# FastScan 编译脚本 - 使用 OLLVM/Clang

# 清理之前的编译
make clean

# 设置编译器路径
export CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang"
export CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
export USE_CLANG=1

# 取消可能冲突的环境变量
unset TARGET_COMPILE
unset CC
unset LD

# 编译
echo "使用 Clang/OLLVM 编译..."
echo "编译器: $CLANG_PATH"
echo "链接器: $CLANG_LD"

make USE_CLANG=1 \
     CLANG_PATH="$CLANG_PATH" \
     CLANG_LD="$CLANG_LD"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ 编译成功！"
    echo "生成文件: fscan-prctl.kpm"
    ls -lh fscan-prctl.kpm
else
    echo ""
    echo "✗ 编译失败"
    exit 1
fi

