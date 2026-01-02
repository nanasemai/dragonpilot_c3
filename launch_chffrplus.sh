#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

source "$DIR/launch_env.sh"

# Load environment variables from .env file if it exists
if [ -f "$DIR/.env" ]; then
  source "$DIR/.env"
  echo "Loaded environment variables from .env file"
else
  # Set default environment variables if .env file doesn't exist
  echo "No .env file found, setting default environment variables..."
  echo "Run 'nana-guide/setup_device_env.sh' to create a complete .env configuration"
  
  # Core environment variables
  export ZMQ=1                    # Enable ZMQ for IPC
  export USE_WEBCAM=1             # Enable webcam support
  export PYTHONPATH="$PWD"        # Set Python path
  export LOG_READABLE="1"         # Enable human-readable log format
  export IMAGE="0"                # Required for CL device image handling
  
  # Camera configuration
  export ROAD_CAM="0"             # Default road camera setting
  export DRIVER_CAM=""            # Disable driver camera
  export WIDE_CAM=""              # Disable wide camera
  
  # GPU configuration (users can manually customize in .env)
  # export DEV="GPU"               # Use GPU for model inference
  # export DEV="AMD"               # Use AMD GPU for model inference
  # export DEV="NVIDIA"            # Use NVIDIA GPU for model inference
fi

# PC environment detection and configuration
if [ ! -f /TICI ]; then
  echo "Detected PC environment, applying PC-specific configuration..."

  # Set default paths if not already set by .env
  if [ -z "$PARAMS_ROOT" ]; then
    export PARAMS_ROOT="$PWD/data/params"
  fi
  if [ -z "$SWAGLOG_ROOT" ]; then
    export SWAGLOG_ROOT="$PWD/data/log"
  fi
  if [ -z "$LOG_ROOT" ]; then
    export LOG_ROOT="$PWD/data/realdata"
  fi
  if [ -z "$MODEL_ROOT" ]; then
    export MODEL_ROOT="$PWD/data/models"
  fi
  if [ -z "$CRASH_LOG_ROOT" ]; then
    export CRASH_LOG_ROOT="$PWD/data/crashes"
  fi
  if [ -z "$MAPD_ROOT" ]; then
    export MAPD_ROOT="$PWD/data/osm"
  fi
  if [ -z "$COMMA_CACHE" ]; then
    export COMMA_CACHE="$PWD/data/cache"
  fi
  if [ -z "$PERSIST_ROOT" ]; then
    export PERSIST_ROOT="$PWD/data/persist"
  fi
  if [ -z "$STATS_ROOT" ]; then
    export STATS_ROOT="$PWD/data/stats"
  fi
  if [ -z "$CONFIG_ROOT" ]; then
    export CONFIG_ROOT="$PWD/data/config"
  fi

  # Create all required directories
  echo "Creating required directories..."
  mkdir -p "$PARAMS_ROOT/d" /tmp/openpilot
  mkdir -p "$SWAGLOG_ROOT"
  mkdir -p "$LOG_ROOT"
  mkdir -p "$MODEL_ROOT"
  mkdir -p "$CRASH_LOG_ROOT"
  mkdir -p "$MAPD_ROOT"
  mkdir -p "$COMMA_CACHE"
  mkdir -p "$PERSIST_ROOT"
  mkdir -p "$STATS_ROOT"
  mkdir -p "$CONFIG_ROOT"

  # Set default parameters if not already set
  if [ ! -f "$PARAMS_ROOT/d/LanguageSetting" ]; then
    echo -n "main_en" > "$PARAMS_ROOT/d/LanguageSetting"
  fi

  # Set HardwareC3xLite to 1 by default (forced)
  echo "1" > "$PARAMS_ROOT/d/HardwareC3xLite"

  # Set DisableDM to 1 by default (forced)
  echo "1" > "$PARAMS_ROOT/d/DisableDM"

  echo "PC environment configuration completed successfully!"
fi

function agnos_init {
  # TODO: move this to agnos
  sudo rm -f /data/etc/NetworkManager/system-connections/*.nmmeta

  # set success flag for current boot slot
  sudo abctl --set_success

  # TODO: do this without udev in AGNOS
  # udev does this, but sometimes we startup faster
  sudo chgrp gpu /dev/adsprpc-smd /dev/ion /dev/kgsl-3d0
  sudo chmod 660 /dev/adsprpc-smd /dev/ion /dev/kgsl-3d0

  # Check if AGNOS update is required
  if [ $(< /VERSION) != "$AGNOS_VERSION" ]; then
    AGNOS_PY="$DIR/system/hardware/tici/agnos.py"
    MANIFEST="$DIR/system/hardware/tici/agnos.json"
    if $AGNOS_PY --verify $MANIFEST; then
      sudo reboot
    fi
    $DIR/system/hardware/tici/updater $AGNOS_PY $MANIFEST
  fi
}

function launch {
  # Remove orphaned git lock if it exists on boot
  [ -f "$DIR/.git/index.lock" ] && rm -f $DIR/.git/index.lock

  # Check to see if there's a valid overlay-based update available. Conditions
  # are as follows:
  #
  # 1. The DIR init file has to exist, with a newer modtime than anything in
  #    the DIR Git repo. This checks for local development work or the user
  #    switching branches/forks, which should not be overwritten.
  # 2. The FINALIZED consistent file has to exist, indicating there's an update
  #    that completed successfully and synced to disk.

  if [ -f "${DIR}/.overlay_init" ]; then
    find ${DIR}/.git -newer ${DIR}/.overlay_init | grep -q '.' 2> /dev/null
    if [ $? -eq 0 ]; then
      echo "${DIR} has been modified, skipping overlay update installation"
    else
      if [ -f "${STAGING_ROOT}/finalized/.overlay_consistent" ]; then
        if [ ! -d /data/safe_staging/old_openpilot ]; then
          echo "Valid overlay update found, installing"
          LAUNCHER_LOCATION="${BASH_SOURCE[0]}"

          mv $DIR /data/safe_staging/old_openpilot
          mv "${STAGING_ROOT}/finalized" $DIR
          cd $DIR

          echo "Restarting launch script ${LAUNCHER_LOCATION}"
          unset AGNOS_VERSION
          exec "${LAUNCHER_LOCATION}"
        else
          echo "openpilot backup found, not updating"
          # TODO: restore backup? This means the updater didn't start after swapping
        fi
      fi
    fi
  fi

  # handle pythonpath
  ln -sfn $(pwd) /data/pythonpath
  export PYTHONPATH="$PWD"

  # hardware specific init
  if [ -f /AGNOS ]; then
    agnos_init
  fi

  # write tmux scrollback to a file
  tmux capture-pane -pq -S-1000 > /tmp/launch_log

  # start manager
  cd system/manager
  if [ ! -f $DIR/prebuilt ]; then
    ./build.py
  fi
  ./manager.py

  # if broken, keep on screen error
  while true; do sleep 1; done
}

launch
