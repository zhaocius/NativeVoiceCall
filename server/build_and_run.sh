#!/bin/bash

# 服务器编译和运行脚本

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 清理函数
cleanup() {
    echo -e "\n${YELLOW}正在停止服务器...${NC}"
    pkill -f udp_server
    echo -e "${GREEN}服务器已停止${NC}"
    exit 0
}

# 设置信号处理
trap cleanup SIGINT SIGTERM

echo -e "${BLUE}=== 编译服务器 ===${NC}"
g++ -o udp_server udp_server.cpp -lpthread

if [ $? -eq 0 ]; then
    echo -e "${GREEN}编译成功！${NC}"
    echo ""
    
    # 检查是否有旧的服务器进程
    if pgrep -f udp_server > /dev/null; then
        echo -e "${YELLOW}发现旧的服务器进程，正在停止...${NC}"
        pkill -f udp_server
        sleep 1
    fi
    
    echo -e "${BLUE}=== 启动服务器 ===${NC}"
    echo -e "${GREEN}服务器将在端口 8080 上运行${NC}"
    echo -e "${YELLOW}日志文件: server.log${NC}"
    echo -e "${YELLOW}按 Ctrl+C 停止服务器${NC}"
    echo ""
    
    # 启动服务器，将stderr重定向到日志文件，前台显示stdout
    ./udp_server -p 8080 2> server.log &
    SERVER_PID=$!
    
    echo -e "${GREEN}服务器已启动 (PID: $SERVER_PID)${NC}"
    echo -e "${BLUE}前台显示用户状态变化，详细日志请查看 server.log${NC}"
    echo ""
    
    # 等待服务器进程
    wait $SERVER_PID
else
    echo -e "${RED}编译失败！${NC}"
    exit 1
fi
