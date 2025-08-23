# NativeVoiceCall 项目模块说明

## 📁 项目结构

### 核心模块

#### 1. core/ - 核心库
**功能**: 语音通话的核心实现
**文件结构**:
```
core/
├── include/voice_call.h          # 公共API头文件
├── src/udp_voice_call.cpp        # UDP语音通话实现
├── CMakeLists.txt                # 核心库构建配置
└── build/                        # 构建输出目录
```

**主要功能**:
- 音频设备管理 (ALSA)
- 实时音频捕获和播放
- UDP网络通信
- 音频数据处理
- 用户和房间管理

**编译**:
```bash
cd core
mkdir -p build && cd build
cmake ..
make
```

**输出**: `libvoice_call.so` - 共享库

#### 2. linux_client/ - Linux客户端
**功能**: Linux平台的语音通话客户端
**文件结构**:
```
linux_client/
├── src/main.cpp                  # 客户端主程序
├── CMakeLists.txt                # 客户端构建配置
└── build/                        # 构建输出目录
    └── bin/voice_call_client     # 可执行文件
```

**主要功能**:
- 命令行界面
- 命令行参数解析
- 语音通话控制
- 实时状态显示

**编译**:
```bash
cd linux_client
mkdir -p build && cd build
cmake ..
make
```

**运行**:
```bash
cd linux_client/build/bin
./voice_call_client -s <服务器IP> -p <端口> -r <房间ID> -u <用户ID>
```

**命令参数**:
- `-s, --server`: 服务器IP地址 (默认: 127.0.0.1)
- `-p, --port`: 服务器端口 (默认: 8080)
- `-r, --room`: 房间ID (默认: test_room)
- `-u, --user`: 用户ID (默认: linux_user)
- `-h, --help`: 显示帮助

**交互命令**:
- `connect`: 连接到通话
- `disconnect`: 断开连接
- `mute`: 静音/取消静音
- `volume <0.0-1.0>`: 设置音量
- `status`: 显示状态
- `quit`: 退出程序

#### 3. server/ - UDP语音通话服务器
**功能**: 核心的UDP语音通话服务器
**文件结构**:
```
server/
├── udp_server.cpp                # UDP服务器实现
├── udp_server.o                  # 编译目标文件
└── udp_server                    # 可执行文件
```

**UDP服务器**:
```bash
cd server
g++ -o udp_server udp_server.cpp -std=c++17 -lpthread
./udp_server -i <监听IP> -p <端口>
```

**参数**:
- `-i, --ip`: 监听IP地址 (默认: 0.0.0.0)
- `-p, --port`: 监听端口 (默认: 8080)
- `-h, --help`: 显示帮助

### 构建脚本

#### scripts/ - 构建和打包脚本
```
scripts/
├── build_linux.sh               # Linux构建脚本
├── build_android.sh             # Android构建脚本 (预留)
├── package_linux.sh             # Linux打包脚本
├── run_demo.sh                   # 运行演示脚本
├── run_udp_demo.sh              # UDP演示脚本
└── test_udp.sh                   # UDP测试脚本
```

**主要构建脚本**:
```bash
# 构建整个项目
./scripts/build_linux.sh

# 打包Linux版本
./scripts/package_linux.sh
```

### 辅助模块

#### 1. docs/ - 项目文档
```
docs/
├── API_Reference.md              # API参考文档
├── User_Guide.md                 # 用户指南
├── Technical_Details.md          # 技术细节
├── Implementation_Summary.md     # 实现总结
└── Cross_Machine_Test.md         # 跨机器测试
```

#### 2. android_client/ - Android客户端 (预留)
**状态**: 框架代码，待完善
**功能**: Android平台的语音通话客户端

### 工具脚本

#### quick_lan_start.sh - 快速启动局域网通话
**功能**: 一键启动局域网语音通话服务器
```bash
chmod +x quick_lan_start.sh
./quick_lan_start.sh
```

## 🏗️ 完整编译流程

### 1. 自动构建 (推荐)
```bash
# 一键构建所有模块
./scripts/build_linux.sh
```

### 2. 手动构建
```bash
# 1. 构建核心库
cd core
mkdir -p build && cd build
cmake .. && make
cd ../..

# 2. 构建Linux客户端
cd linux_client
mkdir -p build && cd build
cmake .. && make
cd ../..

# 3. 构建UDP服务器
cd server
g++ -o udp_server udp_server.cpp -std=c++17 -lpthread
```

## 🚀 运行指南

### 局域网通话

#### 1. 启动服务器
```bash
cd server
./udp_server -i 0.0.0.0 -p 8080
```

#### 2. 启动客户端
```bash
cd linux_client/build/bin
./voice_call_client -s <服务器IP> -p 8080 -r test_room -u user1
```

#### 3. 开始通话
在客户端中输入 `connect` 开始通话

### 本地测试
```bash
# 启动本地服务器
cd server
./udp_server -i 127.0.0.1 -p 8080

# 启动本地客户端
cd linux_client/build/bin
./voice_call_client -s 127.0.0.1 -p 8080 -r test_room -u local_user
```

## 📊 技术特性

### 音频处理
- **采样率**: 16000 Hz
- **声道**: 单声道
- **格式**: S16_LE (16位小端)
- **缓冲**: 20ms 音频缓冲区
- **延迟**: < 100ms

### 网络通信
- **协议**: UDP
- **带宽**: ~30KB/s 每用户
- **房间**: 支持多用户房间
- **控制**: 实时连接管理

### 平台支持
- ✅ Linux (Ubuntu 20.04+)
- ✅ ALSA音频系统
- 🔄 Android (预留)

## 🔧 依赖要求

### 系统依赖
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libasound2-dev

# CentOS/RHEL
sudo yum install gcc-c++ cmake alsa-lib-devel
```

### 运行时依赖
- ALSA音频库
- 标准音频设备 (麦克风/扬声器)
- 网络连接

## 📝 开发说明

### 核心API
- `voice_call_create()`: 创建语音通话实例
- `voice_call_connect()`: 连接到服务器
- `voice_call_set_muted()`: 设置静音状态
- `voice_call_set_volume()`: 设置音量
- `voice_call_disconnect()`: 断开连接
- `voice_call_destroy()`: 销毁实例

### 扩展开发
- 音频编解码 (Opus支持)
- 视频通话功能
- 移动端客户端
- Web客户端

这就是完整的 NativeVoiceCall 项目模块说明！

