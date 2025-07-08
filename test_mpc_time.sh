#!/bin/bash
# 测试MPC时间解析功能

echo "🕒 测试 MPC 时间解析"
echo "==================="

echo ""
echo "📊 MPC 状态输出:"
mpc status

echo ""
echo "🔍 解析时间信息:"

# 获取状态输出并分析
status_output=$(mpc status)
echo "原始状态输出:"
echo "$status_output"

echo ""
echo "查找包含时间的行:"
echo "$status_output" | grep -E '\[playing\]|\[paused\]' | grep '/' || echo "未找到时间信息"

echo ""
echo "📝 时间格式说明:"
echo "  • MPC 时间格式通常为: 当前时间/总时间 (百分比)"
echo "  • 例如: 1:42/4:34 (37%) 表示当前1分42秒，总共4分34秒"
echo "  • 系统会将时间转换为秒数用于歌词同步"

echo ""
echo "🧪 手动测试时间解析:"
time_line=$(echo "$status_output" | grep -E '\[playing\]|\[paused\]' | grep '/')
if [[ -n "$time_line" ]]; then
    echo "时间行: $time_line"
    
    # 提取时间部分（简化版本）
    if [[ $time_line =~ ([0-9]+:[0-9]+)/([0-9]+:[0-9]+) ]]; then
        current_time="${BASH_REMATCH[1]}"
        total_time="${BASH_REMATCH[2]}"
        echo "当前时间: $current_time"
        echo "总时间: $total_time"
        
        # 转换为秒数（简化计算）
        IFS=':' read -r min sec <<< "$current_time"
        current_seconds=$((min * 60 + sec))
        echo "当前时间（秒）: $current_seconds"
    fi
else
    echo "未找到有效的时间信息"
fi
