# 跨机器测试说明

## 准备工作

### 1. 文件准备
- 将 `NativeVoiceCall_Linux_20250821_141007.tar.gz` 复制到两台Linux机器
- 解压文件：`tar -xzf NativeVoiceCall_Linux_20250821_141007.tar.gz`
- 进入目录：`cd NativeVoiceCall_Linux_20250821_141007`

### 2. 依赖安装
在两台机器上都运行：
```bash
./install.sh
```

### 3. 网络配置
确保两台机器在同一局域网内，并记录IP地址：
```bash
# 查看本机IP地址
ip addr show | grep inet
```

## 测试步骤

### 步骤1：启动服务器
在机器A上启动UDP服务器：
```bash
./udp_server
```
服务器将在端口8080上监听。

### 步骤2：修改客户端配置
在机器B上，需要修改客户端中的服务器地址。

编辑 `voice_call_client` 的源代码，或者创建一个配置文件：
```bash
# 方法1：直接修改可执行文件中的IP地址
# 使用sed命令替换127.0.0.1为机器A的IP地址
sed -i 's/127.0.0.1/机器A的IP地址/g' voice_call_client

# 方法2：重新编译（推荐）
# 修改linux_client/src/main.cpp中的服务器地址
# 将 "udp://localhost:8080" 改为 "udp://机器A的IP地址:8080"
```

### 步骤3：运行客户端
在机器B上运行客户端：
```bash
./voice_call_client
```

### 步骤4：测试连接
在客户端中使用以下命令：
```
connect
```

如果连接成功，您应该看到：
- 状态变为"已连接"
- 显示"用户加入"消息

### 步骤5：测试通信
- 使用 `mute` 命令测试静音功能
- 使用 `volume` 命令测试音量控制
- 使用 `status` 命令查看当前状态

## 故障排除

### 1. 连接失败
检查以下项目：
- 两台机器是否在同一网络
- 防火墙是否阻止UDP端口8080
- 服务器IP地址是否正确

### 2. 网络测试
使用ping测试网络连通性：
```bash
ping 机器A的IP地址
```

### 3. 端口测试
使用netcat测试端口连通性：
```bash
# 在机器B上测试
nc -u 机器A的IP地址 8080
```

### 4. 防火墙设置
如果使用UFW防火墙：
```bash
sudo ufw allow 8080/udp
```

如果使用iptables：
```bash
sudo iptables -A INPUT -p udp --dport 8080 -j ACCEPT
```

## 高级配置

### 1. 自定义端口
修改服务器端口：
```bash
# 启动服务器时指定端口
./udp_server 9090
```

修改客户端连接端口：
```bash
# 修改客户端代码中的端口号
# 将 ":8080" 改为 ":9090"
```

### 2. 多客户端测试
可以在多台机器上运行客户端：
```bash
# 在机器C、D、E上分别运行
./voice_call_client
```

### 3. 网络监控
使用tcpdump监控网络流量：
```bash
# 在服务器机器上监控UDP流量
sudo tcpdump -i any udp port 8080
```

## 预期结果

成功连接后，您应该看到：

1. **服务器端**：
   ```
   === UDP Voice Call Server ===
   UDP Server started on port 8080
   User linux_user_1 joined room test_room
   User linux_user_2 joined room test_room
   ```

2. **客户端A**：
   ```
   === NativeVoiceCall Linux客户端 ===
   状态变化: 已连接 - Connected to network
   用户加入: linux_user_2
   ```

3. **客户端B**：
   ```
   === NativeVoiceCall Linux客户端 ===
   状态变化: 已连接 - Connected to network
   用户加入: linux_user_1
   ```

## 注意事项

1. **音频功能**：当前版本主要测试网络连接，音频功能需要额外的ALSA配置
2. **安全性**：UDP协议不提供加密，仅适用于可信的局域网环境
3. **性能**：在局域网环境下，延迟应该小于50ms
4. **扩展性**：可以支持多个客户端同时连接

## 下一步

成功完成跨机器测试后，您可以：
1. 添加真正的音频传输功能
2. 实现视频通话
3. 添加加密和安全功能
4. 优化网络性能
5. 开发图形用户界面






