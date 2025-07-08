#!/bin/bash

# 测试字体大小调整功能
echo "🎨 测试字体大小调整功能"
echo "========================"

echo ""
echo "📌 使用说明："
echo "1. 启动GUI后，使用以下快捷键："
echo "   - Ctrl + : 增大字体"
echo "   - Ctrl - : 减小字体"
echo "2. 字体大小范围: 12px - 48px"
echo "3. 每次调整步长: 2px"
echo "4. 窗口高度会自动调整"

echo ""
echo "🚀 启动测试..."

# 启动后端（如果未运行）
if ! pgrep -f lyrics-backend > /dev/null; then
    echo "启动后端服务..."
    ./build/lyrics-backend &
    BACKEND_PID=$!
    sleep 2
else
    echo "后端服务已在运行"
    BACKEND_PID=""
fi

# 启动GUI
echo "启动GUI..."
echo "请在GUI窗口中测试以下操作："
echo "- 按 Ctrl + 增大字体"
echo "- 按 Ctrl - 减小字体" 
echo "- 观察窗口高度是否同步调整"
echo "- 测试字体大小限制（12px-48px）"

./build/lyrics-gui

# 清理
if [ -n "$BACKEND_PID" ]; then
    echo "停止后端服务..."
    kill $BACKEND_PID 2>/dev/null
fi

echo "测试完成！"
