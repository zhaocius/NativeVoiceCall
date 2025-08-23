# NativeVoiceCall API 参考文档

## 概述

NativeVoiceCall 是一个跨平台的语音通话库，支持 Linux 和 Android 平台。

## 核心API

### 初始化和销毁

```c
// 初始化语音通话
voice_call_handle_t voice_call_init(const voice_call_config_t* config, 
                                   const voice_call_callbacks_t* callbacks);

// 销毁通话句柄
void voice_call_destroy(voice_call_handle_t handle);
```

### 连接管理

```c
// 连接到通话
voice_call_error_t voice_call_connect(voice_call_handle_t handle);

// 断开连接
voice_call_error_t voice_call_disconnect(voice_call_handle_t handle);

// 获取状态
voice_call_state_t voice_call_get_state(voice_call_handle_t handle);
```

### 音频控制

```c
// 设置静音
voice_call_error_t voice_call_set_muted(voice_call_handle_t handle, bool muted);

// 获取静音状态
bool voice_call_is_muted(voice_call_handle_t handle);

// 设置音量
voice_call_error_t voice_call_set_microphone_volume(voice_call_handle_t handle, float volume);
voice_call_error_t voice_call_set_speaker_volume(voice_call_handle_t handle, float volume);
```

## 数据结构

### 配置结构

```c
typedef struct {
    char server_url[256];           // 服务器URL
    char room_id[64];               // 房间ID
    char user_id[64];               // 用户ID
    voice_call_audio_config_t audio_config;
    bool enable_echo_cancellation;  // 回声消除
    bool enable_noise_suppression;  // 噪声抑制
    bool enable_automatic_gain_control; // 自动增益控制
} voice_call_config_t;
```

### 回调结构

```c
typedef struct {
    void (*on_state_changed)(voice_call_state_t state, const char* reason);
    void (*on_peer_joined)(const char* peer_id);
    void (*on_peer_left)(const char* peer_id);
    void (*on_audio_level)(const char* peer_id, float level);
    void (*on_error)(voice_call_error_t error, const char* message);
} voice_call_callbacks_t;
```

## 使用示例

```c
#include "voice_call.h"

// 回调函数
void on_state_changed(voice_call_state_t state, const char* reason) {
    printf("状态: %d - %s\n", state, reason);
}

void on_peer_joined(const char* peer_id) {
    printf("用户加入: %s\n", peer_id);
}

int main() {
    // 配置
    voice_call_config_t config = {};
    strcpy(config.server_url, "ws://localhost:8080");
    strcpy(config.room_id, "test_room");
    strcpy(config.user_id, "test_user");
    
    // 回调
    voice_call_callbacks_t callbacks = {};
    callbacks.on_state_changed = on_state_changed;
    callbacks.on_peer_joined = on_peer_joined;
    
    // 初始化
    voice_call_handle_t handle = voice_call_init(&config, &callbacks);
    
    // 连接
    voice_call_connect(handle);
    
    // 等待
    sleep(10);
    
    // 清理
    voice_call_disconnect(handle);
    voice_call_destroy(handle);
    
    return 0;
}
```
