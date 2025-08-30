#!/bin/bash

# 音频日志分析脚本

echo "=== 音频日志分析 ==="
echo ""

# 分析音频采集日志
echo "1. 音频采集分析:"
echo "=================="
grep "\[AUDIO_CAPTURE\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析音频发送日志
echo "2. 音频发送分析:"
echo "=================="
grep "\[AUDIO_SEND\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析服务器音频验证日志
echo "3. 服务器音频验证分析:"
echo "========================"
grep "\[SERVER_AUDIO_VALIDATE\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析服务器音频转发日志
echo "4. 服务器音频转发分析:"
echo "========================"
grep "\[SERVER_AUDIO_FORWARD\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析音频接收日志
echo "5. 音频接收分析:"
echo "=================="
grep "\[AUDIO_RECV\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析音频播放调试日志
echo "6. 音频播放调试分析:"
echo "======================"
grep "\[AUDIO_PLAY_DEBUG\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析音频播放日志
echo "7. 音频播放分析:"
echo "=================="
grep "\[AUDIO_PLAY\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 分析错误日志
echo "8. 错误日志分析:"
echo "=================="
grep "\[AUDIO_ERROR\]" server_output.log | tail -5 | while read line; do
    echo "$line"
done
echo ""

# 统计信息
echo "9. 统计信息:"
echo "=============="
echo "音频采集次数: $(grep -c "\[AUDIO_CAPTURE\]" server_output.log)"
echo "音频发送次数: $(grep -c "\[AUDIO_SEND\]" server_output.log)"
echo "音频接收次数: $(grep -c "\[AUDIO_RECV\]" server_output.log)"
echo "音频播放次数: $(grep -c "\[AUDIO_PLAY\]" server_output.log)"
echo "错误次数: $(grep -c "\[AUDIO_ERROR\]" server_output.log)"
echo ""

echo "=== 分析完成 ==="
