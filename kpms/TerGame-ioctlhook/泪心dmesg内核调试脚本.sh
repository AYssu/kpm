#!/system/bin/sh
# FastScan KPM 模块日志查看脚本
# 适用于 Android MT 管理器等文件管理器
# 需要 root 权限

# 颜色定义（如果终端支持）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 日志标签
TAG="FastScan"
LOG_PATTERN="\[FastScan\]"

# 显示帮助信息
show_help() {
    echo "=========================================="
    echo "FastScan KPM 模块日志查看工具"
    echo "=========================================="
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help          显示此帮助信息"
    echo "  -a, --all           显示所有内核日志（不过滤）"
    echo "  -f, --follow        实时监控日志（类似 tail -f）"
    echo "  -c, --clear         清除内核日志缓冲区"
    echo "  -w, --watch         持续监控 FastScan 日志（推荐）"
    echo "  -l, --last N        显示最后 N 行日志（默认 50）"
    echo "  -g, --grep PATTERN  使用自定义模式过滤日志"
    echo "  -s, --save FILE     保存日志到文件"
    echo ""
    echo "示例:"
    echo "  $0                  # 显示最近的 FastScan 日志"
    echo "  $0 -w              # 实时监控 FastScan 日志"
    echo "  $0 -c              # 清除日志缓冲区"
    echo "  $0 -l 100          # 显示最后 100 行"
    echo "  $0 -g prctl        # 过滤包含 'prctl' 的日志"
    echo "  $0 -s /sdcard/log.txt  # 保存日志到文件"
    echo ""
}

# 检查 root 权限
check_root() {
    if [ "$(id -u)" != "0" ]; then
        echo "错误: 需要 root 权限运行此脚本"
        echo "请在 MT 管理器中以 root 权限执行"
        exit 1
    fi
}

# 清除日志缓冲区
clear_logs() {
    echo "清除内核日志缓冲区..."
    dmesg -c > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "✓ 日志缓冲区已清除"
    else
        echo "✗ 清除失败（可能需要 root 权限）"
    fi
}

# 显示日志（带过滤）
show_logs() {
    local lines=${1:-50}
    local pattern=${2:-"$LOG_PATTERN"}
    
    echo "=========================================="
    echo "FastScan KPM 模块日志（最后 $lines 行）"
    echo "=========================================="
    echo ""
    
    dmesg | grep -i "$pattern" | tail -n $lines
}

# 显示所有日志
show_all_logs() {
    local lines=${1:-50}
    
    echo "=========================================="
    echo "所有内核日志（最后 $lines 行）"
    echo "=========================================="
    echo ""
    
    dmesg | tail -n $lines
}

# 实时监控日志
follow_logs() {
    local pattern=${1:-"$LOG_PATTERN"}
    
    echo "=========================================="
    echo "实时监控 FastScan 日志"
    echo "按 Ctrl+C 退出"
    echo "=========================================="
    echo ""
    
    # 先显示最近的日志
    dmesg | grep -i "$pattern" | tail -n 20
    
    echo ""
    echo "等待新日志..."
    echo ""
    
    # 持续监控（使用 dmesg -w 如果支持，否则轮询）
    if dmesg -w > /dev/null 2>&1; then
        # Android 较新版本支持 -w 参数
        dmesg -w | grep --line-buffered -i "$pattern"
    else
        # 旧版本使用轮询方式
        while true; do
            sleep 1
            dmesg | grep -i "$pattern" | tail -n 5
        done
    fi
}

# 持续监控（推荐方式）
watch_logs() {
    echo "=========================================="
    echo "持续监控 FastScan 日志"
    echo "按 Ctrl+C 退出"
    echo "=========================================="
    echo ""
    
    local last_count=0
    
    while true; do
        # 获取当前日志数量
        local current_logs=$(dmesg | grep -i "$LOG_PATTERN" | wc -l)
        
        # 如果有新日志，显示它们
        if [ $current_logs -gt $last_count ]; then
            local new_lines=$((current_logs - last_count))
            echo "[$(date '+%H:%M:%S')] 新日志 ($new_lines 条):"
            dmesg | grep -i "$LOG_PATTERN" | tail -n $new_lines
            echo ""
            last_count=$current_logs
        fi
        
        sleep 2
    done
}

# 保存日志到文件
save_logs() {
    local output_file=${1:-"/sdcard/fscan_kpm_logs.txt"}
    local pattern=${2:-"$LOG_PATTERN"}
    
    echo "保存日志到: $output_file"
    
    dmesg | grep -i "$pattern" > "$output_file" 2>&1
    
    if [ $? -eq 0 ]; then
        local line_count=$(wc -l < "$output_file")
        echo "✓ 已保存 $line_count 行日志到 $output_file"
    else
        echo "✗ 保存失败"
    fi
}

# 使用自定义模式过滤
grep_logs() {
    local pattern=$1
    local lines=${2:-100}
    
    echo "=========================================="
    echo "过滤日志: $pattern"
    echo "=========================================="
    echo ""
    
    dmesg | grep -i "$pattern" | tail -n $lines
}

# 主函数
main() {
    # 检查 root 权限
    check_root
    
    # 解析参数
    case "$1" in
        -h|--help)
            show_help
            exit 0
            ;;
        -a|--all)
            show_all_logs ${2:-50}
            ;;
        -f|--follow)
            follow_logs
            ;;
        -c|--clear)
            clear_logs
            ;;
        -w|--watch)
            watch_logs
            ;;
        -l|--last)
            show_logs ${2:-50}
            ;;
        -g|--grep)
            if [ -z "$2" ]; then
                echo "错误: 请提供搜索模式"
                exit 1
            fi
            grep_logs "$2" ${3:-100}
            ;;
        -s|--save)
            if [ -z "$2" ]; then
                echo "错误: 请提供输出文件路径"
                exit 1
            fi
            save_logs "$2"
            ;;
        "")
            # 默认：显示最近的日志
            show_logs 50
            ;;
        *)
            echo "未知选项: $1"
            echo "使用 -h 或 --help 查看帮助"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
