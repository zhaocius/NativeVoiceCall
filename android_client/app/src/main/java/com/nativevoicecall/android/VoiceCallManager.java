package com.nativevoicecall.android;

import android.util.Log;

public class VoiceCallManager {
    private static final String TAG = "VoiceCallManager";
    
    // 加载native库
    static {
        try {
            System.loadLibrary("voice_call");
            Log.d(TAG, "Native library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library", e);
            throw e; // 重新抛出异常，让应用崩溃以便调试
        }
    }
    
    // Native方法声明
    public native long initVoiceCall(String serverUrl, String roomId, String userId);
    public native int connect(long handle);
    public native int disconnect(long handle);
    public native int getState(long handle);
    public native int setMuted(long handle, boolean muted);
    public native boolean isMuted(long handle);
    public native void destroy(long handle);
    public native String getVersion();
    
    // 状态常量
    public static final int STATE_IDLE = 0;
    public static final int STATE_CONNECTING = 1;
    public static final int STATE_CONNECTED = 2;
    public static final int STATE_DISCONNECTED = 3;
    public static final int STATE_ERROR = 4;
    
    // 错误常量
    public static final int ERROR_SUCCESS = 0;
    public static final int ERROR_INVALID_PARAM = -1;
    public static final int ERROR_NETWORK = -2;
    public static final int ERROR_AUDIO = -3;
    
    private long nativeHandle = 0;
    
    public boolean initialize(String serverUrl, String roomId, String userId) {
        try {
            nativeHandle = initVoiceCall(serverUrl, roomId, userId);
            Log.d(TAG, "Voice call initialized with handle: " + nativeHandle);
            return nativeHandle != 0;
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize voice call", e);
            return false;
        }
    }
    
    public boolean connect() {
        if (nativeHandle == 0) {
            Log.e(TAG, "Voice call not initialized");
            return false;
        }
        
        try {
            int result = connect(nativeHandle);
            Log.d(TAG, "Connect result: " + result);
            return result == ERROR_SUCCESS;
        } catch (Exception e) {
            Log.e(TAG, "Failed to connect", e);
            return false;
        }
    }
    
    public boolean disconnect() {
        if (nativeHandle == 0) {
            return false;
        }
        
        try {
            int result = disconnect(nativeHandle);
            Log.d(TAG, "Disconnect result: " + result);
            return result == ERROR_SUCCESS;
        } catch (Exception e) {
            Log.e(TAG, "Failed to disconnect", e);
            return false;
        }
    }
    
    public int getState() {
        if (nativeHandle == 0) {
            return STATE_IDLE;
        }
        
        try {
            return getState(nativeHandle);
        } catch (Exception e) {
            Log.e(TAG, "Failed to get state", e);
            return STATE_ERROR;
        }
    }
    
    public boolean setMuted(boolean muted) {
        if (nativeHandle == 0) {
            return false;
        }
        
        try {
            int result = setMuted(nativeHandle, muted);
            return result == ERROR_SUCCESS;
        } catch (Exception e) {
            Log.e(TAG, "Failed to set muted", e);
            return false;
        }
    }
    
    public boolean isMuted() {
        if (nativeHandle == 0) {
            return false;
        }
        
        try {
            return isMuted(nativeHandle);
        } catch (Exception e) {
            Log.e(TAG, "Failed to get muted state", e);
            return false;
        }
    }
    
    public void destroy() {
        if (nativeHandle != 0) {
            try {
                destroy(nativeHandle);
                Log.d(TAG, "Voice call destroyed");
            } catch (Exception e) {
                Log.e(TAG, "Failed to destroy voice call", e);
            }
            nativeHandle = 0;
        }
    }
    
    public String getNativeVersion() {
        try {
            return getVersion();
        } catch (Exception e) {
            Log.e(TAG, "Failed to get version", e);
            return "Unknown";
        }
    }
}
