#include "voice_call.h"
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>

// WebRTC includes
#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/api/audio/audio_source.h>
#include <webrtc/api/audio/audio_track.h>
#include <webrtc/api/audio/audio_device_module.h>
#include <webrtc/api/audio/audio_processing.h>
#include <webrtc/modules/audio_device/include/audio_device.h>
#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/rtc_base/ssl_adapter.h>
#include <webrtc/rtc_base/network.h>
#include <webrtc/rtc_base/socket_address.h>
#include <webrtc/rtc_base/async_socket.h>
#include <webrtc/rtc_base/async_tcpsocket.h>
#include <webrtc/rtc_base/async_udpsocket.h>
#include <webrtc/rtc_base/socket_server.h>
#include <webrtc/rtc_base/physical_socket_server.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/rtc_base/ssl_adapter.h>
#include <webrtc/rtc_base/network.h>
#include <webrtc/rtc_base/socket_address.h>
#include <webrtc/rtc_base/async_socket.h>
#include <webrtc/rtc_base/async_tcpsocket.h>
#include <webrtc/rtc_base/async_udpsocket.h>
#include <webrtc/rtc_base/socket_server.h>
#include <webrtc/rtc_base/physical_socket_server.h>

// 简化的WebSocket客户端实现
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// WebRTC实现类
class WebRTCVoiceCallImpl : public webrtc::PeerConnectionObserver,
                           public webrtc::CreateSessionDescriptionObserver {
public:
    WebRTCVoiceCallImpl(const voice_call_config_t* config, const voice_call_callbacks_t* callbacks)
        : config_(*config)
        , callbacks_(*callbacks)
        , state_(VOICE_CALL_STATE_IDLE)
        , muted_(false)
        , mic_volume_(1.0f)
        , speaker_volume_(1.0f)
        , peer_connection_(nullptr)
        , audio_track_(nullptr)
        , audio_source_(nullptr)
        , audio_device_module_(nullptr)
        , audio_processing_(nullptr)
        , websocket_fd_(-1)
        , ssl_ctx_(nullptr)
        , ssl_(nullptr) {
        
        // 初始化WebRTC
        rtc::InitializeSSL();
        
        // 创建网络线程
        network_thread_ = rtc::Thread::CreateWithSocketServer();
        network_thread_->Start();
        
        // 创建工作线程
        worker_thread_ = rtc::Thread::Create();
        worker_thread_->Start();
        
        // 创建信令线程
        signaling_thread_ = rtc::Thread::Create();
        signaling_thread_->Start();
        
        // 创建PeerConnectionFactory
        webrtc::PeerConnectionFactoryDependencies dependencies;
        dependencies.network_thread = network_thread_.get();
        dependencies.worker_thread = worker_thread_.get();
        dependencies.signaling_thread = signaling_thread_.get();
        
        peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
        
        if (!peer_connection_factory_) {
            std::cerr << "Failed to create PeerConnectionFactory" << std::endl;
            return;
        }
        
        // 创建音频设备模块
        audio_device_module_ = webrtc::AudioDeviceModule::Create(
            webrtc::AudioDeviceModule::kPlatformDefaultAudio,
            nullptr);
        
        if (!audio_device_module_) {
            std::cerr << "Failed to create AudioDeviceModule" << std::endl;
            return;
        }
        
        // 初始化音频设备
        if (audio_device_module_->Init() != 0) {
            std::cerr << "Failed to initialize AudioDeviceModule" << std::endl;
            return;
        }
        
        // 创建音频处理模块
        webrtc::AudioProcessing::Config ap_config;
        ap_config.echo_canceller.enabled = config->enable_echo_cancellation;
        ap_config.noise_suppression.enabled = config->enable_noise_suppression;
        ap_config.gain_controller1.enabled = config->enable_automatic_gain_control;
        
        audio_processing_ = webrtc::AudioProcessing::Create(ap_config);
        
        if (!audio_processing_) {
            std::cerr << "Failed to create AudioProcessing" << std::endl;
            return;
        }
        
        // 创建音频源
        cricket::AudioOptions audio_options;
        audio_source_ = peer_connection_factory_->CreateAudioSource(audio_options);
        
        if (!audio_source_) {
            std::cerr << "Failed to create AudioSource" << std::endl;
            return;
        }
        
        // 创建音频轨道
        audio_track_ = peer_connection_factory_->CreateAudioTrack("audio", audio_source_);
        
        if (!audio_track_) {
            std::cerr << "Failed to create AudioTrack" << std::endl;
            return;
        }
        
        std::cout << "WebRTC VoiceCall initialized for user: " << config->user_id << std::endl;
    }
    
    ~WebRTCVoiceCallImpl() {
        Disconnect();
        
        if (audio_track_) {
            audio_track_->Release();
        }
        if (audio_source_) {
            audio_source_->Release();
        }
        if (audio_device_module_) {
            audio_device_module_->Terminate();
        }
        
        if (network_thread_) {
            network_thread_->Stop();
        }
        if (worker_thread_) {
            worker_thread_->Stop();
        }
        if (signaling_thread_) {
            signaling_thread_->Stop();
        }
        
        // 清理WebSocket连接
        if (ssl_) {
            SSL_free(ssl_);
        }
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
        }
        if (websocket_fd_ >= 0) {
            close(websocket_fd_);
        }
        
        rtc::CleanupSSL();
        std::cout << "WebRTC VoiceCall destroyed" << std::endl;
    }
    
    voice_call_error_t Connect() {
        if (state_ != VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_ERROR_ALREADY_IN_CALL;
        }
        
        SetState(VOICE_CALL_STATE_CONNECTING);
        
        // 连接到信令服务器
        if (!ConnectToSignalingServer()) {
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 加入房间
        if (!JoinRoom()) {
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_NETWORK;
        }
        
        // 创建PeerConnection
        if (!CreatePeerConnection()) {
            SetState(VOICE_CALL_STATE_ERROR);
            return VOICE_CALL_ERROR_INIT_FAILED;
        }
        
        // 启动信令处理线程
        signaling_thread_ = std::thread(&WebRTCVoiceCallImpl::HandleSignaling, this);
        
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t Disconnect() {
        if (state_ == VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_SUCCESS;
        }
        
        // 离开房间
        LeaveRoom();
        
        // 关闭PeerConnection
        if (peer_connection_) {
            peer_connection_->Close();
            peer_connection_ = nullptr;
        }
        
        // 停止信令线程
        if (signaling_thread_.joinable()) {
            signaling_thread_.join();
        }
        
        // 关闭WebSocket连接
        if (websocket_fd_ >= 0) {
            close(websocket_fd_);
            websocket_fd_ = -1;
        }
        
        SetState(VOICE_CALL_STATE_DISCONNECTED);
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_state_t GetState() const {
        return state_;
    }
    
    voice_call_error_t SetMuted(bool muted) {
        muted_ = muted;
        if (audio_track_) {
            audio_track_->set_enabled(!muted);
        }
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
    bool ConnectToSignalingServer() {
        // 解析服务器URL
        std::string url = config_.server_url;
        bool is_ssl = (url.find("wss://") == 0);
        std::string host = "localhost";
        int port = 8080;
        
        if (url.find("ws://") == 0) {
            url = url.substr(5);
        } else if (url.find("wss://") == 0) {
            url = url.substr(6);
        }
        
        size_t colon_pos = url.find(':');
        if (colon_pos != std::string::npos) {
            host = url.substr(0, colon_pos);
            port = std::stoi(url.substr(colon_pos + 1));
        }
        
        // 创建socket
        websocket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (websocket_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        // 设置非阻塞
        int flags = fcntl(websocket_fd_, F_GETFL, 0);
        fcntl(websocket_fd_, F_SETFL, flags | O_NONBLOCK);
        
        // 连接服务器
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(host.c_str());
        
        if (connect(websocket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            if (errno != EINPROGRESS) {
                std::cerr << "Failed to connect to server" << std::endl;
                return false;
            }
        }
        
        // 等待连接完成
        struct pollfd pfd;
        pfd.fd = websocket_fd_;
        pfd.events = POLLOUT;
        
        if (poll(&pfd, 1, 5000) <= 0) {
            std::cerr << "Connection timeout" << std::endl;
            return false;
        }
        
        // 如果是SSL连接，建立SSL
        if (is_ssl) {
            if (!SetupSSL()) {
                return false;
            }
        }
        
        // 发送WebSocket握手
        if (!SendWebSocketHandshake(host, port)) {
            return false;
        }
        
        std::cout << "Connected to signaling server" << std::endl;
        return true;
    }
    
    bool SetupSSL() {
        SSL_library_init();
        SSL_load_error_strings();
        
        ssl_ctx_ = SSL_CTX_new(SSLv23_client_method());
        if (!ssl_ctx_) {
            std::cerr << "Failed to create SSL context" << std::endl;
            return false;
        }
        
        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) {
            std::cerr << "Failed to create SSL" << std::endl;
            return false;
        }
        
        SSL_set_fd(ssl_, websocket_fd_);
        
        if (SSL_connect(ssl_) != 1) {
            std::cerr << "SSL connect failed" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool SendWebSocketHandshake(const std::string& host, int port) {
        std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // 示例key
        
        std::string handshake = 
            "GET / HTTP/1.1\r\n"
            "Host: " + host + ":" + std::to_string(port) + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + key + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        
        int sent = send(websocket_fd_, handshake.c_str(), handshake.length(), 0);
        if (sent != static_cast<int>(handshake.length())) {
            std::cerr << "Failed to send WebSocket handshake" << std::endl;
            return false;
        }
        
        // 读取响应
        char buffer[1024];
        int received = recv(websocket_fd_, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            std::cerr << "Failed to receive WebSocket response" << std::endl;
            return false;
        }
        
        buffer[received] = '\0';
        std::string response(buffer);
        
        if (response.find("101 Switching Protocols") == std::string::npos) {
            std::cerr << "WebSocket handshake failed" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool JoinRoom() {
        std::string message = 
            "{\"type\":\"join\",\"room_id\":\"" + std::string(config_.room_id) + 
            "\",\"user_id\":\"" + std::string(config_.user_id) + "\"}";
        
        return SendWebSocketMessage(message);
    }
    
    bool LeaveRoom() {
        std::string message = 
            "{\"type\":\"leave\",\"room_id\":\"" + std::string(config_.room_id) + 
            "\",\"user_id\":\"" + std::string(config_.user_id) + "\"}";
        
        return SendWebSocketMessage(message);
    }
    
    bool SendWebSocketMessage(const std::string& message) {
        // 简单的WebSocket帧格式
        std::vector<uint8_t> frame;
        frame.push_back(0x81); // FIN + text frame
        
        if (message.length() < 126) {
            frame.push_back(0x80 | message.length());
        } else if (message.length() < 65536) {
            frame.push_back(0x80 | 126);
            frame.push_back((message.length() >> 8) & 0xFF);
            frame.push_back(message.length() & 0xFF);
        } else {
            frame.push_back(0x80 | 127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((message.length() >> (i * 8)) & 0xFF);
            }
        }
        
        // 添加mask key
        uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78};
        for (int i = 0; i < 4; ++i) {
            frame.push_back(mask_key[i]);
        }
        
        // 添加payload
        for (size_t i = 0; i < message.length(); ++i) {
            frame.push_back(message[i] ^ mask_key[i % 4]);
        }
        
        int sent = send(websocket_fd_, frame.data(), frame.size(), 0);
        return sent == static_cast<int>(frame.size());
    }
    
    bool CreatePeerConnection() {
        // 创建PeerConnection配置
        webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
        rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
        
        // 添加STUN服务器
        webrtc::PeerConnectionInterface::IceServer stun_server;
        stun_server.urls.push_back("stun:stun.l.google.com:19302");
        rtc_config.servers.push_back(stun_server);
        
        // 创建PeerConnection
        peer_connection_ = peer_connection_factory_->CreatePeerConnection(
            rtc_config, nullptr, nullptr, this);
        
        if (!peer_connection_) {
            std::cerr << "Failed to create PeerConnection" << std::endl;
            return false;
        }
        
        // 添加音频轨道
        auto result = peer_connection_->AddTrack(audio_track_, {"audio"});
        if (!result.ok()) {
            std::cerr << "Failed to add audio track" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void HandleSignaling() {
        char buffer[4096];
        
        while (state_ != VOICE_CALL_STATE_DISCONNECTED) {
            struct pollfd pfd;
            pfd.fd = websocket_fd_;
            pfd.events = POLLIN;
            
            if (poll(&pfd, 1, 1000) > 0) {
                int received = recv(websocket_fd_, buffer, sizeof(buffer) - 1, 0);
                if (received > 0) {
                    buffer[received] = '\0';
                    ProcessSignalingMessage(std::string(buffer, received));
                } else if (received == 0) {
                    std::cout << "Server disconnected" << std::endl;
                    break;
                }
            }
        }
    }
    
    void ProcessSignalingMessage(const std::string& message) {
        // 简单的JSON解析（实际项目中应使用proper JSON库）
        if (message.find("\"type\":\"peer_joined\"") != std::string::npos) {
            // 处理用户加入
            if (callbacks_.on_peer_joined) {
                callbacks_.on_peer_joined("peer_user");
            }
        } else if (message.find("\"type\":\"peer_left\"") != std::string::npos) {
            // 处理用户离开
            if (callbacks_.on_peer_left) {
                callbacks_.on_peer_left("peer_user");
            }
        } else if (message.find("\"type\":\"offer\"") != std::string::npos) {
            // 处理offer
            HandleOffer(message);
        } else if (message.find("\"type\":\"answer\"") != std::string::npos) {
            // 处理answer
            HandleAnswer(message);
        } else if (message.find("\"type\":\"ice_candidate\"") != std::string::npos) {
            // 处理ICE候选
            HandleIceCandidate(message);
        }
    }
    
    void HandleOffer(const std::string& message) {
        // 解析SDP并设置远程描述
        // 这里简化处理，实际需要解析JSON
        std::cout << "Received offer" << std::endl;
    }
    
    void HandleAnswer(const std::string& message) {
        // 解析SDP并设置远程描述
        std::cout << "Received answer" << std::endl;
    }
    
    void HandleIceCandidate(const std::string& message) {
        // 解析ICE候选并添加
        std::cout << "Received ICE candidate" << std::endl;
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
    
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source_;
    rtc::scoped_refptr<webrtc::AudioDeviceModule> audio_device_module_;
    std::unique_ptr<webrtc::AudioProcessing> audio_processing_;
    
    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    
    // WebSocket相关
    int websocket_fd_;
    SSL_CTX* ssl_ctx_;
    SSL* ssl_;
    
    // PeerConnectionObserver implementation
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
        switch (new_state) {
            case webrtc::PeerConnectionInterface::kIceConnectionConnected:
                SetState(VOICE_CALL_STATE_CONNECTED);
                break;
            case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
                SetState(VOICE_CALL_STATE_DISCONNECTED);
                break;
            case webrtc::PeerConnectionInterface::kIceConnectionFailed:
                SetState(VOICE_CALL_STATE_ERROR);
                break;
            default:
                break;
        }
    }
    
    void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                   const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override {
        auto track = receiver->track();
        if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
            std::cout << "Remote audio track added" << std::endl;
        }
    }
    
    void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
        auto track = receiver->track();
        if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
            std::cout << "Remote audio track removed" << std::endl;
        }
    }
    
    // CreateSessionDescriptionObserver implementation
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        peer_connection_->SetLocalDescription(this, desc);
    }
    
    void OnFailure(webrtc::RTCError error) override {
        std::cerr << "Failed to create session description: " << error.message() << std::endl;
        SetState(VOICE_CALL_STATE_ERROR);
    }
    
    void OnSetSessionDescriptionSuccess() override {
        std::cout << "Local description set successfully" << std::endl;
    }
    
    void OnSetSessionDescriptionFailure(webrtc::RTCError error) override {
        std::cerr << "Failed to set session description: " << error.message() << std::endl;
        SetState(VOICE_CALL_STATE_ERROR);
    }
};

// C API实现
extern "C" {

voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks) {
    if (!config || !callbacks) {
        return nullptr;
    }
    
    try {
        return new WebRTCVoiceCallImpl(config, callbacks);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create WebRTCVoiceCallImpl: " << e.what() << std::endl;
        return nullptr;
    }
}

voice_call_error_t voice_call_connect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->Connect();
}

voice_call_error_t voice_call_disconnect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->Disconnect();
}

voice_call_state_t voice_call_get_state(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
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
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->SetMicrophoneVolume(volume);
}

voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->SetSpeakerVolume(volume);
}

voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->SetMuted(muted);
}

bool voice_call_is_muted(voice_call_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
    return impl->IsMuted();
}

void voice_call_destroy(voice_call_handle_t handle) {
    if (handle) {
        WebRTCVoiceCallImpl* impl = static_cast<WebRTCVoiceCallImpl*>(handle);
        delete impl;
    }
}

const char* voice_call_get_version(void) {
    return "1.0.0 (WebRTC)";
}

}

