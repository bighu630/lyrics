#!/bin/bash
# 测试播放器检测功能
# 用法: ./test_player_detection.sh

echo "🎵 播放器检测测试"
echo "=================="

# 检测MPC
echo ""
echo "🔍 检测 MPC (Music Player Daemon):"
if command -v mpc >/dev/null 2>&1; then
    echo "✅ mpc 命令可用"
    
    # 检查MPD服务状态
    if mpc status >/dev/null 2>&1; then
        echo "✅ MPD 服务运行中"
        
        # 显示当前状态
        echo "📊 MPC 状态:"
        mpc status || echo "   无法获取状态"
        
        echo "🎵 当前歌曲:"
        mpc current -f "%artist% - %title%" || echo "   无当前歌曲"
    else
        echo "❌ MPD 服务未运行或连接失败"
        echo "   提示: 请确保 MPD 服务已启动"
        echo "   启动命令: systemctl --user start mpd"
    fi
else
    echo "❌ mpc 命令不可用"
    echo "   安装命令 (Arch): sudo pacman -S mpc"
    echo "   安装命令 (Ubuntu): sudo apt install mpc"
fi

# 检测playerctl
echo ""
echo "🔍 检测 playerctl (MPRIS):"
if command -v playerctl >/dev/null 2>&1; then
    echo "✅ playerctl 命令可用"
    
    # 检查是否有活动的播放器
    if playerctl status >/dev/null 2>&1; then
        echo "✅ 检测到活动的媒体播放器"
        
        echo "📊 Playerctl 状态:"
        playerctl status || echo "   无法获取状态"
        
        echo "🎵 当前歌曲:"
        playerctl metadata --format "{{artist}} - {{title}}" || echo "   无当前歌曲"
        
        echo "⏱️ 播放时间:"
        playerctl position || echo "   无法获取播放时间"
    else
        echo "❌ 没有检测到活动的媒体播放器"
        echo "   提示: 请启动支持 MPRIS 的播放器 (如 Spotify, Chrome, VLC 等)"
    fi
else
    echo "❌ playerctl 命令不可用"
    echo "   安装命令 (Arch): sudo pacman -S playerctl"
    echo "   安装命令 (Ubuntu): sudo apt install playerctl"
fi

# 检测常见播放器
echo ""
echo "🔍 检测常见播放器进程:"
for player in "mpd" "spotify" "vlc" "firefox" "chrome" "rhythmbox" "amarok"; do
    if pgrep -x "$player" >/dev/null; then
        echo "✅ $player 正在运行"
    else
        echo "❌ $player 未运行"
    fi
done

# 测试优先级逻辑
echo ""
echo "🧪 测试播放器优先级逻辑:"
echo "   1. 如果 MPC 状态为 [playing]，优先使用 MPC"
echo "   2. 如果 MPC 暂停或不可用，使用 playerctl"
echo "   3. 详细日志会在歌词应用运行时显示"

# 建议
echo ""
echo "💡 使用建议:"
echo "   • 对于本地音乐文件: 推荐使用 MPD + MPC"
echo "   • 对于在线音乐服务: 使用支持 MPRIS 的播放器"
echo "   • 可以同时运行多个播放器，系统会智能选择"

echo ""
echo "✨ 测试完成！"
