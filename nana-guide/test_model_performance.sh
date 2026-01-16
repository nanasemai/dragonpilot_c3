#!/usr/bin/env bash

# 模型性能测试脚本
# 用于多次测试模型在GPU上的性能，评估不同设备上的推理效果
# 作者：Auto-Generated
# 日期：$(date +"%Y-%m-%d")

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}模型性能测试脚本 (GPU)${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# 检查必要的目录和文件
DRAGONPILOT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TINYGRAD_REPO="${DRAGONPILOT_ROOT}/tinygrad_repo"
MODEL_DIR="${DRAGONPILOT_ROOT}/selfdrive/modeld/models"

if [ ! -d "${TINYGRAD_REPO}" ]; then
    echo -e "${RED}错误: tinygrad_repo 目录不存在${NC}"
    exit 1
fi

if [ ! -d "${MODEL_DIR}" ]; then
    echo -e "${RED}错误: 模型目录不存在${NC}"
    exit 1
fi

# 检查模型文件
echo -e "${YELLOW}检查可用的模型文件...${NC}"
AVAILABLE_MODELS=()

if [ -f "${MODEL_DIR}/driving_policy.onnx" ]; then
    AVAILABLE_MODELS+=("driving_policy.onnx")
fi

if [ -f "${MODEL_DIR}/driving_vision.onnx" ]; then
    AVAILABLE_MODELS+=("driving_vision.onnx")
fi

if [ -f "${MODEL_DIR}/dmonitoring_model.onnx" ]; then
    AVAILABLE_MODELS+=("dmonitoring_model.onnx")
fi

if [ ${#AVAILABLE_MODELS[@]} -eq 0 ]; then
    echo -e "${RED}错误: 没有找到可用的模型文件${NC}"
    exit 1
fi

echo -e "${GREEN}找到以下模型:${NC}"
for model in "${AVAILABLE_MODELS[@]}"; do
    model_size=$(du -h "${MODEL_DIR}/${model}" | cut -f1)
    model_hash=$(sha256sum "${MODEL_DIR}/${model}" | cut -d' ' -f1)
    echo -e "  - ${model} (${model_size}, SHA256: ${model_hash:0:16}...)"
done
echo

# 默认参数
DEFAULT_ITERATIONS=10
DEFAULT_WARMUP=3
DEFAULT_DEVICE="GPU"

# 解析命令行参数
ITERATIONS=${1:-$DEFAULT_ITERATIONS}
WARMUP=${2:-$DEFAULT_WARMUP}
DEVICE=${3:-$DEFAULT_DEVICE}

echo -e "${YELLOW}测试配置:${NC}"
echo -e "  迭代次数: ${ITERATIONS}"
echo -e "  预热次数: ${WARMUP}"
echo -e "  目标设备: ${DEVICE}"
echo

# 激活虚拟环境
if [ -f "${DRAGONPILOT_ROOT}/.venv/bin/activate" ]; then
    echo -e "${YELLOW}激活虚拟环境...${NC}"
    source "${DRAGONPILOT_ROOT}/.venv/bin/activate"
else
    echo -e "${RED}警告: 虚拟环境未找到，使用系统Python${NC}"
fi

# 测试函数
test_model() {
    local model_name="$1"
    local model_path="${MODEL_DIR}/${model_name}"
    local results_file="${DRAGONPILOT_ROOT}/test_results_${model_name}_$(date +"%Y%m%d_%H%M%S").txt"
    local model_hash=$(sha256sum "${model_path}" | cut -d' ' -f1)

    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}测试模型: ${model_name}${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${YELLOW}结果将保存到: ${results_file}${NC}"
    echo -e "${YELLOW}模型SHA256: ${model_hash}${NC}"
    echo

    # 输出测试配置到控制台
    echo -e "${YELLOW}测试配置:${NC}"
    echo -e "  模型: ${model_name}"
    echo -e "  模型路径: ${model_path}"
    echo -e "  模型SHA256: ${model_hash}"
    echo -e "  迭代次数: ${ITERATIONS}"
    echo -e "  预热次数: ${WARMUP}"
    echo -e "  目标设备: ${DEVICE}"
    echo -e "  测试时间: $(date)"
    echo -e "  系统信息: $(uname -a)"
    echo -e "  Python版本: $(python3 --version)"
    echo
    # 写入测试配置到结果文件
    echo "测试配置:" > "${results_file}"
    echo "- 模型: ${model_name}" >> "${results_file}"
    echo "- 模型路径: ${model_path}" >> "${results_file}"
    echo "- 模型SHA256: ${model_hash}" >> "${results_file}"
    echo "- 迭代次数: ${ITERATIONS}" >> "${results_file}"
    echo "- 预热次数: ${WARMUP}" >> "${results_file}"
    echo "- 目标设备: ${DEVICE}" >> "${results_file}"
    echo "- 测试时间: $(date)" >> "${results_file}"
    echo "- 系统信息: $(uname -a)" >> "${results_file}"
    echo "- Python版本: $(python3 --version)" >> "${results_file}"
    echo "" >> "${results_file}"

    # 预热运行
    echo -e "${YELLOW}执行预热运行...${NC}"
    for i in $(seq 1 ${WARMUP}); do
        echo -e "  预热 ${i}/${WARMUP}"
        cd "${TINYGRAD_REPO}" && PYTHONPATH="." DEV="${DEVICE}" IMAGE=0 python3 examples/openpilot/compile3.py "${model_path}" > /dev/null 2>&1
    done
    echo

    # 正式测试
    echo -e "${YELLOW}执行正式测试...${NC}"
    local total_time=0
    local min_time=999999
    local max_time=0
    local times=()
    local valid_results=0

    for i in $(seq 1 ${ITERATIONS}); do
        echo -e "  测试 ${i}/${ITERATIONS}"

        # 运行测试并捕获输出，同时输出到控制台
        echo -e "${GREEN}运行测试...${NC}"
        output=$(cd "${TINYGRAD_REPO}" && PYTHONPATH="." DEV="${DEVICE}" IMAGE=0 python3 examples/openpilot/compile3.py "${model_path}" 2>&1 | tee /dev/stderr)

        # 提取运行时间（适配不同的输出格式）
        run_time=$(echo "${output}" | grep -E "total run|enqueue" | tail -5 | grep -E "[0-9]+(\.[0-9]+)? ms" | head -1 | awk '{print $NF}' | sed 's/ms//')

        # 尝试另一种格式
        if [ -z "${run_time}" ] || ! [[ "${run_time}" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
            run_time=$(echo "${output}" | grep -oE "[0-9]+(\.[0-9]+)? ms" | tail -1 | sed 's/ ms//')
        fi

        if [ -n "${run_time}" ] && [[ "${run_time}" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
            # 记录时间
            times+=(${run_time})
            total_time=$(echo "${total_time} + ${run_time}" | bc)
            valid_results=$((valid_results + 1))

            # 更新最小/最大时间
            if (( $(echo "${run_time} < ${min_time}" | bc -l) )); then
                min_time=${run_time}
            fi
            if (( $(echo "${run_time} > ${max_time}" | bc -l) )); then
                max_time=${run_time}
            fi

            echo -e "    运行时间: ${run_time} ms"

            # 提取性能数据
            max_flops=$(echo "${output}" | grep "GFLOPS" | awk '{print $5}' | grep -E '^[0-9]+(\.[0-9]+)?$' | sort -nr | head -1)
            if [ -n "${max_flops}" ]; then
                echo -e "    最高性能: ${max_flops} GFLOPS"
            fi
        else
            echo -e "${RED}    错误: 无法提取运行时间${NC}"
        fi

        echo
    done

    # 计算平均值
    if [ ${valid_results} -gt 0 ]; then
        avg_time=$(echo "${total_time} / ${valid_results}" | bc -l)

        # 计算标准差
        local sum_squares=0
        for time in "${times[@]}"; do
            local diff=$(echo "${time} - ${avg_time}" | bc)
            local square=$(echo "${diff} * ${diff}" | bc)
            sum_squares=$(echo "${sum_squares} + ${square}" | bc)
        done
        std_dev=$(echo "sqrt(${sum_squares} / ${valid_results})" | bc -l 2>/dev/null || echo "0")

        # 生成测试报告
        echo -e "${GREEN}========================================${NC}"
        echo -e "${GREEN}测试报告: ${model_name}${NC}"
        echo -e "${GREEN}========================================${NC}"
        echo -e "${YELLOW}性能统计:${NC}"
        echo -e "  平均运行时间: $(printf "%.2f" ${avg_time}) ms"
        echo -e "  最小运行时间: ${min_time} ms"
        echo -e "  最大运行时间: ${max_time} ms"
        echo -e "  标准差: $(printf "%.3f" ${std_dev}) ms"
        echo -e "  有效样本: ${valid_results}/${ITERATIONS}"
        echo -e "  模型SHA256: ${model_hash}"
        echo
        # 写入详细结果到文件
        echo "详细结果:" >> "${results_file}"
        for i in $(seq 0 $((${#times[@]}-1))); do
            echo "测试 ${i+1}: ${times[$i]} ms" >> "${results_file}"
        done
        echo "" >> "${results_file}"
        echo "性能统计:" >> "${results_file}"
        echo "- 平均运行时间: $(printf "%.2f" ${avg_time}) ms" >> "${results_file}"
        echo "- 最小运行时间: ${min_time} ms" >> "${results_file}"
        echo "- 最大运行时间: ${max_time} ms" >> "${results_file}"
        echo "- 标准差: $(printf "%.3f" ${std_dev}) ms" >> "${results_file}"
        echo "- 有效样本: ${valid_results}/${ITERATIONS}" >> "${results_file}"
        echo "- 模型SHA256: ${model_hash}" >> "${results_file}"

        # 保存完整输出
        echo "" >> "${results_file}"
        echo "完整输出:" >> "${results_file}"
        echo "${output}" >> "${results_file}"

        echo -e "${GREEN}测试完成!${NC}"
        echo -e "${GREEN}详细结果已保存到: ${results_file}${NC}"
        echo
    else
        echo -e "${RED}错误: 没有有效的测试结果${NC}"
    fi
}

# 测试所有模型
for model in "${AVAILABLE_MODELS[@]}"; do
    test_model "${model}"
done

# 生成汇总报告
generate_summary() {
    local summary_file="${DRAGONPILOT_ROOT}/test_summary_$(date +"%Y%m%d_%H%M%S").txt"

    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}生成测试汇总报告${NC}"
    echo -e "${GREEN}========================================${NC}"

    echo "测试汇总报告" > "${summary_file}"
    echo "生成时间: $(date)" >> "${summary_file}"
    echo "测试配置:" >> "${summary_file}"
    echo "- 迭代次数: ${ITERATIONS}" >> "${summary_file}"
    echo "- 预热次数: ${WARMUP}" >> "${summary_file}"
    echo "- 目标设备: ${DEVICE}" >> "${summary_file}"
    echo "" >> "${summary_file}"

    for model in "${AVAILABLE_MODELS[@]}"; do
        # 查找最新的测试结果文件
        local latest_result=$(ls -t "${DRAGONPILOT_ROOT}/test_results_${model}_*.txt" 2>/dev/null | head -1)
        if [ -f "${latest_result}" ]; then
            local model_hash=$(grep "模型SHA256:" "${latest_result}" | head -1 | cut -d':' -f2 | xargs)
            echo "模型: ${model}" >> "${summary_file}"
            echo "模型SHA256: ${model_hash}" >> "${summary_file}"
            echo "结果文件: ${latest_result}" >> "${summary_file}"
            echo "性能统计:" >> "${summary_file}"
            grep -A6 "性能统计:" "${latest_result}" >> "${summary_file}"
            echo "" >> "${summary_file}"
        fi
    done

    echo -e "${GREEN}汇总报告已保存到: ${summary_file}${NC}"
    echo
}

generate_summary

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}所有测试完成!${NC}"
echo -e "${GREEN}========================================${NC}"
echo

echo -e "${YELLOW}使用方法:${NC}"
echo -e "  $0 [迭代次数] [预热次数] [目标设备]"
echo -e "  示例: $0 20 5 GPU  # 20次迭代，5次预热，使用GPU"
echo -e "  示例: $0 5 2 AMD   # 5次迭代，2次预热，使用AMD GPU"
echo
