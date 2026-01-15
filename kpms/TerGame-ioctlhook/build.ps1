# 构建脚本 - 使用 GCC 直接编译 KPM 模块

$ErrorActionPreference = "Stop"

# 设置变量
$TARGET_COMPILE = "aarch64-none-elf-"
$CC = "${TARGET_COMPILE}gcc"
$LD = "${TARGET_COMPILE}ld"
# 如果 $PSScriptRoot 为空，使用当前目录
if ($PSScriptRoot) {
    $SRC_DIR = $PSScriptRoot
} else {
    $SRC_DIR = $PWD.Path
}
$KP_DIR = Join-Path $SRC_DIR "../.."
$KP_DIR = (Resolve-Path $KP_DIR -ErrorAction SilentlyContinue).Path
if (-not $KP_DIR) {
    $KP_DIR = (Join-Path (Split-Path $SRC_DIR) "..")
}
$OUTPUT = "fscan-ioctl.kpm"
$SOURCE = "fscan.c"

# 版本号生成
$VERSION_MAJOR = 1
$VERSION_MINOR = 3
$BUILD_NUMBER = Get-Date -Format "ddHHmm"
$KPM_BUILD_VERSION = "${VERSION_MAJOR}.${VERSION_MINOR}.${BUILD_NUMBER}"

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "Building fscan-ioctlhook KPM Module" -ForegroundColor Cyan
Write-Host "Build Version: $KPM_BUILD_VERSION" -ForegroundColor Cyan
Write-Host "Compiler: $CC" -ForegroundColor Cyan
Write-Host "KP_DIR: $KP_DIR" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan

# 检查编译器
Write-Host "`n检查编译器..." -ForegroundColor Yellow
& $CC --version
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 无法找到编译器 $CC" -ForegroundColor Red
    exit 1
}

# 创建临时版本号头文件（避免 PowerShell 字符串转义问题）
$TEMP_VERSION_FILE = ".kpm_build_version.h"
"#ifndef KPM_BUILD_VERSION`n#define KPM_BUILD_VERSION `"$KPM_BUILD_VERSION`"`n#endif" | Out-File -FilePath $TEMP_VERSION_FILE -Encoding ASCII -NoNewline
Write-Host "创建临时版本号文件: $TEMP_VERSION_FILE" -ForegroundColor Gray

# 编译标志（基础标志）
$CFLAGS_BASE = @(
    "-O2",
    "-Wall",
    "-std=gnu11",
    "-march=armv8-a",
    "-ffreestanding",
    "-nostdlib",
    "-nostdinc",
    "-fno-builtin",
    "-fno-stack-protector",
    "-fno-pic",
    "-fno-pie",
    "-fno-function-sections",
    "-fno-data-sections",
    "-fno-common",
    "-fno-exceptions",
    "-fno-asynchronous-unwind-tables",
    "-include",
    $TEMP_VERSION_FILE
)

# 头文件目录
$INCLUDE_DIRS = @(
    ".",
    "include",
    "patch/include",
    "linux/include",
    "linux/arch/arm64/include",
    "linux/tools/arch/arm64/include"
)

$INCLUDE_FLAGS = @()
foreach ($dir in $INCLUDE_DIRS) {
    $includePath = Join-Path $KP_DIR "kernel/$dir"
    if (Test-Path $includePath) {
        $INCLUDE_FLAGS += "-I$includePath"
    }
}
$INCLUDE_FLAGS += "-I$SRC_DIR"

# 编译对象文件
$OBJ_FILE = "fscan.o"
Write-Host "`n编译源文件: $SOURCE" -ForegroundColor Yellow

# 构建编译参数数组
$compileArgs = @()
$compileArgs += $CFLAGS_BASE
$compileArgs += $INCLUDE_FLAGS
$compileArgs += "-c"
$compileArgs += $SOURCE
$compileArgs += "-o"
$compileArgs += $OBJ_FILE

# 显示命令（用于调试）
$compileCmdStr = "$CC " + ($compileArgs -join ' ')
Write-Host "执行: $compileCmdStr" -ForegroundColor Gray

# 使用 & 调用，直接传递参数数组
& $CC $compileArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "编译失败!" -ForegroundColor Red
    Remove-Item $TEMP_VERSION_FILE -ErrorAction SilentlyContinue
    exit 1
}

# 清理临时版本号文件
Remove-Item $TEMP_VERSION_FILE -ErrorAction SilentlyContinue

# 链接生成 KPM 文件
Write-Host "`n链接生成 KPM 文件: $OUTPUT" -ForegroundColor Yellow
$linkArgs = @("-r", "-o", $OUTPUT, $OBJ_FILE)
Write-Host "执行: $CC $($linkArgs -join ' ')" -ForegroundColor Gray
& $CC $linkArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "链接失败!" -ForegroundColor Red
    exit 1
}

# 清理临时文件
Remove-Item $OBJ_FILE -ErrorAction SilentlyContinue

Write-Host "`n==================================" -ForegroundColor Green
Write-Host "构建成功! 输出文件: $OUTPUT" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Green
