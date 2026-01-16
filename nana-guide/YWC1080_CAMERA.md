# YW1080 摄像头使用说明

## 1. 摄像头概述

本说明文档描述了 dragonpilot 项目中使用的 **YW1080 (Z-Star Venus USB2.0)** 摄像头的规格、配置和优化方法。

### 1.1 硬件规格

| 参数 | 规格 |
|------|------|
| **型号** | YW1080 (Z-Star Venus) |
| **接口** | USB 2.0 |
| **传感器** | CMOS |
| **最大分辨率** | 1920×1080 (Full HD) |
| **最大帧率** | 30 FPS |
| **像素格式** | YUYV 4:2:2 |
| **设备节点** | /dev/video0, /dev/video1 |
| **驱动程序** | uvcvideo |

### 1.2 支持的分辨率

使用 **MJPG 压缩格式** (BUF=3)：

| 分辨率 | 实际帧率 | 推荐程度 |
|--------|---------|----------|
| 1920×1080 | **~20 FPS** | ✅ **最佳画质推荐** |
| 1280×720 | **~20 FPS** | ✅ 平衡选择 |
| 640×480 | **~20 FPS** | ✅ 低带宽场景 |
| 320×240 | **~20 FPS** | 测试用 |

> **重要发现 (2026-01-17 测试结果)**:
> 更换 USB 口后，BUF=3 配置可达到 **19.8 FPS**！
> 推荐使用 **1920×1080 @ 20 FPS (MJPG, BUF=3)**

> **注意**: YUYV 格式在 1080P 和 720P 下无法工作（带宽不足）

---

## 2. 当前系统配置

### 2.1 检测到的摄像头

系统当前检测到两个 USB 摄像头：

```
📷 YW1080: YW1080 (usb-0000:00:14.0-2)
        /dev/video0  ← 主摄像头 (推荐)
        /dev/video1

📷 Bison USB Webcam (usb-0000:00:14.0-3)
        /dev/video2
        /dev/video3
```

### 2.2 摄像头对比

| 特性 | YW1080 (选用) | Bison USB Webcam |
|------|---------------|------------------|
| 最高分辨率 | 1920×1080 | 1280×1024 |
| 最大帧率 | 30 FPS | 25 FPS |
| 稳定性 | 优秀 | 良好 |
| **选择** | ✅ 主摄像头 | ❌ 未使用 |

---

## 3. 代码配置

### 3.1 默认配置

文件: `tools/webcam/camerad.py`

```python
ROAD_CAM = os.getenv("ROAD_CAM", "0")   # 使用 /dev/video0 (YW1080)
WIDE_CAM = None                         # 单摄像头模式
DRIVER_CAM = None                       # 无驾驶员监控
```

### 3.2 分辨率配置

文件: `tools/webcam/camera.py` (CameraMJPG 类)

```python
def _configure_camera_format(self, target_fourcc):
    fourcc = cv.VideoWriter_fourcc(*target_fourcc)
    self.cap.set(cv.CAP_PROP_FOURCC, fourcc)
    self.cap.set(cv.CAP_PROP_FRAME_WIDTH, 1280)  # 目标宽度
    self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, 720)  # 目标高度
    self.cap.set(cv.CAP_PROP_FPS, 20)            # 目标帧率
```

### 3.3 环境变量

```bash
# 设置主摄像头设备号 (默认 0)
export ROAD_CAM=0

# 启用广角摄像头 (可选)
export WIDE_CAM=2

# 启用驾驶员监控摄像头 (可选)
export DRIVER_CAM=2
```

---

## 4. 延迟优化

### 4.1 当前延迟

在 1920×1080 @ 20 FPS (MJPG) 配置下 (BUFFERSIZE=3)：

| 延迟类型 | 典型值 |
|---------|--------|
| 帧捕获延迟 | ~50ms |
| 缓冲区延迟 | ~50ms (BUF=3) |
| **端到端延迟** | **~100ms** |

### 4.2 缓冲区优化

| 缓冲区大小 | 实测帧率 | 推荐程度 |
|-----------|---------|----------|
| 1 | ~10 FPS | ❌ 太低 |
| 2 | ~17 FPS | ⚠️ 偏低 |
| **3** | **~20 FPS** | ✅ **最佳选择** |

**关键发现 (2026-01-17 更新)**:
- BUF=1: 只有 ~10 FPS
- BUF=2: ~17 FPS
- **BUF=3: 19.8 FPS ✅ 最佳**
- BUF=3 是延迟和帧率的最佳平衡点

### 4.2 优化设置

#### 方法 1: 减少缓冲区大小

在 `camera.py` 的 `CameraMJPG.__init__` 中添加：

```python
# 减少缓冲区数量，降低延迟
self.cap.set(cv.CAP_PROP_BUFFERSIZE, 1)
```

#### 方法 2: 优化 uvcvideo 驱动参数

编辑 `/etc/modprobe.d/uvcvideo.conf`：

```bash
# 创建或编辑配置文件
sudo tee /etc/modprobe.d/uvcvideo.conf << 'EOF'
# 减少缓冲区帧数 (默认3)
options uvcvideo nodrop=1 quirks=0x80

# 设置超时参数 (毫秒)
options uvcvideo timeout=5000

# 禁用自动曝光补偿以减少延迟
options uvcvideo exposure_auto=1
EOF

# 重新加载驱动
sudo modprobe -r uvcvideo
sudo modprobe uvcvideo
```

#### 方法 3: 设置实时优先级

```python
import os
import sched
import time

# 设置进程为实时优先级 (需要 root 权限)
os.nice(-20)
```

---

## 5. 性能测试

### 5.1 运行延迟测试

```bash
# 测试摄像头基本功能
python3 test_camera.py

# 测试延迟
python3 test_camera_latency.py
```

### 5.2 预期性能

在优化后的配置下：

| 指标 | 值 |
|------|------|
| 分辨率 | 640×480 |
| 帧率 | 20 FPS |
| 端到端延迟 | < 100ms |
| CPU 占用 | < 10% |

---

## 6. 故障排除

### 6.1 摄像头无法打开

```bash
# 检查设备权限
ls -la /dev/video*

# 添加用户到 video 组
sudo usermod -aG video $USER

# 检查驱动状态
lsmod | grep uvcvideo

# 重新加载驱动
sudo modprobe -r uvcvideo
sudo modprobe uvcvideo
```

### 6.2 帧率不稳定

```bash
# 检查 USB 带宽
lsusb -t

# 检查 CPU 负载
htop

# 关闭其他占用摄像头的程序
ps aux | grep webcam
```

### 6.3 分辨率不支持

```bash
# 查看支持的分辨率
v4l2-ctl -d /dev/video0 --list-formats-ext

# 检查当前格式
v4l2-ctl -d /dev/video0 --all
```

---

## 7. 总结

### 7.1 推荐配置

| 参数 | 值 |
|------|------|
| 摄像头 | YW1080 (/dev/video0) |
| 分辨率 | **1920×1080** (Full HD) |
| 帧率 | **~20 FPS** |
| 格式 | **MJPG** |
| 缓冲区 | **3** |

**实测性能 (2026-01-17 更新)**:
```bash
分辨率: 1920x1080
格式: MJPG
缓冲区: 3
平均帧率: 19.8 FPS ✅
```

> **重要**: YUYV 格式在 1080P 和 720P 下无法工作，仅 VGA 及以下分辨率可用

### 7.2 使用方法

```bash
# 激活环境
source .env
source .venv/bin/activate

# 运行 camerad
cd tools/webcam
python3 camerad.py
```

### 7.3 注意事项

1. **USB 带宽**: 避免同时使用多个高分辨率摄像头
2. **供电**: 使用 USB 2.0 接口直接供电，避免使用 Hub
3. **镜头**: 推荐使用 6mm M12 镜头获得约 60° 视角
4. **安装**: 确保摄像头固定牢固，减少震动

---

## 8. 参考资料

- [UVC 驱动文档](https://www.ideasonboard.org/uvc/)
- [OpenCV VideoCapture](https://docs.opencv.org/master/d8/dfe/classcv_1_1VideoCapture.html)
- [V4L2 文档](https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html)

---

*文档版本: 1.0*
*最后更新: 2026-01-17*
