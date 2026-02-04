#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <QTimer>
#include <QJsonObject>
#include <QPoint>
#include <thread>

// Touch and scroll event injector class
class TouchInjector : public QObject {
  Q_OBJECT

public:
  TouchInjector(QWidget* target);
  ~TouchInjector();

private:
  QPoint scaleToUi(int x, int y);
  QSize getUiSize();

public slots:
  // 注：所有坐标参数均需归一化到UI分辨率体系
  void injectTouch(int x, int y, const QString& type);
  void injectInput(const QJsonObject& inputData); // 推荐通过 inputData 传递 img_w/img_h/x/y 等

private:
  void listenForInputs();
  void injectScroll(int x, int y, double deltaY);
  void injectDrag(int startX, int startY, int currentX, int currentY);
  void injectSwipe(int startX, int startY, int endX, int endY, int duration);
  void injectClick(int x, int y);
  void handleDragEnd(const QJsonObject& inputData);

  QWidget* m_target;
  int m_socket;
  bool m_listening;
  std::thread m_thread;
  // Drag state
  bool m_isDragging;
  QPoint m_dragStart;
};