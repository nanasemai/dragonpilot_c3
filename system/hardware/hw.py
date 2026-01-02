import os
import platform
from pathlib import Path

from openpilot.system.hardware import PC

DEFAULT_DOWNLOAD_CACHE_ROOT = "/tmp/comma_download_cache"

class Paths:
  @staticmethod
  def comma_home() -> str:
    return os.path.join(str(Path.home()), ".comma" + os.environ.get("OPENPILOT_PREFIX", ""))

  @staticmethod
  def log_root() -> str:
    if os.environ.get('LOG_ROOT', False):
      return os.environ['LOG_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "media" / "0" / "realdata")
    else:
      return '/data/media/0/realdata/'

  @staticmethod
  def swaglog_root() -> str:
    if os.environ.get('SWAGLOG_ROOT', False):
      return os.environ['SWAGLOG_ROOT']
    elif PC:
      project_root = Path(__file__).parent.parent.parent.parent
      return str(project_root / "data" / "log")
    else:
      return "/data/log/"

  @staticmethod
  def swaglog_ipc() -> str:
    return "ipc:///tmp/logmessage" + os.environ.get("OPENPILOT_PREFIX", "")

  @staticmethod
  def download_cache_root() -> str:
    if os.environ.get('COMMA_CACHE', False):
      return os.environ['COMMA_CACHE'] + "/"
    return DEFAULT_DOWNLOAD_CACHE_ROOT + os.environ.get("OPENPILOT_PREFIX", "") + "/"

  @staticmethod
  def persist_root() -> str:
    if os.environ.get('PERSIST_ROOT', False):
      return os.environ['PERSIST_ROOT']
    elif PC:
      return os.path.join(Paths.comma_home(), "persist")
    else:
      return "/persist/"

  @staticmethod
  def stats_root() -> str:
    if os.environ.get('STATS_ROOT', False):
      return os.environ['STATS_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "stats")
    else:
      return "/data/stats/"

  @staticmethod
  def config_root() -> str:
    if os.environ.get('CONFIG_ROOT', False):
      return os.environ['CONFIG_ROOT']
    elif PC:
      return Paths.comma_home()
    else:
      return "/tmp/.comma"

  @staticmethod
  def shm_path() -> str:
    if PC and platform.system() == "Darwin":
      return "/tmp"  # This is not really shared memory on macOS, but it's the closest we can get
    return "/dev/shm"

  @staticmethod
  def model_root() -> str:
    if os.environ.get('MODEL_ROOT', False):
      return os.environ['MODEL_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "media" / "0" / "models")
    else:
      return "/data/media/0/models"

  @staticmethod
  def crash_log_root() -> str:
    if os.environ.get('CRASH_LOG_ROOT', False):
      return os.environ['CRASH_LOG_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "community" / "crashes")
    else:
      return "/data/community/crashes"

  @staticmethod
  def mapd_root() -> str:
    if os.environ.get('MAPD_ROOT', False):
      return os.environ['MAPD_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "media" / "0" / "osm")
    else:
      return "/data/media/0/osm"
