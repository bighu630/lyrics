#!/bin/bash

# 测试系统关机/断电场景下的死锁处理
echo "💀 测试系统关机场景下的死锁处理"
echo "=================================="

# 清理之前的测试
pkill -f lyrics-backend 2>/dev/null || true
sleep 1
rm -f /tmp/lyrics_app.sock /tmp/lyrics_app.sock.lock

echo ""
echo "📡 启动第一个实例..."
./build/lyrics-backend &
FIRST_PID=$!
echo "第一个实例 PID: $FIRST_PID"

# 等待第一个实例完全启动
sleep 2

# 检查锁文件
echo ""
echo "🔍 检查锁文件内容:"
if [ -f /tmp/lyrics_app.sock.lock ]; then
    echo "锁文件存在，内容: $(cat /tmp/lyrics_app.sock.lock)"
    echo "锁文件权限: $(ls -l /tmp/lyrics_app.sock.lock)"
else
    echo "锁文件不存在"
fi

# 模拟系统关机/断电 - 强制杀死进程但不清理锁文件
echo ""
echo "💥 模拟系统关机/断电 - 强制杀死进程..."
kill -9 $FIRST_PID
echo "第一个实例已被强制杀死 (PID: $FIRST_PID)"

# 等待一下确保进程完全停止
sleep 1

# 检查进程是否真的停止了
if ps -p $FIRST_PID > /dev/null 2>&1; then
    echo "⚠️  进程仍在运行!"
else
    echo "✅ 进程已停止"
fi

# 检查锁文件是否还存在（模拟关机后的状态）
echo ""
echo "🔍 检查锁文件状态（模拟关机后重启）:"
if [ -f /tmp/lyrics_app.sock.lock ]; then
    echo "锁文件仍然存在（这是关机后的正常情况）"
    echo "锁文件内容: $(cat /tmp/lyrics_app.sock.lock)"
    echo "锁文件指向的 PID: $(cat /tmp/lyrics_app.sock.lock | tr -d '\n')"
    
    # 检查这个PID是否还存在
    OLD_PID=$(cat /tmp/lyrics_app.sock.lock | tr -d '\n')
    if ps -p $OLD_PID > /dev/null 2>&1; then
        echo "⚠️  锁文件中的进程仍在运行 - 不应该发生!"
    else
        echo "✅ 锁文件中的进程已不存在 - 符合预期"
    fi
else
    echo "锁文件已消失 - 不应该发生"
fi

# 现在尝试启动新实例（应该能够清理旧锁文件并成功启动）
echo ""
echo "🔄 尝试启动新实例（应该能够清理旧锁文件并成功启动）..."
./build/lyrics-backend &
NEW_PID=$!
echo "新实例 PID: $NEW_PID"

# 等待新实例启动
sleep 3

# 检查新实例是否成功启动
if ps -p $NEW_PID > /dev/null 2>&1; then
    echo "✅ 新实例成功启动 - 死锁处理成功!"
    
    # 检查新的锁文件
    if [ -f /tmp/lyrics_app.sock.lock ]; then
        echo "新锁文件内容: $(cat /tmp/lyrics_app.sock.lock)"
        NEW_LOCK_PID=$(cat /tmp/lyrics_app.sock.lock | tr -d '\n')
        if [ "$NEW_LOCK_PID" = "$NEW_PID" ]; then
            echo "✅ 锁文件PID匹配新进程PID"
        else
            echo "⚠️  锁文件PID不匹配: 锁文件=$NEW_LOCK_PID, 进程=$NEW_PID"
        fi
    else
        echo "⚠️  新锁文件不存在"
    fi
    
    # 清理
    kill $NEW_PID
    sleep 1
else
    echo "❌ 新实例启动失败 - 死锁处理有问题!"
fi

# 清理测试文件
rm -f /tmp/lyrics_app.sock /tmp/lyrics_app.sock.lock

echo ""
echo "🧹 测试完成!"
