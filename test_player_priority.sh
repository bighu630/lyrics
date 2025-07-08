#!/bin/bash
# 测试播放器优先级切换

echo "🔄 测试播放器优先级切换"
echo "========================"

echo ""
echo "📋 测试步骤:"
echo "  1. 检查当前MPC状态"
echo "  2. 如果MPC正在播放，暂停它"
echo "  3. 启动一个MPRIS播放器（如果需要）"
echo "  4. 观察歌词系统的切换行为"

echo ""
echo "🎵 当前MPC状态:"
if mpc status >/dev/null 2>&1; then
    current_status=$(mpc status | grep -E '\[playing\]|\[paused\]|\[stopped\]')
    echo "  $current_status"
    
    if echo "$current_status" | grep -q '\[playing\]'; then
        echo ""
        echo "⏸️  暂停MPC播放以测试回退:"
        mpc pause
        echo "  MPC已暂停"
        
        echo ""
        echo "📊 暂停后的MPC状态:"
        mpc status | grep -E '\[playing\]|\[paused\]|\[stopped\]'
    else
        echo "  MPC未在播放中"
    fi
else
    echo "  MPC不可用"
fi

echo ""
echo "🔍 检查playerctl状态:"
if playerctl status >/dev/null 2>&1; then
    echo "  ✅ 检测到活动的MPRIS播放器"
    echo "  状态: $(playerctl status)"
    echo "  歌曲: $(playerctl metadata --format '{{artist}} - {{title}}' 2>/dev/null || echo '无歌曲信息')"
else
    echo "  ❌ 没有活动的MPRIS播放器"
    echo ""
    echo "💡 建议启动一个支持MPRIS的播放器来测试回退功能:"
    echo "   • Firefox/Chrome 播放YouTube音乐"
    echo "   • Spotify"
    echo "   • VLC播放本地文件"
fi

echo ""
echo "🚀 启动歌词系统测试:"
echo "   运行: LOG_LEVEL=debug ./build/lyrics-backend"
echo "   观察日志输出，应该看到播放器选择的决策过程"

echo ""
echo "🔄 恢复播放测试:"
echo "   • 如果要测试MPC恢复: mpc play"
echo "   • 系统应该自动切换回MPC"

echo ""
echo "📝 预期行为:"
echo "   • MPC播放时: 使用MPC获取歌曲和时间"
echo "   • MPC暂停时: 自动切换到playerctl"
echo "   • 详细的DEBUG日志会显示决策过程"
