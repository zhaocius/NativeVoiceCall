#include "voice_call.h"
#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

// 简化的实现类
class VoiceCallImpl {
public:
    VoiceCallImpl(const voice_call_config_t* config, const voice_call_callbacks_t* callbacks)
        : config_(*config)
        , callbacks_(*callbacks)
        , state_(VOICE_CALL_STATE_IDLE)
        , muted_(false)
        , mic_volume_(1.0f)
        , speaker_volume_(1.0f) {
        std::cout << "VoiceCall initialized for user: " << config->user_id << std::endl;
    }
    
    ~VoiceCallImpl() {
        Disconnect();
        std::cout << "VoiceCall destroyed" << std::endl;
    }
    
    voice_call_error_t Connect() {
        if (state_ != VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_ERROR_ALREADY_IN_CALL;
        }
        
        SetState(VOICE_CALL_STATE_CONNECTING);
        
        // 模拟连接过程
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            SetState(VOICE_CALL_STATE_CONNECTED);
            
            // 模拟其他用户加入
            if (callbacks_.on_peer_joined) {
                callbacks_.on_peer_joined("peer_user");
            }
        }).detach();
        
        return VOICE_CALL_SUCCESS;
    }
    
    voice_call_error_t Disconnect() {
        if (state_ == VOICE_CALL_STATE_IDLE) {
            return VOICE_CALL_SUCCESS;
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
};

// C API实现
extern "C" {

voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks) {
    if (!config || !callbacks) {
        return nullptr;
    }
    
    try {
        return new VoiceCallImpl(config, callbacks);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create VoiceCallImpl: " << e.what() << std::endl;
        return nullptr;
    }
}

voice_call_error_t voice_call_connect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->Connect();
}

voice_call_error_t voice_call_disconnect(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->Disconnect();
}

voice_call_state_t voice_call_get_state(voice_call_handle_t handle) {
    if (!handle) {
        return VOICE_CALL_STATE_ERROR;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
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
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->SetMicrophoneVolume(volume);
}

voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->SetSpeakerVolume(volume);
}

voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted) {
    if (!handle) {
        return VOICE_CALL_ERROR_INVALID_PARAM;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->SetMuted(muted);
}

bool voice_call_is_muted(voice_call_handle_t handle) {
    if (!handle) {
        return false;
    }
    
    VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
    return impl->IsMuted();
}

void voice_call_destroy(voice_call_handle_t handle) {
    if (handle) {
        VoiceCallImpl* impl = static_cast<VoiceCallImpl*>(handle);
        delete impl;
    }
}

const char* voice_call_get_version(void) {
    return "1.0.0";
}

}
