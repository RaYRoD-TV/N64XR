#include "MainWindow.h"

#include <QApplication>

#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("CartridgeXR");
    QApplication::setApplicationVersion("0.0.1");
    QApplication::setOrganizationName("CartridgeXR");

    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("CartridgeXR {} starting", QApplication::applicationVersion().toStdString());

    cxr::MainWindow w;
    w.show();
    return app.exec();
}
