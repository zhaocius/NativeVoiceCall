#include "voice_call.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <map>

// 简化的UDP语音通话实现
class SimpleUDPVoiceCall {
public:
    SimpleUDPVoiceCall(const voice_call_config_t* config, const voice_call_callbacks_t* callbacks)
        : config_(*config)
        , callbacks_(*callbacks)
        , state_(VOICE_CALL_STATE_IDLE)
        , muted_(false)
        , socket_fd_(-1)
        , running_(false) {
        std::cout << "Simple UDP VoiceCall initialized for user: " << config->user_id << std::endl;
    }
    
    ~SimpleUDPVoiceCall() {
        Disconnect();
        std::cout << "Simple UDP VoiceCall destroyed" << std::endl;
    }
    
    voice_call_error_t Connect() {
        if (state_ != VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_ERROR_ALREADY_IN_CALL;
        }
        
        SetState(VOICE_CALL_STATE_CONNECTING);
        
        // 创建UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create UDP socket" << std::endl;
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 绑定本地端口
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(0);
        
        if (bind(socket_fd_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 设置广播
        int opt = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
        
        // 启动网络线程
        running_ = true;
        network_thread_ = std::thread(&SimpleUDPVoiceCall::NetworkLoop, this);
        
        // 发送加入消息
        SendJoinMessage();
        
        SetState(VOICE_CALL_STATE_CONNECTED);
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t Disconnect() {
        if (state_ == VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_SUCCESS;
        }
        
        running_ = false;
        if (network_thread_.joinable()) {
            network_thread_.join();
        }
        
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        SetState(VOICE_CALL_STATE_DISCONNECTED);
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_state_t GetState() const {
        return state_;
    }
    
    voice_call_error_t SetMuted(bool muted) {
        muted_ = muted;
        std::cout << "Microphone " << (muted ? "muted" : "unmuted") << std::endl;
        return VOICE_CALL_SUCCESS;
    }
    
    bool IsMuted() const {
        return muted_;
    }
    
    voice_call_error_t SetMicrophoneVolume(float volume) {
        if (volume < 0.0f || volume > 1.0f) {
            return VOICE_CALL_ERROR_INVALID_PARAM;
        }
        std::cout << "Microphone volume set to: " << volume << std::endl;
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t SetSpeakerVolume(float volume) {
        if (volume < 0.0f || volume > 1.0f) {
            return VOICE_CALL_ERROR_INVALID_PARAM;
        }
        std::cout << "Speaker volume set to: " << volume << std::endl;
        return VOICE_CALL_SUCCESS;
    }

private:
    void SendJoinMessage() {
        std::string message = "JOIN:" + std::string(config_.room_id) + ":" + std::string(config_.user_id);
        
        struct sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_port = htons(8080);
        broadcast_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        sendto(socket_fd_, message.c_str(), message.length(), 0,
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    }
    
    void NetworkLoop() {
        char buffer[1024];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        while (running_) {
            int received = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&from_addr, &from_len);
            if (received > 0) {
                buffer[received] = '\0';
                ProcessMessage(buffer, received);
            }
        }
    }
    
    void ProcessMessage(const char* message, int length) {
        std::string msg(message, length);
        
        if (msg.find("JOIN:") == 0) {
            size_t pos1 = msg.find(':', 5);
            size_t pos2 = msg.find(':', pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string room_id = msg.substr(5, pos1 - 5);
                std::string user_id = msg.substr(pos1 + 1, pos2 - pos1 - 1);
                
                if (room_id == config_.room_id && user_id != config_.user_id) {
                    std::cout << "User joined: " << user_id << std::endl;
                    if (callbacks_.on_peer_joined) {
                        callbacks_.on_peer_joined(user_id.c_str());
                    }
                }
            }
        } else if (msg.find("AUDIO:") == 0) {
            // 处理音频数据（简化版本）
            std::cout << "Received audio data" << std::endl;
        }
    }
    
    void SetState(voice_call_state_t new_state) {
        if (state_ != new_state) {
            state_ = new_state;
            if (callbacks_.on_state_changed) {
                const char* reason = "";
                switch (new_state) {
                    case VOICE_CALL_STATE_CONNECTING:
                        reason = "Connecting to network...";
                        break;
                    case VOICE_CALL_STATE_CONNECTED:
                        reason = "Connected to network";
                        break;
                    case VOICE_CALL_STATE_DISCONNECTED:
                        reason = "Disconnected";
                        break;
                    case VOICE_CALL_STATE_ERROR:
                        reason = "Network error";
                        break;
                    default:
                        break;
                }
                callbacks_.on_state_changed(new_state, reason);
            }
        }
    }
    
    voice_call_config_t config_;
    voice_call_callbacks_t callbacks_;
    std::atomic<voice_call_state_t> state_;
    std::atomic<bool> muted_;
    
    int socket_fd_;
    std::thread network_thread_;
    std::atomic<bool> running_;
};

// 全局实例管理
static std::map<void*, SimpleUDPVoiceCall*> g_instances;

// C API实现
extern "C" {

voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks) {
    if (!config || !callbacks) {
        return nullptr;
    }
    
    try {
        SimpleUDPVoiceCall* impl = new SimpleUDPVoiceCall(config, callbacks);
        void* handle = static_cast<void*>(impl);
        g_instances[handle] = impl;
        return static_cast<voice_call_handle_t>(handle);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create SimpleUDPVoiceCall: " << e.what() << std::endl;
        return nullptr;
    }
}

voice_call_error_t voice_call_connect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    return it->second->Connect();
}

voice_call_error_t voice_call_disconnect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    return it->second->Disconnect();
}

voice_call_state_t voice_call_get_state(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    return it->second->GetState();
}

voice_call_error_t voice_call_set_audio_input_device(voice_call_handle_t handle, 
                                                    const char* device_name) {
    return VOICE_CALL_SUCCESS;
}

voice_call_error_t voice_call_set_audio_output_device(voice_call_handle_t handle, 
                                                     const char* device_name) {
    return VOICE_CALL_SUCCESS;
}

voice_call_error_t voice_call_set_microphone_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    return it->second->SetMicrophoneVolume(volume);
}

voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    return it->second->SetSpeakerVolume(volume);
}

voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    return it->second->SetMuted(muted);
}

bool voice_call_is_muted(voice_call_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    auto it = g_instances.find(static_cast<void*>(handle));
    if (it == g_instances.end()) {
        return false;
    }
    
    return it->second->IsMuted();
}

void voice_call_destroy(voice_call_handle_t handle) {
    if (handle) {
        auto it = g_instances.find(static_cast<void*>(handle));
        if (it != g_instances.end()) {
            delete it->second;
            g_instances.erase(it);
        }
    }
}

const char* voice_call_get_version(void) {
    return "1.0.0 (Simple UDP)";
}

}
