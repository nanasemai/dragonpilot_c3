## 移植DEV INFO功能到当前UI界面

### 1. 了解当前UI结构

当前UI使用`AnnotatedCameraWidget`类显示摄像头视图和信息，包含：
- `hud`对象：绘制HUD信息
- `dmon`对象：绘制驾驶员监控信息
- `UIScene`结构体：存储UI状态和参数

### 2. 了解ui_c2中的DEV INFO功能

- 由`dev_ui_info`参数控制，通过设置界面调整
- 在`drawHud`方法中绘制，显示实时参数和指标
- 分为静态层（标题和单位）和动态层（数值）
- 显示多种信息：纵向/横向控制状态、车道线概率、扭矩参数等

### 3. 移植步骤

#### 3.1 修改UIScene结构体

在`selfdrive/ui/ui.h`的`UIScene`结构体中添加：
```cpp
int dev_ui_info = 0;
bool torqued_override = false;
bool long_pid_override = false;
// 其他需要的参数...
```

#### 3.2 更新ui_update_params函数

在`selfdrive/ui/ui.cc`的`ui_update_params`函数中添加：
```cpp
s->scene.dev_ui_info = std::atoi(params.get("dp_dev_ui_info").c_str());
s->scene.torqued_override = std::atoi(params.get("dp_torqued_override").c_str());
s->scene.long_pid_override = std::atoi(params.get("dp_long_pid_override").c_str());
// 其他参数的读取...
```

#### 3.3 修改AnnotatedCameraWidget类

在`selfdrive/ui/qt/onroad/annotated_camera.h`中添加：
```cpp
private:
  // DEV UI相关成员变量
  int devUiInfo = 0;
  // 其他需要的成员变量...
  
  // DEV UI绘制方法
  void drawDevUI(QPainter &p, const QRect &rect);
  void updateDevUIState(const UIState &s);
```

#### 3.4 更新updateState方法

在`selfdrive/ui/qt/onroad/annotated_camera.cc`的`updateState`方法中添加：
```cpp
// 更新DEV UI状态
updateDevUIState(s);
```

#### 3.5 实现DEV UI相关方法

在`AnnotatedCameraWidget`类中添加：
```cpp
void AnnotatedCameraWidget::updateDevUIState(const UIState &s) {
  // 更新DEV UI所需的数据
  devUiInfo = s.scene.dev_ui_info;
  // 其他数据更新...
}

void AnnotatedCameraWidget::drawDevUI(QPainter &p, const QRect &rect) {
  if (devUiInfo == 0) return;
  
  // 绘制DEV UI信息
  // 参考ui_c2/qt/onroad.cc中的drawHud方法
  // 分为静态层和动态层绘制
}
```

#### 3.6 修改drawHud方法

在`selfdrive/ui/qt/onroad/hud.cc`的`draw`方法中添加：
```cpp
// 绘制DEV UI
if (dev_ui_info != 0) {
  annotated_camera_widget->drawDevUI(p, rect);
}
```

#### 3.7 在设置界面中添加控制选项

在`selfdrive/ui/qt/offroad/dp_panel.cc`中添加：
```cpp
// 添加DEV UI控制选项
addItem(new ButtonParamControl("dp_dev_ui_info", tr("开发者UI"), tr("显示来自各种来源的实时参数和指标。"), "", dev_ui_settings_texts));
```

### 4. 注意事项

1. 不需要完全复制ui_c2的实现，可以根据当前UI的结构和需求进行调整
2. 考虑性能影响，只在需要时绘制DEV UI
3. 确保与当前UI的设计风格保持一致
4. 测试不同`dev_ui_info`值的显示效果
5. 确保在不同屏幕尺寸下都能正常显示

### 5. 可显示的信息

根据当前cereal消息定义，可以显示：
- 纵向控制状态和参数
- 横向控制状态和参数
- 车道线概率和状态
- 车辆状态信息（速度、加速度、转向角等）
- 雷达信息（前车距离、相对速度等）
- 模型输出信息
- 系统状态信息

通过以上步骤，可以将ui_c2中的DEV INFO功能移植到当前UI界面中，为开发者提供实时的参数和指标显示。