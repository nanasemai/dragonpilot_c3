/**
The MIT License

Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Last updated: July 29, 2024

- REL DIST - 相对距离（与前方主要车辆的间距，单位：米）
- REL SPEED - 相对速度（与前方主要车辆的速度差，单位：km/h或mph）
- REAL STEER - 实际转向角度（当前方向盘转向角度，单位：度）
- ACTUAL LAT - 实际横向加速度（考虑侧倾补偿的横向加速度，单位：m/s²）
- DESIRED DIST - 期望跟车距离（系统计算的理想跟车距离，单位：米）
- DESIRED STEER - 期望转向角度（控制系统计算的目标转向角度，单位：度）
- RAM - 内存使用率（设备内存使用百分比，单位：%）
- RPM - 发动机转速（当前发动机转速，单位：RPM）
- LAT STATE - 横向控制状态（当前使用的横向控制模式）
- ACM - ACM状态（自适应巡航模式开关状态）
- ACC. - 当前加速度（车辆当前加速度，单位：m/s²）
- V.TRN - 视觉弯道状态（视觉弯道控制器的当前状态）
- V.SPD - 视觉弯道速度（视觉弯道建议速度，单位：km/h）
- L.S. - 前车速度（前方主要车辆的速度，单位：km/h或mph）
- FRIC. - 摩擦系数（扭矩控制计算的摩擦系数）
- L.A. - 横向加速度因子（扭矩控制计算的横向加速度因子，单位：m/s²）
- L.KP. - 扭矩控制Kp参数（PID控制的比例系数）
- L.KI. - 扭矩控制Ki参数（PID控制的积分系数）
- E.T. - EPS转向扭矩（电动助力转向系统输出的扭矩，单位：N·dm）
- B.D. - 方位角（当前行驶方向的指南针方位，单位：度）
- ALT. - 海拔高度（当前位置的海拔高度，单位：米）

***/

#include "selfdrive/ui/qt/developer_ui/developer_ui.h"

#include <cmath>
#include <QColor>
#include <QString>

#include "common/util.h"

// ********** metrics **********

// Add Relative Distance to Primary Lead Car 添加与主要引导车的相对距离
// Unit: Meters
UiElement DeveloperUi::getDRel(bool lead_status, float lead_d_rel) {
  QString value = lead_status ? QString::number(lead_d_rel, 'f', 0) : "-";
  QColor color = QColor(255, 255, 255, 255);

  if (lead_status) {
    // Orange if close, Red if very close
    if (lead_d_rel < 5) {
      color = QColor(255, 0, 0, 255);
    } else if (lead_d_rel < 15) {
      color = QColor(255, 188, 0, 255);
    }
  }

  return UiElement(value, "前车相对距离", "m", color);
}

// Add Relative Velocity vs Primary Lead Car 添加与主要领先车辆的相对速度
// Unit: kph if metric, else mph
UiElement DeveloperUi::getVRel(bool lead_status, float lead_v_rel, bool is_metric, const QString &speed_unit) {
  QString value = lead_status ? QString::number(lead_v_rel * (is_metric ? MS_TO_KPH : MS_TO_MPH), 'f', 0) : "-";
  QColor color = QColor(255, 255, 255, 255);

  if (lead_status) {
    // Red if approaching faster than 10mph
    // Orange if approaching (negative)
    if (lead_v_rel < -4.4704) {
      color = QColor(255, 0, 0, 255);
    } else if (lead_v_rel < 0) {
      color = QColor(255, 188, 0, 255);
    }
  }

  return UiElement(value, "前车相对速度", speed_unit, color);
}

// Add Real Steering Angle 添加真实转向角
// Unit: Degrees
UiElement DeveloperUi::getSteeringAngleDeg(float angle_steers, bool mads_enabled, bool lat_active) {
  QString value = QString("%1%2%3").arg(QString::number(angle_steers, 'f', 1)).arg("°").arg("");
  QColor color = (mads_enabled && lat_active) ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);

  // Red if large steering angle
  // Orange if moderate steering angle
  if (std::fabs(angle_steers) > 180) {
    color = QColor(255, 0, 0, 255);
  } else if (std::fabs(angle_steers) > 90) {
    color = QColor(255, 188, 0, 255);
  }

  return UiElement(value, "真实转向角", "", color);
}

// Add Actual Lateral Acceleration (roll compensated) when using Torque 使用扭矩时添加实际横向加速度（滚动补偿）
// Unit: m/s²
UiElement DeveloperUi::getActualLateralAccel(float curvature, float v_ego, float roll, bool mads_enabled, bool lat_active) {
  double actualLateralAccel = (curvature * pow(v_ego, 2)) - (roll * 9.81);

  QString value = QString::number(actualLateralAccel, 'f', 2);
  QColor color = (mads_enabled && lat_active) ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);

  return UiElement(value, "横向加速度", "m/s²", color);
}

// 添加期望跟车距离显示
UiElement DeveloperUi::getDesiredFollowDistance(float distance, bool active) {
  QString value = std::isnan(distance) ? "N/A" : QString::number(distance, 'f', 1);
  QColor color = active ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);
  return UiElement(value, "期望距离", "m", color);
}

// Add Desired Steering Angle when using PID 使用 PID 时添加所需转向角
// Unit: Degrees
UiElement DeveloperUi::getSteeringAngleDesiredDeg(bool mads_enabled, bool lat_active, float steer_angle_desired, float angle_steers) {
  QString value = (mads_enabled && lat_active) ? QString("%1%2%3").arg(QString::number(steer_angle_desired, 'f', 1)).arg("°").arg("") : "-";
  QColor color = QColor(255, 255, 255, 255);

  if (mads_enabled && lat_active) {
    // Red if large steering angle
    // Orange if moderate steering angle
    if (std::fabs(angle_steers) > 180) {
      color = QColor(255, 0, 0, 255);
    } else if (std::fabs(angle_steers) > 90) {
      color = QColor(255, 188, 0, 255);
    } else {
      color = QColor(0, 255, 0, 255);
    }
  }

  return UiElement(value, "所需转向角", "", color);
}

// Add Device Memory (RAM) Usage 添加设备内存 (RAM) 使用情况
// Unit: Percent
UiElement DeveloperUi::getMemoryUsagePercent(int memory_usage_percent) {
  QString value = QString("%1%2").arg(QString::number(memory_usage_percent, 'd', 0)).arg("%");
  QColor color = (memory_usage_percent > 85) ? QColor(255, 188, 0, 255) : QColor(255, 255, 255, 255);

  return UiElement(value, "设备内存", "", color);
}

// Add Engine RPM Display 添加发动机转速显示
// Unit: RPM
UiElement DeveloperUi::getEngineRPM(float engine_rpm) {
  QString value = QString::number(static_cast<int>(engine_rpm));
  QColor color = QColor(255, 255, 255, 255);
  // 转速超过3000显示橙色警告，超过4000显示红色警告
  if (engine_rpm > 4000) {
    color = QColor(255, 0, 0, 255);
  } else if (engine_rpm > 3000) {
    color = QColor(255, 188, 0, 255);
  }
  return UiElement(value, "转速", "", color);
}

// Add Lateral Control State Display 添加横向控制状态显示
// Unit: None
UiElement DeveloperUi::getLateralState(const QString &lateral_state) {
  QString value;
  QColor color = QColor(255, 255, 255, 255);
  if (lateral_state == "torque") {
    value = "TOR";
    color = QColor(0, 255, 0, 255);
  } else if (lateral_state == "pid") {
    value = "PID";
    color = QColor(255, 188, 0, 255);
  } else if (lateral_state == "indi") {
    value = "INDI";
  } else if (lateral_state == "lqr") {
    value = "LQR";
  } else {
    value = lateral_state.toUpper();
  }
  return UiElement(value, "横向模式", "", color);
}

// Add ACM Status Display 添加 ACM 状态显示
// Unit: None
UiElement DeveloperUi::getACMState(bool acm_active) {
  QString value = acm_active ? "ON" : "OFF";
  QColor color = acm_active ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);
  return UiElement(value, "ACM", "", color);
}

// Add Vehicle Current Acceleration 添加车辆当前加速度
// Unit: m/s²
UiElement DeveloperUi::getAEgo(float a_ego) {
  QString value = QString::number(a_ego, 'f', 1);
  QColor color = QColor(255, 255, 255, 255);
  return UiElement(value, "加速度:", "m/s²", color);
}

// Add Vision Turn Controller State 添加视觉弯道控制器状态
// State enum: 0=未启用, 1=进入弯道, 2=转弯中, 3=离开弯道
UiElement DeveloperUi::getVisionTurnState(int state) {
  QString stateText;
  switch(state) {
    case 0: stateText = "关闭"; break;
    case 1: stateText = "进入"; break;
    case 2: stateText = "转弯"; break;
    case 3: stateText = "退出"; break;
    default: stateText = QString::number(state);
  }
  QColor color = QColor(255, 255, 255, 255);
  return UiElement(stateText, "视觉弯道:", "", color);
}
// Add Vision Turn Speed 添加视觉弯道建议速度
// Unit: km/h
UiElement DeveloperUi::getVisionTurnSpeed(float speed) {
  QString value = QString::number(speed * 3.6, 'f', 1); // 将m/s转换为km/h
  QColor color = QColor(255, 255, 255, 255);
  return UiElement(value, "弯道速度:", "km/h", color);
}

// Add Vision Turn Info 添加视觉弯道信息（只显示速度，通过颜色区分状态）
// Unit: km/h
UiElement DeveloperUi::getVisionTurnInfo(int state, float speed) {
  QString value;
  QColor color;
  bool shouldShowSpeed = false;
  
  switch(state) {
    case 0: // 关闭
      color = QColor(100, 100, 100, 200); // 灰色
      value = "-";
      break;
    case 1: // 进入
      color = QColor(255, 165, 0, 255); // 橙色
      shouldShowSpeed = true;
      break;
    case 2: // 转弯
      color = QColor(255, 0, 0, 255); // 红色
      shouldShowSpeed = true;
      break;
    case 3: // 退出
      color = QColor(0, 255, 0, 255); // 绿色
      shouldShowSpeed = true;
      break;
    default: // 未知状态
      color = QColor(100, 100, 100, 200); // 灰色
      value = "-";
      break;
  }
  
  if (shouldShowSpeed) {
    // 只显示速度数值，使用'g'格式去除无意义的小数位
    value = QString::number(speed * 3.6, 'g', 3);
  }

  // 第三个参数(units) 传 "km/h"，让onroad.cc在静态层显示单位
  return UiElement(value, "弯道速度:", "km/h", color);
}

// Add Relative Velocity to Primary Lead Car 为主要领先车辆添加相对速度
// Unit: kph if metric, else mph
UiElement DeveloperUi::getVEgoLead(bool lead_status, float lead_v_rel, float v_ego, bool is_metric, const QString &speed_unit) {
  QString value = lead_status ? QString::number((lead_v_rel + v_ego) * (is_metric ? MS_TO_KPH : MS_TO_MPH), 'f', 0) : "-";
  QColor color = QColor(255, 255, 255, 255);
  if (lead_status) {
    // Red if approaching faster than 10mph
    // Orange if approaching (negative)
    if (lead_v_rel < -4.4704) {
      color = QColor(255, 0, 0, 255);
    } else if (lead_v_rel < 0) {
      color = QColor(255, 188, 0, 255);
    }
  }
  return UiElement(value, "前车速度:", speed_unit, color);
}

// Add Friction Coefficient Raw from torqued 添加扭矩原始摩擦系数
// Unit: None
UiElement DeveloperUi::getFrictionCoefficientFiltered(float friction_coefficient_filtered, bool live_valid) {
  QString value = QString::number(friction_coefficient_filtered, 'f', 3);
  QColor color = live_valid ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);
  return UiElement(value, "摩擦系数:", "", color);
}

// Add Lateral Acceleration Factor Raw from torqued 从扭矩中添加横向加速度因子原始值
// Unit: m/s²
UiElement DeveloperUi::getLatAccelFactorFiltered(float lat_accel_factor_filtered, bool live_valid) {
  QString value = QString::number(lat_accel_factor_filtered, 'f', 3);
  QColor color = live_valid ? QColor(0, 255, 0, 255) : QColor(255, 255, 255, 255);
  return UiElement(value, "横向加速度:", "m/s²", color);
}


// 添加扭矩控制Kp参数显示
UiElement DeveloperUi::getLateralTorqueKp(float kp_value, bool valid) {
  QString value = QString::number(kp_value, 'f', 2);
  QColor color = valid ? QColor(255, 255, 255, 255) : QColor(255, 0, 0, 255);
  return UiElement(value, "横向扭矩Kp:", "", color);
}

// 添加扭矩控制Ki参数显示
UiElement DeveloperUi::getLateralTorqueKi(float ki_value, bool valid) {
  QString value = QString::number(ki_value, 'f', 2);
  QColor color = valid ? QColor(255, 255, 255, 255) : QColor(255, 0, 0, 255);
  return UiElement(value, "横向扭矩Ki:", "", color);
}

// Add Steering Torque from Car EPS 从汽车 EPS 添加转向扭矩
// Unit: Newton Meters
UiElement DeveloperUi::getSteeringTorqueEps(float steering_torque_eps) {
  QString value = QString::number(std::fabs(steering_torque_eps), 'f', 1);
  QColor color = QColor(255, 255, 255, 255);
  return UiElement(value, "EPS:", "N·dm", color);
}

// Add Bearing Degree and Direction from Car (Compass) 从汽车（指南针）添加方位角和方向
// Unit: Meters
UiElement DeveloperUi::getBearingDeg(float bearing_accuracy_deg, float bearing_deg) {
  QString value = (bearing_accuracy_deg != 180.00) ? QString("%1%2%3").arg(QString::number(bearing_deg, 'd', 0)).arg("°").arg("") : "-";
  QColor color = QColor(255, 255, 255, 255);
  QString dir_value;
  if (bearing_accuracy_deg != 180.00) {
    if (((bearing_deg >= 337.5) && (bearing_deg <= 360)) || ((bearing_deg >= 0) && (bearing_deg <= 22.5))) {
      dir_value = "N";
    } else if ((bearing_deg > 22.5) && (bearing_deg < 67.5)) {
      dir_value = "NE";
    } else if ((bearing_deg >= 67.5) && (bearing_deg <= 112.5)) {
      dir_value = "E";
    } else if ((bearing_deg > 112.5) && (bearing_deg < 157.5)) {
      dir_value = "SE";
    } else if ((bearing_deg >= 157.5) && (bearing_deg <= 202.5)) {
      dir_value = "S";
    } else if ((bearing_deg > 202.5) && (bearing_deg < 247.5)) {
      dir_value = "SW";
    } else if ((bearing_deg >= 247.5) && (bearing_deg <= 292.5)) {
      dir_value = "W";
    } else if ((bearing_deg > 292.5) && (bearing_deg < 337.5)) {
      dir_value = "NW";
    }
  } else {
    dir_value = "OFF";
  }
  return UiElement(QString("%1 | %2").arg(dir_value).arg(value), "B.D.", "", color);
}

// Add Altitude of Current Location 添加当前位置的海拔高度
// Unit: Meters
UiElement DeveloperUi::getAltitude(float gps_accuracy, float altitude) {
  QString value = (gps_accuracy != 0.00) ? QString::number(altitude, 'f', 1) : "-";
  QColor color = QColor(255, 255, 255, 255);
  return UiElement(value, "海拔:", "m", color);
}

// [新增] 获取 Torque 核心动态数据 (前馈 F, 误差 E)
UiElement DeveloperUi::getTorqueDyn(float f_val, float e_val) {
  QString val = QString("F:%1 E:%2")
                  .arg(f_val, 0, 'f', 2)
                  .arg(e_val, 0, 'f', 2);
  QColor color = (std::abs(e_val) > 2.0) ? QColor(255, 100, 100, 255) : QColor(255, 255, 255, 255);
  return UiElement(val, "NNFF:", "", color);
}

UiElement DeveloperUi::getLaneProb(float left_prob, float right_prob) {
  QString val = QString("%1|%2").arg(left_prob, 0, 'f', 1).arg(right_prob, 0, 'f', 1);
  QColor color = ((left_prob + right_prob) / 2 < 0.3) ? QColor(255, 80, 80, 255) : QColor(255, 255, 255, 255);
  return UiElement(val, "车道线:", "", color);
}

UiElement DeveloperUi::getSteerSaturated(int saturated) {
  QString val = saturated ? "是" : "否";
  QColor color = saturated ? QColor(255, 50, 50, 255) : QColor(180, 180, 180, 255);
  return UiElement(val, "饱和:", "", color);
}

UiElement DeveloperUi::getTorqueTuneParamsAF(const QString &val_str, QColor color) {
  return UiElement(val_str, "横向A/F:", "", color);
}

UiElement DeveloperUi::getTorqueTuneParamsPI(const QString &val_str, QColor color) {
  return UiElement(val_str, "横向P/I:", "", color);
}

UiElement DeveloperUi::getLongTuneParams(const QString &val_str, QColor color) {
  return UiElement(val_str, "纵向P/I:", "", color);
}

UiElement DeveloperUi::getLongSource(int source) {
  QString src_name;
  QColor color = QColor(255, 255, 255, 255);

  switch (source) {
    case 0:
      src_name = "巡航";
      color = QColor(0, 200, 255, 255);
      break;
    case 1:
      src_name = "跟车";
      color = QColor(100, 255, 100, 255);
      break;
    case 2:
      src_name = "转弯";
      color = QColor(255, 165, 0, 255);
      break;
    case 3:
      src_name = "限速";
      color = QColor(255, 200, 100, 255);
      break;
    case 4:
      src_name = "端到端";
      color = QColor(200, 100, 255, 255);
      break;
    default:
      src_name = "-";
      color = QColor(150, 150, 150, 255);
      break;
  }
  return UiElement(src_name, "纵向控制源", "", color);
}

UiElement DeveloperUi::getRoadRoll(float roll) {
  float roll_deg = roll * (180.0 / M_PI);
  QString val = QString::number(roll_deg, 'f', 1) + "°";
  QColor color = (std::abs(roll_deg) > 3.0) ? QColor(255, 200, 100, 255) : QColor(255, 255, 255, 255);
  return UiElement(val, "侧倾角度:", "", color);
}

UiElement DeveloperUi::getSteeringAngle(float angle) {
  QString val = QString::number(angle, 'f', 1) + "°";
  return UiElement(val, "转向角度:", "", QColor(255, 255, 255, 255));
}

UiElement DeveloperUi::getLateralControlState(bool lat_active) {
  QString state;
  QColor color;
  
  if (lat_active) {
    state = "激活";
    color = QColor(100, 255, 100, 255);  // 绿色：横向控制激活
  } else {
    state = "未激活";
    color = QColor(255, 100, 100, 255);  // 红色：横向控制未激活
  }
  
  return UiElement(state, "横向控制", "", color);
}

UiElement DeveloperUi::getLongitudinalControlInfo(bool long_active, bool op_long_control, bool e2e_active) {
  QString state;
  QColor color;
  
  if (long_active) {
    if (op_long_control) {
      if (e2e_active) {
        state = "OP+E2E";
        color = QColor(100, 255, 100, 255);  // 绿色：OP纵向+端到端激活
      } else {
        state = "OP纵向";
        color = QColor(100, 200, 255, 255);  // 蓝色：OP纵向激活
      }
    } else {
      state = "原车ACC";
      color = QColor(255, 200, 100, 255);    // 黄色：原车ACC激活
    }
  } else {
    state = "未激活";
    color = QColor(255, 100, 100, 255);      // 红色：纵向控制未激活
  }
  
  return UiElement(state, "纵向控制", "", color);
}

