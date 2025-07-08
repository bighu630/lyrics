#!/bin/bash

# 测试歌词调度器在快速切换歌曲时的行为
echo "🎵 测试歌词调度器的单例模式"
echo "=============================="

# 清理之前的进程
echo "🧹 清理之前的进程..."
pkill -f lyrics-backend || true
sleep 1

# 启动后端
echo "🚀 启动后端服务..."
./build/lyrics-backend &
BACKEND_PID=$!
sleep 2

# 启动GUI客户端
echo "🖥️ 启动GUI客户端..."
./build/lyrics-gui &
GUI_PID=$!
sleep 2

echo ""
echo "📊 测试步骤:"
echo "1. 快速切换歌曲"
echo "2. 检查日志中没有多个调度器并行运行"
echo "3. 验证旧的调度器被正确取消"

# 快速播放几首歌曲（模拟）
for i in {1..5}; do
  echo ""
  echo "🎵 播放歌曲 $i..."
  # 通过playerctl模拟切换歌曲
  mpc clear
  mpc add "test$i"
  mpc play
  # 很短时间后切换到下一首
  sleep 2
done

echo ""
echo "📝 检查日志..."
echo "查看调度器启动和停止日志..."
echo "(观察日志中应该能看到'Stopping previous lyric scheduler'和'Lyric scheduler stopped'交替出现)"

sleep 2

# 清理
echo ""
echo "🧹 清理进程..."
kill $GUI_PID $BACKEND_PID 2>/dev/null || true
sleep 1

echo "✅ 测试完成!"
echo "请检查日志确认没有多个调度器并行运行"
