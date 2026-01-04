#!/usr/bin/env bash

export OMP_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export VECLIB_MAXIMUM_THREADS=1
if [ -s /data/params/d/dp_device_model_selected ]; then
  export FINGERPRINT="$(cat /data/params/d/dp_device_model_selected)"
fi

if [ -z "$AGNOS_VERSION" ]; then
  export AGNOS_VERSION="12.8"
fi

export STAGING_ROOT="/data/safe_staging"

# 重新加载 udev 规则并触发设备，解决 Panda USB 权限问题
#sudo udevadm control --reload-rules 2>/dev/null || true
#sudo udevadm trigger 2>/dev/null || true
