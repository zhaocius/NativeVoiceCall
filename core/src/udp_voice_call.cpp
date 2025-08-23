#include "voice_call.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <queue>
#include <chrono>
#include <cstring>
#include <cmath>

// 网络相关头文件
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

// 音频相关头文件
#include <alsa/asoundlib.h>
#include <pthread.h>

// 音频包结构
struct AudioPacket {
    uint32_t sequence;
    uint32_t timestamp;
    uint32_t user_id;
    uint16_t data_size;
    uint8_t data[1024];
} __attribute__((packed));

// UDP语音通话实现类
class UDPVoiceCallImpl {
public:
    UDPVoiceCallImpl(const voice_call_config_t* config, const voice_call_callbacks_t* callbacks)
        : config_(*config)
        , callbacks_(*callbacks)
        , state_(VOICE_CALL_STATE_IDLE)
        , muted_(false)
        , mic_volume_(1.0f)
        , speaker_volume_(1.0f)
        , socket_fd_(-1)
        , audio_capture_handle_(nullptr)
        , audio_playback_handle_(nullptr)
        , running_(false)
        , sequence_(0) {
        
        std::cout << "UDP VoiceCall initialized for user: " << config->user_id << std::endl;
    }
    
    ~UDPVoiceCallImpl() {
        Disconnect();
        std::cout << "UDP VoiceCall destroyed" << std::endl;
    }
    
    voice_call_error_t Connect() {
        if (state_ != VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_ERROR_ALREADY_IN_CALL;
        }
        
        SetState(VOICE_CALL_STATE_CONNECTING);
        
        // 解析服务器地址
        std::string server_url = config_.server_url;
        std::string host = "localhost";
        int port = 8080;
        
        if (server_url.find("udp://") == 0) {
            server_url = server_url.substr(6);
        }
        
        size_t colon_pos = server_url.find(':');
        if (colon_pos != std::string::npos) {
            host = server_url.substr(0, colon_pos);
            port = std::stoi(server_url.substr(colon_pos + 1));
        }
        
        // 创建UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create UDP socket" << std::endl;
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 设置socket选项
        int opt = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // 绑定本地端口
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(0); // 随机端口
        
        if (bind(socket_fd_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 设置服务器地址
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port);
        server_addr_.sin_addr.s_addr = inet_addr(host.c_str());
        
        // 初始化音频设备
        if (!InitializeAudio()) {
            close(socket_fd_);
            socket_fd_ = -1;
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_AUDIO;
        }
        
        // 启动音频处理线程
        running_ = true;
        audio_thread_ = std::thread(&UDPVoiceCallImpl::AudioLoop, this);
        network_thread_ = std::thread(&UDPVoiceCallImpl::NetworkLoop, this);
        
        // 发送加入房间消息
        SendJoinMessage();
        
        SetState(VOICE_CALL_STATE_CONNECTED);
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t Disconnect() {
        if (state_ == VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_SUCCESS;
        }
        
        // 停止线程
        running_ = false;
        
        if (audio_thread_.joinable()) {
            audio_thread_.join();
        }
        if (network_thread_.joinable()) {
            network_thread_.join();
        }
        
        // 发送离开消息
        SendLeaveMessage();
        
        // 关闭音频设备
        CloseAudio();
        
        // 关闭socket
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
        mic_volume_ = volume;
        std::cout << "Microphone volume set to: " << volume << std::endl;
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t SetSpeakerVolume(float volume) {
        if (volume < 0.0f || volume > 1.0f) {
            return VOICE_CALL_ERROR_INVALID_PARAM;
        }
        speaker_volume_ = volume;
        std::cout << "Speaker volume set to: " << volume << std::endl;
        return VOICE_CALL_SUCCESS;
    }

private:
    bool InitializeAudio() {
        int err;
        
        std::cout << "Initializing audio devices..." << std::endl;
        
        // 打开音频捕获设备
        err = snd_pcm_open(&audio_capture_handle_, "default", SND_PCM_STREAM_CAPTURE, 0);
        if (err < 0) {
            std::cerr << "Failed to open audio capture device: " << snd_strerror(err) << std::endl;
            std::cerr << "Trying to use 'hw:0,0'..." << std::endl;
            
            // 尝试使用硬件设备
            err = snd_pcm_open(&audio_capture_handle_, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
            if (err < 0) {
                std::cerr << "Failed to open hardware audio capture device: " << snd_strerror(err) << std::endl;
                return false;
            }
        }
        
        // 打开音频播放设备
        err = snd_pcm_open(&audio_playback_handle_, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            std::cerr << "Failed to open audio playback device: " << snd_strerror(err) << std::endl;
            std::cerr << "Trying to use 'hw:0,0'..." << std::endl;
            
            // 尝试使用硬件设备
            err = snd_pcm_open(&audio_playback_handle_, "hw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
            if (err < 0) {
                std::cerr << "Failed to open hardware audio playback device: " << snd_strerror(err) << std::endl;
                snd_pcm_close(audio_capture_handle_);
                audio_capture_handle_ = nullptr;
                return false;
            }
        }
        
        // 设置音频参数
        snd_pcm_hw_params_t* hw_params;
        snd_pcm_hw_params_alloca(&hw_params);
        
        // 配置捕获设备
        snd_pcm_hw_params_any(audio_capture_handle_, hw_params);
        snd_pcm_hw_params_set_access(audio_capture_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(audio_capture_handle_, hw_params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_rate(audio_capture_handle_, hw_params, config_.audio_config.sample_rate, 0);
        snd_pcm_hw_params_set_channels(audio_capture_handle_, hw_params, config_.audio_config.channels);
        
        // 计算合适的缓冲区大小 (20ms)
        snd_pcm_uframes_t buffer_size = config_.audio_config.sample_rate * config_.audio_config.channels * 2 / 50;
        snd_pcm_uframes_t period_size = buffer_size / 4;
        
        // 设置缓冲区大小
        err = snd_pcm_hw_params_set_buffer_size(audio_capture_handle_, hw_params, buffer_size);
        if (err < 0) {
            std::cerr << "Failed to set capture buffer size: " << snd_strerror(err) << std::endl;
            // 尝试使用默认值
            snd_pcm_hw_params_set_buffer_size_near(audio_capture_handle_, hw_params, &buffer_size);
        }
        
        // 设置周期大小
        err = snd_pcm_hw_params_set_period_size(audio_capture_handle_, hw_params, period_size, 0);
        if (err < 0) {
            std::cerr << "Failed to set capture period size: " << snd_strerror(err) << std::endl;
            // 尝试使用默认值
            snd_pcm_hw_params_set_period_size_near(audio_capture_handle_, hw_params, &period_size, 0);
        }
        
        err = snd_pcm_hw_params(audio_capture_handle_, hw_params);
        if (err < 0) {
            std::cerr << "Failed to set capture parameters: " << snd_strerror(err) << std::endl;
            return false;
        }
        
        // 配置播放设备
        snd_pcm_hw_params_any(audio_playback_handle_, hw_params);
        snd_pcm_hw_params_set_access(audio_playback_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(audio_playback_handle_, hw_params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_rate(audio_playback_handle_, hw_params, config_.audio_config.sample_rate, 0);
        snd_pcm_hw_params_set_channels(audio_playback_handle_, hw_params, config_.audio_config.channels);
        
        // 设置缓冲区大小
        err = snd_pcm_hw_params_set_buffer_size(audio_playback_handle_, hw_params, buffer_size);
        if (err < 0) {
            std::cerr << "Failed to set playback buffer size: " << snd_strerror(err) << std::endl;
            snd_pcm_hw_params_set_buffer_size_near(audio_playback_handle_, hw_params, &buffer_size);
        }
        
        // 设置周期大小
        err = snd_pcm_hw_params_set_period_size(audio_playback_handle_, hw_params, period_size, 0);
        if (err < 0) {
            std::cerr << "Failed to set playback period size: " << snd_strerror(err) << std::endl;
            snd_pcm_hw_params_set_period_size_near(audio_playback_handle_, hw_params, &period_size, 0);
        }
        
        err = snd_pcm_hw_params(audio_playback_handle_, hw_params);
        if (err < 0) {
            std::cerr << "Failed to set playback parameters: " << snd_strerror(err) << std::endl;
            return false;
        }
        
        std::cout << "Audio devices initialized successfully" << std::endl;
        std::cout << "Sample rate: " << config_.audio_config.sample_rate << " Hz" << std::endl;
        std::cout << "Channels: " << config_.audio_config.channels << std::endl;
        std::cout << "Buffer size: " << buffer_size << " bytes" << std::endl;
        std::cout << "Period size: " << period_size << " bytes" << std::endl;
        return true;
    }
    
    void CloseAudio() {
        if (audio_capture_handle_) {
            snd_pcm_close(audio_capture_handle_);
            audio_capture_handle_ = nullptr;
        }
        if (audio_playback_handle_) {
            snd_pcm_close(audio_playback_handle_);
            audio_playback_handle_ = nullptr;
        }
    }
    
    void SendJoinMessage() {
        std::string message = "JOIN:" + std::string(config_.room_id) + ":" + std::string(config_.user_id);
        std::cout << "发送JOIN消息: " << message << std::endl;
        int sent = sendto(socket_fd_, message.c_str(), message.length(), 0, 
               (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if (sent < 0) {
            std::cerr << "发送JOIN消息失败: " << strerror(errno) << std::endl;
        } else {
            std::cout << "JOIN消息发送成功 (" << sent << " bytes)" << std::endl;
        }
    }
    
    void SendLeaveMessage() {
        std::string message = "LEAVE:" + std::string(config_.room_id) + ":" + std::string(config_.user_id);
        sendto(socket_fd_, message.c_str(), message.length(), 0, 
               (struct sockaddr*)&server_addr_, sizeof(server_addr_));
    }
    
    void AudioLoop() {
        const int frame_size = config_.audio_config.sample_rate * config_.audio_config.channels * 2 / 50; // 20ms
        std::vector<int16_t> audio_buffer(frame_size);
        std::vector<int16_t> silence_buffer(frame_size, 0);
        
        std::cout << "Audio loop started, frame size: " << frame_size << " bytes" << std::endl;
        
        while (running_) {
            // 捕获音频
            if (!muted_ && audio_capture_handle_) {
                snd_pcm_sframes_t frames = snd_pcm_readi(audio_capture_handle_, audio_buffer.data(), frame_size / 2);
                if (frames > 0) {
                    // 应用音量
                    for (int i = 0; i < frames * config_.audio_config.channels; ++i) {
                        audio_buffer[i] = static_cast<int16_t>(audio_buffer[i] * mic_volume_);
                    }
                    
                    // 发送音频包
                    SendAudioPacket(audio_buffer.data(), frames * config_.audio_config.channels * 2);
                    
                    // 显示音频电平
                    if (callbacks_.on_audio_level) {
                        float level = CalculateAudioLevel(audio_buffer.data(), frames * config_.audio_config.channels);
                        callbacks_.on_audio_level(config_.user_id, level);
                    }
                } else if (frames < 0) {
                    // 处理音频错误
                    snd_pcm_recover(audio_capture_handle_, frames, 0);
                }
            }
            
            // 播放接收到的音频
            if (audio_playback_handle_) {
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                if (!audio_queue_.empty()) {
                    AudioPacket packet = audio_queue_.front();
                    audio_queue_.pop();
                    
                    // 转换字节序
                    uint16_t data_size = ntohs(packet.data_size);
                    
                    // 应用音量
                    std::vector<int16_t> playback_buffer(data_size / 2);
                    memcpy(playback_buffer.data(), packet.data, data_size);
                    
                    for (int i = 0; i < playback_buffer.size(); ++i) {
                        playback_buffer[i] = static_cast<int16_t>(playback_buffer[i] * speaker_volume_);
                    }
                    
                    snd_pcm_sframes_t frames = snd_pcm_writei(audio_playback_handle_, 
                                                             playback_buffer.data(), 
                                                             playback_buffer.size() / config_.audio_config.channels);
                    if (frames < 0) {
                        snd_pcm_recover(audio_playback_handle_, frames, 0);
                    }
                } else {
                    // 播放静音以避免音频设备停止
                    snd_pcm_sframes_t frames = snd_pcm_writei(audio_playback_handle_, 
                                                             silence_buffer.data(), 
                                                             silence_buffer.size() / config_.audio_config.channels);
                    if (frames < 0) {
                        snd_pcm_recover(audio_playback_handle_, frames, 0);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 50Hz
        }
        
        std::cout << "Audio loop stopped" << std::endl;
    }
    
    void NetworkLoop() {
        char buffer[2048];
        
        while (running_) {
            struct pollfd pfd;
            pfd.fd = socket_fd_;
            pfd.events = POLLIN;
            
            if (poll(&pfd, 1, 100) > 0) {
                struct sockaddr_in from_addr;
                socklen_t from_len = sizeof(from_addr);
                
                int received = recvfrom(socket_fd_, buffer, sizeof(buffer), 0, 
                                       (struct sockaddr*)&from_addr, &from_len);
                if (received > 0) {
                    ProcessNetworkMessage(buffer, received, from_addr);
                }
            }
        }
    }
    
    void SendAudioPacket(const void* data, size_t size) {
        // 限制音频数据大小，避免UDP包过大
        const size_t max_audio_size = 960; // 限制为960字节，确保总包大小不超过1024
        
        if (size > max_audio_size) {
            size = max_audio_size;
        }
        
        AudioPacket packet;
        packet.sequence = htonl(sequence_++);
        packet.timestamp = htonl(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        packet.user_id = htonl(std::hash<std::string>{}(config_.user_id));
        packet.data_size = htons(size);
        
        memcpy(packet.data, data, size);
        
        int packet_size = sizeof(packet) - sizeof(packet.data) + size;
        std::cout << "DEBUG: sizeof(packet)=" << sizeof(packet) << ", sizeof(packet.data)=" << sizeof(packet.data) << ", size=" << size << ", packet_size=" << packet_size << std::endl;
        int sent = sendto(socket_fd_, &packet, packet_size, 0,
               (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        
        static auto last_send_print = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (sent > 0) {
            if (now - last_send_print > std::chrono::seconds(5)) {
                std::cout << "发送音频包: 原始大小=" << size << ", 包大小=" << packet_size << ", 发送=" << sent << " bytes, 序列=" << ntohl(packet.sequence) << std::endl;
                last_send_print = now;
            }
        } else {
            std::cout << "发送音频包失败: " << strerror(errno) << std::endl;
        }
    }
    
    void ProcessNetworkMessage(const char* buffer, int size, const struct sockaddr_in& from_addr) {
        if (size >= sizeof(AudioPacket)) {
            const AudioPacket* packet = reinterpret_cast<const AudioPacket*>(buffer);
            
            // 检查是否是其他用户的音频包
            uint32_t my_id = std::hash<std::string>{}(config_.user_id);
            uint32_t packet_user_id = ntohl(packet->user_id);
            if (packet_user_id != my_id) {
                // 添加到播放队列
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                if (audio_queue_.size() < 10) { // 限制队列大小
                    audio_queue_.push(*packet);
                    static auto last_recv_print = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    if (now - last_recv_print > std::chrono::seconds(5)) {
                        std::cout << "收到音频包: 大小=" << size << " bytes, 队列大小=" << audio_queue_.size() << std::endl;
                        last_recv_print = now;
                    }
                }
            }
        } else {
            // 处理控制消息
            std::string message(buffer, size);
            if (message.find("JOIN:") == 0) {
                // 用户加入
                size_t pos1 = message.find(':', 5);
                size_t pos2 = message.find(':', pos1 + 1);
                if (pos1 != std::string::npos && pos2 != std::string::npos) {
                    std::string room_id = message.substr(5, pos1 - 5);
                    std::string user_id = message.substr(pos1 + 1, pos2 - pos1 - 1);
                    
                    if (room_id == config_.room_id && user_id != config_.user_id) {
                        if (callbacks_.on_peer_joined) {
                            callbacks_.on_peer_joined(user_id.c_str());
                        }
                    }
                }
            } else if (message.find("LEAVE:") == 0) {
                // 用户离开
                size_t pos1 = message.find(':', 6);
                size_t pos2 = message.find(':', pos1 + 1);
                if (pos1 != std::string::npos && pos2 != std::string::npos) {
                    std::string room_id = message.substr(6, pos1 - 6);
                    std::string user_id = message.substr(pos1 + 1, pos2 - pos1 - 1);
                    
                    if (room_id == config_.room_id && user_id != config_.user_id) {
                        if (callbacks_.on_peer_left) {
                            callbacks_.on_peer_left(user_id.c_str());
                        }
                    }
                }
            }
        }
    }
    
    float CalculateAudioLevel(const int16_t* audio_data, int samples) {
        if (samples <= 0) return 0.0f;
        
        // 计算RMS (Root Mean Square)
        double sum = 0.0;
        for (int i = 0; i < samples; ++i) {
            double sample = static_cast<double>(audio_data[i]) / 32768.0; // 归一化到[-1, 1]
            sum += sample * sample;
        }
        
        double rms = sqrt(sum / samples);
        
        // 转换为分贝，然后归一化到[0, 1]
        double db = 20.0 * log10(rms + 1e-10);
        float level = std::max(0.0f, std::min(1.0f, static_cast<float>((db + 60.0) / 60.0)));
        
        return level;
    }
    
    void SetState(voice_call_state_t new_state) {
        if (state_ != new_state) {
            state_ = new_state;
            if (callbacks_.on_state_changed) {
                const char* reason = "";
                switch (new_state) {
                    case VOICE_CALL_STATE_CONNECTING:
                        reason = "Connecting to server...";
                        break;
                    case VOICE_CALL_STATE_CONNECTED:
                        reason = "Connected successfully";
                        break;
                    case VOICE_CALL_STATE_DISCONNECTED:
                        reason = "Disconnected";
                        break;
                    case VOICE_CALL_STATE_ERROR:
                        reason = "Connection error";
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
    float mic_volume_;
    float speaker_volume_;
    
    int socket_fd_;
    struct sockaddr_in server_addr_;
    
    snd_pcm_t* audio_capture_handle_;
    snd_pcm_t* audio_playback_handle_;
    
    std::thread audio_thread_;
    std::thread network_thread_;
    std::atomic<bool> running_;
    
    std::queue<AudioPacket> audio_queue_;
    std::mutex audio_queue_mutex_;
    
    std::atomic<uint32_t> sequence_;
};

// C API实现
extern "C" {

voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks) {
    if (!config || !callbacks) {
        return nullptr;
    }
    
    try {
        return new UDPVoiceCallImpl(config, callbacks);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create UDPVoiceCallImpl: " << e.what() << std::endl;
        return nullptr;
    }
}

voice_call_error_t voice_call_connect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->Connect();
}

voice_call_error_t voice_call_disconnect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->Disconnect();
}

voice_call_state_t voice_call_get_state(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->GetState();
}

voice_call_error_t voice_call_set_audio_input_device(voice_call_handle_t handle, 
                                                    const char* device_name) {
    // 简化实现
    return VOICE_CALL_SUCCESS;
}

voice_call_error_t voice_call_set_audio_output_device(voice_call_handle_t handle, 
                                                     const char* device_name) {
    // 简化实现
    return VOICE_CALL_SUCCESS;
}

voice_call_error_t voice_call_set_microphone_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->SetMicrophoneVolume(volume);
}

voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->SetSpeakerVolume(volume);
}

voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->SetMuted(muted);
}

bool voice_call_is_muted(voice_call_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
    return impl->IsMuted();
}

void voice_call_destroy(voice_call_handle_t handle) {
    if (handle) {
        UDPVoiceCallImpl* impl = static_cast<UDPVoiceCallImpl*>(handle);
        delete impl;
    }
}

const char* voice_call_get_version(void) {
    return "1.0.0 (UDP Audio)";
}

}
