#pragma once
#include <memory>
#include <QPushButton>
#include <QStackedLayout>
#include <QWidget>
#include "common/util.h"
#include "selfdrive/ui/ui.h"
#ifdef QCOM
#include "selfdrive/ui/qt/widgets/cameraview_qcom.h"
#else
#include "selfdrive/ui/qt/widgets/cameraview.h"
#endif
#include "selfdrive/ui/qt/developer_ui/developer_ui.h"
#include <QPen>
#include <QBrush>
const int btn_size = 170;
const int img_size = (btn_size / 4) * 3;


// ***** onroad widgets *****
class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {}
  void updateAlert(const Alert &a);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor bg;
  Alert alert = {};
  int dev_ui_info = 0;
};

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;
};


class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT
  Q_PROPERTY(float speed MEMBER speed);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit);
  Q_PROPERTY(float setSpeed MEMBER setSpeed);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set);
  Q_PROPERTY(bool is_metric MEMBER is_metric);

  Q_PROPERTY(bool hideBottomIcons MEMBER hideBottomIcons);
  Q_PROPERTY(int status MEMBER status);


  Q_PROPERTY(bool use_lanelines MEMBER use_lanelines);
  Q_PROPERTY(bool showDateTime MEMBER show_date_time)

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

  static constexpr int DEV_UI_BAR_HEIGHT = 81;
  static constexpr int DEV_UI_TEXT_OFFSET = 20;

private:
  // 定义渲染模式：告诉函数这次是画背景/单位，还是画数值
  enum DevUiRenderMode {
    MODE_STATIC,  // 静态模式：绘制背景、标题、单位
    MODE_DYNAMIC  // 动态模式：只绘制变化的数值
  };

  // 静态缓存相关变量
  QPixmap staticDevUiBuffer;     // 存储整屏幕的静态内容（背景条+标题+单位）
  bool static_ui_dirty = true;   // 标记是否需要重新生成静态缓存
  bool last_is_metric = false;   // 记录上一次的单位制，用于检测切换

  // 动态缓存相关变量
  QPixmap dynamicDevUiBuffer;    // 存储动态内容（数值）

  // 时间显示缓存相关变量
  QPixmap timeDisplayBuffer;     // 存储时间显示

  // 帧计数器：用于替代时间戳控制刷新频率
  uint64_t frame_count = 0;

  QPen devUiPen;
  QBrush devUiBrush;
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  // 专门用于更新静态缓存的函数
  void updateStaticDevUi();
  // ############################## DEV UI START ##############################
  void drawColoredText(QPainter &p, int x, int y, const QString &text, QColor color);
  void drawRightDevUi(QPainter &p, int x, int y, DevUiRenderMode mode);
  void drawLeftDevUi(QPainter &p, int x, int y, DevUiRenderMode mode);
  int drawDevUiRight(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, DevUiRenderMode mode);
  int drawDevUiLeft(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, DevUiRenderMode mode);
  int drawNewDevUi(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, int total_width, DevUiRenderMode mode);
  void drawNewDevUi2(QPainter &p, int x, int y, DevUiRenderMode mode);
  void drawNewDevUi3(QPainter &p, int x, int y, DevUiRenderMode mode);
  void drawCenteredLeftText(QPainter &p, int x, int y, const QString &text1, QColor color1, const QString &text2, const QString &text3, QColor color2, int total_width, DevUiRenderMode mode);
  // ############################## DEV UI END ##############################

  QVBoxLayout *main_layout;
  ExperimentalButton *experimental_btn;
  float speed;
  QString speedUnit;
  float setSpeed;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool hideBottomIcons = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  std::unique_ptr<PubMaster> pm;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;


  bool use_lanelines = false;

  // ############################## DEV UI START ##############################
  // 缓存 UI 元素，避免 paint 循环中创建
  UiElement cached_torqueDyn;
  UiElement cached_tuneParamsAF;
  UiElement cached_tuneParamsPI;
  UiElement cached_longTune;
  UiElement cached_epsTorque;
  UiElement cached_aEgo;
  UiElement cached_vEgoLead;
  UiElement cached_visTurn;
  UiElement cached_satState;

  bool lead_status;
  float lead_d_rel = 0;
  float lead_v_rel = 0;
  QString lateralState;
  float angleSteers = 0;
  float steerAngleDesired = 0;
  float curvature;
  float roll;
  //int memoryUsagePercent;
  int devUiInfo;
  //float gpsAccuracy;
  //float altitude;
  float vEgo;
  float aEgo;
  float steeringTorqueEps;
  //float bearingAccuracyDeg;
  //float bearingDeg;
  bool torquedUseParams;
  float latAccelFactorFiltered;
  float frictionCoefficientFiltered;
  bool liveValid;
  float kpValue;
  float kiValue;
  bool reversing;
  QString torqueTuneParamsAFStr = "-";
  QString torqueTuneParamsPIStr = "-";
  QString longTuneParamsStr = "-";
  QColor torqueTuneParamsColor;
  QColor longTuneParamsColor;
  int torqueTuneTimer = 0;
  int paramsUpdateTimer = 0;
  float f_val = 0;
  float e_val = 0;
  int longSrcVal = -1;
  float l_prob = 0.0;
  float r_prob = 0.0;
  QString laneProbStr = "-";
  QColor laneProbColor;
  int saturated = 0;
  //float engineRPM;
  bool acmActive = false;  // 添加 ACM 激活状态变量
  float desired_follow_distance;
  bool longActive = false;
  bool opLongControl = false;  // 是否使用OP纵向控制
  bool e2eActive = false;      // 端到端是否激活
  bool lat_active = false;     // 横向控制是否激活
  bool alka_enabled = false;   // ALKA功能是否启用
  bool dp_lon_acm = false;     // ACM功能是否启用
  bool show_date_time = false;  // 是否显示日期时间
  int visionTurnControllerState = 0;  // 视觉弯道控制器状态
  float visionTurnSpeed = 0.0f;  // 视觉弯道速度

  // 车辆默认参数 (从 carParams 获取，用于 Override 关闭时显示)
  float torque_kp_default = 0;
  float torque_ki_default = 0;
  float long_kp_default = 0;
  float long_ki_default = 0;
  // ############################## DEV UI END ##############################

  // 优化：预创建QRect对象，避免在drawKnightScanner中每帧创建
  QRect knightScannerRect;
  // 优化：缓存前车距离和TTC值，避免不必要的文本重绘
  float cached_lead_dist = -1.0f;
  float cached_lead_ttc = -1.0f;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd, float v_ego);
  void drawHud(QPainter &p);
  #ifndef QCOM
  void drawDriverState(QPainter &painter, const UIState *s);
  #endif

  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;
  const int radius = 192;

  double lane_lines_start_time = 0;

  //dp
  void drawKnightScanner(QPainter &p);
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);


private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadAlerts *alerts;
  AnnotatedCameraWidget *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];

  QHBoxLayout* split;
  // dp indicators
  bool dp_brake_pressed = false;
  bool dp_blinker_left = false;
  bool dp_blinker_right = false;
  bool dp_bsm_left = false;
  bool dp_bsm_right = false;
  bool dp_indicator_left_show = false;
  bool dp_indicator_right_show = false;
  int dp_indicator_left_count = 0;
  int dp_indicator_right_count = 0;
  bool dp_repaint_prev = false;
  void updateIndicatorState(bool blinkerState, bool bsmState, bool& indicatorShow, int& indicatorCount, QColor& indicatorColor);
  QColor dp_indicator_left_color;
  QColor dp_indicator_right_color;
  QColor dp_yellow_color = QColor(0xff, 0xff, 0, 255);
  QColor dp_green_color = QColor(0, 0xff, 0, 255);

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
