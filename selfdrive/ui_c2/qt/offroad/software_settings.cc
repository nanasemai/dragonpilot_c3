#include "selfdrive/ui/qt/offroad/settings.h"

#include <cassert>
#include <cmath>
#include <string>

#include <QDebug>
#include <QLabel>

#include "common/params.h"
#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "system/hardware/hw.h"


void SoftwarePanel::checkForUpdates() {
  std::system("pkill -SIGUSR1 -f selfdrive.updated");
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  onroadLbl = new QLabel(tr("Updates are only downloaded while the car is off."));
  onroadLbl->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left; padding-top: 30px; padding-bottom: 30px;");
  addItem(onroadLbl);

  // current version
  versionLbl = new LabelControl(tr("Current Version"), "");
  addItem(versionLbl);

  // 禁用自动更新
  auto disableUpdates = new ToggleControl(tr("禁用自动更新"), tr("启用后将停止自动更新功能\n需要重启生效"),
    "", Params().getBool("DisableUpdates"));
    connect(disableUpdates, &ToggleControl::toggleFlipped, [=](bool state) {
    Params().putBool("DisableUpdates", state);
  });
  addItem(disableUpdates);

  // 在线/离线模式切换
  onOffRoadBtn = new ButtonControl(tr("在线/离线模式"), tr("切换到离线"));
  connect(onOffRoadBtn, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("确定要切换模式吗？"), tr("确认"), this)) {
      bool val = params.getBool("dp_device_go_off_road");
      // 直接设置参数，不判断返回值
      params.putBool("dp_device_go_off_road", !val);
      // 强制刷新界面状态
      updateLabels();
      // 根据当前实际参数值显示提示
      bool newVal = params.getBool("dp_device_go_off_road");
      if (newVal != val) {
        // 切换成功 - 修正提示消息与实际状态的对应关系
        QString msg = newVal ? tr("已切换到离线模式") : tr("已切换到在线模式");
        ConfirmationDialog::alert(msg, this);
      } else {
        // 切换失败
        ConfirmationDialog::alert(tr("模式切换失败，请重试"), this);
      }
    }
  });
  addItem(onOffRoadBtn);

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), tr("CHECK"));
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);
    if (downloadBtn->text() == tr("CHECK")) {
      checkForUpdates();
    } else {
      std::system("pkill -SIGHUP -f selfdrive.updated");
    }
  });
  addItem(downloadBtn);

  // install update btn
  installBtn = new ButtonControl(tr("Install Update"), tr("INSTALL"));
  connect(installBtn, &ButtonControl::clicked, [=]() {
    installBtn->setEnabled(false);
    params.putBool("DoReboot", true);
  });
  addItem(installBtn);

  //#ifndef QCOM
  // branch selecting
  targetBranchBtn = new ButtonControl(tr("Target Branch"), tr("SELECT"));
  connect(targetBranchBtn, &ButtonControl::clicked, [=]() {
    auto current = params.get("GitBranch");
    QStringList branches = QString::fromStdString(params.get("UpdaterAvailableBranches")).split(",");
    for (QString b : {current.c_str(), "devel-staging", "devel", "nightly", "master-ci", "master"}) {
      auto i = branches.indexOf(b);
      if (i >= 0) {
        branches.removeAt(i);
        branches.insert(0, b);
      }
    }

    QString cur = QString::fromStdString(params.get("UpdaterTargetBranch"));
    QString selection = MultiOptionDialog::getSelection(tr("Select a branch"), branches, cur, this);
    if (!selection.isEmpty()) {
      params.put("UpdaterTargetBranch", selection.toStdString());
      targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));
      checkForUpdates();
    }
  });
  //if (!params.getBool("IsTestedBranch")) {
  addItem(targetBranchBtn);
  //}
  //#endif

  // uninstall button
  auto uninstallBtn = new ButtonControl(tr("Uninstall %1").arg(getBrand()), tr("UNINSTALL"));
  connect(uninstallBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to uninstall?"), tr("Uninstall"), this)) {
      params.putBool("DoUninstall", true);
    }
  });
  addItem(uninstallBtn);

  fs_watch = new ParamWatcher(this);
  QObject::connect(fs_watch, &ParamWatcher::paramChanged, [=](const QString &param_name, const QString &param_value) {
    updateLabels();
  });

  connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    is_onroad = !offroad;
    updateLabels();
  });

  updateLabels();
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  // nice for testing on PC
  installBtn->setEnabled(true);

  updateLabels();
}

void SoftwarePanel::updateLabels() {
  // add these back in case the files got removed
  fs_watch->addParam("LastUpdateTime");
  fs_watch->addParam("UpdateFailedCount");
  fs_watch->addParam("UpdaterState");
  fs_watch->addParam("UpdateAvailable");
  fs_watch->addParam("dp_device_go_off_road");

  if (!isVisible()) {
    return;
  }

  // updater only runs offroad
  onroadLbl->setVisible(is_onroad);
  downloadBtn->setVisible(!is_onroad);

  // on/off road text change
  if (params.getBool("dp_device_go_off_road")) {
    onOffRoadBtn->setText(tr("切换到在线"));
  } else {
    onOffRoadBtn->setText(tr("切换到离线"));
  }
  // download update
  QString updater_state = QString::fromStdString(params.get("UpdaterState"));
  bool failed = std::atoi(params.get("UpdateFailedCount").c_str()) > 0;
  if (updater_state != "idle") {
    downloadBtn->setEnabled(false);
    // 解析进度信息
    if (updater_state.contains("downloading...")) {
      QString progress = updater_state;
      if (updater_state.contains("%")) {
        progress = updater_state.split("%").first() + "%";
      }
      downloadBtn->setValue(progress);
      // 设置下载状态的样式
      downloadBtn->setStyleSheet("QPushButton { background-color: #30BF78; }");
    } else {
      downloadBtn->setValue(updater_state);
    }
  } else {
    // 重置按钮样式
    downloadBtn->setStyleSheet("");
    if (failed) {
      downloadBtn->setText(tr("CHECK"));
      downloadBtn->setValue(tr("failed to check for update"));
    } else if (params.getBool("UpdaterFetchAvailable")) {
      downloadBtn->setText(tr("DOWNLOAD"));
      downloadBtn->setValue(tr("update available"));
    } else {
      QString lastUpdate = tr("never");
      auto tm = params.get("LastUpdateTime");
      if (!tm.empty()) {
        lastUpdate = timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
      }
      downloadBtn->setText(tr("CHECK"));
      downloadBtn->setValue(tr("up to date, last checked %1").arg(lastUpdate));
    }
    downloadBtn->setEnabled(true);
  }
  #ifndef QCOM
  targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));
  #endif

  // current + new versions
  versionLbl->setText(QString::fromStdString(params.get("UpdaterCurrentDescription")));
  versionLbl->setDescription(QString::fromStdString(params.get("UpdaterCurrentReleaseNotes")));
  installBtn->setVisible(!is_onroad && params.getBool("UpdateAvailable"));
  installBtn->setValue(QString::fromStdString(params.get("UpdaterNewDescription")));
  installBtn->setDescription(QString::fromStdString(params.get("UpdaterNewReleaseNotes")));

  update();
}
