#pragma once

#include "selfdrive/ui/qt/widgets/controls.h"
#include <map>
#include <string>

// ********** DP Control Panel **********
// 包含驾驶辅助功能的开关（横向/纵向）
class DPControlPanel : public ListWidget {
  Q_OBJECT
public:
  explicit DPControlPanel(QWidget *parent = nullptr);
  void showEvent(QShowEvent *event) override;

private:
  Params params;
  std::map<std::string, ParamControl *> toggles;
  LabelControl *nnff_status_label;

  void updateToggles();
  void addLateralToggles();
  void addLongitudinalToggles();

public slots:
  void expandToggleDescription(const QString &param);

signals:
  void toggleChanged();
};

// ********** DP Tuning Panel **********
// 包含所有数值调整、PID、偏移量、感度调节
class DPTuningPanel : public ListWidget {
  Q_OBJECT
public:
  explicit DPTuningPanel(QWidget *parent = nullptr);
  void showEvent(QShowEvent *event) override;

public slots:
  void updateToggles();

private:
  Params params;
  std::map<std::string, ParamControl *> toggles;

  // 横向调参控件
  ParamSpinBoxControl *dp_torque_lat_accel_factor_toggle;
  ParamSpinBoxControl *dp_torque_friction_toggle;
  ParamSpinBoxControl *dp_lat_kp_toggle;
  ParamSpinBoxControl *dp_lat_ki_toggle;
  ParamSpinBoxControl *speed_based_lane_priority_toggle;

  // 纵向/通用调参控件
  ParamSpinBoxControl *lead_start_threshold_toggle;
  ParamSpinBoxControl *lead_stop_time_threshold_toggle;
  ParamControl *lon_acm_downhill_toggle;
  ParamSpinBoxControl *lon_acm_min_accel_toggle;
  ParamSpinBoxControl *vt_sensitivity_toggle;
  ParamSpinBoxControl *vt_decel_ratio_toggle;
  ParamSpinBoxControl *vt_speed_ratio_toggle;
  ParamSpinBoxControl *vt_smooth_factor_toggle;
  ParamSpinBoxControl *long_pid_kp_toggle;
  ParamSpinBoxControl *long_pid_ki_toggle;

  void addLateralTuning();
  void addLongitudinalTuning();
};

// ********** DP Car Panel **********
// 包含特定车型的专属设置
class DPCarPanel : public ListWidget {
  Q_OBJECT
public:
  explicit DPCarPanel(QWidget *parent = nullptr);

private:
  Params params;
  std::map<std::string, ParamControl *> toggles;

  void addToyotaToggles();
  void addHKGToggles();
  void addVAGToggles();
  void addBYDToggles();
};

// ********** DP System Panel **********
// 包含设备、UI、行车记录仪、服务监控及重置
class DPSystemPanel : public ListWidget {
  Q_OBJECT
public:
  explicit DPSystemPanel(QWidget *parent = nullptr);
  void showEvent(QShowEvent *event) override;

private:
  Params params;
  std::map<std::string, ParamControl *> toggles;

  // 系统/设备控件
  ParamSpinBoxControl *auto_shutdown_timer_toggle;
  ButtonParamControl *dashcam_quality_setting;
  ParamSpinBoxControl *dashcam_duration_toggle;
  ParamSpinBoxControl *dashcam_kept_hours_toggle;

  void updateToggles();
  void addDeviceToggles();
  void addUIToggles();
  void addDashcamToggles();
  void addServiceToggles();
};
