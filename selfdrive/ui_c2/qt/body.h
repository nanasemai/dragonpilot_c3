#pragma once

#include <QMovie>
#include <QLabel>

#include "common/util.h"
#include "selfdrive/ui/ui.h"

class BodyWindow : public QWidget {
  Q_OBJECT

public:
  BodyWindow(QWidget* parent = 0);

private:
  bool charging = false;
  FirstOrderFilter fuel_filter;
  QLabel *face;
  QMovie *awake, *sleep;
  void paintEvent(QPaintEvent*) override;

private slots:
  void updateState(const UIState &s);
  void offroadTransition(bool onroad);
};
