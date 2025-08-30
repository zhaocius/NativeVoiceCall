package com.nativevoicecall.android;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity {
    
    private static final String TAG = "MainActivity";
    private static final int PERMISSION_REQUEST_CODE = 1001;
    
    private EditText serverIpEdit;
    private EditText serverPortEdit;
    private EditText roomIdEdit;
    private EditText userIdEdit;
    private TextView statusText;
    private Button connectButton;
    private Button disconnectButton;
    private Button muteButton;
    private VoiceCallManager voiceCallManager;
    private boolean isConnected = false;
    private boolean isMuted = false;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // 检查并请求权限
        checkAndRequestPermissions();
        
        // 初始化UI组件
        serverIpEdit = findViewById(R.id.serverIpEdit);
        serverPortEdit = findViewById(R.id.serverPortEdit);
        roomIdEdit = findViewById(R.id.roomIdEdit);
        userIdEdit = findViewById(R.id.userIdEdit);
        
        // 设置默认值
        serverIpEdit.setText("192.168.1.33");
        serverPortEdit.setText("8080");
        roomIdEdit.setText("test_room");
        userIdEdit.setText("android_user");
        statusText = findViewById(R.id.statusText);
        connectButton = findViewById(R.id.connectButton);
        disconnectButton = findViewById(R.id.disconnectButton);
        muteButton = findViewById(R.id.muteButton);
        
        // 初始化语音通话管理器
        voiceCallManager = new VoiceCallManager();
        
        // 设置按钮事件
        connectButton.setOnClickListener(v -> {
            // 获取服务器配置
            String serverIp = serverIpEdit.getText().toString().trim();
            String serverPort = serverPortEdit.getText().toString().trim();
            String roomId = roomIdEdit.getText().toString().trim();
            String userId = userIdEdit.getText().toString().trim();
            
            // 验证输入
            if (serverIp.isEmpty() || serverPort.isEmpty() || roomId.isEmpty() || userId.isEmpty()) {
                Toast.makeText(this, "请填写完整的服务器配置信息", Toast.LENGTH_SHORT).show();
                return;
            }
            
            // 构建服务器URL
            String serverUrl = "udp://" + serverIp + ":" + serverPort;
            
            // 初始化语音通话
            if (!voiceCallManager.initialize(serverUrl, roomId, userId)) {
                Toast.makeText(this, "初始化语音通话失败", Toast.LENGTH_SHORT).show();
                return;
            }
            
            // 检查录音权限
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) 
                    != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "需要录音权限才能使用语音通话功能", Toast.LENGTH_LONG).show();
                ActivityCompat.requestPermissions(this, 
                    new String[]{Manifest.permission.RECORD_AUDIO}, 
                    PERMISSION_REQUEST_CODE);
                return;
            }
            
            // 连接到服务器
            if (voiceCallManager.connect()) {
                isConnected = true;
                String connectionInfo = String.format("连接到服务器: %s:%s, 房间: %s, 用户: %s", 
                    serverIp, serverPort, roomId, userId);
                Toast.makeText(this, connectionInfo, Toast.LENGTH_LONG).show();
                Log.d(TAG, connectionInfo);
            } else {
                Toast.makeText(this, "连接服务器失败", Toast.LENGTH_SHORT).show();
            }
            updateUI();
        });
        
        disconnectButton.setOnClickListener(v -> {
            // 断开连接
            if (voiceCallManager.disconnect()) {
                isConnected = false;
                Toast.makeText(this, "断开语音通话连接", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "断开连接失败", Toast.LENGTH_SHORT).show();
            }
            updateUI();
        });
        
        muteButton.setOnClickListener(v -> {
            if (voiceCallManager.setMuted(!isMuted)) {
                isMuted = !isMuted;
                Toast.makeText(this, isMuted ? "已静音" : "取消静音", Toast.LENGTH_SHORT).show();
                updateUI();
            } else {
                Toast.makeText(this, "设置静音状态失败", Toast.LENGTH_SHORT).show();
            }
        });
        
        updateUI();
    }
    
    private void updateUI() {
        statusText.setText("Status: " + (isConnected ? "Connected" : "Disconnected"));
        connectButton.setEnabled(!isConnected);
        disconnectButton.setEnabled(isConnected);
        muteButton.setEnabled(isConnected);
        muteButton.setText(isMuted ? "Unmute" : "Mute");
        
        // 连接时禁用配置输入框
        boolean enabled = !isConnected;
        serverIpEdit.setEnabled(enabled);
        serverPortEdit.setEnabled(enabled);
        roomIdEdit.setEnabled(enabled);
        userIdEdit.setEnabled(enabled);
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (voiceCallManager != null) {
            voiceCallManager.destroy();
        }
    }
    
    private void checkAndRequestPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) 
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, 
                new String[]{Manifest.permission.RECORD_AUDIO}, 
                PERMISSION_REQUEST_CODE);
        } else {
            Log.d(TAG, "录音权限已授予");
        }
    }
    
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d(TAG, "录音权限已授予");
                Toast.makeText(this, "录音权限已授予", Toast.LENGTH_SHORT).show();
            } else {
                Log.e(TAG, "录音权限被拒绝");
                Toast.makeText(this, "需要录音权限才能使用语音通话功能", Toast.LENGTH_LONG).show();
            }
        }
    }
}
