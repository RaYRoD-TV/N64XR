#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;

namespace cxr {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onOpenRom();
    void onLaunchVr();

private:
    QPushButton* m_openRomBtn  = nullptr;
    QPushButton* m_launchVrBtn = nullptr;
    QLabel*      m_status      = nullptr;
    QString      m_currentRom;
};

}  // namespace cxr
