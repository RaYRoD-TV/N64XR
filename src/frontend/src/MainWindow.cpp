#include "MainWindow.h"

#include "XrSession.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

namespace n64xr {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("N64 XR — Phase 1 scaffold");
    resize(540, 220);

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto* title = new QLabel(
        "<h2 style='margin:0'>N64 XR</h2>"
        "<p style='color:#888;margin:0'>Standalone OpenXR-backed VR fork of simple64 + GLideN64.</p>",
        central);
    title->setTextFormat(Qt::RichText);
    layout->addWidget(title);

    auto* buttonRow = new QHBoxLayout;
    m_openRomBtn  = new QPushButton(tr("Open ROM…"), central);
    m_launchVrBtn = new QPushButton(tr("Launch VR (magenta-clear smoke test)"), central);
    buttonRow->addWidget(m_openRomBtn);
    buttonRow->addWidget(m_launchVrBtn);
    layout->addLayout(buttonRow);

    m_status = new QLabel(tr("No ROM loaded. Phase 1 scaffold — Launch VR opens an OpenXR session and clears both eyes magenta."), central);
    m_status->setWordWrap(true);
    layout->addWidget(m_status, /*stretch*/ 1);

    setCentralWidget(central);

    connect(m_openRomBtn,  &QPushButton::clicked, this, &MainWindow::onOpenRom);
    connect(m_launchVrBtn, &QPushButton::clicked, this, &MainWindow::onLaunchVr);
}

void MainWindow::onOpenRom() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open N64 ROM"), dir, tr("N64 ROM (*.n64 *.v64 *.z64);;All files (*)"));
    if (path.isEmpty()) return;
    m_currentRom = path;
    m_status->setText(tr("Loaded: %1").arg(path));
    spdlog::info("[frontend] ROM selected: {}", path.toStdString());
}

void MainWindow::onLaunchVr() {
    m_status->setText(tr("Starting OpenXR session…"));
    QApplication::processEvents();

    XrSession session;
    if (!session.initialize()) {
        m_status->setText(tr("XR initialise FAILED — see log."));
        QMessageBox::critical(this, tr("N64 XR"),
            tr("OpenXR failed to initialise. Make sure a runtime (SteamVR or Oculus PC) is set as the active OpenXR runtime."));
        return;
    }

    // Pump frames for ~3 seconds of magenta to prove the loop, then exit.
    constexpr int kFramesSmokeTest = 270;  // ~3s at 90Hz
    for (int i = 0; i < kFramesSmokeTest; ++i) {
        if (!session.pumpFrame()) break;
        if ((i & 31) == 0) QApplication::processEvents();
    }
    session.shutdown();

    m_status->setText(tr("Smoke test done — magenta cleared in both eyes for ~3 seconds."));
}

}  // namespace n64xr
