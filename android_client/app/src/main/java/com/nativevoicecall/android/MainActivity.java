package com.nativevoicecall.android;

import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    
    private static final String TAG = "MainActivity";
    
    private TextView statusText;
    private Button connectButton;
    private Button muteButton;
    private boolean isConnected = false;
    private boolean isMuted = false;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // 初始化UI组件
        statusText = findViewById(R.id.statusText);
        connectButton = findViewById(R.id.connectButton);
        muteButton = findViewById(R.id.muteButton);
        
        // 设置按钮事件
        connectButton.setOnClickListener(v -> {
            if (!isConnected) {
                // 模拟连接
                isConnected = true;
                Toast.makeText(this, "连接到语音通话服务器", Toast.LENGTH_SHORT).show();
            } else {
                // 模拟断开
                isConnected = false;
                Toast.makeText(this, "断开语音通话连接", Toast.LENGTH_SHORT).show();
            }
            updateUI();
        });
        
        muteButton.setOnClickListener(v -> {
            isMuted = !isMuted;
            Toast.makeText(this, isMuted ? "已静音" : "取消静音", Toast.LENGTH_SHORT).show();
            updateUI();
        });
        
        updateUI();
    }
    
    private void updateUI() {
        statusText.setText("状态: " + (isConnected ? "已连接" : "未连接"));
        connectButton.setText(isConnected ? "断开" : "连接");
        muteButton.setText(isMuted ? "取消静音" : "静音");
    }
}
