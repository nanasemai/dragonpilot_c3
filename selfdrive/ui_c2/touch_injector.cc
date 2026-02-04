#include "touch_injector.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QWidget>
#include <QWindow>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <QThread>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <cstring>
#include "ui.h" // 确保能访问fb_w, fb_h

TouchInjector::TouchInjector(QWidget* target) : m_target(target), m_listening(false), m_isDragging(false) {
  // Initialize debug file
  // Set up Unix domain socket server
  unlink("/tmp/ui_touch_socket");

  struct sockaddr_un addr;
  m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, "/tmp/ui_touch_socket", sizeof(addr.sun_path) - 1);

  if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
    listen(m_socket, 5);

    // Start listening thread
    m_listening = true;
    m_thread = std::thread(&TouchInjector::listenForInputs, this);
    // writeDebug("Touch injector socket created successfully at /tmp/ui_touch_socket"); // 移除此行
  } else {
    // writeDebug(QString("Failed to create touch injector socket: %1").arg(strerror(errno))); // 移除此行
  }
}

TouchInjector::~TouchInjector() {
  m_listening = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
  close(m_socket);
  unlink("/tmp/ui_touch_socket");
  // writeDebug("=== TouchInjector Destroyed ==="); // 移除此行
  // debugFile.close(); // 移除此行
}

void TouchInjector::listenForInputs() {
  // writeDebug("Input listener thread started"); // 移除此行

  while (m_listening) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_socket, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int activity = select(m_socket + 1, &readfds, NULL, NULL, &timeout);

    if (activity > 0 && FD_ISSET(m_socket, &readfds)) {
      int client_socket = accept(m_socket, NULL, NULL);
      if (client_socket >= 0) {
        char buffer[2048] = {0};
        int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
          // writeDebug(QString("Received raw data: %1").arg(QString::fromLatin1(buffer, bytes_read))); // 移除此行

          QJsonDocument doc = QJsonDocument::fromJson(QByteArray(buffer, bytes_read));
          QJsonObject obj = doc.object();

          // Handle different input types
          QString type = obj["type"].toString();
          // writeDebug(QString("Parsed input type: %1").arg(type)); // 移除此行

          // Emit signal to inject on main thread
          if (type == "touch" || type == "click" || type == "tap") {
            // Legacy touch/click handling
            int x = obj["x"].toInt();
            int y = obj["y"].toInt();
            QMetaObject::invokeMethod(this, "injectTouch", Qt::QueuedConnection,
                                    Q_ARG(int, x), Q_ARG(int, y), Q_ARG(QString, type));
          } else {
            // New input handling
            QMetaObject::invokeMethod(this, "injectInput", Qt::QueuedConnection,
                                    Q_ARG(QJsonObject, obj));
          }
        }
        close(client_socket);
      }
    }
  }
  // writeDebug("Input listener thread ended"); // 移除此行
}

// 工具函数：坐标缩放（假设UI分辨率固定为1080P，支持 High DPI）
QPoint TouchInjector::scaleToUi(int x, int y) {
  // 假设UI分辨率固定为 1920x1080，UI端已经完成了截图分辨率到UI坐标的转换
  // 这里只需要考虑High DPI缩放
  if (!m_target) {
    return QPoint(x, y);
  }
  
  qreal dpr = m_target->devicePixelRatioF();
  int scaled_x = qRound(x * dpr);
  int scaled_y = qRound(y * dpr);
  
  return QPoint(scaled_x, scaled_y);
}

// 工具函数：获取UI像素大小（假设UI分辨率固定为1920x1080）
QSize TouchInjector::getUiSize() {
  // 假设UI窗口大小固定为1920x1080，再考虑High DPI
  if (!m_target) return QSize(1920, 1080);
  
  qreal dpr = m_target->devicePixelRatioF();
  return QSize(int(1920 * dpr), int(1080 * dpr));
}

void TouchInjector::injectInput(const QJsonObject& inputData) {
  QString type = inputData["type"].toString();
  
  if (type == "click" || type == "tap") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    QPoint uiPt = scaleToUi(x, y);
    qDebug() << "[TouchInjector] 点击坐标 (UI坐标系):" << uiPt;
    injectClick(uiPt.x(), uiPt.y());
    return;
  }
  if (type == "scroll") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    double deltaY = inputData["deltaY"].toDouble();
    QPoint uiPt = scaleToUi(x, y);
    qDebug() << "[TouchInjector] 滚动坐标 (UI坐标系):" << uiPt << "deltaY:" << deltaY;
    injectScroll(uiPt.x(), uiPt.y(), deltaY);
  } else if (type == "drag") {
    int startX = inputData["startX"].toInt();
    int startY = inputData["startY"].toInt();
    int currentX = inputData["x"].toInt();
    int currentY = inputData["y"].toInt();
    QPoint uiStart = scaleToUi(startX, startY);
    QPoint uiCurrent = scaleToUi(currentX, currentY);
    qDebug() << "[TouchInjector] 拖拽坐标 (UI坐标系): (" << uiStart << ") -> (" << uiCurrent << ")";
    injectDrag(uiStart.x(), uiStart.y(), uiCurrent.x(), uiCurrent.y());
  } else if (type == "dragend") {
    handleDragEnd(inputData);
    m_isDragging = false;
  } else if (type == "swipe") {
    int startX = inputData["x"].toInt();
    int startY = inputData["y"].toInt();
    int endX = inputData["endX"].toInt();
    int endY = inputData["endY"].toInt();
    int duration = inputData["duration"].toInt(300);
    
    QPoint uiStart = scaleToUi(startX, startY);
    QPoint uiEnd = scaleToUi(endX, endY);
    
    qDebug() << "[TouchInjector] Swipe (UI坐标系): (" << uiStart << ") -> (" << uiEnd << ") duration:" << duration;
    
    injectSwipe(uiStart.x(), uiStart.y(), uiEnd.x(), uiEnd.y(), duration);
  } else if (type == "mousedown" || type == "touchstart") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    QPoint uiPt = scaleToUi(x, y);
    m_dragStart = uiPt;
    m_isDragging = false;
    
    QPoint globalPos = m_target->mapToGlobal(uiPt);
    QMouseEvent pressEvent(QEvent::MouseButtonPress, uiPt, globalPos,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_target, &pressEvent);
    qDebug() << "[TouchInjector] Mousedown sent MouseButtonPress at" << uiPt;
  }
}

void TouchInjector::injectScroll(int x, int y, double deltaY) {
  QPoint localPos(x, y);
  QPoint globalPos = m_target->mapToGlobal(localPos);
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalPos);
  if (widgetUnderPoint) {
    QPoint widgetLocalPos = widgetUnderPoint->mapFromGlobal(globalPos);
    QWheelEvent wheelEvent(widgetLocalPos, globalPos, QPoint(0, 0), QPoint(0, int(deltaY * 15)),
                          Qt::NoButton, Qt::NoModifier, Qt::ScrollPhase::NoScrollPhase, false);
    QApplication::sendEvent(widgetUnderPoint, &wheelEvent);
    QWheelEvent wheelEvent2(localPos, globalPos, QPoint(0, 0), QPoint(0, int(deltaY * 15)),
                           Qt::NoButton, Qt::NoModifier, Qt::ScrollPhase::NoScrollPhase, false);
    QApplication::sendEvent(m_target, &wheelEvent2);
  }
}

void TouchInjector::injectSwipe(int startX, int startY, int endX, int endY, int duration) {
  QPoint startPos(startX, startY);
  QPoint endPos(endX, endY);
  QPoint globalStartPos = m_target->mapToGlobal(startPos);
  QPoint globalEndPos = m_target->mapToGlobal(endPos);
  
  QMouseEvent pressEvent(QEvent::MouseButtonPress, startPos, globalStartPos,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_target, &pressEvent);
  
  int steps = qMax(2, duration / 16);
  for (int i = 1; i <= steps; ++i) {
    qreal t = qreal(i) / steps;
    int x = startPos.x() + qRound((endPos.x() - startPos.x()) * t);
    int y = startPos.y() + qRound((endPos.y() - startPos.y()) * t);
    
    QPoint currPos(x, y);
    QPoint globalCurrPos = m_target->mapToGlobal(currPos);
    
    QMouseEvent moveEvent(QEvent::MouseMove, currPos, globalCurrPos,
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_target, &moveEvent);
    
    QWidget* widgetUnderPoint = QApplication::widgetAt(globalCurrPos);
    if (widgetUnderPoint && widgetUnderPoint != m_target) {
      QPoint widgetLocalPos = widgetUnderPoint->mapFromGlobal(globalCurrPos);
      QMouseEvent moveEvent2(QEvent::MouseMove, widgetLocalPos, globalCurrPos,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(widgetUnderPoint, &moveEvent2);
    }
    
    QThread::msleep(16);
  }
  
  QMouseEvent releaseEvent(QEvent::MouseButtonRelease, endPos, globalEndPos,
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(m_target, &releaseEvent);
}

void TouchInjector::injectDrag(int startX, int startY, int currentX, int currentY) {
  QPoint startPos(startX, startY);
  QPoint currentPos(currentX, currentY);
  QPoint globalCurrentPos = m_target->mapToGlobal(currentPos);
  
  QMouseEvent moveEvent(QEvent::MouseMove, currentPos, globalCurrentPos,
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_target, &moveEvent);
  
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalCurrentPos);
  if (widgetUnderPoint && widgetUnderPoint != m_target) {
    QPoint widgetLocalPos = widgetUnderPoint->mapFromGlobal(globalCurrentPos);
    QMouseEvent moveEvent2(QEvent::MouseMove, widgetLocalPos, globalCurrentPos,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widgetUnderPoint, &moveEvent2);
  }
}

void TouchInjector::handleDragEnd(const QJsonObject& inputData) {
  if (!m_isDragging) return;
  
  int x = inputData["x"].toInt();
  int y = inputData["y"].toInt();
  QPoint endPos = scaleToUi(x, y);
  QPoint globalEndPos = m_target->mapToGlobal(endPos);
  
  qDebug() << "[TouchInjector] Drag end (UI坐标系):" << endPos;
  
  QMouseEvent releaseEvent(QEvent::MouseButtonRelease, endPos, globalEndPos,
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(m_target, &releaseEvent);
  
  m_isDragging = false;
}

void TouchInjector::injectClick(int x, int y) {
  QPoint localPos(x, y);
  QPoint globalPos = m_target->mapToGlobal(localPos);
  
  // 尝试找到点击位置下的 widget
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalPos);
  QWidget* targetWidget = m_target;
  QPoint targetLocalPos = localPos;
  
  if (widgetUnderPoint && widgetUnderPoint != m_target) {
    // 如果找到了子控件，使用子控件的本地坐标
    targetWidget = widgetUnderPoint;
    targetLocalPos = widgetUnderPoint->mapFromGlobal(globalPos);
  } else {
    // 如果没找到子控件，尝试查找所有层级
    QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget* w : widgets) {
      if (w != m_target && w->isVisible()) {
        QPoint widgetLocal = w->mapFromGlobal(globalPos);
        if (w->rect().contains(widgetLocal)) {
          // 找到包含点击位置的顶层窗口
          QWidget* child = w->childAt(widgetLocal);
          if (child) {
            targetWidget = child;
            targetLocalPos = child->mapFromGlobal(globalPos);
          } else {
            targetWidget = w;
            targetLocalPos = widgetLocal;
          }
          break;
        }
      }
    }
  }
  
  // 如果目标控件仍然不是子控件，尝试在 m_target 层级查找
  if (targetWidget == m_target || targetWidget == nullptr) {
    QWidget* childAtTarget = m_target->childAt(localPos);
    if (childAtTarget) {
      targetWidget = childAtTarget;
      targetLocalPos = localPos;
    }
  }
  
  qDebug() << "[TouchInjector] Click target:" << targetWidget << "localPos:" << targetLocalPos << "globalPos:" << globalPos;
  
  QMouseEvent* pressEvent = new QMouseEvent(QEvent::MouseButtonPress, targetLocalPos, globalPos,
                                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, targetLocalPos, globalPos,
                                             Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(targetWidget, pressEvent);
  QTimer::singleShot(50, [=]() {
    QApplication::sendEvent(targetWidget, releaseEvent);
    delete pressEvent;
    delete releaseEvent;
  });
}

void TouchInjector::injectTouch(int x, int y, const QString& type) {
  injectClick(x, y);
}