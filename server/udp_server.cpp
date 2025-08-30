#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

// ============================================================================
// 配置类 - 管理服务器配置
// ============================================================================
class ServerConfig {
public:
    std::string bind_ip = "0.0.0.0";
    int port = 8080;
    
    static ServerConfig parseCommandLine(int argc, char* argv[]);
    void showUsage(const char* program_name) const;
};

// ============================================================================
// 客户端信息类 - 存储客户端状态
// ============================================================================
class ClientInfo {
public:
    std::string user_id;
    std::string room_id;
    struct sockaddr_in address;
    
    ClientInfo(const std::string& uid, const std::string& rid, const struct sockaddr_in& addr)
        : user_id(uid), room_id(rid), address(addr) {}
    
    std::string getKey() const {
        return std::string(inet_ntoa(address.sin_addr)) + ":" + std::to_string(ntohs(address.sin_port));
    }
};

// ============================================================================
// 房间管理类 - 管理房间和用户
// ============================================================================
class RoomManager {
private:
    std::map<std::string, std::set<std::string>> rooms_;  // room_id -> set of client_keys
    std::map<std::string, std::shared_ptr<ClientInfo>> clients_;  // client_key -> ClientInfo
    
public:
    // 添加用户到房间
    bool addUserToRoom(const std::string& client_key, const std::string& user_id, 
                      const std::string& room_id, const struct sockaddr_in& address);
    
    // 从房间移除用户
    bool removeUserFromRoom(const std::string& client_key, const std::string& room_id);
    
    // 获取用户所在房间
    std::string getUserRoom(const std::string& client_key) const;
    
    // 获取房间内的所有客户端
    std::vector<std::shared_ptr<ClientInfo>> getRoomClients(const std::string& room_id) const;
    
    // 检查用户是否在房间中
    bool isUserInRoom(const std::string& client_key, const std::string& room_id) const;
    
    // 获取房间数量
    size_t getRoomCount() const { return rooms_.size(); }
    
    // 获取客户端数量
    size_t getClientCount() const { return clients_.size(); }
};

// ============================================================================
// 消息处理类 - 处理不同类型的消息
// ============================================================================
class MessageHandler {
private:
    RoomManager& room_manager_;
    int server_fd_;
    
public:
    MessageHandler(RoomManager& rm, int fd) : room_manager_(rm), server_fd_(fd) {}
    
    // 处理接收到的消息
    void handleMessage(const char* message, int length, const struct sockaddr_in& from_addr);
    
private:
    // 处理JOIN消息
    void handleJoinMessage(const std::string& message, const struct sockaddr_in& from_addr);
    
    // 处理LEAVE消息
    void handleLeaveMessage(const std::string& message, const struct sockaddr_in& from_addr);
    
    // 处理音频包
    void handleAudioPacket(const char* data, int length, const struct sockaddr_in& from_addr);
    
    // 广播消息到房间
    void broadcastToRoom(const std::string& room_id, const std::string& message, 
                        const struct sockaddr_in& exclude_addr);
    
    // 广播音频包到房间
    void broadcastAudioPacket(const std::string& room_id, const char* data, int length,
                             const struct sockaddr_in& exclude_addr);
};

// ============================================================================
// 网络管理类 - 管理网络连接和通信
// ============================================================================
class NetworkManager {
private:
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::string bind_ip_;
    int port_;
    
public:
    NetworkManager() : server_fd_(-1), running_(false), port_(8080) {}
    ~NetworkManager() { stop(); }
    
    // 初始化网络
    bool initialize(const std::string& bind_ip, int port);
    
    // 启动服务器
    void start(MessageHandler& handler);
    
    // 停止服务器
    void stop();
    
    // 获取服务器文件描述符
    int getServerFd() const { return server_fd_; }
    
private:
    // 服务器主循环
    void serverLoop(MessageHandler& handler);
};

// ============================================================================
// UDP服务器主类 - 协调所有组件
// ============================================================================
class UDPServer {
private:
    ServerConfig config_;
    std::unique_ptr<NetworkManager> network_manager_;
    std::unique_ptr<RoomManager> room_manager_;
    std::unique_ptr<MessageHandler> message_handler_;
    
public:
    UDPServer(const ServerConfig& config) : config_(config) {}
    
    // 启动服务器
    bool start();
    
    // 停止服务器
    void stop();
    
    // 检查服务器是否运行
    bool isRunning() const;
    
private:
    // 初始化组件
    bool initializeComponents();
};

// ============================================================================
// 实现部分
// ============================================================================

// ServerConfig 实现
ServerConfig ServerConfig::parseCommandLine(int argc, char* argv[]) {
    ServerConfig config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            config.showUsage(argv[0]);
            exit(0);
        }
        else if (arg == "-i" || arg == "--ip") {
            if (i + 1 < argc) {
                config.bind_ip = argv[++i];
            } else {
                std::cerr << "错误: --ip 需要指定IP地址" << std::endl;
                exit(1);
            }
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                config.port = std::atoi(argv[++i]);
                if (config.port <= 0 || config.port > 65535) {
                    std::cerr << "错误: 端口号必须在 1-65535 之间" << std::endl;
                    exit(1);
                }
            } else {
                std::cerr << "错误: --port 需要指定端口号" << std::endl;
                exit(1);
            }
        }
        else {
            std::cerr << "错误: 未知参数 " << arg << std::endl;
            config.showUsage(argv[0]);
            exit(1);
        }
    }
    return config;
}

void ServerConfig::showUsage(const char* program_name) const {
    std::cout << "用法: " << program_name << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -h, --help              显示此帮助信息" << std::endl;
    std::cout << "  -i, --ip <IP>           设置监听IP地址 (默认: 0.0.0.0)" << std::endl;
    std::cout << "  -p, --port <PORT>       设置监听端口 (默认: 8080)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " -i 192.168.1.100 -p 8080" << std::endl;
    std::cout << "  " << program_name << " --ip 0.0.0.0 --port 9000" << std::endl;
}

// RoomManager 实现
bool RoomManager::addUserToRoom(const std::string& client_key, const std::string& user_id, 
                               const std::string& room_id, const struct sockaddr_in& address) {
    // 创建客户端信息
    auto client_info = std::make_shared<ClientInfo>(user_id, room_id, address);
    clients_[client_key] = client_info;
    
    // 添加到房间
    rooms_[room_id].insert(client_key);
    
    std::cout << "User " << user_id << " joined room " << room_id << std::endl;
    return true;
}

bool RoomManager::removeUserFromRoom(const std::string& client_key, const std::string& room_id) {
    // 从房间移除
    if (rooms_.find(room_id) != rooms_.end()) {
        rooms_[room_id].erase(client_key);
        if (rooms_[room_id].empty()) {
            rooms_.erase(room_id);
        }
    }
    
    // 从客户端列表移除
    if (clients_.find(client_key) != clients_.end()) {
        std::string user_id = clients_[client_key]->user_id;
        clients_.erase(client_key);
        std::cout << "User " << user_id << " left room " << room_id << std::endl;
        return true;
    }
    return false;
}

std::string RoomManager::getUserRoom(const std::string& client_key) const {
    auto it = clients_.find(client_key);
    if (it != clients_.end()) {
        return it->second->room_id;
    }
    return "";
}

std::vector<std::shared_ptr<ClientInfo>> RoomManager::getRoomClients(const std::string& room_id) const {
    std::vector<std::shared_ptr<ClientInfo>> result;
    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        for (const auto& client_key : it->second) {
            auto client_it = clients_.find(client_key);
            if (client_it != clients_.end()) {
                result.push_back(client_it->second);
            }
        }
    }
    return result;
}

bool RoomManager::isUserInRoom(const std::string& client_key, const std::string& room_id) const {
    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        return it->second.find(client_key) != it->second.end();
    }
    return false;
}

// MessageHandler 实现
void MessageHandler::handleMessage(const char* message, int length, const struct sockaddr_in& from_addr) {
    try {
        // 检查是否是字符串消息
        std::string msg(message, length);
        
        if (msg.find("JOIN:") == 0) {
            handleJoinMessage(msg, from_addr);
        } else if (msg.find("LEAVE:") == 0) {
            handleLeaveMessage(msg, from_addr);
        } else {
            // 尝试解析为音频包
            handleAudioPacket(message, length, from_addr);
        }
    } catch (const std::exception& e) {
        std::cerr << "[SERVER_LOG] 处理消息时发生异常: " << e.what() << std::endl;
    }
}

void MessageHandler::handleJoinMessage(const std::string& message, const struct sockaddr_in& from_addr) {
    size_t pos1 = message.find(':', 5);
    if (pos1 != std::string::npos) {
        std::string room_id = message.substr(5, pos1 - 5);
        std::string user_id = message.substr(pos1 + 1);
        
        std::string client_key = std::string(inet_ntoa(from_addr.sin_addr)) + ":" + 
                                std::to_string(ntohs(from_addr.sin_port));
        
        // 添加用户到房间
        room_manager_.addUserToRoom(client_key, user_id, room_id, from_addr);
        
        // 发送JOIN_OK响应
        std::string response = "JOIN_OK:" + room_id + ":" + user_id;
        sendto(server_fd_, response.c_str(), response.length(), 0,
               (struct sockaddr*)&from_addr, sizeof(from_addr));
        
        // 广播给房间内其他用户
        broadcastToRoom(room_id, "JOIN:" + room_id + ":" + user_id, from_addr);
    }
}

void MessageHandler::handleLeaveMessage(const std::string& message, const struct sockaddr_in& from_addr) {
    size_t pos1 = message.find(':', 6);
    if (pos1 != std::string::npos) {
        std::string room_id = message.substr(6, pos1 - 6);
        std::string user_id = message.substr(pos1 + 1);
        
        std::string client_key = std::string(inet_ntoa(from_addr.sin_addr)) + ":" + 
                                std::to_string(ntohs(from_addr.sin_port));
        
        // 从房间移除用户
        if (room_manager_.removeUserFromRoom(client_key, room_id)) {
            // 广播给房间内其他用户
            broadcastToRoom(room_id, "LEAVE:" + room_id + ":" + user_id, from_addr);
        }
    }
}

void MessageHandler::handleAudioPacket(const char* data, int length, const struct sockaddr_in& from_addr) {
    // 检查最小长度
    if (length < 14) return;
    
    // 解析音频包头部
    uint32_t sequence = ntohl(*reinterpret_cast<const uint32_t*>(data));
    uint32_t timestamp = ntohl(*reinterpret_cast<const uint32_t*>(data + 4));
    uint32_t user_id = ntohl(*reinterpret_cast<const uint32_t*>(data + 8));
    uint16_t raw_data_size = ntohs(*reinterpret_cast<const uint16_t*>(data + 12));
    uint16_t data_size = ntohs(raw_data_size);
    
    // 记录调试信息
    static auto last_audio_print = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - last_audio_print > std::chrono::seconds(5)) {
        std::cerr << "[SERVER_LOG] 尝试解析音频包: length=" << length << ", sequence=" << sequence 
                  << ", timestamp=" << timestamp << ", user_id=" << user_id 
                  << ", raw_data_size=0x" << std::hex << raw_data_size << std::dec
                  << ", data_size=" << data_size << ", 验证=" << (data_size <= 1024 && length >= (14 + data_size)) << std::endl;
        last_audio_print = now;
    }
    
    // 验证音频包
    if (data_size <= 1024 && length >= (14 + data_size)) {
        std::string client_key = std::string(inet_ntoa(from_addr.sin_addr)) + ":" + 
                                std::to_string(ntohs(from_addr.sin_port));
        
        // 检查客户端是否在列表中
        if (room_manager_.getUserRoom(client_key).empty()) {
            std::cerr << "[SERVER_LOG] 警告: 客户端 " << client_key << " 不在客户端列表中，忽略音频包" << std::endl;
            return;
        }
        
        std::string user_room_id = room_manager_.getUserRoom(client_key);
        
        // 检查房间是否存在
        if (user_room_id.empty()) {
            std::cerr << "[SERVER_LOG] 警告: 房间不存在，忽略音频包" << std::endl;
            return;
        }
        
        // 检查用户是否在房间中
        if (!room_manager_.isUserInRoom(client_key, user_room_id)) {
            return;
        }
        
        // 广播音频包
        broadcastAudioPacket(user_room_id, data, length, from_addr);
    }
}

void MessageHandler::broadcastToRoom(const std::string& room_id, const std::string& message, 
                                   const struct sockaddr_in& exclude_addr) {
    auto clients = room_manager_.getRoomClients(room_id);
    for (const auto& client : clients) {
        if (client->address.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
            client->address.sin_port != exclude_addr.sin_port) {
            sendto(server_fd_, message.c_str(), message.length(), 0,
                   (struct sockaddr*)&client->address, sizeof(client->address));
        }
    }
}

void MessageHandler::broadcastAudioPacket(const std::string& room_id, const char* data, int length,
                                        const struct sockaddr_in& exclude_addr) {
    auto clients = room_manager_.getRoomClients(room_id);
    for (const auto& client : clients) {
        if (client->address.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
            client->address.sin_port != exclude_addr.sin_port) {
            sendto(server_fd_, data, length, 0,
                   (struct sockaddr*)&client->address, sizeof(client->address));
        }
    }
}

// NetworkManager 实现
bool NetworkManager::initialize(const std::string& bind_ip, int port) {
    bind_ip_ = bind_ip;
    port_ = port;
    
    // 创建UDP socket
    server_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    if (bind_ip == "0.0.0.0") {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, bind_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid IP address: " << bind_ip << std::endl;
            close(server_fd_);
            return false;
        }
    }
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(server_fd_);
        return false;
    }
    
    std::cout << "监听地址: " << bind_ip << ":" << port << std::endl;
    std::cout << "================================\n";
    return true;
}

void NetworkManager::start(MessageHandler& handler) {
    if (running_) return;
    
    running_ = true;
    server_thread_ = std::thread(&NetworkManager::serverLoop, this, std::ref(handler));
    
    std::cout << "UDP Server started on " << bind_ip_ << ":" << port_ << std::endl;
    std::cout << "Server is running. Press Ctrl+C to stop...\n";
}

void NetworkManager::stop() {
    if (!running_) return;
    
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
}

void NetworkManager::serverLoop(MessageHandler& handler) {
    char buffer[2048];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (running_) {
        int received = recvfrom(server_fd_, buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr*)&client_addr, &client_len);
        if (received > 0) {
            buffer[received] = '\0';
            handler.handleMessage(buffer, received, client_addr);
        }
    }
}

// UDPServer 实现
bool UDPServer::initializeComponents() {
    // 创建网络管理器
    network_manager_ = std::make_unique<NetworkManager>();
    if (!network_manager_->initialize(config_.bind_ip, config_.port)) {
        return false;
    }
    
    // 创建房间管理器
    room_manager_ = std::make_unique<RoomManager>();
    
    // 创建消息处理器
    message_handler_ = std::make_unique<MessageHandler>(*room_manager_, network_manager_->getServerFd());
    
    return true;
}

bool UDPServer::start() {
    if (!initializeComponents()) {
        return false;
    }
    
    network_manager_->start(*message_handler_);
    return true;
}

void UDPServer::stop() {
    if (network_manager_) {
        network_manager_->stop();
    }
}

bool UDPServer::isRunning() const {
    return network_manager_ && network_manager_->getServerFd() >= 0;
}

// ============================================================================
// 主函数
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "=== UDP Voice Call Server (Refactored) ===" << std::endl;
    
    // 解析配置
    ServerConfig config = ServerConfig::parseCommandLine(argc, argv);
    
    // 创建并启动服务器
    UDPServer server(config);
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    // 等待用户中断
    std::cout << "Press Enter to stop server..." << std::endl;
    std::cin.get();
    
    // 停止服务器
    server.stop();
    std::cout << "Server stopped" << std::endl;
    
    return 0;
}
