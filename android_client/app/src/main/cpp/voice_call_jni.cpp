#include <jni.h>
#include "voice_call.h"
#include <android/log.h>

#define LOG_TAG "VoiceCallJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_initVoiceCall(
    JNIEnv* env, jobject thiz, jobject config, jobject callbacks) {
    
    // 简化的配置
    voice_call_config_t c_config = {};
    strcpy(c_config.server_url, "ws://localhost:8080");
    strcpy(c_config.room_id, "test_room");
    strcpy(c_config.user_id, "android_user");
    
    c_config.audio_config.sample_rate = 48000;
    c_config.audio_config.channels = 1;
    c_config.audio_config.bits_per_sample = 16;
    c_config.audio_config.frame_size = 20;
    
    c_config.enable_echo_cancellation = true;
    c_config.enable_noise_suppression = true;
    c_config.enable_automatic_gain_control = true;
    
    // 简化的回调
    voice_call_callbacks_t c_callbacks = {};
    
    voice_call_handle_t handle = voice_call_init(&c_config, &c_callbacks);
    LOGI("Voice call initialized: %p", handle);
    return reinterpret_cast<jlong>(handle);
}

JNIEXPORT jint JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_connect(JNIEnv* env, jobject thiz, jlong handle) {
    return voice_call_connect(reinterpret_cast<voice_call_handle_t>(handle));
}

JNIEXPORT jint JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_disconnect(JNIEnv* env, jobject thiz, jlong handle) {
    return voice_call_disconnect(reinterpret_cast<voice_call_handle_t>(handle));
}

JNIEXPORT jint JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_getState(JNIEnv* env, jobject thiz, jlong handle) {
    return static_cast<jint>(voice_call_get_state(reinterpret_cast<voice_call_handle_t>(handle)));
}

JNIEXPORT jint JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_setMuted(JNIEnv* env, jobject thiz, jlong handle, jboolean muted) {
    return voice_call_set_muted(reinterpret_cast<voice_call_handle_t>(handle), muted);
}

JNIEXPORT jboolean JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_isMuted(JNIEnv* env, jobject thiz, jlong handle) {
    return voice_call_is_muted(reinterpret_cast<voice_call_handle_t>(handle));
}

JNIEXPORT void JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_destroy(JNIEnv* env, jobject thiz, jlong handle) {
    voice_call_destroy(reinterpret_cast<voice_call_handle_t>(handle));
}

JNIEXPORT jstring JNICALL
Java_com_nativevoicecall_android_VoiceCallManager_getVersion(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF(voice_call_get_version());
}

}
