# NVIDIA GT640M OpenCL兼容性测试指南

## 测试环境准备

### 1. 验证环境变量配置

首先，确保你的`.env`文件包含了正确的配置。对于NVIDIA GT640M，应该包含：

```bash
export DEV=GPU
export IMAGE=0
export CL_HALF=0
export CL_INT64=0
export CL_OPTIMIZATION_LEVEL=0
```

### 2. 启用调试输出

为了看到自动检测和警告信息，需要启用DEBUG模式：

```bash
export DEBUG=1
```

## 测试步骤

### 测试1：基础OpenCL识别

```bash
PYTHONPATH=. DEBUG=1 python3 -c "
from tinygrad.runtime.ops_gpu import CLDevice
dev = CLDevice()
print(f'Device: {dev.device_name}')
print(f'Driver: {dev.driver_version}')
print(f'FP16 Support: {dev.supports_fp16}')
print(f'Int64 Support: {dev.supports_int64}')
print(f'Double Support: {dev.supports_double}')
"
```

**预期输出**：
- Device名称应该包含"GT 640M"
- FP16 Support应该为False
- Int64 Support应该为False
- 应该看到GT640M特定的警告信息

### 测试2：环境变量读取

```bash
PYTHONPATH=. DEBUG=1 CL_HALF=0 CL_INT64=0 CL_OPTIMIZATION_LEVEL=0 python3 -c "
from tinygrad.runtime.ops_gpu import CLCompiler
from tinygrad.runtime.ops_gpu import CLDevice

dev = CLDevice()
compiler = CLCompiler(dev, 'test')
print(f'CL_HALF: {compiler.cl_half}')
print(f'CL_INT64: {compiler.cl_int64}')
print(f'OPT_LEVEL: {compiler.opt_level}')
"
```

**预期输出**：
- CL_HALF: 0
- CL_INT64: 0  
- OPT_LEVEL: 0

### 测试3：编译选项生成

```bash
PYTHONPATH=. DEBUG=1 CL_HALF=0 CL_INT64=0 CL_OPTIMIZATION_LEVEL=2 python3 -c "
from tinygrad.runtime.ops_gpu import CLCompiler
from tinygrad.runtime.ops_gpu import CLDevice

dev = CLDevice()
compiler = CLCompiler(dev, 'test')
compile_args = compiler._get_compile_args()
print(f'Compile args: {compile_args}')
"
```

**预期输出**：
- `Compile args: -DCL_HALF_DISABLED -DCL_INT64_DISABLED -cl-mad-enable -cl-fast-relaxed-math -cl-unsafe-math-optimizations`

### 测试4：模型编译测试

使用你的实际模型进行编译测试：

```bash
cd /home/ubuntu/dragonpilot_pc
source .env
DEBUG=1 python examples/openpilot/compile3.py selfdrive/modeld/models/driving_policy.onnx
```

**观察要点**：
- 是否检测到GT640M并显示警告
- 编译是否成功
- 是否有性能问题

## 常见问题排查

### 问题1：无法识别GPU

**症状**：`Number of platforms: 0`

**解决方案**：
1. 检查NVIDIA驱动是否安装：`nvidia-smi`
2. 检查OpenCL支持：`clinfo`
3. 重新安装CUDA Toolkit

### 问题2：编译错误

**症状**：`OpenCL Compile Error`

**解决方案**：
1. 确保设置`CL_HALF=0`和`CL_INT64=0`
2. 设置`CL_OPTIMIZATION_LEVEL=0`
3. 检查环境变量是否正确加载：`echo $CL_HALF`

### 问题3：性能问题

**症状**：运行缓慢或内存不足

**解决方案**：
1. 使用更保守的优化设置
2. 减少批处理大小
3. 检查GPU显存：`nvidia-smi`

## 性能基准测试

### 不同配置的性能对比

| 配置 | CL_HALF | CL_INT64 | OPT_LEVEL | 预期性能 |
|------|---------|----------|-----------|----------|
| 保守配置 | 0 | 0 | 0 | 最稳定，但最慢 |
| 标准配置 | 1 | 1 | 1 | 平衡 |
| 激进配置 | 1 | 1 | 2 | 最快，但可能不稳定 |

### 性能测试命令

```bash
# 保守配置
time PYTHONPATH=. DEV=GPU CL_HALF=0 CL_INT64=0 CL_OPTIMIZATION_LEVEL=0 IMAGE=0 python examples/openpilot/compile3.py selfdrive/modeld/models/driving_policy.onnx

# 标准配置  
time PYTHONPATH=. DEV=GPU CL_HALF=1 CL_INT64=1 CL_OPTIMIZATION_LEVEL=1 IMAGE=0 python examples/openpilot/compile3.py selfdrive/modeld/models/driving_policy.onnx
```

## 其他GPU配置的参考

### AMD R7 4700U (现代APU)

```bash
export CL_HALF=1
export CL_INT64=1
export CL_OPTIMIZATION_LEVEL=2
export CL_ARCH_DETECTION=1
```

### Intel集成显卡

```bash
export CL_HALF=0
export CL_INT64=0
export CL_OPTIMIZATION_LEVEL=0
```

### 现代NVIDIA GPU (GTX 1060+)

```bash
export CL_HALF=1
export CL_INT64=1
export CL_OPTIMIZATION_LEVEL=2
```

## 监控和日志

### 启用详细日志

```bash
export DEBUG=4
```

### 监控GPU使用

```bash
# 在另一个终端运行
watch -n 1 nvidia-smi
```

## 总结

通过这个测试指南，你可以：

1. ✅ 验证你的NVIDIA GT640M是否被正确识别
2. ✅ 确认环境变量是否被正确读取和使用
3. ✅ 测试不同的配置组合
4. ✅ 找到适合你GPU的最佳配置
5. ✅ 解决常见的兼容性问题

记住：NVIDIA GT640M是一个老旧的GPU，使用保守配置可以确保稳定性，但可能会牺牲一些性能。
