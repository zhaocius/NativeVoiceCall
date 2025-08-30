# UDP Voice Call Server

## 🏗️ 架构设计

服务器采用面向对象设计，职责分离，代码更清晰易懂。

### 📁 类结构

```
UDPServer (主类)
├── ServerConfig (配置管理)
├── NetworkManager (网络管理)
├── RoomManager (房间管理)
└── MessageHandler (消息处理)
    └── ClientInfo (客户端信息)
```

## 🎯 核心类说明

### 1. ServerConfig - 配置管理
```cpp
class ServerConfig {
    std::string bind_ip = "0.0.0.0";
    int port = 8080;
    
    static ServerConfig parseCommandLine(int argc, char* argv[]);
    void showUsage(const char* program_name) const;
};
```
**职责**: 管理服务器配置，解析命令行参数

### 2. ClientInfo - 客户端信息
```cpp
class ClientInfo {
    std::string user_id;
    std::string room_id;
    struct sockaddr_in address;
    
    std::string getKey() const;  // 生成客户端唯一标识
};
```
**职责**: 存储客户端状态信息

### 3. RoomManager - 房间管理
```cpp
class RoomManager {
    std::map<std::string, std::set<std::string>> rooms_;      // 房间 -> 客户端集合
    std::map<std::string, std::shared_ptr<ClientInfo>> clients_; // 客户端 -> 信息
    
    bool addUserToRoom(...);
    bool removeUserFromRoom(...);
    std::string getUserRoom(...);
    std::vector<std::shared_ptr<ClientInfo>> getRoomClients(...);
};
```
**职责**: 管理房间和用户的加入/离开

### 4. MessageHandler - 消息处理
```cpp
class MessageHandler {
    RoomManager& room_manager_;
    int server_fd_;
    
    void handleMessage(...);           // 主消息处理入口
    void handleJoinMessage(...);       // 处理JOIN消息
    void handleLeaveMessage(...);      // 处理LEAVE消息
    void handleAudioPacket(...);       // 处理音频包
    void broadcastToRoom(...);         // 广播消息
    void broadcastAudioPacket(...);    // 广播音频包
};
```
**职责**: 处理所有类型的消息，包括JOIN/LEAVE和音频包

### 5. NetworkManager - 网络管理
```cpp
class NetworkManager {
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    bool initialize(...);              // 初始化网络
    void start(MessageHandler& handler); // 启动服务器
    void stop();                       // 停止服务器
    void serverLoop(...);              // 服务器主循环
};
```
**职责**: 管理UDP socket和网络通信

### 6. UDPServer - 主协调类
```cpp
class UDPServer {
    ServerConfig config_;
    std::unique_ptr<NetworkManager> network_manager_;
    std::unique_ptr<RoomManager> room_manager_;
    std::unique_ptr<MessageHandler> message_handler_;
    
    bool start();                      // 启动服务器
    void stop();                       // 停止服务器
    bool initializeComponents();       // 初始化所有组件
};
```
**职责**: 协调所有组件，提供统一的服务器接口

## 🔄 消息流程

### 1. 用户加入流程
```
客户端发送 JOIN:room_id:user_id
    ↓
MessageHandler::handleJoinMessage()
    ↓
RoomManager::addUserToRoom()
    ↓
发送 JOIN_OK 响应
    ↓
广播 JOIN 消息给房间内其他用户
```

### 2. 音频传输流程
```
客户端发送音频包
    ↓
MessageHandler::handleAudioPacket()
    ↓
验证音频包格式
    ↓
检查用户权限
    ↓
RoomManager::getRoomClients()
    ↓
广播音频包给房间内其他用户
```

### 3. 用户离开流程
```
客户端发送 LEAVE:room_id:user_id
    ↓
MessageHandler::handleLeaveMessage()
    ↓
RoomManager::removeUserFromRoom()
    ↓
广播 LEAVE 消息给房间内其他用户
```

## 🚀 使用方法

### 编译和运行
```bash
# 使用脚本（推荐）
./build_and_run.sh

# 手动编译
g++ -o udp_server udp_server.cpp -lpthread

# 运行
./udp_server -p 8080
```

### 命令行参数
```bash
./udp_server [选项]
  -h, --help              显示帮助信息
  -i, --ip <IP>           设置监听IP地址 (默认: 0.0.0.0)
  -p, --port <PORT>       设置监听端口 (默认: 8080)
```

## ✨ 重构优势

### 1. 职责分离
- 每个类都有明确的职责
- 代码更容易理解和维护
- 便于单元测试

### 2. 面向对象设计
- 使用继承和组合
- 封装内部实现细节
- 提供清晰的接口

### 3. 内存管理
- 使用智能指针管理资源
- 避免内存泄漏
- RAII原则

### 4. 错误处理
- 统一的异常处理机制
- 详细的错误日志
- 优雅的错误恢复

### 5. 可扩展性
- 易于添加新功能
- 模块化设计
- 低耦合高内聚

## 📊 性能优化

1. **智能指针**: 自动内存管理
2. **引用传递**: 避免不必要的拷贝
3. **线程安全**: 使用atomic和mutex
4. **缓冲区优化**: 固定大小的缓冲区

## 🔧 调试和日志

- 前台只显示用户状态变化
- 详细日志保存到 `server.log`
- 使用Python脚本分析日志
- 支持实时监控

## 🎯 新手友好特性

1. **清晰的类名**: 一看就知道用途
2. **详细的注释**: 每个方法都有说明
3. **简单的接口**: 易于理解和使用
4. **模块化设计**: 可以单独学习每个模块
5. **示例代码**: 完整的使用示例
