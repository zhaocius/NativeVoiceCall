# NativeVoiceCall 使用指南

## 快速开始

### Linux 平台

1. **安装依赖**
   ```bash
   sudo apt-get update
   sudo apt-get install cmake build-essential libssl-dev
   ```

2. **构建项目**
   ```bash
   chmod +x scripts/build_linux.sh
   ./scripts/build_linux.sh
   ```

3. **运行客户端**
   ```bash
   cd linux_client/build/bin
   ./voice_call_client
   ```

### Android 平台

1. **安装Android Studio和NDK**

2. **构建APK**
   ```bash
   chmod +x scripts/build_android.sh
   ./scripts/build_android.sh
   ```

3. **安装到设备**
   ```bash
   adb install android_client/app/build/outputs/apk/debug/app-debug.apk
   ```

## 使用说明

### Linux 客户端

启动后，您可以使用以下命令：

- `connect` - 连接到通话
- `disconnect` - 断开连接
- `mute` - 静音/取消静音
- `volume` - 设置音量
- `status` - 显示状态
- `help` - 显示帮助
- `quit` - 退出程序

### Android 客户端

1. 启动应用
2. 点击"连接"按钮开始通话
3. 点击"静音"按钮控制麦克风
4. 支持与Linux客户端互通

## 配置说明

### 服务器配置

默认配置使用本地服务器：
- 服务器URL: `ws://localhost:8080`
- 房间ID: `test_room`
- 用户ID: 自动生成

### 音频配置

默认音频设置：
- 采样率: 48000 Hz
- 声道: 单声道
- 位深度: 16位
- 帧大小: 20ms

### 音频处理

默认启用：
- 回声消除
- 噪声抑制
- 自动增益控制

## 故障排除

### 常见问题

1. **连接失败**
   - 检查网络连接
   - 确认服务器地址正确
   - 检查防火墙设置

2. **音频问题**
   - 检查音频设备权限
   - 确认麦克风和扬声器工作正常
   - 尝试调整音量设置

3. **编译错误**
   - 确认已安装所有依赖
   - 检查CMake版本（需要3.16+）
   - 确认编译器支持C++17

### 调试信息

启用调试输出：
```bash
export VOICE_CALL_DEBUG=1
./voice_call_client
```

### 日志文件

Linux客户端日志输出到控制台，Android客户端使用Android日志系统。

## 高级功能

### 自定义音频设备

Linux平台支持指定音频设备：
```c
voice_call_set_audio_input_device(handle, "hw:0,0");
voice_call_set_audio_output_device(handle, "hw:0,1");
```

### 音量控制

```c
// 设置麦克风音量 (0.0 - 1.0)
voice_call_set_microphone_volume(handle, 0.8f);

// 设置扬声器音量 (0.0 - 1.0)
voice_call_set_speaker_volume(handle, 0.7f);
```

### 状态监控

```c
voice_call_state_t state = voice_call_get_state(handle);
bool muted = voice_call_is_muted(handle);
```

## 性能优化

### 网络优化

- 使用有线网络连接
- 确保网络延迟低于100ms
- 避免网络拥塞

### 音频优化

- 使用高质量音频设备
- 避免环境噪音
- 适当调整音频处理参数

### 系统优化

- 关闭不必要的后台程序
- 确保足够的CPU和内存资源
- 使用SSD存储提高I/O性能

## 安全注意事项

1. **网络安全**
   - 使用HTTPS/WSS连接
   - 实施适当的认证机制
   - 加密敏感数据

2. **隐私保护**
   - 注意录音权限
   - 保护用户隐私
   - 遵守相关法规

3. **系统安全**
   - 定期更新依赖库
   - 使用安全的编译选项
   - 实施代码签名

## 技术支持

如遇到问题，请：

1. 查看本文档的故障排除部分
2. 检查项目GitHub页面的Issues
3. 提交详细的错误报告
4. 提供系统信息和错误日志

