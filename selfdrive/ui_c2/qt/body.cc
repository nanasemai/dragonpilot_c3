#include "selfdrive/ui/qt/body.h"

#include <cmath>
#include <algorithm>

#include <QPainter>
#include <QStackedLayout>

#include "common/params.h"
#include "common/timing.h"

BodyWindow::BodyWindow(QWidget *parent) : fuel_filter(1.0, 5., 1. / UI_FREQ), QWidget(parent) {
  QStackedLayout *layout = new QStackedLayout(this);
  layout->setStackingMode(QStackedLayout::StackAll);

  QWidget *w = new QWidget;
  QVBoxLayout *vlayout = new QVBoxLayout(w);
  vlayout->setMargin(45);
  layout->addWidget(w);

  // face
  face = new QLabel();
  face->setAlignment(Qt::AlignCenter);
  layout->addWidget(face);
  awake = new QMovie("../assets/body/awake.gif");
  awake->setCacheMode(QMovie::CacheAll);
  sleep = new QMovie("../assets/body/sleep.gif");
  sleep->setCacheMode(QMovie::CacheAll);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &BodyWindow::updateState);
}

void BodyWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(0, 0, 0));

  // battery outline + detail
  p.translate(width() - 136, 16);
  const QColor gray = QColor("#737373");
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(gray, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawRoundedRect(2, 2, 78, 36, 8, 8);

  p.setPen(Qt::NoPen);
  p.setBrush(gray);
  p.drawRoundedRect(84, 12, 6, 16, 4, 4);
  p.drawRect(84, 12, 3, 16);

  // battery level
  double fuel = std::clamp(fuel_filter.x(), 0.2f, 1.0f);
  const int m = 5; // manual margin since we can't do an inner border
  p.setPen(Qt::NoPen);
  p.setBrush(fuel > 0.25 ? QColor("#32D74B") : QColor("#FF453A"));
  p.drawRoundedRect(2 + m, 2 + m, (78 - 2*m)*fuel, 36 - 2*m, 4, 4);

  // charging status
  if (charging) {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    const QPolygonF charger({
      QPointF(12.31, 0),
      QPointF(12.31, 16.92),
      QPointF(18.46, 16.92),
      QPointF(6.15, 40),
      QPointF(6.15, 23.08),
      QPointF(0, 23.08),
    });
    p.drawPolygon(charger.translated(98, 0));
  }
}

void BodyWindow::offroadTransition(bool offroad) {
  fuel_filter.reset(1.0);
}

void BodyWindow::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  const SubMaster &sm = *(s.sm);
  auto cs = sm["carState"].getCarState();

  charging = cs.getCharging();
  fuel_filter.update(cs.getFuelGauge());

  // TODO: use carState.standstill when that's fixed
  const bool standstill = std::abs(cs.getVEgo()) < 0.01;
  QMovie *m = standstill ? sleep : awake;
  if (m != face->movie()) {
    face->setMovie(m);
    face->movie()->start();
  }

  update();
}
