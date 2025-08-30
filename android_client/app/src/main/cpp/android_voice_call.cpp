#include "voice_call.h"
#include <android/log.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define LOG_TAG "VoiceCallAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 前向声明
extern "C" void AudioPlaybackCallback(SLAndroidSimpleBufferQueueItf caller, void* context);

// 简化的Android语音通话实现
class AndroidVoiceCallImpl {
public:
    AndroidVoiceCallImpl(const voice_call_config_t* config, const voice_call_callbacks_t* callbacks)
        : config_(*config)
        , callbacks_(*callbacks)
        , state_(VOICE_CALL_STATE_IDLE)
        , muted_(false)
        , mic_volume_(1.0f)
        , speaker_volume_(1.0f)
        , running_(false)
        , engine_(nullptr)
        , engine_interface_(nullptr)
        , recorder_(nullptr)
        , recorder_interface_(nullptr)
        , player_(nullptr)
        , player_interface_(nullptr)
        , audio_buffer_(nullptr)
        , buffer_size_(0) {
        LOGI("Android voice call initialized for user: %s", config->user_id);
        InitializeAudio();
    }
    
    ~AndroidVoiceCallImpl() {
        Disconnect();
        StopAudioCapture();
        
        // 清理播放器资源
        if (player_interface_) {
            (*player_interface_)->SetPlayState(player_interface_, SL_PLAYSTATE_STOPPED);
        }
        if (player_) {
            (*player_)->Destroy(player_);
            player_ = nullptr;
            player_interface_ = nullptr;
            player_buffer_queue_ = nullptr;
        }
        
        if (output_mix_) {
            (*output_mix_)->Destroy(output_mix_);
            output_mix_ = nullptr;
        }
        
        if (audio_buffer_) {
            delete[] audio_buffer_;
            audio_buffer_ = nullptr;
        }
        
        if (engine_) {
            (*engine_)->Destroy(engine_);
            engine_ = nullptr;
        }
        
        LOGI("Android voice call destroyed");
    }
    
    voice_call_error_t Connect() {
        if (state_ != VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_ERROR_ALREADY_IN_CALL;
        }
        
        SetState(VOICE_CALL_STATE_CONNECTING);
        
        // 解析服务器地址
        std::string server_url = config_.server_url;
        std::string host;
        int port = 8080;
        
        // 处理 udp:// 前缀
        if (server_url.find("udp://") == 0) {
            server_url = server_url.substr(6);
        }
        
        // 解析主机和端口
        size_t colon_pos = server_url.find(':');
        if (colon_pos != std::string::npos) {
            host = server_url.substr(0, colon_pos);
            std::string port_str = server_url.substr(colon_pos + 1);
            if (!port_str.empty()) {
                try {
                    port = std::stoi(port_str);
                } catch (const std::exception& e) {
                    LOGE("Invalid port number: %s", port_str.c_str());
                    SetState(VOICE_CALL_STATE_ERROR);
                    return VOICE_CALL_ERROR_NETWORK;
                }
            }
        } else {
            host = server_url;
        }
        
        LOGI("Connecting to server: %s:%d", host.c_str(), port);
        
        // 创建UDP socket
        LOGI("Creating UDP socket...");
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            LOGE("Failed to create socket: %s", strerror(errno));
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        LOGI("UDP socket created successfully: %d", socket_fd_);
        
        // 设置服务器地址
        LOGI("Setting up server address...");
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port);
        server_addr_.sin_addr.s_addr = inet_addr(host.c_str());
        
        if (server_addr_.sin_addr.s_addr == INADDR_NONE) {
            LOGE("Invalid server address: %s", host.c_str());
            close(socket_fd_);
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        LOGI("Server address set successfully: %s:%d", inet_ntoa(server_addr_.sin_addr), ntohs(server_addr_.sin_port));
        
        // 发送JOIN消息
        std::string join_msg = "JOIN:" + std::string(config_.room_id) + ":" + std::string(config_.user_id);
        LOGI("Preparing to send JOIN message: '%s' (length=%zu)", join_msg.c_str(), join_msg.length());
        LOGI("Server address: %s:%d", inet_ntoa(server_addr_.sin_addr), ntohs(server_addr_.sin_port));
        
        ssize_t sent = sendto(socket_fd_, join_msg.c_str(), join_msg.length(), 0,
                             (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        
        if (sent < 0) {
            LOGE("Failed to send JOIN message: %s", strerror(errno));
            close(socket_fd_);
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        LOGI("JOIN message sent successfully: %s (sent=%zd bytes)", join_msg.c_str(), sent);
        
        // 等待一段时间确保JOIN消息被发送
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 启动接收线程
        running_ = true;
        std::thread([this]() {
            char buffer[2048];  // 增加缓冲区大小
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            bool joined = false;
            
            while (running_) {
                ssize_t received = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                          (struct sockaddr*)&from_addr, &from_len);
                if (received > 0) {
                    buffer[received] = '\0';
                    LOGI("Received: '%s' (length=%zd)", buffer, received);
                    
                    // 显示前32个字节的十六进制内容
                    if (received > 0) {
                        std::string hex_str = "Hex: ";
                        for (size_t i = 0; i < std::min(received, (ssize_t)32); i++) {
                            char hex[4];
                            snprintf(hex, sizeof(hex), "%02x ", (unsigned char)buffer[i]);
                            hex_str += hex;
                        }
                        LOGI("%s", hex_str.c_str());
                    }
                    
                    // 处理服务器响应
                    if (strncmp(buffer, "JOIN_OK", 7) == 0) {
                        SetState(VOICE_CALL_STATE_CONNECTED);
                        joined = true;
                        LOGI("Successfully connected to server");
                        // 等待少量时间再启动音频采集，减少延迟
                        LOGI("Waiting 100ms before starting audio capture...");
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        LOGI("Starting audio capture...");
                        StartAudioCapture();
                    } else if (strncmp(buffer, "JOIN_FAIL", 9) == 0) {
                        SetState(VOICE_CALL_STATE_ERROR);
                        LOGE("Failed to join room");
                        break;
                    } else if (received >= 654 && joined) {
                        // 只有在成功加入房间后才处理音频数据包
                        LOGI("Received audio packet, playing...");
                        // 添加调试信息
                        static auto last_packet_log = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        if (now - last_packet_log > std::chrono::seconds(5)) {
                            LOGI("Audio packet debug: received=%zd bytes, expected=654 bytes", received);
                            last_packet_log = now;
                        }
                        PlayAudioData(buffer, received);
                    } else if (strlen(buffer) == 0) {
                        LOGI("Received empty response from server");
                    } else {
                        LOGI("Received unknown response: '%s'", buffer);
                    }
                }
            }
            
            close(socket_fd_);
        }).detach();
        
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t Disconnect() {
        if (state_ == VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_SUCCESS;
        }
        
        running_ = false;
        StopAudioCapture();
        
        // 发送LEAVE消息给服务器
        if (state_ == VOICE_CALL_STATE_CONNECTED && socket_fd_ >= 0) {
            std::string leave_msg = "LEAVE:" + std::string(config_.room_id) + ":" + std::string(config_.user_id);
            int sent = sendto(socket_fd_, leave_msg.c_str(), leave_msg.length(), 0,
                             (struct sockaddr*)&server_addr_, sizeof(server_addr_));
            if (sent > 0) {
                LOGI("LEAVE message sent successfully: %s (sent=%d bytes)", leave_msg.c_str(), sent);
            } else {
                LOGE("Failed to send LEAVE message: %s", leave_msg.c_str());
            }
        }
        
        // 关闭socket
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        SetState(VOICE_CALL_STATE_DISCONNECTED);
        LOGI("Disconnected from server");
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_state_t GetState() const {
        return state_;
    }
    
    voice_call_error_t SetMuted(bool muted) {
        muted_ = muted;
        LOGI("Microphone %s", muted ? "muted" : "unmuted");
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
        LOGI("Microphone volume set to: %f", volume);
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t SetSpeakerVolume(float volume) {
        if (volume < 0.0f || volume > 1.0f) {
            return VOICE_CALL_ERROR_INVALID_PARAM;
        }
        speaker_volume_ = volume;
        LOGI("Speaker volume set to: %f", volume);
        return VOICE_CALL_SUCCESS;
    }

private:
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
    
    bool InitializeAudio() {
        // 创建OpenSL ES引擎
        SLresult result = slCreateEngine(&engine_, 0, nullptr, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create OpenSL ES engine");
            return false;
        }
        
        result = (*engine_)->Realize(engine_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize engine");
            return false;
        }
        
        result = (*engine_)->GetInterface(engine_, SL_IID_ENGINE, &engine_interface_);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get engine interface");
            return false;
        }
        
        // 配置音频参数
        int sample_rate = config_.audio_config.sample_rate;
        int channels = config_.audio_config.channels;
        int bits_per_sample = config_.audio_config.bits_per_sample;
        int frame_size = config_.audio_config.frame_size;
        
        buffer_size_ = sample_rate * channels * bits_per_sample / 8 * frame_size / 1000;
        audio_buffer_ = new int16_t[buffer_size_ / 2];
        
        LOGI("Audio initialized: %d Hz, %d ch, %d bits, buffer=%zu bytes", 
             sample_rate, channels, bits_per_sample, buffer_size_);
        
        // 创建输出混音器
        result = (*engine_interface_)->CreateOutputMix(engine_interface_, &output_mix_, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create output mix");
            return false;
        }
        
        result = (*output_mix_)->Realize(output_mix_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize output mix");
            return false;
        }
        
        // 创建音频播放器
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, output_mix_};
        SLDataSink audio_sink = {&loc_outmix, nullptr};
        
        SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
        SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(channels),
            static_cast<SLuint32>(sample_rate * 1000), // OpenSL ES使用毫赫兹
            static_cast<SLuint32>(bits_per_sample),
            static_cast<SLuint32>(bits_per_sample),
            channels == 1 ? (SL_SPEAKER_FRONT_CENTER) : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource audio_src = {&loc_bq, &format_pcm};
        
        const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        
        result = (*engine_interface_)->CreateAudioPlayer(engine_interface_, &player_, &audio_src, 
                                                        &audio_sink, 1, id, req);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create audio player");
            return false;
        }
        
        result = (*player_)->Realize(player_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize player");
            return false;
        }
        
        result = (*player_)->GetInterface(player_, SL_IID_PLAY, &player_interface_);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get player interface");
            return false;
        }
        
        // 获取缓冲区队列接口
        result = (*player_)->GetInterface(player_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &player_buffer_queue_);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get player buffer queue interface");
            return false;
        }
        
        // 设置缓冲区队列回调
        result = (*player_buffer_queue_)->RegisterCallback(player_buffer_queue_, 
                                                          AudioPlaybackCallback, this);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to register player buffer queue callback");
            return false;
        }
        
        // 设置播放状态为播放
        result = (*player_interface_)->SetPlayState(player_interface_, SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to set player state to playing");
            return false;
        }
        
        LOGI("Audio player initialized successfully");
        return true;
    }
    
    void StartAudioCapture() {
        LOGI("StartAudioCapture called");
        if (!engine_interface_) {
            LOGE("Audio not initialized");
            return;
        }
        
        // 配置录音器
        SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, 
                                         SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
        SLDataSource audio_src = {&loc_dev, nullptr};
        
        SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
        SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(config_.audio_config.channels),
            static_cast<SLuint32>(config_.audio_config.sample_rate * 1000), // OpenSL ES使用毫赫兹
            static_cast<SLuint32>(config_.audio_config.bits_per_sample),
            static_cast<SLuint32>(config_.audio_config.bits_per_sample),
            config_.audio_config.channels == 1 ? (SL_SPEAKER_FRONT_CENTER) : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSink audio_sink = {&loc_bq, &format_pcm};
        
        const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        
        SLresult result = (*engine_interface_)->CreateAudioRecorder(engine_interface_, &recorder_, &audio_src, 
                                                                   &audio_sink, 1, id, req);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to create audio recorder");
            return;
        }
        
        result = (*recorder_)->Realize(recorder_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to realize recorder");
            return;
        }
        
        result = (*recorder_)->GetInterface(recorder_, SL_IID_RECORD, &recorder_interface_);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get recorder interface");
            return;
        }
        
        // 设置录音回调
        SLAndroidSimpleBufferQueueItf buffer_queue;
        result = (*recorder_)->GetInterface(recorder_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &buffer_queue);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to get buffer queue interface");
            return;
        }
        
        result = (*buffer_queue)->RegisterCallback(buffer_queue, AudioRecordCallback, this);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to register record callback");
            return;
        }
        
        // 开始录音
        LOGI("Setting record state to RECORDING...");
        result = (*recorder_interface_)->SetRecordState(recorder_interface_, SL_RECORDSTATE_RECORDING);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to start recording: %d", result);
            return;
        }
        
        // 入队第一个缓冲区
        LOGI("Enqueuing first buffer...");
        result = (*buffer_queue)->Enqueue(buffer_queue, audio_buffer_, buffer_size_);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to enqueue buffer: %d", result);
            return;
        }
        
        LOGI("Audio capture started successfully");
    }
    
    void StopAudioCapture() {
        if (recorder_interface_) {
            (*recorder_interface_)->SetRecordState(recorder_interface_, SL_RECORDSTATE_STOPPED);
        }
        if (recorder_) {
            (*recorder_)->Destroy(recorder_);
            recorder_ = nullptr;
            recorder_interface_ = nullptr;
        }
        LOGI("Audio capture stopped");
    }
    
    static void AudioRecordCallback(SLAndroidSimpleBufferQueueItf caller, void* context) {
        AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(context);
        if (impl && !impl->muted_) {
            // 处理录制的音频数据
            LOGI("Audio captured: %zu bytes", impl->buffer_size_);
            
            // 发送音频数据到服务器
            impl->SendAudioData(impl->audio_buffer_, impl->buffer_size_);
        }
        
        // 重新入队缓冲区
        if (caller && impl) {
            (*caller)->Enqueue(caller, impl->audio_buffer_, impl->buffer_size_);
        }
    }
    
    void PlayAudioData(const char* data, size_t length) {
        LOGI("PlayAudioData called: length=%zu, player=%p, buffer_queue=%p", 
             length, player_, player_buffer_queue_);
        if (!player_ || !player_buffer_queue_) {
            LOGI("Audio player not initialized, skipping playback");
            return;
        }
        
        // 使用与Linux客户端兼容的AudioPacket格式
        struct AudioPacket {
            uint32_t sequence;
            uint32_t timestamp;
            uint32_t user_id;
            uint16_t data_size;
            uint8_t data[1024];
        } __attribute__((packed));
        
        // 解析音频包头部
        if (length < sizeof(AudioPacket) - sizeof(AudioPacket::data)) {
            LOGE("Audio packet too short: %zu bytes", length);
            return;
        }
        
        const AudioPacket* packet = reinterpret_cast<const AudioPacket*>(data);
        uint32_t sequence = ntohl(packet->sequence);
        uint32_t timestamp = ntohl(packet->timestamp);
        uint32_t user_id = ntohl(packet->user_id);
        uint16_t data_size = ntohs(packet->data_size);
        
        // 添加调试信息
        static auto last_debug_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last_debug_log > std::chrono::seconds(1)) {
            LOGI("Audio packet debug: sequence=%u, timestamp=%u, user_id=0x%x, data_size=%u bytes", 
                 sequence, timestamp, user_id, data_size);
            last_debug_log = now;
        }
        
        // 验证包大小
        int expected_size = sizeof(AudioPacket) - sizeof(AudioPacket::data) + data_size;
        if (length != expected_size) {
            LOGE("Audio packet size mismatch: expected %d, got %zu", expected_size, length);
            return;
        }
        
        // 添加更详细的调试信息
        static auto last_detail_log = std::chrono::steady_clock::now();
        auto now_detail = std::chrono::steady_clock::now();
        if (now_detail - last_detail_log > std::chrono::seconds(5)) {
            LOGI("Audio packet detail: length=%zu, expected=%d, data_size=%u, header_size=%zu", 
                 length, expected_size, data_size, sizeof(AudioPacket) - sizeof(AudioPacket::data));
            last_detail_log = now_detail;
        }
        
        // 获取音频数据
        const int16_t* audio_data = reinterpret_cast<const int16_t*>(packet->data);
        
        // 直接复制音频数据并应用音量（音频数据已经是小端序格式）
        std::vector<int16_t> converted_audio(data_size / sizeof(int16_t));
        
        // 检查音频数据是否有效
        bool has_valid_audio = false;
        for (size_t i = 0; i < converted_audio.size(); ++i) {
            converted_audio[i] = static_cast<int16_t>(audio_data[i] * speaker_volume_);
            if (audio_data[i] != 0) {
                has_valid_audio = true;
            }
        }
        
        // 如果没有有效音频数据，跳过播放
        if (!has_valid_audio) {
            static auto last_skip_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_skip_log > std::chrono::seconds(5)) {
                LOGI("Skipping silent audio packet");
                last_skip_log = now;
            }
            return;
        }
        
        // 添加音频数据调试信息
        static auto last_audio_debug = std::chrono::steady_clock::now();
        auto now_debug = std::chrono::steady_clock::now();
        if (now_debug - last_audio_debug > std::chrono::seconds(10)) {
            LOGI("Audio data debug: first_sample=%d, last_sample=%d, samples_count=%zu", 
                 converted_audio[0], converted_audio[converted_audio.size()-1], converted_audio.size());
            last_audio_debug = now_debug;
        }
        
        // 将音频数据发送到播放器（data_size是字节数，直接使用）
        SLresult result = (*player_buffer_queue_)->Enqueue(player_buffer_queue_, converted_audio.data(), data_size);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("Failed to enqueue audio data for playback: %d", result);
            // 如果缓冲区满了，等待一下再重试
            if (result == SL_RESULT_BUFFER_INSUFFICIENT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                result = (*player_buffer_queue_)->Enqueue(player_buffer_queue_, converted_audio.data(), data_size);
                if (result != SL_RESULT_SUCCESS) {
                    LOGE("Retry failed to enqueue audio data: %d", result);
                } else {
                    LOGI("Retry succeeded to enqueue audio data");
                }
            }
        } else {
            static auto last_play_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_play_log > std::chrono::seconds(5)) {
                LOGI("Audio playback: sequence=%u, timestamp=%u, size=%u bytes", 
                     sequence, timestamp, data_size);
                last_play_log = now;
            }
        }
    }
    
    void SendAudioData(const int16_t* audio_data, size_t length) {
        if (!running_ || muted_ || state_ != VOICE_CALL_STATE_CONNECTED) {
            return;
        }
        
        // 使用与Linux客户端兼容的AudioPacket格式
        struct AudioPacket {
            uint32_t sequence;
            uint32_t timestamp;
            uint32_t user_id;
            uint16_t data_size;
            uint8_t data[1024];
        } __attribute__((packed));
        
        static uint32_t sequence = 0;
        
        // 限制音频数据大小（以字节为单位），与Linux客户端保持一致
        size_t data_bytes = length * sizeof(int16_t);
        if (data_bytes > 640) {  // 与Linux客户端保持一致
            data_bytes = 640;
            length = data_bytes / sizeof(int16_t);
        }
        
        // 添加调试日志
        static auto last_debug_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last_debug_log > std::chrono::seconds(5)) {
            LOGI("Audio debug: length_samples=%zu, data_bytes=%zu, sizeof(int16_t)=%zu", 
                 length, data_bytes, sizeof(int16_t));
            last_debug_log = now;
        }
        
        AudioPacket packet;
        packet.sequence = htonl(sequence++);
        packet.timestamp = htonl(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        packet.user_id = htonl(0x12345678); // 用户ID，暂时固定
        packet.data_size = htons(data_bytes);
        
        // 复制音频数据
        memcpy(packet.data, audio_data, data_bytes);
        
        // 计算包大小
        int packet_size = sizeof(AudioPacket) - sizeof(AudioPacket::data) + data_bytes;
        
        // 发送到服务器
        ssize_t sent = sendto(socket_fd_, &packet, packet_size, 0,
                             (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        
        if (sent > 0) {
            static auto last_send_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_send_log > std::chrono::seconds(5)) {
                LOGI("Audio packet sent: sequence=%u, timestamp=%u, size=%d, sent=%zd", 
                     sequence-1, ntohl(packet.timestamp), packet_size, sent);
                last_send_log = now;
            }
        } else {
            LOGE("Failed to send audio packet: %s", strerror(errno));
        }
    }
    
    voice_call_config_t config_;
    voice_call_callbacks_t callbacks_;
    std::atomic<voice_call_state_t> state_;
    std::atomic<bool> muted_;
    float mic_volume_;
    float speaker_volume_;
    std::atomic<bool> running_;
    
    // OpenSL ES音频相关
    SLObjectItf engine_;
    SLEngineItf engine_interface_;
    SLObjectItf output_mix_;
    SLObjectItf recorder_;
    SLRecordItf recorder_interface_;
    SLObjectItf player_;
    SLPlayItf player_interface_;
    SLAndroidSimpleBufferQueueItf player_buffer_queue_;
    int16_t* audio_buffer_;
    size_t buffer_size_;
    
    // 网络相关
    int socket_fd_;
    struct sockaddr_in server_addr_;
};

// C API实现
extern "C" {

voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks) {
    if (!config || !callbacks) {
        return nullptr;
    }
    
    try {
        return new AndroidVoiceCallImpl(config, callbacks);
    } catch (const std::exception& e) {
        LOGE("Failed to create AndroidVoiceCallImpl: %s", e.what());
        return nullptr;
    }
}

voice_call_error_t voice_call_connect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->Connect();
}

voice_call_error_t voice_call_disconnect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->Disconnect();
}

voice_call_state_t voice_call_get_state(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
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
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->SetMicrophoneVolume(volume);
}

voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->SetSpeakerVolume(volume);
}

voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->SetMuted(muted);
}

bool voice_call_is_muted(voice_call_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
    return impl->IsMuted();
}

void voice_call_destroy(voice_call_handle_t handle) {
    if (handle) {
        AndroidVoiceCallImpl* impl = static_cast<AndroidVoiceCallImpl*>(handle);
        delete impl;
    }
}

const char* voice_call_get_version(void) {
    return "1.0.0 (Android)";
}

// 音频播放回调函数
extern "C" void AudioPlaybackCallback(SLAndroidSimpleBufferQueueItf caller, void* context) {
    // 这个回调在音频播放完成后被调用
    // 我们可以在这里添加一些统计信息
    static int callback_count = 0;
    callback_count++;
    
    if (callback_count % 100 == 0) {
        LOGI("Audio playback callback called %d times", callback_count);
    }
}

}
