# NVIDIA GT640M 快速使用指南

## ✅ 已完成的配置

你的NVIDIA GT640M已经配置完成，可以正常使用OpenCL了！

## 📋 当前设置

```bash
export CL_HALF=0      # 禁用半精度（GPU不支持）
export CL_INT64=0     # 禁用64位整数（保守设置）
export CL_OPTIMIZATION_LEVEL=0  # 保守优化
export DEV=GPU        # 启用OpenCL
export IMAGE=0        # 禁用图像优化
```

## 🚀 使用方法

### 方法1：每次运行时加载环境

```bash
cd /home/ubuntu/dragonpilot_pc
source .env
PYTHONPATH=. python3 your_script.py
```

### 方法2：在脚本中自动加载

```bash
#!/bin/bash
source /home/ubuntu/dragonpilot_pc/.env
cd /home/ubuntu/dragonpilot_pc
python3 your_script.py
```

## 📊 性能建议

### 保守配置（当前使用）
- **优点**：最稳定，兼容性最好
- **缺点**：性能较低
- **适用**：开发和调试

### 激进配置（可选）
如果你想提高性能，可以尝试：

```bash
export CL_HALF=0      # 保持禁用（GPU不支持）
export CL_INT64=1     # 启用64位整数（GPU支持）
export CL_OPTIMIZATION_LEVEL=2  # 激进优化
```

**注意**：激进配置可能在某些情况下导致不稳定。

## 🧪 测试验证

运行以下命令验证配置：

```bash
cd /home/ubuntu/dragonpilot_pc
bash -c 'source .env && PYTHONPATH=. python3 test_gt640m_compat.py'
```

预期输出应该显示所有测试通过。

## 🔧 故障排除

### 问题1：编译错误
```
解决：确保运行 source .env
```

### 问题2：性能问题
```
解决：检查环境变量是否正确加载
     env | grep CL_
```

### 问题3：FP16相关错误
```
解决：正确，GT640M不支持FP16，已自动禁用
```

## 📈 监控GPU使用

在另一个终端运行：

```bash
watch -n 1 nvidia-smi
```

## 🎯 下一步

1. ✅ 运行你的实际程序测试性能
2. 🔄 根据需要调整CL_INT64设置（从0改为1可能提升性能）
3. 📊 对比不同设置下的性能差异
4. 🔧 如有问题，运行测试脚本诊断

## 📞 获取帮助

如果遇到问题：
1. 运行测试脚本：`bash -c 'source .env && PYTHONPATH=. python3 test_gt640m_compat.py'`
2. 查看详细调试信息：`export DEBUG=1`
3. 检查环境变量：`env | grep CL_`

**祝你使用愉快！** 🚗💨
