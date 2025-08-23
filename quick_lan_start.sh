#!/bin/bash

echo "=== 快速启动局域网通话 ==="
echo ""

# 获取IP地址
IP=$(hostname -I | awk '{print $1}')
echo "服务器IP: $IP"
echo ""

# 启动服务器
echo "启动UDP服务器..."
cd server
./udp_server -i 0.0.0.0 -p 8080 &
SERVER_PID=$!
sleep 2

echo "✓ 服务器已启动"
echo "✓ 监听地址: 0.0.0.0:8080"
echo "✓ 局域网地址: $IP:8080"
echo ""

echo "=== 客户端连接命令 ==="
echo "在其他设备上运行:"
echo "cd linux_client/build/bin"
echo "./voice_call_client -s $IP -p 8080 -r test_room -u <用户名>"
echo ""

echo "按 Enter 键停止服务器..."
read
kill $SERVER_PID
echo "服务器已停止"
