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
        
        // 等待用户输入退出
        std::cout << "Press Enter to stop server..." << std::endl;
        std::cin.get();
        
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
                static auto last_print = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (now - last_print > std::chrono::seconds(5)) {
                    std::cout << "收到UDP包: 大小=" << received << " bytes, 来自=" 
                              << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
                    last_print = now;
                }
                ProcessMessage(buffer, received, client_addr);
            }
        }
    }
    
    void ProcessMessage(const char* message, int length, const struct sockaddr_in& from_addr) {
        try {
            std::cout << "处理消息: 长度=" << length << ", 来自=" << inet_ntoa(from_addr.sin_addr) 
                      << ":" << ntohs(from_addr.sin_port) << std::endl;
            
            // 打印前32个字节的十六进制内容
            std::cout << "消息内容(hex): ";
            for (int i = 0; i < std::min(length, 32); i++) {
                printf("%02x ", (unsigned char)message[i]);
            }
            std::cout << std::endl;
            
            // 尝试打印为字符串
            std::cout << "消息内容(str): ";
            for (int i = 0; i < std::min(length, 32); i++) {
                char c = message[i];
                if (c >= 32 && c <= 126) {
                    std::cout << c;
                } else {
                    std::cout << ".";
                }
            }
            std::cout << std::endl;
        
            // 首先检查是否是字符串消息
            std::string msg(message, length);
            
            if (msg.find("JOIN:") == 0) {
                std::cout << "收到JOIN消息: " << msg << " 来自: " << inet_ntoa(from_addr.sin_addr) << ":" << ntohs(from_addr.sin_port) << std::endl;
                HandleJoin(msg, from_addr);
                return;
            } else if (msg.find("LEAVE:") == 0) {
                std::cout << "收到LEAVE消息: " << msg << " 来自: " << inet_ntoa(from_addr.sin_addr) << ":" << ntohs(from_addr.sin_port) << std::endl;
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
                              << ", data_size=" << data_size << ", 验证=" << (data_size <= 1024 && length == (14 + data_size)) << std::endl;
                    last_audio_print = now;
                }
                
                // 验证数据大小是否合理（最大1024字节，与AudioPacket结构体一致）
                std::cout << "音频包验证: data_size=" << data_size << ", length=" << length 
                          << ", 验证条件1=" << (data_size <= 1024) 
                          << ", 验证条件2=" << (length == (14 + data_size)) << std::endl;
                if (data_size <= 1024 && length == (14 + data_size)) {
                    // 这是一个AudioPacket，直接广播给房间内其他用户
                    std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                           std::to_string(ntohs(from_addr.sin_port));
                    
                    // 查找用户所在的房间
                    std::cout << "查找用户房间: " << client_key << std::endl;
                    std::cout << "当前房间数量: " << rooms_.size() << std::endl;
                    
                    // 检查客户端是否在客户端列表中
                    if (clients_.find(client_key) == clients_.end()) {
                        std::cout << "警告: 客户端 " << client_key << " 不在客户端列表中，忽略音频包" << std::endl;
                        return;
                    }
                    
                    std::string user_room_id = clients_[client_key].room_id;
                    std::cout << "用户 " << client_key << " 在房间: " << user_room_id << std::endl;
                    
                    // 检查房间是否存在
                    if (rooms_.find(user_room_id) == rooms_.end()) {
                        std::cout << "警告: 房间 " << user_room_id << " 不存在，忽略音频包" << std::endl;
                        return;
                    }
                    
                    // 检查用户是否在房间中
                    if (rooms_[user_room_id].find(client_key) == rooms_[user_room_id].end()) {
                        std::cout << "警告: 用户 " << client_key << " 不在房间 " << user_room_id << " 中，忽略音频包" << std::endl;
                        return;
                    }
                    
                    // 找到房间，广播音频包
                    std::cout << "找到用户房间，准备转发音频包: 房间=" << user_room_id 
                              << ", 用户=" << client_key 
                              << ", 数据大小=" << data_size << " bytes" << std::endl;
                    BroadcastAudioPacket(user_room_id, message, length, from_addr);
                    std::cout << "音频包转发完成" << std::endl;
                    return;
                } else {
                    // 音频包验证失败，记录日志但不继续处理
                    std::cout << "音频包验证失败，忽略此消息" << std::endl;
                    return;
                }
            }
            
            // 如果既不是字符串消息也不是音频包，记录未知消息
            std::cout << "收到未知消息类型，忽略" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "处理消息时发生异常: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "处理消息时发生未知异常" << std::endl;
        }
    }
    
    void HandleJoin(const std::string& message, const struct sockaddr_in& from_addr) {
        try {
            std::cout << "开始处理JOIN消息..." << std::endl;
            size_t pos1 = message.find(':', 5);
            std::cout << "解析JOIN消息: " << message << ", pos1=" << pos1 << std::endl;
            
            if (pos1 != std::string::npos) {
                std::string room_id = message.substr(5, pos1 - 5);
                std::string user_id = message.substr(pos1 + 1);
                std::cout << "解析结果: room_id=" << room_id << ", user_id=" << user_id << std::endl;
                
                // 记录用户
                std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                       std::to_string(ntohs(from_addr.sin_port));
                std::cout << "客户端key: " << client_key << std::endl;
                
                clients_[client_key] = {user_id, room_id, from_addr};
                std::cout << "用户记录已添加" << std::endl;
                
                // 添加到房间
                rooms_[room_id].insert(client_key);
                std::cout << "用户已添加到房间" << std::endl;
                
                std::cout << "User " << user_id << " joined room " << room_id << std::endl;
                
                // 发送JOIN_OK响应给客户端
                std::string response = "JOIN_OK:" + room_id + ":" + user_id;
                std::cout << "准备发送响应: " << response << std::endl;
                sendto(server_fd_, response.c_str(), response.length(), 0,
                       (struct sockaddr*)&from_addr, sizeof(from_addr));
                std::cout << "JOIN_OK响应已发送" << std::endl;
                
                // 广播给房间内其他用户
                std::cout << "准备广播给房间内其他用户..." << std::endl;
                BroadcastToRoom(room_id, "JOIN:" + room_id + ":" + user_id, from_addr);
                std::cout << "广播完成" << std::endl;
            } else {
                std::cout << "Invalid JOIN message format: " << message << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "HandleJoin异常: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "HandleJoin发生未知异常" << std::endl;
        }
    }
    
    void HandleLeave(const std::string& message, const struct sockaddr_in& from_addr) {
        try {
            std::cout << "开始处理LEAVE消息..." << std::endl;
            size_t pos1 = message.find(':', 6);
            std::cout << "解析LEAVE消息: " << message << ", pos1=" << pos1 << std::endl;
            
            if (pos1 != std::string::npos) {
                std::string room_id = message.substr(6, pos1 - 6);
                std::string user_id = message.substr(pos1 + 1);
                std::cout << "解析结果: room_id=" << room_id << ", user_id=" << user_id << std::endl;
                
                std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                       std::to_string(ntohs(from_addr.sin_port));
                std::cout << "客户端key: " << client_key << std::endl;
                
                // 从房间移除
                if (rooms_.find(room_id) != rooms_.end()) {
                    std::cout << "从房间 " << room_id << " 移除用户 " << client_key << std::endl;
                    rooms_[room_id].erase(client_key);
                    if (rooms_[room_id].empty()) {
                        std::cout << "房间 " << room_id << " 为空，删除房间" << std::endl;
                        rooms_.erase(room_id);
                    }
                } else {
                    std::cout << "警告: 房间 " << room_id << " 不存在" << std::endl;
                }
                
                // 从客户端列表移除
                if (clients_.find(client_key) != clients_.end()) {
                    std::cout << "从客户端列表移除 " << client_key << std::endl;
                    clients_.erase(client_key);
                } else {
                    std::cout << "警告: 客户端 " << client_key << " 不在客户端列表中" << std::endl;
                }
                
                std::cout << "User " << user_id << " left room " << room_id << std::endl;
                
                // 广播给房间内其他用户
                if (rooms_.find(room_id) != rooms_.end()) {
                    std::cout << "准备广播LEAVE消息给房间内其他用户..." << std::endl;
                    BroadcastToRoom(room_id, "LEAVE:" + room_id + ":" + user_id, from_addr);
                } else {
                    std::cout << "房间已删除，跳过广播" << std::endl;
                }
            } else {
                std::cout << "Invalid LEAVE message format: " << message << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "HandleLeave异常: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "HandleLeave发生未知异常" << std::endl;
        }
    }
    

    
    void BroadcastToRoom(const std::string& room_id, const std::string& message, 
                        const struct sockaddr_in& exclude_addr) {
        try {
            if (rooms_.find(room_id) == rooms_.end()) {
                std::cout << "房间 " << room_id << " 不存在，无法广播" << std::endl;
                return;
            }
            
            std::cout << "广播消息到房间 " << room_id << ": " << message << std::endl;
            std::cout << "房间中的客户端数量: " << rooms_[room_id].size() << std::endl;
            
            for (const auto& client_key : rooms_[room_id]) {
                std::cout << "检查客户端: " << client_key << std::endl;
                if (clients_.find(client_key) != clients_.end()) {
                    const auto& client = clients_[client_key];
                    std::cout << "客户端地址: " << inet_ntoa(client.addr.sin_addr) << ":" << ntohs(client.addr.sin_port) << std::endl;
                    std::cout << "排除地址: " << inet_ntoa(exclude_addr.sin_addr) << ":" << ntohs(exclude_addr.sin_port) << std::endl;
                    
                    if (client.addr.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
                        client.addr.sin_port != exclude_addr.sin_port) {
                        
                        std::cout << "准备发送广播消息到: " << client_key << std::endl;
                        int sent = sendto(server_fd_, message.c_str(), message.length(), 0,
                               (struct sockaddr*)&client.addr, sizeof(client.addr));
                        if (sent > 0) {
                            std::cout << "广播消息发送到 " << client_key << ": " << sent << " bytes" << std::endl;
                        } else {
                            std::cout << "广播消息发送失败到 " << client_key << ": " << strerror(errno) << std::endl;
                        }
                    } else {
                        std::cout << "跳过发送给发送者: " << client_key << std::endl;
                    }
                } else {
                    std::cout << "警告: 客户端 " << client_key << " 不在客户端列表中" << std::endl;
                }
            }
            std::cout << "广播完成" << std::endl;
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
                std::cout << "房间 " << room_id << " 不存在，无法转发音频包" << std::endl;
                return;
            }
            
            for (const auto& client_key : rooms_[room_id]) {
                if (clients_.find(client_key) != clients_.end()) {
                    const auto& client = clients_[client_key];
                    if (client.addr.sin_addr.s_addr != exclude_addr.sin_addr.s_addr ||
                        client.addr.sin_port != exclude_addr.sin_port) {
                        
                        int sent = sendto(server_fd_, data, length, 0,
                               (struct sockaddr*)&client.addr, sizeof(client.addr));
                        static auto last_send_print = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        if (now - last_send_print > std::chrono::seconds(5)) {
                            if (sent > 0) {
                                std::cout << "转发音频包到 " << client_key << ": 发送=" << sent << " bytes" << std::endl;
                            } else {
                                std::cout << "转发音频包失败到 " << client_key << ": " << strerror(errno) << std::endl;
                            }
                            last_send_print = now;
                        }
                    }
                } else {
                    std::cout << "警告: 客户端 " << client_key << " 不在客户端列表中，无法转发音频包" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "BroadcastAudioPacket异常: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "BroadcastAudioPacket发生未知异常" << std::endl;
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

