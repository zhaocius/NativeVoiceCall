#ifndef VOICE_CALL_H
#define VOICE_CALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// 错误码定义
typedef enum {
    VOICE_CALL_SUCCESS = 0,
    VOICE_CALL_ERROR_INVALID_PARAM = -1,
    VOICE_CALL_ERROR_INIT_FAILED = -2,
    VOICE_CALL_ERROR_NETWORK = -3,
    VOICE_CALL_ERROR_AUDIO = -4,
    VOICE_CALL_ERROR_PEER_NOT_FOUND = -5,
    VOICE_CALL_ERROR_ALREADY_IN_CALL = -6
} voice_call_error_t;

// 通话状态
typedef enum {
    VOICE_CALL_STATE_IDLE = 0,
    VOICE_CALL_STATE_CONNECTING,
    VOICE_CALL_STATE_CONNECTED,
    VOICE_CALL_STATE_DISCONNECTED,
    VOICE_CALL_STATE_ERROR
} voice_call_state_t;

// 音频配置
typedef struct {
    int sample_rate;      // 采样率 (8000, 16000, 32000, 48000)
    int channels;         // 声道数 (1=单声道, 2=立体声)
    int bits_per_sample;  // 位深度 (16, 24, 32)
    int frame_size;       // 帧大小 (毫秒)
} voice_call_audio_config_t;

// 通话配置
typedef struct {
    char server_url[256];           // 信令服务器URL
    char room_id[64];               // 房间ID
    char user_id[64];               // 用户ID
    voice_call_audio_config_t audio_config;
    bool enable_echo_cancellation;  // 回声消除
    bool enable_noise_suppression;  // 噪声抑制
    bool enable_automatic_gain_control; // 自动增益控制
} voice_call_config_t;

// 通话事件回调
typedef struct {
    void (*on_state_changed)(voice_call_state_t state, const char* reason);
    void (*on_peer_joined)(const char* peer_id);
    void (*on_peer_left)(const char* peer_id);
    void (*on_audio_level)(const char* peer_id, float level);
    void (*on_error)(voice_call_error_t error, const char* message);
} voice_call_callbacks_t;

// 通话句柄
typedef void* voice_call_handle_t;

// 核心API函数

/**
 * 初始化语音通话库
 * @param config 通话配置
 * @param callbacks 事件回调函数
 * @return 通话句柄，失败返回NULL
 */
voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks);

/**
 * 连接到通话房间
 * @param handle 通话句柄
 * @return 错误码
 */
voice_call_error_t voice_call_connect(voice_call_handle_t handle);

/**
 * 断开通话连接
 * @param handle 通话句柄
 * @return 错误码
 */
voice_call_error_t voice_call_disconnect(voice_call_handle_t handle);

/**
 * 获取当前通话状态
 * @param handle 通话句柄
 * @return 通话状态
 */
voice_call_state_t voice_call_get_state(voice_call_handle_t handle);

/**
 * 设置音频输入设备
 * @param handle 通话句柄
 * @param device_name 设备名称，NULL表示使用默认设备
 * @return 错误码
 */
voice_call_error_t voice_call_set_audio_input_device(voice_call_handle_t handle, 
                                                    const char* device_name);

/**
 * 设置音频输出设备
 * @param handle 通话句柄
 * @param device_name 设备名称，NULL表示使用默认设备
 * @return 错误码
 */
voice_call_error_t voice_call_set_audio_output_device(voice_call_handle_t handle, 
                                                     const char* device_name);

/**
 * 设置麦克风音量
 * @param handle 通话句柄
 * @param volume 音量 (0.0 - 1.0)
 * @return 错误码
 */
voice_call_error_t voice_call_set_microphone_volume(voice_call_handle_t handle, float volume);

/**
 * 设置扬声器音量
 * @param handle 通话句柄
 * @param volume 音量 (0.0 - 1.0)
 * @return 错误码
 */
voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume);

/**
 * 静音/取消静音
 * @param handle 通话句柄
 * @param muted 是否静音
 * @return 错误码
 */
voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted);

/**
 * 获取是否静音
 * @param handle 通话句柄
 * @return 是否静音
 */
bool voice_call_is_muted(voice_call_handle_t handle);

/**
 * 销毁通话句柄
 * @param handle 通话句柄
 */
void voice_call_destroy(voice_call_handle_t handle);

/**
 * 获取库版本信息
 * @return 版本字符串
 */
const char* voice_call_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // VOICE_CALL_H
