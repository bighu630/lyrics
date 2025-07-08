#!/bin/bash
# 测试IPC进程锁功能

echo "🔒 测试IPC进程锁功能"
echo "===================="

# 清理之前的进程和文件
pkill lyrics-backend 2>/dev/null || true
sleep 1

# 清理锁文件和socket文件
rm -f /tmp/lyrics_app.sock*

echo ""
echo "📡 启动第一个后端进程..."
./build/lyrics-backend &
PID1=$!
echo "进程1 PID: $PID1"

# 等待一下让第一个进程完全启动
sleep 2

echo ""
echo "📡 尝试启动第二个后端进程（应该失败）..."
./build/lyrics-backend 2>&1 &
PID2=$!
echo "进程2 PID: $PID2"

# 等待一下看输出
sleep 3

echo ""
echo "🔍 检查进程状态:"
echo "进程1状态: $(ps -p $PID1 > /dev/null && echo '运行中' || echo '已停止')"
echo "进程2状态: $(ps -p $PID2 > /dev/null && echo '运行中' || echo '已停止')"

echo ""
echo "📁 检查文件状态:"
ls -la /tmp/lyrics_app.sock* 2>/dev/null || echo "没有找到相关文件"

echo ""
echo "🛑 停止第一个进程..."
kill $PID1 2>/dev/null || true
sleep 2

echo ""
echo "📡 现在尝试启动新的进程（应该成功）..."
./build/lyrics-backend &
PID3=$!
echo "进程3 PID: $PID3"

sleep 2
echo "进程3状态: $(ps -p $PID3 > /dev/null && echo '运行中' || echo '已停止')"

echo ""
echo "🧹 清理所有进程..."
kill $PID2 $PID3 2>/dev/null || true
pkill lyrics-backend 2>/dev/null || true

echo ""
echo "✅ 测试完成"
