#include "voice_call.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <sys/select.h>
#include <unistd.h>

std::atomic<bool> g_running(true);
voice_call_handle_t g_voice_call = nullptr;

// 默认配置
std::string g_server_ip = "127.0.0.1";
int g_server_port = 8080;
std::string g_room_id = "test_room";
std::string g_user_id = "linux_user";

// 显示使用帮助
void show_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -h, --help              显示此帮助信息" << std::endl;
    std::cout << "  -s, --server <IP>       设置服务器IP地址 (默认: 127.0.0.1)" << std::endl;
    std::cout << "  -p, --port <PORT>       设置服务器端口 (默认: 8080)" << std::endl;
    std::cout << "  -r, --room <ROOM_ID>    设置房间ID (默认: test_room)" << std::endl;
    std::cout << "  -u, --user <USER_ID>    设置用户ID (默认: linux_user)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " -s 192.168.1.100 -p 8080" << std::endl;
    std::cout << "  " << program_name << " --server 10.0.0.5 --port 9000 --room my_room --user alice" << std::endl;
}

// 解析命令行参数
bool parse_arguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
            return false;
        }
        else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                g_server_ip = argv[++i];
            } else {
                std::cerr << "错误: --server 需要指定IP地址" << std::endl;
                return false;
            }
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                g_server_port = std::atoi(argv[++i]);
                if (g_server_port <= 0 || g_server_port > 65535) {
                    std::cerr << "错误: 端口号必须在 1-65535 之间" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "错误: --port 需要指定端口号" << std::endl;
                return false;
            }
        }
        else if (arg == "-r" || arg == "--room") {
            if (i + 1 < argc) {
                g_room_id = argv[++i];
            } else {
                std::cerr << "错误: --room 需要指定房间ID" << std::endl;
                return false;
            }
        }
        else if (arg == "-u" || arg == "--user") {
            if (i + 1 < argc) {
                g_user_id = argv[++i];
            } else {
                std::cerr << "错误: --user 需要指定用户ID" << std::endl;
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

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "\n收到退出信号，正在关闭..." << std::endl;
    g_running = false;
}

// 状态变化回调
void on_state_changed(voice_call_state_t state, const char* reason) {
    std::cout << "状态变化: ";
    switch (state) {
        case VOICE_CALL_STATE_IDLE:
            std::cout << "空闲";
            break;
        case VOICE_CALL_STATE_CONNECTING:
            std::cout << "连接中";
            break;
        case VOICE_CALL_STATE_CONNECTED:
            std::cout << "已连接";
            break;
        case VOICE_CALL_STATE_DISCONNECTED:
            std::cout << "已断开";
            break;
        case VOICE_CALL_STATE_ERROR:
            std::cout << "错误";
            break;
    }
    if (reason && strlen(reason) > 0) {
        std::cout << " - " << reason;
    }
    std::cout << std::endl;
}

// 用户加入回调
void on_peer_joined(const char* peer_id) {
    std::cout << "用户加入: " << peer_id << std::endl;
}

// 用户离开回调
void on_peer_left(const char* peer_id) {
    std::cout << "用户离开: " << peer_id << std::endl;
}

// 音频电平回调
void on_audio_level(const char* peer_id, float level) {
    // 显示音频电平条
    int bars = static_cast<int>(level * 20);
    std::string level_bar(bars, '#');
    std::cout << "\r音频电平 [" << peer_id << "]: [" << level_bar << std::string(20 - bars, ' ') << "] " 
              << static_cast<int>(level * 100) << "%" << std::flush;
}

// 错误回调
void on_error(voice_call_error_t error, const char* message) {
    std::cout << "错误: ";
    switch (error) {
        case VOICE_CALL_ERROR_INVALID_PARAM:
            std::cout << "无效参数";
            break;
        case VOICE_CALL_ERROR_INIT_FAILED:
            std::cout << "初始化失败";
            break;
        case VOICE_CALL_ERROR_NETWORK:
            std::cout << "网络错误";
            break;
        case VOICE_CALL_ERROR_AUDIO:
            std::cout << "音频错误";
            break;
        case VOICE_CALL_ERROR_PEER_NOT_FOUND:
            std::cout << "用户未找到";
            break;
        case VOICE_CALL_ERROR_ALREADY_IN_CALL:
            std::cout << "已在通话中";
            break;
        default:
            std::cout << "未知错误";
            break;
    }
    if (message && strlen(message) > 0) {
        std::cout << " - " << message;
    }
    std::cout << std::endl;
}

// 显示帮助信息
void show_help() {
    std::cout << "\n=== 语音通话客户端 ===" << std::endl;
    std::cout << "命令列表:" << std::endl;
    std::cout << "  connect     - 连接到通话" << std::endl;
    std::cout << "  disconnect  - 断开连接" << std::endl;
    std::cout << "  mute        - 静音/取消静音" << std::endl;
    std::cout << "  volume      - 设置音量" << std::endl;
    std::cout << "  status      - 显示状态" << std::endl;
    std::cout << "  help        - 显示帮助" << std::endl;
    std::cout << "  quit        - 退出程序" << std::endl;
    std::cout << "=====================" << std::endl;
}

// 处理音量设置命令
void handle_volume_command() {
    std::cout << "设置音量 (0.0 - 1.0): ";
    float volume;
    std::cin >> volume;
    
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "无效的音量值" << std::endl;
        return;
    }
    
    if (volume < 0.0f || volume > 1.0f) {
        std::cout << "音量值必须在 0.0 到 1.0 之间" << std::endl;
        return;
    }
    
    voice_call_set_speaker_volume(g_voice_call, volume);
    std::cout << "扬声器音量已设置为: " << volume << std::endl;
}

// 显示状态信息
void show_status() {
    if (!g_voice_call) {
        std::cout << "语音通话未初始化" << std::endl;
        return;
    }
    
    voice_call_state_t state = voice_call_get_state(g_voice_call);
    bool muted = voice_call_is_muted(g_voice_call);
    
    std::cout << "当前状态:" << std::endl;
    std::cout << "  连接状态: ";
    switch (state) {
        case VOICE_CALL_STATE_IDLE:
            std::cout << "空闲";
            break;
        case VOICE_CALL_STATE_CONNECTING:
            std::cout << "连接中";
            break;
        case VOICE_CALL_STATE_CONNECTED:
            std::cout << "已连接";
            break;
        case VOICE_CALL_STATE_DISCONNECTED:
            std::cout << "已断开";
            break;
        case VOICE_CALL_STATE_ERROR:
            std::cout << "错误";
            break;
    }
    std::cout << std::endl;
    std::cout << "  静音状态: " << (muted ? "已静音" : "未静音") << std::endl;
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (!parse_arguments(argc, argv)) {
        return 1;
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "=== NativeVoiceCall Linux客户端 ===" << std::endl;
    std::cout << "版本: " << voice_call_get_version() << std::endl;
    std::cout << "服务器: " << g_server_ip << ":" << g_server_port << std::endl;
    std::cout << "房间ID: " << g_room_id << std::endl;
    std::cout << "用户ID: " << g_user_id << std::endl;
    std::cout << "================================" << std::endl;
    
    // 配置语音通话
    voice_call_config_t config = {};
    std::string server_url = "udp://" + g_server_ip + ":" + std::to_string(g_server_port);
    strcpy(config.server_url, server_url.c_str());
    strcpy(config.room_id, g_room_id.c_str());
    strcpy(config.user_id, g_user_id.c_str());
    
    config.audio_config.sample_rate = 48000;
    config.audio_config.channels = 1;
    config.audio_config.bits_per_sample = 16;
    config.audio_config.frame_size = 20;
    
    config.enable_echo_cancellation = true;
    config.enable_noise_suppression = true;
    config.enable_automatic_gain_control = true;
    
    // 设置回调函数
    voice_call_callbacks_t callbacks = {};
    callbacks.on_state_changed = on_state_changed;
    callbacks.on_peer_joined = on_peer_joined;
    callbacks.on_peer_left = on_peer_left;
    callbacks.on_audio_level = on_audio_level;
    callbacks.on_error = on_error;
    
    // 初始化语音通话
    g_voice_call = voice_call_init(&config, &callbacks);
    if (!g_voice_call) {
        std::cerr << "初始化语音通话失败" << std::endl;
        return 1;
    }
    
    std::cout << "语音通话初始化成功" << std::endl;
    show_help();
    
    // 主循环
    std::string command;
    while (g_running) {
        std::cout << "\n> ";
        std::cout.flush();
        
        // 使用select检查输入是否可用，以便能够响应信号
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;  // 1秒超时
        timeout.tv_usec = 0;
        
        int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        
        if (result < 0) {
            // select错误，可能是被信号中断
            if (errno == EINTR) {
                continue;
            }
            break;
        } else if (result == 0) {
            // 超时，继续循环
            continue;
        }
        
        // 有输入可用
        if (!std::getline(std::cin, command)) {
            break;
        }
        
        if (command == "connect") {
            if (voice_call_connect(g_voice_call) == VOICE_CALL_SUCCESS) {
                std::cout << "正在连接..." << std::endl;
            } else {
                std::cout << "连接失败" << std::endl;
            }
        }
        else if (command == "disconnect") {
            if (voice_call_disconnect(g_voice_call) == VOICE_CALL_SUCCESS) {
                std::cout << "正在断开连接..." << std::endl;
            } else {
                std::cout << "断开连接失败" << std::endl;
            }
        }
        else if (command == "mute") {
            bool current_muted = voice_call_is_muted(g_voice_call);
            if (voice_call_set_muted(g_voice_call, !current_muted) == VOICE_CALL_SUCCESS) {
                std::cout << "麦克风已" << (!current_muted ? "静音" : "取消静音") << std::endl;
            } else {
                std::cout << "设置静音状态失败" << std::endl;
            }
        }
        else if (command == "volume") {
            handle_volume_command();
        }
        else if (command == "status") {
            show_status();
        }
        else if (command == "help") {
            show_help();
        }
        else if (command == "quit" || command == "exit") {
            g_running = false;
        }
        else if (!command.empty()) {
            std::cout << "未知命令: " << command << std::endl;
            std::cout << "输入 'help' 查看可用命令" << std::endl;
        }
    }
    
    // 清理资源
    if (g_voice_call) {
        voice_call_disconnect(g_voice_call);
        voice_call_destroy(g_voice_call);
    }
    
    std::cout << "程序已退出" << std::endl;
    return 0;
}
