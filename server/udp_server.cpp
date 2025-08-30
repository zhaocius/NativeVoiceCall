#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

// 默认配置
std::string g_bind_ip = "0.0.0.0";
int g_bind_port = 8080;

// 显示使用帮助
void show_usage(const char* program_name) {
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

// 解析命令行参数
bool parse_arguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
            return false;
        }
        else if (arg == "-i" || arg == "--ip") {
            if (i + 1 < argc) {
                g_bind_ip = argv[++i];
            } else {
                std::cerr << "错误: --ip 需要指定IP地址" << std::endl;
                return false;
            }
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                g_bind_port = std::atoi(argv[++i]);
                if (g_bind_port <= 0 || g_bind_port > 65535) {
                    std::cerr << "错误: 端口号必须在 1-65535 之间" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "错误: --port 需要指定端口号" << std::endl;
                return false;
            }
        }
        else {
            std::cerr << "错误: 未知参数 " << arg << std::endl;
            show_usage(argv[0]);
            return false;
        }
    }
    return true;
}

class UDPServer {
public:
    UDPServer(const std::string& bind_ip, int port) : bind_ip_(bind_ip), port_(port), running_(false) {}
    
    void Start() {
        // 创建UDP socket
        server_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (server_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }
        
        // 设置socket选项
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // 绑定地址
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        
        if (bind_ip_ == "0.0.0.0") {
            server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            if (inet_pton(AF_INET, bind_ip_.c_str(), &server_addr.sin_addr) <= 0) {
                std::cerr << "Invalid IP address: " << bind_ip_ << std::endl;
                close(server_fd_);
                return;
            }
        }
        server_addr.sin_port = htons(port_);
        
        if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket to " << bind_ip_ << ":" << port_ << std::endl;
            close(server_fd_);
            return;
        }
        
        std::cout << "UDP Server started on " << bind_ip_ << ":" << port_ << std::endl;
        
        running_ = true;
        server_thread_ = std::thread(&UDPServer::ServerLoop, this);
        
        // 持续运行，直到被信号中断
        std::cout << "Server is running. Press Ctrl+C to stop..." << std::endl;
        
        // 等待信号
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        Stop();
    }
    
    void Stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
        std::cout << "Server stopped" << std::endl;
    }
    
private:
    void ServerLoop() {
        char buffer[2048];  // 增加缓冲区大小以支持Android客户端(1038字节)和Linux客户端(974字节)
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (running_) {
            int received = recvfrom(server_fd_, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&client_addr, &client_len);
            if (received > 0) {
                buffer[received] = '\0';
                            // 移除UDP包接收日志，只保留用户状态变化
                ProcessMessage(buffer, received, client_addr);
            }
        }
    }
    
    void ProcessMessage(const char* message, int length, const struct sockaddr_in& from_addr) {
        try {
            // 移除详细的消息处理日志，只保留用户状态变化
        
            // 首先检查是否是字符串消息
            std::string msg(message, length);
            
            if (msg.find("JOIN:") == 0) {
                HandleJoin(msg, from_addr);
                return;
            } else if (msg.find("LEAVE:") == 0) {
                HandleLeave(msg, from_addr);
                return;
            }
            
            // 如果不是字符串消息，再检查是否是AudioPacket结构体
            if (length >= 14) { // AudioPacket的最小大小（头部）
                // 尝试解析为AudioPacket
                const uint32_t* header = reinterpret_cast<const uint32_t*>(message);
                uint32_t sequence = ntohl(header[0]);
                uint32_t timestamp = ntohl(header[1]);
                uint32_t user_id = ntohl(header[2]);
                uint16_t raw_data_size = *reinterpret_cast<const uint16_t*>(message + 12);
                uint16_t data_size = ntohs(raw_data_size);
                
                static auto last_audio_print = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (now - last_audio_print > std::chrono::seconds(5)) {
                    std::cout << "尝试解析音频包: length=" << length << ", sequence=" << sequence 
                              << ", timestamp=" << timestamp << ", user_id=" << user_id 
                              << ", raw_data_size=0x" << std::hex << raw_data_size << std::dec
                              << ", data_size=" << data_size << ", 验证=" << (data_size <= 1024 && length >= (14 + data_size)) << std::endl;
                    last_audio_print = now;
                }
                
                // 验证数据大小是否合理（最大1024字节，与AudioPacket结构体一致）
                // 音频包验证日志已移除，只保留用户状态变化
                if (data_size <= 1024 && length >= (14 + data_size)) {
                    // 这是一个AudioPacket，直接广播给房间内其他用户
                    std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                           std::to_string(ntohs(from_addr.sin_port));
                    
                            // 查找用户所在的房间
                    
                    // 检查客户端是否在客户端列表中
                    if (clients_.find(client_key) == clients_.end()) {
                        std::cout << "警告: 客户端 " << client_key << " 不在客户端列表中，忽略音频包" << std::endl;
                        return;
                    }
                    
                    std::string user_room_id = clients_[client_key].room_id;

                    
                    // 检查房间是否存在
                    if (rooms_.find(user_room_id) == rooms_.end()) {
                        std::cout << "警告: 房间 " << user_room_id << " 不存在，忽略音频包" << std::endl;
                        return;
                    }
                    
                                    // 检查用户是否在房间中
                if (rooms_[user_room_id].find(client_key) == rooms_[user_room_id].end()) {
                    return;
                }
                    
                    // 找到房间，广播音频包
                    BroadcastAudioPacket(user_room_id, message, length, from_addr);
                    return;
                } else {
                                    // 音频包验证失败，忽略此消息
                    return;
                }
            }
            
            // 如果既不是字符串消息也不是音频包，忽略
        } catch (const std::exception& e) {
            std::cerr << "[SERVER_LOG] 处理消息时发生异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[SERVER_LOG] 处理消息时发生未知异常" << std::endl;
        }
    }
    
    void HandleJoin(const std::string& message, const struct sockaddr_in& from_addr) {
        try {
            size_t pos1 = message.find(':', 5);
            
            if (pos1 != std::string::npos) {
                std::string room_id = message.substr(5, pos1 - 5);
                std::string user_id = message.substr(pos1 + 1);
                
                // 记录用户
                std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                       std::to_string(ntohs(from_addr.sin_port));
                
                clients_[client_key] = {user_id, room_id, from_addr};
                
                // 添加到房间
                rooms_[room_id].insert(client_key);
                
                // 只打印用户状态变化到控制台
                std::cout << "User " << user_id << " joined room " << room_id << std::endl;
                
                // 发送JOIN_OK响应给客户端
                std::string response = "JOIN_OK:" + room_id + ":" + user_id;
                sendto(server_fd_, response.c_str(), response.length(), 0,
                       (struct sockaddr*)&from_addr, sizeof(from_addr));
                
                // 广播给房间内其他用户
                BroadcastToRoom(room_id, "JOIN:" + room_id + ":" + user_id, from_addr);
            }
        } catch (const std::exception& e) {
            std::cerr << "[SERVER_LOG] HandleJoin异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[SERVER_LOG] HandleJoin发生未知异常" << std::endl;
        }
    }
    
    void HandleLeave(const std::string& message, const struct sockaddr_in& from_addr) {
        try {
            size_t pos1 = message.find(':', 6);
            
            if (pos1 != std::string::npos) {
                std::string room_id = message.substr(6, pos1 - 6);
                std::string user_id = message.substr(pos1 + 1);
                
                std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                       std::to_string(ntohs(from_addr.sin_port));
                
                // 从房间移除
                if (rooms_.find(room_id) != rooms_.end()) {
                    rooms_[room_id].erase(client_key);
                    if (rooms_[room_id].empty()) {
                        rooms_.erase(room_id);
                    }
                }
                
                // 从客户端列表移除
                if (clients_.find(client_key) != clients_.end()) {
                    clients_.erase(client_key);
                }
                
                // 只打印用户状态变化到控制台
                std::cout << "User " << user_id << " left room " << room_id << std::endl;
                
                // 广播给房间内其他用户
                if (rooms_.find(room_id) != rooms_.end()) {
                    BroadcastToRoom(room_id, "LEAVE:" + room_id + ":" + user_id, from_addr);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[SERVER_LOG] HandleLeave异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[SERVER_LOG] HandleLeave发生未知异常" << std::endl;
        }
    }
    

    
    void BroadcastToRoom(const std::string& room_id, const std::string& message, 
                        const struct sockaddr_in& exclude_addr) {
        try {
            if (rooms_.find(room_id) == rooms_.end()) {
                std::cout << "房间 " << room_id << " 不存在，无法广播" << std::endl;
                return;
            }
            
            for (const auto& client_key : rooms_[room_id]) {
                if (clients_.find(client_key) != clients_.end()) {
                    const auto& client = clients_[client_key];
                    
                    if (client.addr.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
                        client.addr.sin_port != exclude_addr.sin_port) {
                        
                        sendto(server_fd_, message.c_str(), message.length(), 0,
                               (struct sockaddr*)&client.addr, sizeof(client.addr));
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cout << "BroadcastToRoom异常: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "BroadcastToRoom发生未知异常" << std::endl;
        }
    }
    
    void BroadcastAudioPacket(const std::string& room_id, const char* data, int length,
                             const struct sockaddr_in& exclude_addr) {
        try {
            if (rooms_.find(room_id) == rooms_.end()) {
                return;
            }
            
            for (const auto& client_key : rooms_[room_id]) {
                if (clients_.find(client_key) != clients_.end()) {
                    const auto& client = clients_[client_key];
                    if (client.addr.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
                        client.addr.sin_port != exclude_addr.sin_port) {
                        
                        sendto(server_fd_, data, length, 0,
                               (struct sockaddr*)&client.addr, sizeof(client.addr));
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[SERVER_LOG] BroadcastAudioPacket异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[SERVER_LOG] BroadcastAudioPacket发生未知异常" << std::endl;
        }
    }
    
    struct ClientInfo {
        std::string user_id;
        std::string room_id;
        struct sockaddr_in addr;
    };
    
    std::string bind_ip_;
    int port_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    std::map<std::string, ClientInfo> clients_;
    std::map<std::string, std::set<std::string>> rooms_;
};

int main(int argc, char* argv[]) {
    std::cout << "=== UDP Voice Call Server ===" << std::endl;
    
    // 解析命令行参数
    if (!parse_arguments(argc, argv)) {
        return 1;
    }
    
    std::cout << "监听地址: " << g_bind_ip << ":" << g_bind_port << std::endl;
    std::cout << "================================" << std::endl;

    UDPServer server(g_bind_ip, g_bind_port);
    server.Start();
    
    return 0;
}

