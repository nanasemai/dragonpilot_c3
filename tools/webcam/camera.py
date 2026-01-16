import av
import cv2 as cv
import platform

class Camera:
  def __init__(self, cam_type_state, stream_type, camera_id):
    try:
      camera_id = int(camera_id)
    except ValueError: # allow strings, ex: /dev/video0
      pass
    self.cam_type_state = cam_type_state
    self.stream_type = stream_type
    self.cur_frame_id = 0

    print(f"Opening {cam_type_state} at {camera_id}")

    self.cap = cv.VideoCapture(camera_id)

    self.cap.set(cv.CAP_PROP_FRAME_WIDTH, 1280.0)
    self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, 720.0)
    self.cap.set(cv.CAP_PROP_FPS, 25.0)

    self.W = self.cap.get(cv.CAP_PROP_FRAME_WIDTH)
    self.H = self.cap.get(cv.CAP_PROP_FRAME_HEIGHT)

  @classmethod
  def bgr2nv12(self, bgr):
    frame = av.VideoFrame.from_ndarray(bgr, format='bgr24')
    return frame.reformat(format='nv12').to_ndarray()

  def read_frames(self):
    while True:
      ret, frame = self.cap.read()
      if not ret:
        break
      # Rotate the frame 180 degrees (flip both axes)
      frame = cv.flip(frame, -1)
      yuv = Camera.bgr2nv12(frame)
      yield yuv.data.tobytes()
    self.cap.release()

class CameraMJPG:
    def __init__(self, cam_type_state, stream_type, camera_id):
        try:
            camera_id = int(camera_id)
        except ValueError:
            pass

        if platform.system() == "Darwin":
          camera_id = int(camera_id[-1])
          self.cap = cv.VideoCapture(camera_id, cv.CAP_AVFOUNDATION)
        else:
          self.cap = cv.VideoCapture(camera_id)

        if not self.cap.isOpened():
            raise IOError(f"无法打开摄像头设备 {camera_id}")

        # 优先尝试设置 MJPG 格式（高分辨率 + 高帧率）
        self._configure_camera_format("MJPG")
        actual_format = self._get_current_format()
        print("数据格式: ", actual_format)

        # 获取实际设置的FPS并打印
        self.fps = self.cap.get(cv.CAP_PROP_FPS)
        print(f"摄像头初始化后的FPS设置: {self.fps}")

        # ====== 延迟优化配置 ======
        # 实测结果 (2026-01-17, 更换USB口后):
        # - BUF=3: 1080P MJPG 达到 19.8 FPS ✅
        # - BUF=2: 1080P MJPG 只有 ~17 FPS
        # - BUF=1: 只有 ~10 FPS
        # 推荐使用 BUF=3 获得最佳帧率
        self.cap.set(cv.CAP_PROP_BUFFERSIZE, 3)
        print("缓冲区大小: 3")

        # 获取分辨率
        self.W = int(self.cap.get(cv.CAP_PROP_FRAME_WIDTH))
        self.H = int(self.cap.get(cv.CAP_PROP_FRAME_HEIGHT))
        self.cur_frame_id = 0
        self.cam_type_state = cam_type_state
        self.stream_type = stream_type
        self.current_format = actual_format

        # 1080P 可达 ~17 FPS，满足驾驶辅助需求
        # 如果需要更高帧率，可以考虑降级到 720P 或 VGA
        if self.fps < 10:
            print(f"警告: 当前帧率过低 ({self.fps:.1f} FPS)，尝试降级到 720P")
            self.cap.set(cv.CAP_PROP_FRAME_WIDTH, 1280)
            self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, 720)
            self.cap.set(cv.CAP_PROP_FPS, 20)
            self.W = 1280
            self.H = 720
            self.fps = 20
            print(f"调整后: {self.W}x{self.H} @ {self.fps:.0f} FPS")

    def _configure_camera_format(self, target_fourcc):
        """尝试设置摄像头的FourCC格式

        优化策略: 使用 MJPG 压缩格式 + 1920x1080 分辨率
        实测可达 20 FPS，满足实时驾驶需求
        """
        fourcc = cv.VideoWriter_fourcc(*target_fourcc)
        self.cap.set(cv.CAP_PROP_FOURCC, fourcc)
        self.cap.set(cv.CAP_PROP_FOURCC, fourcc)
        self.cap.set(cv.CAP_PROP_FRAME_WIDTH, 1920)  # 1920x1080 Full HD
        self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, 1080)
        self.cap.set(cv.CAP_PROP_FPS, 20)  # 目标帧率


    def _get_current_format(self):
        """获取当前实际格式"""
        fourcc_code = int(self.cap.get(cv.CAP_PROP_FOURCC))
        return ''.join([chr((fourcc_code >> 8 * i) & 0xFF) for i in range(4)])

    @staticmethod
    def _bgr_to_nv12(bgr_frame):
        frame = av.VideoFrame.from_ndarray(bgr_frame, format='bgr24')
        return frame.reformat(format='nv12').to_ndarray().data.tobytes()

    def read_frames(self):
        """持续读取帧并转换为 NV12"""
        while True:
            ret, frame = self.cap.read()
            if not ret:
                break

            if self.current_format == "MJPG":
                # 直接解码为 NV12 格式，避免中间转换
                # 使用更高效的路径
                if frame.shape != (self.H, self.W, 3):
                    raise ValueError("MJPG 解码后帧形状异常，请检查摄像头设置")
                yield self._bgr_to_nv12(frame)
            else:
                # print("MJPEG", self.W, "  ", self.H)
                yield self._bgr_to_nv12(frame)
        self.cap.release()

    def __del__(self):
        if hasattr(self, 'cap') and self.cap.isOpened():
            self.cap.release()