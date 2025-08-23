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
        
        // 运行一段时间后自动停止
        std::cout << "Server running for 30 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
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
        char buffer[1024];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (running_) {
            int received = recvfrom(server_fd_, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&client_addr, &client_len);
            if (received > 0) {
                buffer[received] = '\0';
                ProcessMessage(buffer, received, client_addr);
            }
        }
    }
    
    void ProcessMessage(const char* message, int length, const struct sockaddr_in& from_addr) {
        std::string msg(message, length);
        
        std::cout << "收到消息: " << msg << " 来自: " << inet_ntoa(from_addr.sin_addr) << ":" << ntohs(from_addr.sin_port) << std::endl;
        
        if (msg.find("JOIN:") == 0) {
            HandleJoin(msg, from_addr);
        } else if (msg.find("LEAVE:") == 0) {
            HandleLeave(msg, from_addr);
        } else if (msg.find("AUDIO:") == 0) {
            HandleAudio(msg, from_addr);
        }
    }
    
    void HandleJoin(const std::string& message, const struct sockaddr_in& from_addr) {
        size_t pos1 = message.find(':', 5);
        std::cout << "解析JOIN消息: " << message << ", pos1=" << pos1 << std::endl;
        
        if (pos1 != std::string::npos) {
            std::string room_id = message.substr(5, pos1 - 5);
            std::string user_id = message.substr(pos1 + 1);
            
            // 记录用户
            std::string client_key = inet_ntoa(from_addr.sin_addr) + std::string(":") + 
                                   std::to_string(ntohs(from_addr.sin_port));
            clients_[client_key] = {user_id, room_id, from_addr};
            
            // 添加到房间
            rooms_[room_id].insert(client_key);
            
            std::cout << "User " << user_id << " joined room " << room_id << std::endl;
            
            // 广播给房间内其他用户
            BroadcastToRoom(room_id, "JOIN:" + room_id + ":" + user_id, from_addr);
        } else {
            std::cout << "Invalid JOIN message format: " << message << std::endl;
        }
    }
    
    void HandleLeave(const std::string& message, const struct sockaddr_in& from_addr) {
        size_t pos1 = message.find(':', 6);
        size_t pos2 = message.find(':', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string room_id = message.substr(6, pos1 - 6);
            std::string user_id = message.substr(pos1 + 1, pos2 - pos1 - 1);
            
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
            clients_.erase(client_key);
            
            std::cout << "User " << user_id << " left room " << room_id << std::endl;
            
            // 广播给房间内其他用户
            BroadcastToRoom(room_id, "LEAVE:" + room_id + ":" + user_id, from_addr);
        }
    }
    
    void HandleAudio(const std::string& message, const struct sockaddr_in& from_addr) {
        size_t pos1 = message.find(':', 6);
        size_t pos2 = message.find(':', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string room_id = message.substr(6, pos1 - 6);
            std::string user_id = message.substr(pos1 + 1, pos2 - pos1 - 1);
            
            // 广播音频数据给房间内其他用户
            BroadcastToRoom(room_id, message, from_addr);
        }
    }
    
    void BroadcastToRoom(const std::string& room_id, const std::string& message, 
                        const struct sockaddr_in& exclude_addr) {
        if (rooms_.find(room_id) == rooms_.end()) {
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

