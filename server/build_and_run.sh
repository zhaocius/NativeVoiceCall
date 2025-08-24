#!/bin/bash

# 服务器编译和运行脚本

echo "=== 编译服务器 ==="
g++ -o udp_server udp_server.cpp -lpthread

if [ $? -eq 0 ]; then
    echo "编译成功！"
    echo ""
    echo "=== 启动服务器 ==="
    echo "服务器将在端口 8080 上运行"
    echo "按 Ctrl+C 停止服务器"
    echo ""
    ./udp_server -p 8080
else
    echo "编译失败！"
    exit 1
fi
