#include "selfdrive/ui/qt/onroad.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

#include <QDebug>
#include <QMouseEvent>
#include "common/swaglog.h"
#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"


static void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.setOpacity(opacity);
  p.drawPixmap(center - QPoint(img.width() / 2, img.height() / 2), img);
  p.setOpacity(1.0);
}

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, true, this);
    split->insertWidget(0, arCam);
  }



  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

// Function to update indicator state
void OnroadWindow::updateIndicatorState(bool blinkerState, bool bsmState, bool& indicatorShow, int& indicatorCount, QColor& indicatorColor) {
  if (!blinkerState && !bsmState) {
    indicatorShow = false;
    indicatorCount = 0;
  } else {
    indicatorCount += 1;

    if (bsmState && blinkerState) {
      indicatorShow = indicatorCount % 4 == 0? !indicatorShow : indicatorShow;
      indicatorColor = dp_yellow_color;
    } else if (blinkerState) {
      indicatorShow = indicatorCount % 8 == 0? !indicatorShow : indicatorShow;
      indicatorColor = dp_green_color;
    } else {
      indicatorShow = bsmState;
      indicatorColor = dp_yellow_color;
    }
  }
}

void OnroadWindow::updateState(const UIState &s) {
  if (!s.scene.started) {
    return;
  }

  QColor bgColor = bg_colors[s.scene.lat_active && s.scene.alka_active && s.status == STATUS_DISENGAGED? STATUS_ALKA : s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  alerts->updateAlert(alert);

  nvg->updateState(s);

  // dp blinker & bsm
  const SubMaster& sm = *(s.sm);
  auto cs = sm["carState"].getCarState();
  dp_brake_pressed = cs.getBrakePressed();
  dp_blinker_left = cs.getLeftBlinker();
  dp_blinker_right = cs.getRightBlinker();
  dp_bsm_left = cs.getLeftBlindspot();
  dp_bsm_right = cs.getRightBlindspot();

  // left
  updateIndicatorState(dp_blinker_left, dp_bsm_left, dp_indicator_left_show, dp_indicator_left_count, dp_indicator_left_color);

  // right
  updateIndicatorState(dp_blinker_right, dp_bsm_right, dp_indicator_right_show, dp_indicator_right_count, dp_indicator_right_color);

  bool dp_repaint = dp_indicator_left_show || dp_indicator_right_show || dp_brake_pressed;

  // repaint border
  if (bg != bgColor || dp_repaint || dp_repaint != dp_repaint_prev) {
    // repaint border
    bg = bgColor;
    dp_repaint_prev = dp_repaint;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
  alerts->updateAlert({});
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));

  // dp - draw brake
  if (dp_brake_pressed) {
    p.fillRect(
      QRect(0, height() - UI_BORDER_SIZE, width(), 30),
      QColor(0xff, 0, 0, 255)
    );
  }
  // dp - draw indicators
  if (dp_indicator_left_show) {
    p.fillRect(
      QRect(0, 0, width()*0.2, height()),
      dp_indicator_left_color
    );
  }
  if (dp_indicator_right_show) {
    p.fillRect(
      QRect(width()*0.8, 0, width()*0.2, height()),
      dp_indicator_right_color
    );
  }
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a) {
  if (!alert.equal(a)) {
    alert = a;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_heights = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_heights[alert.size];

  int margin = 40;
  int radius = 30;
  if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    margin = 0;
    radius = 0;
  }
  // Adjust position to be above developer UI bar (80px tall)
  // Reserve basic margins always, and extra space for left/right UI elements when DEV UI is enabled and not FULL size
  int left_offset = margin;
  int right_reserved = margin;
  if (alert.size != cereal::ControlsState::AlertSize::FULL) {
    // When DEV UI is enabled and alert is not FULL, reserve space for left/right UI panels
    left_offset = UI_BORDER_SIZE + 184;   // Space for left UI
    right_reserved = UI_BORDER_SIZE + 184; // Space for right UI
  }
  // Otherwise, keep basic margin for all alert types
  int available_width = width() - left_offset - right_reserved;
  QRect r = QRect(left_offset, height() - h - 40 + margin, available_width, h - margin*2);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setBrush(QBrush(alert_colors[alert.status]));
//  p.drawRoundedRect(r, radius, radius);
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
//  p.drawRoundedRect(r, radius, radius);
  p.drawRect(r);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    p.setFont(InterFont(74, QFont::DemiBold));
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    p.setFont(InterFont(88, QFont::Bold));
    p.drawText(QRect(r.x(), c.y() - 125, r.width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    p.setFont(InterFont(66));
    p.drawText(QRect(r.x(), c.y() + 21, r.width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    p.setFont(InterFont(l ? 132 : 177, QFont::Bold));
    p.drawText(QRect(r.x(), r.y() + (l ? 240 : 270), r.width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    p.setFont(InterFont(88));
    p.drawText(QRect(r.x(), r.height() - (l ? 361 : 420), r.width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// ExperimentalButton
ExperimentalButton::ExperimentalButton(QWidget *parent) : experimental_mode(false), engageable(false), QPushButton(parent) {
  setFixedSize(btn_size, btn_size);
  //params = Params();
  engage_img = loadBased64Image("iVBORw0KGgoAAAANSUhEUgAAAQoAAAEKCAMAAADdFev7AAAAVFBMVEVHcEz///////////////////////////////////////////////////////////////////////////////////////////////////////////+DS+nTAAAAG3RSTlMAKEIf6zXh+gTzUBYOCM3XwoaRcHq4nVymZbCV8JeNAAAgAElEQVR42uxc2WKrOgysAe/GNmaH///PC17AEHoKSdqk5x6eWkIKlqXRaCT68fHv+Hf8O/4dv+cg/0zgD9g1Rdmm/69F86Qvmq6p2GbdbJwPs/gGgTDl+V/tKqCQ1K56xKqIjNHbc1myGExmSGr29xoi79EYHXJZ+UflznR5MFk2/9r/tZYQBXYOQb1nZMu2G3eChhNwvgIvHxPG/i4cqawlZMVg0mu7dAT9R513lEF47JgvpXCNF6wM43+NJRLr9I3bXm5wFBF57U2BW3dta50GLF+1TqJM8iutQYTYJoB8mJdXh8UI+2vmNl7IgB/afV7OP6t0Fz8jlkUifpcd0tYMWndlHOAw24S/2+mxctcrGzrTGeygsrC/h1VztWIt1eXvcY08aZDDx1H1+XK63W70h7Bw0ZE1YZSzo0h7RWM9KHyZOZBR/q9K9ksIB+zouoe4yDf5Ukfu3aw77xOGdZTZT8iwminERwbTtnPJmFb5b7AEQz5heluU4YNibwprG2SdILEJIyFzIlEgeIwJ4aaCjxDQa7z5q+9sCevqyrRJW8c46FeuojhvV9wMCcPiSRHAodjEh1+9sCk5g+8fHbNPYJO6fDljHCAxt17zY8iQbEkYk4MQ48hGimKy2Yzx4vMu9pj3zaD2qY2PZF5FmY/4IIjqkZVcu4TBvSmN846RxfEx5Btvkt69CHhT2LBLUOnnnwUKtS6xXPZd594oGbQOE8zW4m09YuMF+ZtA1bxnrFhH73aVByTRyquoXF9M4RLGMF8H5pNNG+EB2caHdyFvivnDzIA3NEUTo53z31L7QsNlhSYKEBTcJP5oRtdsiDbe4QbWJXShYI3lXCjwtPr9GCjZIRpnDVo9odlxLOvoFkfjhJHOHBxH2ab1xGrM6hJynrhCrorI/PiOqbWLSCIBlXSrkPGalsd2RYhFQ7fx/pM+LN1xENJFCkemlOMryMUEo5t8/VZl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", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
  QObject::connect(this, &QPushButton::clicked, this, &ExperimentalButton::changeMode);
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && Params().getBool("ExperimentalModeConfirmed");
  if (can_change) {
    Params().putBool("ExperimentalMode", !experimental_mode);
  }
}

void ExperimentalButton::updateState(const UIState &s) {
  const auto cs = (*s.sm)["controlsState"].getControlsState();
  bool eng = cs.getEngageable() || cs.getEnabled();
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
    update();
  }
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  QPixmap img = experimental_mode ? experimental_img : engage_img;
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2), img, QColor(0, 0, 0, 166), (isDown() || !engageable) ? 0.6 : 1.0);
}


AnnotatedCameraWidget::AnnotatedCameraWidget(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraWidget("camerad", type, true, parent) {
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"uiDebug"});

  // 初始化 devUiPen 和 devUiBrush
  devUiPen = QPen(Qt::NoPen);
  devUiBrush = QBrush(QColor(0, 0, 0, 100));

  main_layout = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  main_layout->setSpacing(0);

  experimental_btn = new ExperimentalButton(this);
  main_layout->addWidget(experimental_btn, 0, Qt::AlignTop | Qt::AlignRight);

  // 初始化调参字符串变量
  torqueTuneParamsAFStr = "-";
  torqueTuneParamsPIStr = "-";
  longTuneParamsStr = "-";
}

void AnnotatedCameraWidget::updateState(const UIState &s) {
  // 1. 初始化定时器 (静态变量，生命周期伴随程序)
  static int frame_timer = 0;
  frame_timer++;

  // ----------------- 高频部分 (每一帧都跑) -----------------
  // 这部分负责 UI 的流畅动画，不能卡顿
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);
  const bool cs_alive = sm.alive("controlsState");
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();

  // 处理巡航速度 (保持原样)
  float v_cruise =  cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  float set_speed = cs_alive ? v_cruise : SET_SPEED_NA;
  bool cruise_set = set_speed > 0 && (int)set_speed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    set_speed *= KM_TO_MILE;
  }

  // 处理当前车速 (保持原样)
  float v_ego;
  if (car_state.getVEgoCluster() == 0.0 && !v_ego_cluster_seen) {
    v_ego = car_state.getVEgo();
  } else {
    v_ego = car_state.getVEgoCluster();
    v_ego_cluster_seen = true;
  }
  float cur_speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  cur_speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  // 设置属性 (保持原样)
  setProperty("is_cruise_set", cruise_set);
  setProperty("is_metric", s.scene.is_metric);
  setProperty("speed", cur_speed);
  setProperty("setSpeed", set_speed);
  setProperty("speedUnit", s.scene.is_metric ? tr("km/h") : tr("mph"));
  setProperty("status", s.status);

  // 更新按钮状态 (保持原样)
  experimental_btn->updateState(s);
  setProperty("use_lanelines", sm["lateralPlan"].getLateralPlan().getUseLaneLines());

  // 关键数据：为了绘制 Lead 车和仪表盘，这些需要高频更新
  bool radar_alive = sm.alive("radarState");
  if (radar_alive) {
    const auto& lead_one = sm["radarState"].getRadarState().getLeadOne();
    lead_d_rel = lead_one.getDRel();
    lead_v_rel = lead_one.getVRel();
    lead_status = lead_one.getStatus();
  } else {
    lead_d_rel = 0; lead_v_rel = 0; lead_status = false;
  }

  // 关键数据：仪表盘指针/数值
  vEgo = car_state.getVEgo();
  aEgo = car_state.getAEgo();
  angleSteers = car_state.getSteeringAngleDeg();
  steeringTorqueEps = car_state.getSteeringTorqueEps();
  steerAngleDesired = cs.getLateralControlState().getPidState().getSteeringAngleDesiredDeg();
  curvature = cs.getCurvature();

  // ----------------- 中频部分 (每 20 帧更新一次，约 1秒) -----------------
  // 字符串转换、规划模式、非动画类数据、车辆默认参数初始化放在这里
  // 优化：从15帧改为20帧，减少约33%的更新频率
  if (frame_timer % 20 == 0) {
      devUiInfo = s.scene.dev_ui_info;

      // 1. Lateral State (字符串转换消耗性能，不需要每帧做)
      if (sm.alive("controlsStateExt")) {
        lateralState = QString::fromStdString(sm["controlsStateExt"].getControlsStateExt().getLateralState());
      } else {
        lateralState = "-";
      }
      torquedUseParams = (lateralState == "torque");

      // 2. 纵向控制状态信息 (中频更新)
      // longActive 已经在低频部分更新，这里不需要重复获取

      if (sm.alive("carParams")) {
        opLongControl = sm["carParams"].getCarParams().getOpenpilotLongitudinalControl();
      }

      if (sm.alive("longitudinalPlanExt")) {
        e2eActive = sm["longitudinalPlanExt"].getLongitudinalPlanExt().getDpE2EIsBlended();
      }

      // 3. 横向控制相关状态 (中频更新)
      lat_active =s.scene.lat_active;
      alka_enabled = s.scene.alka_enabled;

      // 4. 缓存配置参数 (中频更新)
      dp_lon_acm = s.scene.dp_lon_acm;
      show_date_time = s.scene.dp_show_date_time;

      // 3. 初始化车辆默认参数 (从 carParams 获取，只执行一次，后续低频部分直接使用缓存值)
      if (sm.alive("carParams")) {
        auto cp = sm["carParams"].getCarParams();
        torque_kp_default = cp.getLateralTuning().getTorque().getKp();
        torque_ki_default = cp.getLateralTuning().getTorque().getKi();
        if (cp.getLongitudinalTuning().getKpV().size() > 0) long_kp_default = cp.getLongitudinalTuning().getKpV()[0];
        if (cp.getLongitudinalTuning().getKiV().size() > 0) long_ki_default = cp.getLongitudinalTuning().getKiV()[0];
      }

      // 3. Torque Live Params (实时学习参数)
      if (sm.alive("liveTorqueParameters")) {
        auto ltp = sm["liveTorqueParameters"].getLiveTorqueParameters();
        latAccelFactorFiltered = ltp.getLatAccelFactorFiltered();
        frictionCoefficientFiltered = ltp.getFrictionCoefficientFiltered();
        liveValid = ltp.getLiveValid();
      } else {
        latAccelFactorFiltered = 0; frictionCoefficientFiltered = 0; liveValid = false;
      }

      // 4. Longitudinal Plan Ext (ACM, Vision Turn) - 规划数据变化慢
      if (sm.alive("longitudinalPlanExt")) {
        auto plan_ext = sm["longitudinalPlanExt"].getLongitudinalPlanExt();
        acmActive = plan_ext.getAcmActive();
        desired_follow_distance = plan_ext.getDesiredFollowDistance();
        visionTurnControllerState = static_cast<int>(plan_ext.getVisionTurnControllerState());
        visionTurnSpeed = plan_ext.getVisionTurnSpeed();
        longSrcVal = static_cast<int>(plan_ext.getLongitudinalPlanExtSource());
      } else {
        acmActive = false; desired_follow_distance = 0;
        visionTurnControllerState = 0; visionTurnSpeed = 0; longSrcVal = -1;
      }

      longActive = cs.getEnabled() && !car_state.getGasPressed();

      // 5. 车道线概率 (复用你原有的逻辑，放进来)
      if (sm.alive("modelV2")) {
        auto model = sm["modelV2"].getModelV2();
        auto lane_line_probs = model.getLaneLineProbs();
        if (lane_line_probs.size() > 2) {
          l_prob = lane_line_probs[1];
          r_prob = lane_line_probs[2];
        }
      } else {
        l_prob = 0.0; r_prob = 0.0;
      }

      // 格式化车道线字符串
      if (l_prob > 0 || r_prob > 0) {
        laneProbStr = QString("%1|%2").arg(l_prob, 0, 'f', 1).arg(r_prob, 0, 'f', 1);
        laneProbColor = ((l_prob + r_prob) / 2 < 0.3) ? QColor(255, 80, 80, 255) : QColor(255, 255, 255, 255);
      } else {
        laneProbStr = "-";
        laneProbColor = QColor(150, 150, 150, 255);
      }

      // 7. 更新 Torque/PID 的 P/I/F/E 值 (数值读取，放在中频即可)
      if (lateralState == "torque") {
        // 扭矩状态获取方式需要检查正确的字段名
        kpValue = 0; kiValue = 0; f_val = 0; e_val = 0;
      } else {
        kpValue = 0; kiValue = 0; f_val = 0; e_val = 0;
      }

      // 8. 实时数据格式化 (Override 关闭时使用实时数据)
      if (torquedUseParams) {
        if (!s.scene.torqued_override) {
          // 横向参数：使用实时学习值或默认值
          torqueTuneParamsAFStr = QString("A:%1 F:%2").arg(latAccelFactorFiltered, 0, 'f', 3).arg(frictionCoefficientFiltered, 0, 'f', 3);
          torqueTuneParamsPIStr = QString("P:%1 I:%2").arg(torque_kp_default, 0, 'f', 2).arg(torque_ki_default, 0, 'f', 2);
          torqueTuneParamsColor = QColor(255, 255, 255, 200);
        }

        // 纵向参数：使用默认值
        longTuneParamsStr = QString("P:%1 I:%2").arg(long_kp_default, 0, 'f', 2).arg(long_ki_default, 0, 'f', 2);
        longTuneParamsColor = QColor(255, 255, 255, 200);
      }
  }

  // ----------------- 低频部分 (每 50 帧更新一次，约 2.5秒) -----------------
  // 涉及到 Params() 读取、字符串格式化等重操作
  if (frame_timer % 50 == 0) {
      // 3. Params 参数格式化 (Override 开启时使用手动设置参数)
      if (torquedUseParams) {
        if (s.scene.torqued_override) {
          // 横向参数：使用UIState中的用户手动设置参数
          try {
            float la = std::stof(s.scene.dp_torque_lat_accel_factor.empty() ? "2.5" : s.scene.dp_torque_lat_accel_factor) * 0.01;
            float fric = std::stof(s.scene.dp_torque_friction.empty() ? "0.2" : s.scene.dp_torque_friction) * 0.001;
            float kp = std::stof(s.scene.dp_lateral_torque_kp.empty() ? "1.0" : s.scene.dp_lateral_torque_kp) * 0.01;
            float ki = std::stof(s.scene.dp_lateral_torque_ki.empty() ? "0.1" : s.scene.dp_lateral_torque_ki) * 0.01;
            torqueTuneParamsAFStr = QString("A:%1 F:%2").arg(la, 0, 'f', 3).arg(fric, 0, 'f', 3);
            torqueTuneParamsPIStr = QString("P:%1 I:%2").arg(kp, 0, 'f', 2).arg(ki, 0, 'f', 2);
            torqueTuneParamsColor = QColor(144, 238, 144, 255);
          } catch (const std::exception &e) {
            LOGW("Failed to parse torque params: %s", e.what());
            torqueTuneParamsAFStr = QString("A:%1 F:%2").arg(0.025, 0, 'f', 3).arg(0.0002, 0, 'f', 3);
            torqueTuneParamsPIStr = QString("P:%1 I:%2").arg(0.01, 0, 'f', 2).arg(0.001, 0, 'f', 2);
            torqueTuneParamsColor = QColor(255, 255, 0, 255);
          }
        }
      }
      // 纵向参数：使用UIState中的用户手动设置参数
      if (torquedUseParams && s.scene.long_pid_override) {
        try {
          float kp = std::stof(s.scene.dp_long_pid_kp.empty() ? "1.0" : s.scene.dp_long_pid_kp) * 0.01;
          float ki = std::stof(s.scene.dp_long_pid_ki.empty() ? "0.2" : s.scene.dp_long_pid_ki) * 0.01;
          longTuneParamsStr = QString("P:%1 I:%2").arg(kp, 0, 'f', 2).arg(ki, 0, 'f', 2);
          longTuneParamsColor = QColor(144, 238, 144, 255);
        } catch (const std::exception &e) {
          LOGW("Failed to parse long params: %s", e.what());
          longTuneParamsStr = QString("P:%1 I:%2").arg(0.01, 0, 'f', 2).arg(0.002, 0, 'f', 2);
          longTuneParamsColor = QColor(255, 255, 0, 255);
        }
      }
    }

    // ----------------- 倒车逻辑 (保持每帧) -----------------
    // 倒车影像需要反应快，保持高频
    static int reverse_delay = 0;
    bool reverse_allowed = false;
    if (int(car_state.getGearShifter()) != 4) {
      reverse_delay = 0;
      reverse_allowed = false;
    } else {
      reverse_delay += 50;
      if (reverse_delay >= 1000) {
        reverse_allowed = true;
      }
    }
    reversing = reverse_allowed;
    // ############################## DEV UI END ##############################
}

void AnnotatedCameraWidget::drawHud(QPainter &p) {
  p.save();

  frame_count++;

  // Header gradient
  QLinearGradient bg(0, UI_HEADER_HEIGHT - (UI_HEADER_HEIGHT / 2.5), 0, UI_HEADER_HEIGHT);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), UI_HEADER_HEIGHT, bg);

  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed)) : "–";

  const QSize default_size = {172, 204};
  QSize set_speed_size = default_size;

  int top_radius = 32;
  int bottom_radius = 32;

    // 添加时间显示 - 优化：每300帧（约5秒）刷新一次，减少不必要的重绘
    if (show_date_time) {
      bool size_changed = (timeDisplayBuffer.size() != size());
      if (size_changed) {
        timeDisplayBuffer = QPixmap(size());
      }

      if (size_changed || frame_count % 300 == 0) {
        timeDisplayBuffer.fill(Qt::transparent);
        QPainter pTime(&timeDisplayBuffer);
        pTime.setFont(InterFont(35, QFont::DemiBold));
        QRect timeRect = pTime.fontMetrics().boundingRect("yyyy-MM-dd hh:mm:ss");
        timeRect.moveCenter({rect().center().x(), 25});
        pTime.setPen(whiteColor(200));
        pTime.drawText(timeRect, Qt::AlignCenter, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
      }

      p.drawPixmap(0, 0, timeDisplayBuffer);
    }

  QRect set_speed_rect(QPoint(60 + (default_size.width() - set_speed_size.width()) / 2, 45), set_speed_size);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, set_speed_rect, top_radius, top_radius, bottom_radius, bottom_radius);

  // Draw MAX and set speed text and color only if GPS is not disabled
  QColor max_color = QColor(0x80, 0xd8, 0xa6, 0xff);
  QColor set_speed_color = whiteColor();
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
        max_color = whiteColor();
    } else if (status == STATUS_OVERRIDE) {
        max_color = QColor(0x91, 0x9b, 0x95, 0xff);
    }
  } else {
    max_color = QColor(0xa6, 0xa6, 0xa6, 0xff);
    set_speed_color = QColor(0x72, 0x72, 0x72, 0xff);
  }
  p.setFont(InterFont(40, QFont::DemiBold));
  p.setPen(max_color);
  p.drawText(set_speed_rect.adjusted(0, 27, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("MAX"));
  p.setFont(InterFont(90, QFont::Bold));
  p.setPen(set_speed_color);
  p.drawText(set_speed_rect.adjusted(0, 77, 0, 0), Qt::AlignTop | Qt::AlignHCenter, setSpeedStr);

  // current speed
  p.setFont(InterFont(176, QFont::Bold));
  drawText(p, rect().center().x(), 210, speedStr);
  p.setFont(InterFont(66));
  drawText(p, rect().center().x(), 290, speedUnit, 200);

  // ############################## DEV UI BEGIN ##############################
  if (!reversing && devUiInfo != 0) {

      // ================= 1. 静态层 (背景、标题、单位) =================
      if (last_is_metric != is_metric) {
          static_ui_dirty = true;
          last_is_metric = is_metric;
      }
      updateStaticDevUi();
      p.drawPixmap(0, 0, staticDevUiBuffer);


      // ================= 2. 动态层 (数值) =================
      // 优化：从5帧改为10帧，减少50%的更新频率
      bool dyn_size_changed = (dynamicDevUiBuffer.size() != size());
      if (dyn_size_changed) {
          dynamicDevUiBuffer = QPixmap(size());
      }

      if (dyn_size_changed || frame_count % 10 == 0) {
          dynamicDevUiBuffer.fill(Qt::transparent);
          QPainter pDyn(&dynamicDevUiBuffer);
          pDyn.setRenderHint(QPainter::Antialiasing);
          pDyn.setRenderHint(QPainter::TextAntialiasing);

          QRect bar_rect1(rect().left(), rect().bottom() - DEV_UI_BAR_HEIGHT + 1, rect().width(), DEV_UI_BAR_HEIGHT);
          drawNewDevUi3(pDyn, bar_rect1.left(), bar_rect1.center().y() - DEV_UI_TEXT_OFFSET, MODE_DYNAMIC);
          drawNewDevUi2(pDyn, bar_rect1.left(), bar_rect1.center().y() + DEV_UI_TEXT_OFFSET, MODE_DYNAMIC);

          if (devUiInfo == 2 || devUiInfo == 3) {
              QRect rc2Left(rect().left() + UI_BORDER_SIZE, UI_BORDER_SIZE * 3, 184, 152);
              drawLeftDevUi(pDyn, rect().left() + UI_BORDER_SIZE, UI_BORDER_SIZE * 4 + rc2Left.height(), MODE_DYNAMIC);
          }

          if (devUiInfo == 3) {
              QRect rc2Right(rect().right() - UI_BORDER_SIZE, UI_BORDER_SIZE * 2, 184, 152);
              drawRightDevUi(pDyn, rect().right() - 184 - UI_BORDER_SIZE, UI_BORDER_SIZE * 2.5 + rc2Right.height(), MODE_DYNAMIC);
          }
      }

      p.drawPixmap(0, 0, dynamicDevUiBuffer);
  }
  // ############################## DEV UI END ##############################
  p.restore();
}

void AnnotatedCameraWidget::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = p.fontMetrics().boundingRect(text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::drawColoredText(QPainter &p, int x, int y, const QString &text, QColor color) {
  QRect real_rect = p.fontMetrics().boundingRect(text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::initializeGL() {
  CameraWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void AnnotatedCameraWidget::updateFrameMat() {
  CameraWidget::updateFrameMat();
  UIState *s = uiState();
  int w = width(), h = height();

  s->fb_w = w;
  s->fb_h = h;

  #ifdef QCOM
  auto intrinsic_matrix = FCAM_INTRINSIC_MATRIX;
  float zoom = ZOOM / intrinsic_matrix.v[0];
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2, h / 2 + y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
  #else

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
  #endif
}

void AnnotatedCameraWidget::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  const UIScene &scene = s->scene;
  SubMaster &sm = *(s->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    if (use_lanelines) {
      painter.setBrush(QColor::fromRgbF(0.0, 1.0, 0.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    } else {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    }
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  #ifndef QCOM
  QLinearGradient bg(0, height(), 0, 0);
  if (sm["controlsState"].getControlsState().getExperimentalMode()) {
    // The first half of track_vertices are the points for the right side of the path
    // and the indices match the positions of accel from uiPlan
    const auto &acceleration = sm["uiPlan"].getUiPlan().getAccel();
    const int max_len = std::min<int>(scene.track_vertices.length() / 2, acceleration.size());

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      if (scene.track_vertices[i].y() < 0 || scene.track_vertices[i].y() > height()) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height() - scene.track_vertices[i].y()) / height();

      // speed up: 120, slow down: 0
      float path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
      // FIXME: painter.drawPolygon can be slow if hue is not rounded
      path_hue = int(path_hue * 100 + 0.5) / 100;

      float saturation = fmin(fabs(acceleration[i] * 1.5), 1);
      float lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);  // lighter when grey
      float alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);  // matches previous alpha fade
      bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360., saturation, lightness, alpha));

      // Skip a point, unless next is last
      i += (i + 2) < max_len ? 1 : 0;
    }
  }
  #else
  QLinearGradient bg(0, height(), 0, height() / 4);
  if (sm["controlsState"].getControlsState().getExperimentalMode() && sm["longitudinalPlanExt"].getLongitudinalPlanExt().getDpE2EIsBlended()) {
    float start_hue, end_hue;
    const auto &acceleration = sm["modelV2"].getModelV2().getAcceleration();
    float acceleration_future = 0;
    if (acceleration.getZ().size() > 16) {
      acceleration_future = acceleration.getX()[16];  // 2.5 seconds
    }
    start_hue = 60;
    // speed up: 120, slow down: 0
    end_hue = fmax(fmin(start_hue + acceleration_future * 45, 148), 0);

    // FIXME: painter.drawPolygon can be slow if hue is not rounded
    end_hue = int(end_hue * 100 + 0.5) / 100;

    bg.setColorAt(0.0, QColor::fromHslF(start_hue / 360., 0.97, 0.56, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(end_hue / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(end_hue / 360., 1.0, 0.68, 0.0));
  }
  #endif
  else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);
  drawKnightScanner(painter);

  painter.restore();
}

void AnnotatedCameraWidget::drawKnightScanner(QPainter &painter) {
    UIState *s = uiState();
    int widgetHeight = rect().height();
    float halfHeightAbs = std::abs(s->scene.dpAccel) * widgetHeight;
    const float scannerWidth = 15;
    // 优化：复用类成员变量，避免每帧创建新的QRect对象
    QRect& scannerRect = knightScannerRect;

    if (s->scene.dpAccel > 0) {
        painter.setBrush(QColor(0, 245, 0, 200));
        // Move scanner to the left side
        scannerRect = QRect(0, widgetHeight / 2 - halfHeightAbs / 2, scannerWidth, halfHeightAbs / 2);
    } else {
        painter.setBrush(QColor(245, 0, 0, 200));
        // Move scanner to the left side
        scannerRect = QRect(0, widgetHeight / 2, scannerWidth, halfHeightAbs / 2);
    }

    painter.drawRect(scannerRect);
}

void AnnotatedCameraWidget::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd, float v_ego) {
  painter.save();

  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getDRel();
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  // DP: Chevron detailed info.
  // 优化：只在距离值显著变化时才更新显示，减少文本渲染开销
  if (d_rel > 0) {
    bool need_update_dist = std::abs(d_rel - cached_lead_dist) > 0.5f;
    if (need_update_dist) {
      cached_lead_dist = d_rel;
    }
    QString dist = is_metric? QString::number(d_rel,'f',1) + "m" : QString::number(d_rel*3.2808,'f',1) + "ft";
    int str_w = 350;
    painter.setFont(InterFont(50, QFont::Bold));
    painter.setPen(blackColor(200));
    painter.drawText(QRect(x+4-(str_w/2), y+52, str_w, 55), Qt::AlignVCenter | Qt::AlignCenter, need_update_dist ? dist : QString());
    painter.setPen(whiteColor());
    painter.drawText(QRect(x+2-(str_w/2), y+50, str_w, 55), Qt::AlignVCenter | Qt::AlignCenter, need_update_dist ? dist : QString());
    painter.setPen(Qt::NoPen);

    // 只在TTC较小时显示TTC信息
    // 优化：只在TTC值显著变化时才更新
    if (d_rel > 0 && v_ego > 0) {
      float ttc = d_rel / v_ego;
      if (ttc < 3.5) {
        bool need_update_ttc = std::abs(ttc - cached_lead_ttc) > 0.2f;
        if (need_update_ttc) {
          cached_lead_ttc = ttc;
        }
        QString ttc_str = QString::number(ttc, 'f', 1) + "s";
        painter.setFont(InterFont(40, QFont::Bold));
        painter.setPen(blackColor(200));
        painter.drawText(QRect(x+4-(str_w/2), y+102, str_w, 45), Qt::AlignVCenter | Qt::AlignCenter, need_update_ttc ? ttc_str : QString());
        painter.setPen(whiteColor());
        painter.drawText(QRect(x+2-(str_w/2), y+100, str_w, 45), Qt::AlignVCenter | Qt::AlignCenter, need_update_ttc ? ttc_str : QString());
        painter.setPen(Qt::NoPen);
      } else {
        cached_lead_ttc = -1.0f;  // 重置TTC缓存
      }
    }
  } else {
    cached_lead_dist = -1.0f;
    cached_lead_ttc = -1.0f;
  }
  painter.restore();
}



#ifndef QCOM
void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);
  const double start_draw_t = millis_since_boot();

  // 安全获取传感器数据
  bool model_alive = sm.alive("modelV2");
  bool radar_alive = sm.alive("radarState");
  bool carState_alive = sm.alive("carState");
  bool controlsState_alive = sm.alive("controlsState");
  bool uiPlan_alive = sm.alive("uiPlan");
  bool driverStateV2_alive = sm.alive("driverStateV2");

  // 只有在数据可用时才获取
  const cereal::ModelDataV2::Reader &model = model_alive ? sm["modelV2"].getModelV2() : cereal::ModelDataV2::Reader();
  const cereal::RadarState::Reader &radar_state = radar_alive ? sm["radarState"].getRadarState() : cereal::RadarState::Reader();

  // 对自动驾驶核心功能重要的处理逻辑，无论是否绘制摄像头都需要执行
  bool has_wide_cam = available_streams.count(VISION_STREAM_WIDE_ROAD);
  if (has_wide_cam && carState_alive && controlsState_alive) {
    float v_ego = car_state.getVEgo();
    if ((v_ego < 10) || available_streams.size() == 1) {
      wide_cam_requested = true;
    } else if (v_ego > 15) {
      wide_cam_requested = false;
    }
    wide_cam_requested = wide_cam_requested && controls_state.getExperimentalMode();
    wide_cam_requested = wide_cam_requested && s->scene.calibration_wide_valid;
  }
  CameraWidget::setStreamType(wide_cam_requested ? VISION_STREAM_WIDE_ROAD : VISION_STREAM_ROAD);
  s->scene.wide_cam = CameraWidget::getStreamType() == VISION_STREAM_WIDE_ROAD;
  if (s->scene.calibration_valid) {
    auto calib = s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib;
    CameraWidget::updateCalibration(calib);
  } else {
    CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
  }
  if (model_alive) {
    CameraWidget::setFrameId(model.getFrameId());
  }
  // 只有在需要绘制摄像头时才执行的逻辑
  std::lock_guard lk(frame_lock);
  if (frames.empty()) {
      if (skip_frame_count > 0) {
        skip_frame_count--;
        qDebug() << "skipping frame, not ready";
        return;
      }
  } else {
      skip_frame_count = 5;
  }
  CameraWidget::paintGL();

  lane_lines_start_time = millis_since_boot();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (s->worldObjectsVisible()) {
    if (sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_model(s, model, sm["uiPlan"].getUiPlan());
      if (sm.rcv_frame("radarState") > s->scene.started_frame) {
        update_leads(s, radar_state, model.getPosition());
      }
    }
    drawLaneLines(painter, s);
    if (!s->scene.dp_hide_ui) {
      if (s->scene.longitudinal_control) {
          const cereal::CarState::Reader &car_state = sm["carState"].getCarState();
          float v_ego = car_state.getVEgo();
          auto lead_one = radar_state.getLeadOne();
          auto lead_two = radar_state.getLeadTwo();
          if (lead_one.getStatus()) {
            drawLead(painter, lead_one, s->scene.lead_vertices[0], v_ego); //, 0, radar_state.getLeadOne().getDRel(), v_ego, radar_state.getLeadOne().getVRel(), s->scene.is_metric);
          }
          if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
            drawLead(painter, lead_two, s->scene.lead_vertices[1], v_ego); //, 1, radar_state.getLeadOne().getDRel(), v_ego, radar_state.getLeadTwo().getVRel(), s->scene.is_metric);
          }
      }
    }
  }

  if (!s->scene.dp_hide_ui) {
    drawHud(painter);
  }
  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  double fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
    double lane_lines_time = millis_since_boot() - lane_lines_start_time;  // 车道线绘制耗时
    double total_draw_time = cur_draw_t - start_draw_t;  // 总绘制耗时
    double hud_draw_time = cur_draw_t - lane_lines_start_time - lane_lines_time; // HUD绘制耗时
    LOGW("慢帧率[PC]: %.2f fps, 总耗时: %.2f ms (车道线: %.2f ms, HUD: %.2f ms), 帧间隔: %.2f ms, 帧ID: %d",
         fps, total_draw_time, lane_lines_time, hud_draw_time, dt, model.getFrameId());
  }
  if (fps < 18) {
    painter.setPen(QColor(255, 255, 255, 200));
    painter.setFont(InterFont(35, QFont::Bold));
    painter.drawText(50, 27,
                 QString("FPS: %1").arg(fps, 0, 'f', 2));
  }
  prev_draw_t = cur_draw_t;

  // publish debug msg
  MessageBuilder msg;
  auto m = msg.initEvent().initUiDebug();
  m.setDrawTimeMillis(cur_draw_t - start_draw_t);
  pm->send("uiDebug", msg);
}
#else
void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);
  const double start_draw_t = millis_since_boot();  // 添加绘制开始时间

  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();
  const cereal::RadarState::Reader &radar_state = sm["radarState"].getRadarState();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  // 核心功能逻辑，无论是否绘制摄像头都需要执行
  CameraWidget::setStreamType(VISION_STREAM_ROAD);
  if (s->scene.calibration_valid) {
    CameraWidget::updateCalibration(s->scene.view_from_calib);
  } else {
    CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
  }
  CameraWidget::setFrameId(model.getFrameId());
  CameraWidget::paintGL();
  lane_lines_start_time = millis_since_boot();
  if (s->worldObjectsVisible()) {
    if (sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_model(s, sm["modelV2"].getModelV2(), sm["uiPlan"].getUiPlan());
      if (sm.rcv_frame("radarState") > s->scene.started_frame) {
        update_leads(s, radar_state, sm["modelV2"].getModelV2().getPosition());
      }
    }
    drawLaneLines(painter, s);
    if (!s->scene.dp_hide_ui) {
        if (s->scene.longitudinal_control) {
          const cereal::CarState::Reader &car_state = sm["carState"].getCarState();
          float v_ego = car_state.getVEgo();
          auto lead_one = radar_state.getLeadOne();
          auto lead_two = radar_state.getLeadTwo();
          if (lead_one.getStatus()) {
            drawLead(painter, lead_one, s->scene.lead_vertices[0], v_ego);
          }
          if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
            drawLead(painter, lead_two, s->scene.lead_vertices[1], v_ego);
          }
        }
    }
  }
  if (!s->scene.dp_hide_ui) {
    drawHud(painter);
  }

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  double fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
    double lane_lines_time = millis_since_boot() - lane_lines_start_time;  // 车道线绘制耗时
    double total_draw_time = cur_draw_t - start_draw_t;  // 总绘制耗时
    double hud_draw_time = cur_draw_t - lane_lines_start_time - lane_lines_time; // HUD绘制耗时
    LOGW("慢帧率[QCOM]: %.2f fps, 总耗时: %.2f ms (车道线: %.2f ms, HUD: %.2f ms), 帧间隔: %.2f ms, 帧ID: %d, 相机类型: %d",
         fps, total_draw_time, lane_lines_time, hud_draw_time, dt, model.getFrameId(), CameraWidget::getStreamType());
  }
  if (fps < 18) {
    painter.setPen(QColor(255, 255, 255, 200));
    painter.setFont(InterFont(35, QFont::Bold));
    painter.drawText(50, 27,
                 QString("FPS: %1").arg(fps, 0, 'f', 2));
  }
  prev_draw_t = cur_draw_t;
}
#endif

void AnnotatedCameraWidget::showEvent(QShowEvent *event) {
  CameraWidget::showEvent(event);
  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}

// ############################## DEV UI START ##############################

void AnnotatedCameraWidget::drawCenteredLeftText(QPainter &p, int x, int y, const QString &text1, QColor color1, const QString &text2, const QString &text3, QColor color2, int total_width, DevUiRenderMode mode) {
  QFontMetrics fm(p.font());
  // 如果没有指定宽度，使用默认宽度计算
  if (total_width <= 0) {
    total_width = (width() - 2 * UI_BORDER_SIZE) / 5;
  }
  // 创建总绘制区域（从x位置开始，不居中）
  QRect total_rect(x, y - fm.height() / 2, total_width, fm.height());

  // 模式 A: 静态绘制 (只画标题和单位)
  if (mode == MODE_STATIC) {
    // 1. 画标题 (左对齐)
    p.setPen(color1);
    p.drawText(total_rect, Qt::AlignLeft | Qt::AlignVCenter, text1);

    // 2. 画单位 (右对齐)
    // 注意：单位使用稍暗的颜色或同色，画在最右边
    if (!text3.isEmpty() && text3 != "0") {
        p.setPen(color2);
        p.drawText(total_rect, Qt::AlignRight | Qt::AlignVCenter, text3);
    }
  }

  // 模式 B: 动态绘制 (每帧绘制，只画数值)
  if (mode == MODE_DYNAMIC) {
    // 计算单位占用的宽度 + 间距，防止数值覆盖单位
    int unit_width = 0;
    if (!text3.isEmpty() && text3 != "0") {
        // 这里的 10 是数值和单位之间的空隙，需根据实际字体调整
        unit_width = fm.width(text3) + 10;
    }

    // 创建数值的绘制区域：原区域减去单位的宽度
    QRect value_rect = total_rect;
    value_rect.setWidth(total_rect.width() - unit_width);

    // 画数值 (在剩余空间右对齐)
    p.setPen(color2);
    p.drawText(value_rect, Qt::AlignRight | Qt::AlignVCenter, text2);
  }
}


int AnnotatedCameraWidget::drawDevUiRight(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, DevUiRenderMode mode) {
  // 模式 A: 静态绘制 (画标题和单位)
  if (mode == MODE_STATIC) {
      // 1. 画标题 (Label)
      p.setFont(InterFont(30, QFont::Bold));
      drawText(p, x + 92, y + 85 + 43, label, 255);

      // 2. 画旋转的单位 (Units)
      if (units.length() > 0) {
        p.save();
        p.translate(x + 54 + 32 - 3 + 92 + 32, y + 39 + 26);
        p.rotate(-90);
        drawText(p, 0, 0, units, 255);
        p.restore();
      }
  }

  // 模式 B: 动态绘制 (只画数值)
  if (mode == MODE_DYNAMIC) {
      p.setFont(InterFont(32 * 2, QFont::Bold));
      drawColoredText(p, x + 92, y + 85, value, color);
  }
  return 120;
}

int AnnotatedCameraWidget::drawDevUiLeft(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, DevUiRenderMode mode) {
  // 模式 A: 静态绘制 (画标题和单位)
  if (mode == MODE_STATIC) {
      p.setFont(InterFont(30, QFont::Bold));
      drawText(p, x + 92, y + 85 + 43, label, 255);
      if (units.length() > 0) {
        p.save();
        p.translate(x + 54 + 32 - 3 + 92 + 32, y + 39 + 26);
        p.rotate(-90);
        drawText(p, 0, 0, units, 255);
        p.restore();
      }
  }

  // 模式 B: 动态绘制 (只画数值)
  if (mode == MODE_DYNAMIC) {
      p.setFont(InterFont(32 * 2, QFont::Bold));
      drawColoredText(p, x + 92, y + 85, value, color);
  }
  return 120;
}

// #######  COLUMN Left INFO ########
void AnnotatedCameraWidget::drawLeftDevUi(QPainter &p, int x, int y, DevUiRenderMode mode) {
  int rh = 10;// 初始行高基准值
  int ry = y;// 垂直起始坐标继承参数y值

  //Add Relative Distance to Primary Lead Car 添加与主要引导车的相对距离
  UiElement dRelElement = DeveloperUi::getDRel(lead_status, lead_d_rel);
  rh += drawDevUiLeft(p, x, ry, dRelElement.value, dRelElement.label, dRelElement.units, dRelElement.color, mode);
  ry = y + rh;

  //Add Relative Velocity vs Primary Lead Car 添加与主要领先车辆的相对速度
  UiElement vRelElement = DeveloperUi::getVRel(lead_status, lead_v_rel, is_metric, speedUnit);
  rh += drawDevUiLeft(p, x, ry, vRelElement.value, vRelElement.label, vRelElement.units, vRelElement.color, mode);
  ry = y + rh;

  //Add Real Steering Angle 添加真实转向角
  UiElement steeringAngleDegElement = DeveloperUi::getSteeringAngleDeg(angleSteers, alka_enabled, lat_active);
  rh += drawDevUiLeft(p, x, ry, steeringAngleDegElement.value, steeringAngleDegElement.label, steeringAngleDegElement.units, steeringAngleDegElement.color, mode);
  ry = y + rh;

  if (lateralState == "torque") {
    // Add Actual Lateral Acceleration (roll compensated) when using Torque 使用扭矩时添加实际横向加速度（滚动补偿）
    UiElement actualLateralAccelElement = DeveloperUi::getActualLateralAccel(curvature, vEgo, roll, alka_enabled, lat_active);
    rh += drawDevUiLeft(p, x, ry, actualLateralAccelElement.value, actualLateralAccelElement.label, actualLateralAccelElement.units, actualLateralAccelElement.color, mode);
  } else {
    //Add Desired Steering Angle when using PID 使用 PID 时添加所需转向角
    UiElement steeringAngleDesiredDegElement = DeveloperUi::getSteeringAngleDesiredDeg(alka_enabled, lat_active, steerAngleDesired, angleSteers);
    rh += drawDevUiLeft(p, x, ry, steeringAngleDesiredDegElement.value, steeringAngleDesiredDegElement.label, steeringAngleDesiredDegElement.units, steeringAngleDesiredDegElement.color, mode);
  }
  ry = y + rh;

  // 添加期望跟车距离显示
  UiElement followDistanceElement = DeveloperUi::getDesiredFollowDistance(desired_follow_distance, longActive);
  rh += drawDevUiLeft(p, x, ry, followDistanceElement.value, followDistanceElement.label, followDistanceElement.units, followDistanceElement.color, mode);
  ry = y + rh;

  rh += 25;
  p.setBrush(QColor(0, 0, 0, 0));
  QRect ldu(x, y, 184, rh);
}


// #######  COLUMN Right INFO ########
void AnnotatedCameraWidget::drawRightDevUi(QPainter &p, int x, int y, DevUiRenderMode mode) {
  int rh = 10;// 初始行高基准值
  int ry = y;// 垂直起始坐标继承参数y值

  // 添加横向控制状态显示
  UiElement lateralStateElement = DeveloperUi::getLateralState(lateralState);
  rh += drawDevUiRight(p, x, ry, lateralStateElement.value, lateralStateElement.label, lateralStateElement.units, lateralStateElement.color, mode);
  ry = y + rh;

  // 添加横向控制状态显示
  UiElement lateralControlState = DeveloperUi::getLateralControlState(lat_active);
  rh += drawDevUiRight(p, x, ry, lateralControlState.value, lateralControlState.label, lateralControlState.units, lateralControlState.color, mode);
  ry = y + rh;


  // 添加纵向控制状态信息
  UiElement longControlInfo = DeveloperUi::getLongitudinalControlInfo(longActive, opLongControl, e2eActive);
  rh += drawDevUiRight(p, x, ry, longControlInfo.value, longControlInfo.label, longControlInfo.units, longControlInfo.color, mode);
  ry = y + rh;

  // 添加纵向控制源显示 (Lead/Turn/Cruise)
  UiElement longSrc = DeveloperUi::getLongSource(longSrcVal);
  rh += drawDevUiRight(p, x, ry, longSrc.value, longSrc.label, longSrc.units, longSrc.color, mode);
  ry = y + rh;


  // 添加 ACM 激活状态显示
  if (dp_lon_acm) {
    UiElement acmStateElement = DeveloperUi::getACMState(acmActive);
    rh += drawDevUiRight(p, x, ry, acmStateElement.value, acmStateElement.label, acmStateElement.units, acmStateElement.color, mode);
    ry = y + rh;
  }

  rh += 25;
  p.setBrush(QColor(0, 0, 0, 0));
  QRect ldu(x, y, 184, rh);
}

void AnnotatedCameraWidget::updateStaticDevUi() {
  // 1. 如果缓存大小不匹配，重建缓存
  if (staticDevUiBuffer.size() != size()) {
      staticDevUiBuffer = QPixmap(size());
      static_ui_dirty = true;
  }

  // 2. 如果不需要更新，直接返回
  if (!static_ui_dirty) return;

  // 3. 开始绘制静态内容
  staticDevUiBuffer.fill(Qt::transparent); // 清空画布
  QPainter p(&staticDevUiBuffer);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  // A. 绘制底部半透明黑条背景
  // 注意：我们直接画在全屏 Buffer 的对应位置
  QRect bar_rect1(rect().left(), rect().bottom() - DEV_UI_BAR_HEIGHT + 1, rect().width(), DEV_UI_BAR_HEIGHT);
  p.setPen(devUiPen);
  p.setBrush(devUiBrush);
  p.drawRect(bar_rect1);

  // B. 绘制底部文字 (MODE_STATIC)
  // 这会画出 Label (如 "Eps Trq") 和 Units (如 "Nm")，不画数值
  drawNewDevUi3(p, bar_rect1.left(), bar_rect1.center().y() - DEV_UI_TEXT_OFFSET, MODE_STATIC);
  drawNewDevUi2(p, bar_rect1.left(), bar_rect1.center().y() + DEV_UI_TEXT_OFFSET, MODE_STATIC);

  // C. 绘制左侧栏 (MODE_STATIC)
  if (devUiInfo == 2 || devUiInfo == 3) {
      // 如果侧边栏也有背景色，在这里画 p.drawRect(...)
      QRect rc2Left(rect().left() + UI_BORDER_SIZE, UI_BORDER_SIZE * 3, 184, 152);
      drawLeftDevUi(p, rect().left() + UI_BORDER_SIZE, UI_BORDER_SIZE * 4 + rc2Left.height(), MODE_STATIC);
  }

  // D. 绘制右侧栏 (MODE_STATIC)
  if (devUiInfo == 3) {
      QRect rc2Right(rect().right() - UI_BORDER_SIZE, UI_BORDER_SIZE * 2, 184, 152);
      drawRightDevUi(p, rect().right() - 184 - UI_BORDER_SIZE, UI_BORDER_SIZE * 2.5 + rc2Right.height(), MODE_STATIC);
  }

  // 4. 标记为干净，无需再次绘制
  static_ui_dirty = false;
}

// #######  ROW INFO ########
int AnnotatedCameraWidget::drawNewDevUi(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color, int total_width, DevUiRenderMode mode) {
  p.setFont(InterFont(35, QFont::Bold));
  drawCenteredLeftText(p, x, y, label, whiteColor(), value, units, color, total_width, mode);
  // Return the total width used (不包含边距，边距在调用处处理)
  return total_width;
}


//下层显示(按照五个排列)
void AnnotatedCameraWidget::drawNewDevUi2(QPainter &p, int x, int y, DevUiRenderMode mode) {
  int rw = 10;  // Start with border size margin
  const int element_spacing = 20;  // 元素之间的边距

  // [2] NNFF 核心
  UiElement torqueDynElement = DeveloperUi::getTorqueDyn(f_val, e_val);
  rw += drawNewDevUi(p, rw, y, torqueDynElement.value, torqueDynElement.label, torqueDynElement.units, torqueDynElement.color, 360, mode);
  rw += element_spacing;  // 添加元素间边距

  // [2] 实时调参 A/F
  UiElement tuneParamsAFElement = DeveloperUi::getTorqueTuneParamsAF(torqueTuneParamsAFStr, torqueTuneParamsColor);
  rw += drawNewDevUi(p, rw, y, tuneParamsAFElement.value, tuneParamsAFElement.label, tuneParamsAFElement.units, torqueTuneParamsColor, 420, mode);
  rw += element_spacing;  // 添加元素间边距

  // [2b] 实时调参 P/I
  UiElement tuneParamsPIElement = DeveloperUi::getTorqueTuneParamsPI(torqueTuneParamsPIStr, torqueTuneParamsColor);
  rw += drawNewDevUi(p, rw, y, tuneParamsPIElement.value, tuneParamsPIElement.label, tuneParamsPIElement.units, torqueTuneParamsColor, 360, mode);
  rw += element_spacing;  // 添加元素间边距

  // [3] 纵向 PID 调参 (L_P / L_I)
  UiElement longTuneElement = DeveloperUi::getLongTuneParams(longTuneParamsStr, longTuneParamsColor);
  rw += drawNewDevUi(p, rw, y, longTuneElement.value, longTuneElement.label, longTuneElement.units, longTuneParamsColor, 360, mode);
  rw += element_spacing;  // 添加元素间边距

}

//上层显示(按照五个排列)
void AnnotatedCameraWidget::drawNewDevUi3(QPainter &p, int x, int y, DevUiRenderMode mode) {
  int rw = 10;  // Start with border size margin
  const int element_spacing = 20;  // 元素之间的边距

  // [1] 加速度
  UiElement aEgoElement = DeveloperUi::getAEgo(aEgo);
  rw += drawNewDevUi(p, rw, y, aEgoElement.value, aEgoElement.label, aEgoElement.units, aEgoElement.color, 300, mode);
  rw += element_spacing;  // 添加元素间边距

  // [1] 前车相对速度
  UiElement vEgoLeadElement = DeveloperUi::getVEgoLead(lead_status, lead_v_rel, vEgo, is_metric, speedUnit);
  rw += drawNewDevUi(p, rw, y, vEgoLeadElement.value, vEgoLeadElement.label, vEgoLeadElement.units, vEgoLeadElement.color, 320, mode);
  rw += element_spacing;  // 添加元素间边距

//  // [4] EPS 扭矩
//  UiElement steeringTorqueEpsElement = DeveloperUi::getSteeringTorqueEps(steeringTorqueEps);
//  rw += drawNewDevUi(p, rw, y, steeringTorqueEpsElement.value, steeringTorqueEpsElement.label, steeringTorqueEpsElement.units, steeringTorqueEpsElement.color, 290, mode);
//  rw += element_spacing;  // 添加元素间边距

    // [3] 车道线概率
  UiElement laneProbElement = DeveloperUi::getLaneProb(l_prob, r_prob);
  rw += drawNewDevUi(p, rw, y, laneProbElement.value, laneProbElement.label, laneProbElement.units, laneProbElement.color, 270, mode);
  rw += element_spacing;  // 添加元素间边距

  // [5] 视觉弯道信息
  UiElement visionTurnInfoElement = DeveloperUi::getVisionTurnInfo(visionTurnControllerState, visionTurnSpeed);
  rw += drawNewDevUi(p, rw, y, visionTurnInfoElement.value, visionTurnInfoElement.label, visionTurnInfoElement.units, visionTurnInfoElement.color, 280, mode);
  rw += element_spacing;  // 添加元素间边距


}
// ############################## DEV UI END ##############################
