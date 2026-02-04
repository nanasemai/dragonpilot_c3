#include "selfdrive/ui/qt/offroad/settings_dp.h"
#include "common/params.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"

#include <QDebug>

static void setupToggle(ListWidget *parent, std::map<std::string, ParamControl *> &toggles, Params &params,
                        const std::string &param, const QString &title, const QString &desc, const QString &icon = "") {
  auto toggle = new ParamControl(QString::fromStdString(param), title, desc, icon, parent);
  bool locked = params.getBool(param + "Lock");
  toggle->setEnabled(!locked);
  parent->addItem(toggle);
  toggles[param] = toggle;
}

DPControlPanel::DPControlPanel(QWidget *parent) : ListWidget(parent) {
  addLateralToggles();
  addLongitudinalToggles();
}

void DPControlPanel::addLateralToggles() {
  addItem(new LabelControl(QString::fromUtf8("🚗 ") + tr("控制 - 横向") + QString::fromUtf8(" 🚗"), ""));
  std::vector<QString> dp_lat_controller_texts{tr("默认"), tr("INDI"), tr("LQR"), tr("PID"), tr("扭矩")};
  addItem(new ButtonParamControl("dp_lat_controller", tr("横向控制器"),tr("更换横向控制器。\n使用风险自负！\n需要重新启动。"),"", dp_lat_controller_texts));
  setupToggle(this, toggles, params, "dp_alka", tr("启用全时居中"), tr("启用后，当ACC MAIN开启时，openpilot横向控制将始终保持激活状态。\n注意：\n1. 启用后界面状态栏将显示绿色\n2. 禁用时恢复标准蓝色\n需要重启生效。"));
  setupToggle(this, toggles, params, "dp_use_nnff", tr("神经网络前馈控制 (NNFF)"), tr("启用神经网络前馈控制，提供更准确的转向前馈控制。\n启用后可大幅改善转向手感，减少转向滞后。\n注意：开启后建议适当降低下方的 PID P值 (Kp)，否则可能导致画龙。"));
  setupToggle(this, toggles, params, "dp_use_nnff_lite", tr("轻量级前馈控制 (NNFF-Lite)"), tr("NNFF 的低增益模式。\n如果标准模式下方向盘震动明显或手感过重，请尝试此选项。\n适用于路径预测不稳定的路况。"));
    if (toggles.count("dp_use_nnff")) {
    connect(toggles["dp_use_nnff"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  }
  nnff_status_label = new LabelControl(tr("NNFF状态"), "");
  addItem(nnff_status_label);
  setupToggle(this, toggles, params, "dp_lat_lane_priority_mode", tr("启用车道线优先模式"), tr("针对 0813 模型强烈推荐开启。\n0813 模型的无车道线能力较弱，启用此选项可强制锚定车道线，显著减少直线行驶时的画龙和摆动。\n仅在车道线丢失或模糊时，才会自动降级使用模型预测轨迹。\n状态说明:\n- <font color='#4BD864'><b>绿色</b></font>: 锁定车道线 (最佳)\n- <font color='#FFFFFF'><b>白色</b></font>: 模型轨迹 (替补)"));
  connect(toggles["dp_lat_lane_priority_mode"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  addItem(new ParamSpinBoxControl("dp_lat_lane_priority_mode_speed_based", tr("车道优先激活速度"), tr("仅当速度高于此值时使用车道线优先模式。"), "", 0, 120, 1, tr(" 千米/时"), tr("全速域")));
  addItem(new ParamSpinBoxControl("dp_lat_lane_change_assist_speed", tr("辅助车道变换速度设定"), tr("调整辅助车道变换激活速度。\n关闭 = 禁用车道变换辅助。\n1 千米/时 = 0.62 英里/时"), "", 0, 80, 1, tr(" 千米/时"), tr("关闭")));
  setupToggle(this, toggles, params, "dp_lat_lane_change_abort_check", tr("辅助换道中止检查"), tr("启用后，在辅助换道过程中会检查驾驶员的反向转向和反向转向灯操作，以便及时中止换道。\n关闭后，换道过程将不会因为这些操作而中止。"));
  setupToggle(this, toggles, params, "dp_lateral_road_edge_detected", tr("启用道路边缘检测"), tr("启用后，当车辆过于靠近道路边缘时，系统将阻止换道操作。\n这个值将在换道辅助启用时才会使用。\n- 红色: 检测到道路边缘"));
}

void DPControlPanel::updateToggles() {
  QString nnff_car_model = QString::fromStdString(params.get("NNFFModelName"));
  bool is_0813 = params.getBool("dp_0813");
  bool is_nnff_on = params.getBool("dp_use_nnff");

  if (is_0813 && is_nnff_on) {
    if (!nnff_car_model.isEmpty()) {
      nnff_status_label->setText(tr("✅ 0813 已加载: ") + nnff_car_model);
    } else {
      nnff_status_label->setText(tr("✅ 0813 已激活"));
    }
    nnff_status_label->setStyleSheet("color: #4BD864; font-size: 35px; font-weight: bold;");
  } else if (is_nnff_on && !nnff_car_model.isEmpty()) {
    nnff_status_label->setText(tr("✅ 已加载: ") + nnff_car_model);
    nnff_status_label->setStyleSheet("color: #4BD864; font-size: 35px; font-weight: bold;");
  } else if (is_nnff_on) {
    nnff_status_label->setText(tr("⚠️ 未找到模型文件"));
    nnff_status_label->setStyleSheet("color: #E22C2C; font-size: 35px;");
  } else {
    nnff_status_label->setText(tr("NNFF 已禁用"));
    nnff_status_label->setStyleSheet("color: #808080; font-size: 35px;");
  }

  emit toggleChanged();
}

void DPControlPanel::addLongitudinalToggles() {
  addItem(new LabelControl(QString::fromUtf8("🚘 ") + tr("控制 - 纵向") + QString::fromUtf8(" 🚘"), ""));
  std::vector<QString> dp_long_accel_profile_texts{tr("OP"), tr("经济"), tr("标准"), tr("运动")};
  addItem(new ButtonParamControl("dp_long_accel_profile", tr("加速度配置"), tr("OP - OP调校.\n经济 - 节能调校.\n标准 - 正常调校.\n运动 - 运动调校。"), "", dp_long_accel_profile_texts));
  setupToggle(this, toggles, params, "dp_long_de2e", tr("启用动态端到端纵向控制 (仅限 0816+)"), tr("注意：此功能在 0813 模型下不可用！\n0813 模型缺乏红绿灯和路口识别能力。\n启用此选项不会有任何效果，系统将强制保持在 ACC 模式。"));
  setupToggle(this, toggles, params, "dp_long_use_df_tune", tr("启用动态跟车距离 (Dynamic Following)"), tr("0813 用户推荐开启。\n根据前车速度动态调整跟车距离，改善传统 ACC 的跟车体验。"));
  setupToggle(this, toggles, params, "dp_long_use_krkeegen_tune", tr("启用起步加速优化 (SNG Boost)"), tr("优化 Stop & Go (SNG) 表现。\n在排队跟车或红绿灯起步时，提供更积极的加速响应。"));
  setupToggle(this, toggles, params, "dp_mapd_vision_turn_control", tr("启用视觉弯道减速 (强烈推荐)"), tr("0813 模型的核心安全功能。\n利用视觉路径曲率计算安全过弯速度，防止高速冲出匝道。"));
  connect(toggles["dp_mapd_vision_turn_control"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  setupToggle(this, toggles, params, "dp_lon_acm", tr("启用自适应滑行模式 (ACM)"), tr("自适应滑行模式通过智能控制制动系统来实现更平顺的滑行。"));
  connect(toggles["dp_lon_acm"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  setupToggle(this, toggles, params, "dp_long_missing_lead_warning", tr("前车丢失预警"), tr("启用后，当车辆以70公里/小时以上的速度行驶时，如果前车丢失超过2秒，系统将向驾驶员发出警告。"));
  setupToggle(this, toggles, params, "dp_lead_start_alert", tr("前车起步提醒"), tr("启用后当前车从停止状态起步时，系统会发出提醒"));
  connect(toggles["dp_lead_start_alert"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
}


void DPControlPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void DPControlPanel::expandToggleDescription(const QString &param) {
  if (toggles.count(param.toStdString())) {
    toggles[param.toStdString()]->showDescription();
  }
}

DPTuningPanel::DPTuningPanel(QWidget *parent) : ListWidget(parent) {
  addLateralTuning();
  addLongitudinalTuning();
}

void DPTuningPanel::addLateralTuning() {
  addItem(new LabelControl(QString::fromUtf8("🔧 ") + tr("调校 - 横向") + QString::fromUtf8(" 🔧"), ""));
  addItem(new ParamSpinBoxControl("dp_lateral_camera_offset", tr("相机偏移量"), tr("相机到车辆中心线的距离。C2默认是-6厘米。\n在左边是负值，在右边是正值。"), "", -100, 100, 1, tr(" cm"), tr("关闭")));
  addItem(new ParamSpinBoxControl("dp_lateral_path_offset", tr("路径偏移量"), tr("调整规划路径的横向偏移量。\n正值靠右，负值靠左。"), "", -100, 100, 1, tr(" cm"), tr("关闭")));
  setupToggle(this, toggles, params, "dp_torqued_override", tr("启用横向-自定义参数"), 
              tr("解锁下方的扭矩参数调节功能。\n对于 0813 模型，启用此选项才能通过调节参数获得良好的转向手感。\n注意：启用后将覆盖车辆的原厂默认参数。"));
  connect(toggles["dp_torqued_override"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  dp_torque_lat_accel_factor_toggle = new ParamSpinBoxControl("dp_torque_lat_accel_factor", tr("横向-转向刚度系数 (Lat Accel Factor)"), tr("数值越小：力矩越大，转向越硬。\n数值越大：力矩越小，转向越软。\n0813开启NNFF后的核心调节参数。"), "", 1, 500, 1, tr(" x0.01"));
  addItem(dp_torque_lat_accel_factor_toggle);
  dp_torque_friction_toggle = new ParamSpinBoxControl("dp_torque_friction", tr("横向-转向摩擦补偿 (Friction)"), tr("补偿转向系统的静摩擦力。"), "", 1, 800, 1, tr(" x0.001"));
  addItem(dp_torque_friction_toggle);
  dp_lat_kp_toggle = new ParamSpinBoxControl("dp_lateral_torque_kp", tr("横向-PID 比例系数 (Kp)"), tr("调整误差修正的力度。(x0.01)"), "", 0, 500, 5, tr(" x0.01"));
  addItem(dp_lat_kp_toggle);
  dp_lat_ki_toggle = new ParamSpinBoxControl("dp_lateral_torque_ki", tr("横向-PID 积分系数 (Ki)"), tr("调整持续误差的消除速度。(x0.01)"), "", 0, 100, 1, tr(" x0.01"));
  addItem(dp_lat_ki_toggle);
  addItem(new ParamSpinBoxControl("dp_toyota_steer_rate_safety_margin", tr("丰田转向安全余量"), tr("调整丰田车型的转向安全余量。\n默认值：10。"), "", 10, 200, 10, tr(" deg/s"), tr("关闭")));

  }

void DPTuningPanel::addLongitudinalTuning() {
  addItem(new LabelControl(QString::fromUtf8("🚀 ") + tr("调校 - 纵向") + QString::fromUtf8(" 🚀"), ""));
  setupToggle(this, toggles, params, "dp_long_pid_override", tr("启用自定义纵向PID参数"), tr("开启后将使用自定义的纵向PID参数进行控制"));
  connect(toggles["dp_long_pid_override"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  long_pid_kp_toggle = new ParamSpinBoxControl("dp_long_pid_kp", tr("纵向-PID比例增益 (Kp)"), tr("调整纵向控制的反应速度"), "", 50, 200, 1, tr(" x0.01"));
  addItem(long_pid_kp_toggle);
  long_pid_ki_toggle = new ParamSpinBoxControl("dp_long_pid_ki", tr("纵向-PID积分增益 (Ki)"), tr("调整纵向控制的稳态误差消除"), "", 0, 50, 1, tr(" x0.01"));
  addItem(long_pid_ki_toggle);

  
  vt_sensitivity_toggle = new ParamSpinBoxControl("dp_vt_sensitivity", tr("弯道灵敏度系数 (Vision Turn)"), tr("数值越小，弯道减速越早且越灵敏"), "", 5, 20, 1, tr(" x0.1"));
  addItem(vt_sensitivity_toggle);
  vt_decel_ratio_toggle = new ParamSpinBoxControl("dp_vt_decel_ratio", tr("弯道减速强度系数"), tr("数值越大，弯道减速越强"), "", 5, 20, 1, tr(" x0.1"));
  addItem(vt_decel_ratio_toggle);
  vt_speed_ratio_toggle = new ParamSpinBoxControl("dp_vt_speed_ratio", tr("弯道速度系数"), tr("数值越大，弯道通过速度越快"), "", 5, 20, 1, tr(" x0.1"));
  addItem(vt_speed_ratio_toggle);
  vt_smooth_factor_toggle = new ParamSpinBoxControl("dp_vt_smooth_factor", tr("弯道平滑因子"), tr("数值越大，加速度变化越平滑"), "", 5, 20, 1, tr(" x0.1"));
  addItem(vt_smooth_factor_toggle);

  lon_acm_downhill_toggle = new ParamControl("dp_lon_acm_downhill", tr("(ACM)仅在下坡时启用"), tr("限制ACM功能仅在检测到下坡(坡度>4%)时激活"), "", this);
  addItem(lon_acm_downhill_toggle);
  
  lon_acm_min_accel_toggle = new ParamSpinBoxControl("dp_lon_acm_min_accel", tr("ACM滑行最小减速度"), tr("设置滑行时的最小减速度\n正值减速，负值加速"), "", -50, 50, 1, tr(" x0.01"));
  addItem(lon_acm_min_accel_toggle);
  
  lead_start_threshold_toggle = new ParamSpinBoxControl("dp_lead_start_alert_threshold", tr("前车起步速度阈值"), tr("单位：米/秒"), "", 1, 50, 1, tr(" x0.1 m/s"));
  addItem(lead_start_threshold_toggle);
  lead_stop_time_threshold_toggle = new ParamSpinBoxControl("dp_lead_stop_time_threshold", tr("前车停止时间阈值"), tr("单位：秒"), "", 1, 100, 1, tr(" x0.1 s"));
  addItem(lead_stop_time_threshold_toggle);
}

void DPTuningPanel::updateToggles() {
  bool lat_override = params.getBool("dp_torqued_override");
  if (dp_torque_lat_accel_factor_toggle) dp_torque_lat_accel_factor_toggle->setVisible(lat_override);
  if (dp_torque_friction_toggle) dp_torque_friction_toggle->setVisible(lat_override);
  if (dp_lat_kp_toggle) dp_lat_kp_toggle->setVisible(lat_override);
  if (dp_lat_ki_toggle) dp_lat_ki_toggle->setVisible(lat_override);

  bool long_override = params.getBool("dp_long_pid_override");
  if (long_pid_kp_toggle) long_pid_kp_toggle->setVisible(long_override);
  if (long_pid_ki_toggle) long_pid_ki_toggle->setVisible(long_override);

  if (speed_based_lane_priority_toggle) speed_based_lane_priority_toggle->setVisible(params.getBool("dp_lat_lane_priority_mode"));
  
  bool acm_on = params.getBool("dp_lon_acm");
  if (lon_acm_min_accel_toggle) lon_acm_min_accel_toggle->setVisible(acm_on);
  if (lon_acm_downhill_toggle) lon_acm_downhill_toggle->setVisible(acm_on);

  bool vt_on = params.getBool("dp_mapd_vision_turn_control");
  if (vt_sensitivity_toggle) vt_sensitivity_toggle->setVisible(vt_on);
  if (vt_decel_ratio_toggle) vt_decel_ratio_toggle->setVisible(vt_on);
  if (vt_speed_ratio_toggle) vt_speed_ratio_toggle->setVisible(vt_on);
  if (vt_smooth_factor_toggle) vt_smooth_factor_toggle->setVisible(vt_on);

  bool alert_on = params.getBool("dp_lead_start_alert");
  if (lead_start_threshold_toggle) lead_start_threshold_toggle->setVisible(alert_on);
  if (lead_stop_time_threshold_toggle) lead_stop_time_threshold_toggle->setVisible(alert_on);
}

void DPTuningPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

DPCarPanel::DPCarPanel(QWidget *parent) : ListWidget(parent) {
  addToyotaToggles();
  addHKGToggles();
  addVAGToggles();
  addBYDToggles();
}

void DPCarPanel::addToyotaToggles() {
  addItem(new LabelControl(QString::fromUtf8("👹 ") + tr("丰田 / 雷克萨斯") + QString::fromUtf8(" 👹"), ""));
  setupToggle(this, toggles, params, "dp_toyota_sng", tr("启用停车起步(SnG)破解"), tr("启用后，openpilot将在车辆完全停止时停止发送静止信号。"));
  setupToggle(this, toggles, params, "dp_toyota_auto_lock", tr("启用车门自动上锁"), tr("启用后，当车速超过10公里/小时时，openpilot将尝试锁上车门。"));
  setupToggle(this, toggles, params, "dp_toyota_auto_unlock", tr("启用车门自动解锁"), tr("启用后，当挂入驻车档时，openpilot将尝试解锁车门。"));
  setupToggle(this, toggles, params, "dp_toyota_zss", tr("启用ZSS(零转向传感器)支持"), tr("除非您已安装ZSS，否则请勿启用。"));
  setupToggle(this, toggles, params, "dp_toyota_enhanced_bsm", tr("启用增强型盲点监测(BSM)"), tr("启用后，openpilot将使用调试CAN消息接收未经过滤的BSM信号。"));
}

void DPCarPanel::addHKGToggles() {
  addItem(new LabelControl(QString::fromUtf8("👹 ") + tr("现代 / 起亚 / 捷尼赛思") + QString::fromUtf8(" 👹"), ""));
  setupToggle(this, toggles, params, "dp_hkg_min_steer_speed_bypass", tr("启用最低转向速度旁路"), tr("启用后，openpilot将控制转向到0公里/小时。"));
}

void DPCarPanel::addVAGToggles() {
  addItem(new LabelControl(QString::fromUtf8("👹 ") + tr("大众 / 斯柯达 / 奥迪") + QString::fromUtf8(" 👹"), ""));
  setupToggle(this, toggles, params, "dp_vag_timebomb_bypass", tr("启用横向控制定时炸弹旁路"), tr("临时禁用横向控制当它达到定时炸弹限制时。"));
}

void DPCarPanel::addBYDToggles() {
  addItem(new LabelControl(QString::fromUtf8("👹 ") + tr("比亚迪") + QString::fromUtf8(" 👹"), ""));
  setupToggle(this, toggles, params, "dp_lat_use_siglin", tr("启用 Siglin 模式"), tr("启用后，横向加速度因子将不会生效。"));
  setupToggle(this, toggles, params, "BydModifiedStockLong", tr("启用改进版原车纵向控制"), tr("通过毫米波雷达检测到的前车距离，动态调整平滑系数来实现纵向控制。"));
  setupToggle(this, toggles, params, "BydUseRadar", tr("启用雷达进行纵向控制"), tr("仅在关闭比亚迪改装版纵向控制且开启 OP 纵向控制时生效。"));
}

DPSystemPanel::DPSystemPanel(QWidget *parent) : ListWidget(parent) {
  addUIToggles();
  addDashcamToggles();
  addDeviceToggles();
  addServiceToggles();

  auto resetBtn = new ButtonControl(tr("重置配置"), tr("重置"));
  connect(resetBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("您确定要重置所有dp配置吗？"), tr("重置"), this)) {
      params.putBool("dp_reset_conf", true);
    }
  });
  addItem(resetBtn);
}

void DPSystemPanel::addDeviceToggles() {
  addItem(new LabelControl(QString::fromUtf8("📱 ") + tr("设备") + QString::fromUtf8(" 📱"), ""));
  setupToggle(this, toggles, params, "dp_show_date_time", tr("显示时间"), tr("在UI界面上显示当前的日期和时间信息"));
  setupToggle(this, toggles, params, "dp_device_disable_temp_check", tr("禁用温度检查"), tr("启用后，openpilot将禁用设备温度检查。\n**注意**过热的设备可能会导致随机停机或延迟。\n需要重新启动。"));
  setupToggle(this, toggles, params, "dp_device_auto_shutdown", tr("启用自动关机"), tr("启用后，openpilot将会自动关闭设备。\n需要重新启动。"));
  connect(toggles["dp_device_auto_shutdown"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });
  setupToggle(this, toggles, params, "dp_device_no_ir_ctrl", tr("禁用红外线"), tr("启用时，openpilot 将完全禁用红外线。\n需要重新启动。"));
  setupToggle(this, toggles, params, "dp_disable_gps", tr("禁用GPS功能"), tr("禁用所有GPS相关功能。\n需要重启生效。"));
  auto_shutdown_timer_toggle = new ParamSpinBoxControl("dp_device_auto_shutdown_timer", tr("自动关机计时器"), tr("自动关机的分钟数。"), "", 1, 60, 1, tr(" 分钟"));
  addItem(auto_shutdown_timer_toggle);
  
  std::vector<QString> display_mode_texts{tr("标准"), tr("道路"), tr("MAIN"), tr("OP")};
  addItem(new ButtonParamControl("dp_device_display_off_mode", tr("显示屏关闭模式"), tr("设置设备显示屏的关闭模式。\n标准 - 点火开启时屏幕保持唤醒\n道路 - 行驶时屏幕关闭\nMAIN - 仅ACC开启时屏幕关闭\nOP - 仅开放领航启用时屏幕关闭"), "", display_mode_texts));
  
  // 声音设置
  std::vector<QString> audible_alert_mode_texts{tr("标准"), tr("警告"), tr("关闭")};
  addItem(new ButtonParamControl("dp_device_audible_alert_mode", tr("声音警报模式"), 
                                 tr("标准 - 标准行为。\n警告 - 仅在有警告时发出声音。\n关闭 - 完全不发出任何声音。"), 
                                 "", audible_alert_mode_texts));
}

void DPSystemPanel::addUIToggles() {
  addItem(new LabelControl(QString::fromUtf8("🎨 ") + tr("用户界面") + QString::fromUtf8(" 🎨"), ""));

  std::vector<QString> log_level_texts{tr("警告"), tr("信息"), tr("调试")};
  addItem(new ButtonParamControl("dp_log_level", tr("日志级别"), tr("调整日志级别，默认警告。"), "", log_level_texts));

  std::vector<QString> dev_ui_settings_texts{tr("关闭"), tr("行"), tr("列"), tr("全部")};
  addItem(new ButtonParamControl("dp_dev_ui_info", tr("开发者UI"), tr("显示来自各种来源的实时参数和指标。"), "", dev_ui_settings_texts));
  setupToggle(this, toggles, params, "dp_hide_ui", tr("隐藏界面UI信息"), tr("隐藏HUD显示的部分信息。"));

  std::vector<QString> device_mode_texts{tr("节能"), tr("普通"), tr("性能")};
  addItem(new ButtonParamControl("dp_device_mode", tr("设备运行模式"), tr("节能模式 - 降低功耗和性能\n普通模式 - 平衡性能和功耗\n性能模式 - 最大化性能"), "", device_mode_texts));


}

void DPSystemPanel::addDashcamToggles() {
  addItem(new LabelControl(QString::fromUtf8("📹 ") + tr("行车记录仪") + QString::fromUtf8(" 📹"), ""));

  setupToggle(this, toggles, params, "dp_on_road_dashcam", tr("启用道路行车记录仪"), tr("驾驶时录制视频。"));
  connect(toggles["dp_on_road_dashcam"], &ToggleControl::toggleFlipped, [=]() { updateToggles(); });

  std::vector<QString> dashcam_quality_texts{tr("480p"), tr("720p"), tr("1080p")};
  dashcam_quality_setting = new ButtonParamControl("dp_dashcam_quality", tr("行车记录仪质量"), tr("视频录制质量。"), "", dashcam_quality_texts);
  addItem(dashcam_quality_setting);

  dashcam_duration_toggle = new ParamSpinBoxControl("dp_dashcam_duration", tr("片段时长"), tr("每个视频片段的时长（分钟）。"), "", 1, 10, 1, tr(" 分钟"));
  addItem(dashcam_duration_toggle);

  dashcam_kept_hours_toggle = new ParamSpinBoxControl("dp_dashcam_kept_hours", tr("保留时长"), tr("保留录像的小时数。"), "", 1, 24, 1, tr(" 小时"));
  addItem(dashcam_kept_hours_toggle);
}

void DPSystemPanel::addServiceToggles() {
  addItem(new LabelControl(QString::fromUtf8("🔧 ") + tr("服务") + QString::fromUtf8(" 🔧"), ""));
  setupToggle(this, toggles, params, "dp_car_dashcam_mode_removal", tr("绕过行车记录仪模式"), tr("如果您在路上看到'行车记录仪模式'，请启用该选项，这将强制启用 openpilot 控制。\n行车记录仪模式通常意味着您的车辆未得到完全支持。\n使用风险自负！\n需要重新启动。"));
  setupToggle(this, toggles, params, "dp_fleet_fileserv", tr("启用文件服务器"), tr("开启后，您可以通过浏览器在端口5050访问设备数据。\n需要在同一网络下使用（如局域网）。\n需要重启生效。"));
  setupToggle(this, toggles, params, "dp_otisserv", tr("启用Otisserv"), tr("开启后，您可以通过 drangonpilot.org 远程访问某些功能。"));
  setupToggle(this, toggles, params, "dp_gpxd", tr("启用GPS记录"), tr("开启后，openpilot 将记录您的轨迹到 /data/media/0/gpx_logs/ 目录。\n需要重启生效。"));
  setupToggle(this, toggles, params, "dp_mapd", tr("启用MAPD功能"), tr("启用后，openpilot 将使用MAPD（地图辅助驾驶）功能。\n需要重启生效。"));
  setupToggle(this, toggles, params, "dp_panda_monitoring", tr("Panda性能监控"), tr("监控Panda设备的性能数据。\n日志保存在/data/media/0/c2_logs/panda_logs/目录下。\n需要重启生效。"));
  setupToggle(this, toggles, params, "dp_control_monitoring", tr("控制性能监控"), tr("监控Openpilot控制输出与车辆实际状态的差异。\n日志保存在/data/media/0/c2_logs/control_logs/目录下。\n需要重启生效。"));
}

void DPSystemPanel::updateToggles() {
  bool shutdown_enabled = params.getBool("dp_device_auto_shutdown");
  if (auto_shutdown_timer_toggle) auto_shutdown_timer_toggle->setVisible(shutdown_enabled);

  bool dashcam_enabled = params.getBool("dp_on_road_dashcam");
  if (dashcam_quality_setting) dashcam_quality_setting->setVisible(dashcam_enabled);
  if (dashcam_duration_toggle) dashcam_duration_toggle->setVisible(dashcam_enabled);
  if (dashcam_kept_hours_toggle) dashcam_kept_hours_toggle->setVisible(dashcam_enabled);
}

void DPSystemPanel::showEvent(QShowEvent *event) {
  updateToggles();
}
