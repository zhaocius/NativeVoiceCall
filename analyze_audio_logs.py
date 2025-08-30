#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import sys
from collections import defaultdict, Counter
from datetime import datetime

class AudioLogAnalyzer:
    def __init__(self):
        self.audio_capture_logs = []
        self.audio_send_logs = []
        self.audio_recv_logs = []
        self.audio_play_logs = []
        self.audio_play_debug_logs = []
        self.server_audio_logs = []
        self.errors = []
        
    def parse_log_line(self, line):
        """解析日志行"""
        # 音频采集日志
        if '[AUDIO_CAPTURE]' in line:
            match = re.search(r'\[AUDIO_CAPTURE\] frames=(\d+), data_size=(\d+), first_sample=(-?\d+), last_sample=(-?\d+), mic_volume=([\d.]+)', line)
            if match:
                self.audio_capture_logs.append({
                    'frames': int(match.group(1)),
                    'data_size': int(match.group(2)),
                    'first_sample': int(match.group(3)),
                    'last_sample': int(match.group(4)),
                    'mic_volume': float(match.group(5)),
                    'line': line.strip()
                })
        
        # 音频发送日志
        elif '[AUDIO_SEND]' in line:
            match = re.search(r'\[AUDIO_SEND\] data_size=(\d+), packet_size=(\d+), sequence=(\d+)', line)
            if match:
                self.audio_send_logs.append({
                    'data_size': int(match.group(1)),
                    'packet_size': int(match.group(2)),
                    'sequence': int(match.group(3)),
                    'line': line.strip()
                })
        
        # 音频接收日志
        elif '[AUDIO_RECV]' in line:
            match = re.search(r'\[AUDIO_RECV\] data_size=(\d+), sequence=(\d+), user_id=(\d+)', line)
            if match:
                self.audio_recv_logs.append({
                    'data_size': int(match.group(1)),
                    'sequence': int(match.group(2)),
                    'user_id': int(match.group(3)),
                    'line': line.strip()
                })
        
        # 音频播放日志
        elif '[AUDIO_PLAY]' in line:
            match = re.search(r'\[AUDIO_PLAY\] frames=(\d+), buffer_size=(\d+), data_size=(\d+), speaker_volume=([\d.]+)', line)
            if match:
                self.audio_play_logs.append({
                    'frames': int(match.group(1)),
                    'buffer_size': int(match.group(2)),
                    'data_size': int(match.group(3)),
                    'speaker_volume': float(match.group(4)),
                    'line': line.strip()
                })
        
        # 音频播放调试日志
        elif '[AUDIO_PLAY_DEBUG]' in line:
            match = re.search(r'\[AUDIO_PLAY_DEBUG\] raw_first_sample=(-?\d+), raw_last_sample=(-?\d+), samples_count=(\d+)', line)
            if match:
                self.audio_play_debug_logs.append({
                    'raw_first_sample': int(match.group(1)),
                    'raw_last_sample': int(match.group(2)),
                    'samples_count': int(match.group(3)),
                    'line': line.strip()
                })
        
        # 服务器音频日志
        elif '[SERVER_AUDIO_VALIDATE]' in line:
            match = re.search(r'\[SERVER_AUDIO_VALIDATE\] data_size=(\d+), length=(\d+), condition1=(\w+), condition2=(\w+)', line)
            if match:
                self.server_audio_logs.append({
                    'data_size': int(match.group(1)),
                    'length': int(match.group(2)),
                    'condition1': match.group(3) == 'true',
                    'condition2': match.group(4) == 'true',
                    'line': line.strip()
                })
        
        # 新的服务器音频包解析日志
        elif '[SERVER_LOG] 尝试解析音频包' in line:
            match = re.search(r'\[SERVER_LOG\] 尝试解析音频包: length=(\d+), sequence=(\d+), timestamp=(\d+), user_id=(\d+), raw_data_size=0x([0-9a-fA-F]+), data_size=(\d+), 验证=(\d+)', line)
            if match:
                self.server_audio_logs.append({
                    'length': int(match.group(1)),
                    'sequence': int(match.group(2)),
                    'timestamp': int(match.group(3)),
                    'user_id': int(match.group(4)),
                    'raw_data_size_hex': match.group(5),
                    'data_size': int(match.group(6)),
                    'validation': int(match.group(7)) == 1,
                    'line': line.strip()
                })
        
        # 错误日志
        elif '[AUDIO_ERROR]' in line:
            self.errors.append(line.strip())
    
    def analyze_audio_capture(self):
        """分析音频采集"""
        print("=== 音频采集分析 ===")
        if not self.audio_capture_logs:
            print("没有找到音频采集日志")
            return
        
        print(f"采集日志数量: {len(self.audio_capture_logs)}")
        
        # 分析数据大小
        data_sizes = [log['data_size'] for log in self.audio_capture_logs]
        print(f"数据大小范围: {min(data_sizes)} - {max(data_sizes)} bytes")
        print(f"平均数据大小: {sum(data_sizes) / len(data_sizes):.1f} bytes")
        
        # 分析帧数
        frames = [log['frames'] for log in self.audio_capture_logs]
        print(f"帧数范围: {min(frames)} - {max(frames)}")
        print(f"平均帧数: {sum(frames) / len(frames):.1f}")
        
        # 分析音频样本
        first_samples = [log['first_sample'] for log in self.audio_capture_logs]
        last_samples = [log['last_sample'] for log in self.audio_capture_logs]
        print(f"第一个样本范围: {min(first_samples)} - {max(first_samples)}")
        print(f"最后一个样本范围: {min(last_samples)} - {max(last_samples)}")
        
        # 检查是否有异常值
        zero_samples = sum(1 for s in first_samples if s == 0)
        print(f"第一个样本为0的次数: {zero_samples}/{len(first_samples)}")
        
        print()
    
    def analyze_audio_send(self):
        """分析音频发送"""
        print("=== 音频发送分析 ===")
        if not self.audio_send_logs:
            print("没有找到音频发送日志")
            return
        
        print(f"发送日志数量: {len(self.audio_send_logs)}")
        
        # 分析数据大小
        data_sizes = [log['data_size'] for log in self.audio_send_logs]
        print(f"发送数据大小范围: {min(data_sizes)} - {max(data_sizes)} bytes")
        print(f"平均发送数据大小: {sum(data_sizes) / len(data_sizes):.1f} bytes")
        
        # 分析包大小
        packet_sizes = [log['packet_size'] for log in self.audio_send_logs]
        print(f"包大小范围: {min(packet_sizes)} - {max(packet_sizes)} bytes")
        print(f"平均包大小: {sum(packet_sizes) / len(packet_sizes):.1f} bytes")
        
        # 分析序列号
        sequences = [log['sequence'] for log in self.audio_send_logs]
        if len(sequences) > 1:
            gaps = [sequences[i+1] - sequences[i] for i in range(len(sequences)-1)]
            print(f"序列号间隔范围: {min(gaps)} - {max(gaps)}")
            print(f"平均序列号间隔: {sum(gaps) / len(gaps):.1f}")
        
        print()
    
    def analyze_audio_receive(self):
        """分析音频接收"""
        print("=== 音频接收分析 ===")
        if not self.audio_recv_logs:
            print("没有找到音频接收日志")
            return
        
        print(f"接收日志数量: {len(self.audio_recv_logs)}")
        
        # 分析数据大小
        data_sizes = [log['data_size'] for log in self.audio_recv_logs]
        print(f"接收数据大小范围: {min(data_sizes)} - {max(data_sizes)} bytes")
        print(f"平均接收数据大小: {sum(data_sizes) / len(data_sizes):.1f} bytes")
        
        # 分析序列号
        sequences = [log['sequence'] for log in self.audio_recv_logs]
        if len(sequences) > 1:
            gaps = [sequences[i+1] - sequences[i] for i in range(len(sequences)-1)]
            print(f"接收序列号间隔范围: {min(gaps)} - {max(gaps)}")
            print(f"平均接收序列号间隔: {sum(gaps) / len(gaps):.1f}")
        
        # 分析用户ID
        user_ids = [log['user_id'] for log in self.audio_recv_logs]
        user_id_counts = Counter(user_ids)
        print(f"接收到的用户ID: {list(user_id_counts.keys())}")
        
        print()
    
    def analyze_audio_play(self):
        """分析音频播放"""
        print("=== 音频播放分析 ===")
        if not self.audio_play_logs:
            print("没有找到音频播放日志")
            return
        
        print(f"播放日志数量: {len(self.audio_play_logs)}")
        
        # 分析播放数据
        data_sizes = [log['data_size'] for log in self.audio_play_logs]
        buffer_sizes = [log['buffer_size'] for log in self.audio_play_logs]
        frames = [log['frames'] for log in self.audio_play_logs]
        
        print(f"播放数据大小范围: {min(data_sizes)} - {max(data_sizes)} bytes")
        print(f"播放缓冲区大小范围: {min(buffer_sizes)} - {max(buffer_sizes)}")
        print(f"播放帧数范围: {min(frames)} - {max(frames)}")
        
        # 分析音量
        volumes = [log['speaker_volume'] for log in self.audio_play_logs]
        print(f"播放音量范围: {min(volumes)} - {max(volumes)}")
        print(f"平均播放音量: {sum(volumes) / len(volumes):.2f}")
        
        print()
    
    def analyze_audio_play_debug(self):
        """分析音频播放调试信息"""
        print("=== 音频播放调试分析 ===")
        if not self.audio_play_debug_logs:
            print("没有找到音频播放调试日志")
            return
        
        print(f"播放调试日志数量: {len(self.audio_play_debug_logs)}")
        
        # 分析原始音频样本
        first_samples = [log['raw_first_sample'] for log in self.audio_play_debug_logs]
        last_samples = [log['raw_last_sample'] for log in self.audio_play_debug_logs]
        sample_counts = [log['samples_count'] for log in self.audio_play_debug_logs]
        
        print(f"原始第一个样本范围: {min(first_samples)} - {max(first_samples)}")
        print(f"原始最后一个样本范围: {min(last_samples)} - {max(last_samples)}")
        print(f"样本数量范围: {min(sample_counts)} - {max(sample_counts)}")
        
        # 检查是否有异常值
        zero_first = sum(1 for s in first_samples if s == 0)
        zero_last = sum(1 for s in last_samples if s == 0)
        print(f"原始第一个样本为0的次数: {zero_first}/{len(first_samples)}")
        print(f"原始最后一个样本为0的次数: {zero_last}/{len(last_samples)}")
        
        # 检查样本值范围
        all_samples = first_samples + last_samples
        print(f"所有样本值范围: {min(all_samples)} - {max(all_samples)}")
        
        print()
    
    def analyze_server_audio(self):
        """分析服务器音频处理"""
        print("=== 服务器音频处理分析 ===")
        if not self.server_audio_logs:
            print("没有找到服务器音频日志")
            return
        
        print(f"服务器音频日志数量: {len(self.server_audio_logs)}")
        
        # 分析数据大小
        data_sizes = [log['data_size'] for log in self.server_audio_logs]
        lengths = [log['length'] for log in self.server_audio_logs]
        
        print(f"服务器接收数据大小范围: {min(data_sizes)} - {max(data_sizes)} bytes")
        print(f"服务器接收包长度范围: {min(lengths)} - {max(lengths)} bytes")
        
        # 分析序列号
        if 'sequence' in self.server_audio_logs[0]:
            sequences = [log['sequence'] for log in self.server_audio_logs]
            print(f"音频包序列号范围: {min(sequences)} - {max(sequences)}")
            
            # 检查序列号连续性
            sequence_gaps = []
            for i in range(1, len(sequences)):
                gap = sequences[i] - sequences[i-1]
                if gap != 1:
                    sequence_gaps.append(gap)
            
            if sequence_gaps:
                print(f"发现序列号跳跃: {sequence_gaps}")
            else:
                print("序列号连续，无跳跃")
        
        # 分析验证结果
        if 'validation' in self.server_audio_logs[0]:
            validation_passed = sum(1 for log in self.server_audio_logs if log['validation'])
            print(f"音频包验证通过率: {validation_passed}/{len(self.server_audio_logs)} ({validation_passed/len(self.server_audio_logs)*100:.1f}%)")
        else:
            # 旧格式的验证分析
            condition1_passed = sum(1 for log in self.server_audio_logs if log.get('condition1', False))
            condition2_passed = sum(1 for log in self.server_audio_logs if log.get('condition2', False))
            
            print(f"条件1通过率: {condition1_passed}/{len(self.server_audio_logs)} ({condition1_passed/len(self.server_audio_logs)*100:.1f}%)")
            print(f"条件2通过率: {condition2_passed}/{len(self.server_audio_logs)} ({condition2_passed/len(self.server_audio_logs)*100:.1f}%)")
        
        print()
    
    def analyze_errors(self):
        """分析错误"""
        print("=== 错误分析 ===")
        if not self.errors:
            print("没有找到错误日志")
            return
        
        print(f"错误数量: {len(self.errors)}")
        for error in self.errors:
            print(f"  {error}")
        
        print()
    
    def analyze_audio_quality_issues(self):
        """分析音频质量问题"""
        print("=== 音频质量问题分析 ===")
        
        issues = []
        
        # 检查音频采集问题
        if self.audio_capture_logs:
            first_samples = [log['first_sample'] for log in self.audio_capture_logs]
            zero_ratio = sum(1 for s in first_samples if s == 0) / len(first_samples)
            if zero_ratio > 0.5:
                issues.append(f"音频采集问题: {zero_ratio*100:.1f}%的样本为0，可能是麦克风问题")
        
        # 检查数据大小不一致
        if self.audio_send_logs and self.audio_recv_logs:
            send_sizes = set(log['data_size'] for log in self.audio_send_logs)
            recv_sizes = set(log['data_size'] for log in self.audio_recv_logs)
            if send_sizes != recv_sizes:
                issues.append(f"数据大小不一致: 发送{send_sizes} vs 接收{recv_sizes}")
        
        # 检查序列号问题
        if self.audio_send_logs and self.audio_recv_logs:
            send_seq = [log['sequence'] for log in self.audio_send_logs]
            recv_seq = [log['sequence'] for log in self.audio_recv_logs]
            if len(send_seq) > len(recv_seq) * 1.5:
                issues.append(f"音频包丢失: 发送{len(send_seq)}个，接收{len(recv_seq)}个")
        
        # 检查音量问题
        if self.audio_play_logs:
            volumes = [log['speaker_volume'] for log in self.audio_play_logs]
            avg_volume = sum(volumes) / len(volumes)
            if avg_volume < 0.1:
                issues.append(f"播放音量过低: 平均{avg_volume:.2f}")
            elif avg_volume > 2.0:
                issues.append(f"播放音量过高: 平均{avg_volume:.2f}")
        
        if issues:
            for issue in issues:
                print(f"⚠️  {issue}")
        else:
            print("✅ 未发现明显的音频质量问题")
        
        print()
    
    def run_analysis(self, log_file):
        """运行完整分析"""
        print(f"正在分析日志文件: {log_file}")
        print("=" * 50)
        
        try:
            with open(log_file, 'r', encoding='utf-8') as f:
                for line in f:
                    self.parse_log_line(line)
        except FileNotFoundError:
            print(f"错误: 找不到日志文件 {log_file}")
            return
        except Exception as e:
            print(f"错误: 读取日志文件时出错: {e}")
            return
        
        # 运行各项分析
        self.analyze_audio_capture()
        self.analyze_audio_send()
        self.analyze_audio_receive()
        self.analyze_audio_play()
        self.analyze_audio_play_debug()
        self.analyze_server_audio()
        self.analyze_errors()
        self.analyze_audio_quality_issues()
        
        print("分析完成！")

def main():
    if len(sys.argv) != 2:
        print("用法: python3 analyze_audio_logs.py <log_file>")
        print("示例: python3 analyze_audio_logs.py voice_call.log")
        sys.exit(1)
    
    log_file = sys.argv[1]
    analyzer = AudioLogAnalyzer()
    analyzer.run_analysis(log_file)

if __name__ == "__main__":
    main()
