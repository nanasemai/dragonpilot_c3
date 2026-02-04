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
***/

#pragma once

#include "selfdrive/ui/qt/developer_ui/ui_elements.h"

class DeveloperUi {
public:
  static UiElement getDRel(bool lead_status, float lead_d_rel);
  static UiElement getVRel(bool lead_status, float lead_v_rel, bool is_metric, const QString &speed_unit);
  static UiElement getSteeringAngleDeg(float angle_steers, bool mads_enabled, bool lat_active);
  static UiElement getActualLateralAccel(float curvature, float v_ego, float roll, bool mads_enabled, bool lat_active);
  static UiElement getDesiredFollowDistance(float distance, bool active);
  static UiElement getSteeringAngleDesiredDeg(bool mads_enabled, bool lat_active, float steer_angle_desired, float angle_steers);
  static UiElement getMemoryUsagePercent(int memory_usage_percent);
  static UiElement getEngineRPM(float engine_rpm);
  static UiElement getLateralState(const QString &lateral_state);
  static UiElement getACMState(bool acm_active);  // 添加 ACM 状态显示函数声明
  static UiElement getAEgo(float a_ego);
  static UiElement getVEgoLead(bool lead_status, float lead_v_rel, float v_ego, bool is_metric, const QString &speed_unit);
  static UiElement getFrictionCoefficientFiltered(float friction_coefficient_filtered, bool live_valid);
  static UiElement getLatAccelFactorFiltered(float lat_accel_factor_filtered, bool live_valid);
  static UiElement getSteeringTorqueEps(float steering_torque_eps);
  static UiElement getBearingDeg(float bearing_accuracy_deg, float bearing_deg);
  static UiElement getAltitude(float gps_accuracy, float altitude);
  static UiElement getVisionTurnState(int state);
  static UiElement getVisionTurnSpeed(float speed);
  static UiElement getVisionTurnInfo(int state, float speed);
  static UiElement getLateralTorqueKp(float kp_value, bool valid);
  static UiElement getLateralTorqueKi(float ki_value, bool valid);
  static UiElement getTorqueDyn(float f_val, float e_val);
  static UiElement getLongSource(int source);
  static UiElement getLaneProb(float left_prob, float right_prob);
  static UiElement getSteerSaturated(int saturated);
  static UiElement getTorqueTuneParamsAF(const QString &val_str, QColor color);
  static UiElement getTorqueTuneParamsPI(const QString &val_str, QColor color);
  static UiElement getLongTuneParams(const QString &val_str, QColor color);
  static UiElement getRoadRoll(float roll);
  static UiElement getSteeringAngle(float angle);
  static UiElement getLateralControlState(bool lat_active);
  static UiElement getLongitudinalControlInfo(bool long_active, bool op_long_control, bool e2e_active);
};
