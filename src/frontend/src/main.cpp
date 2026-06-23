#include "MainWindow.h"

#include <QApplication>

#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("N64XR");
    QApplication::setApplicationVersion("0.0.1");
    QApplication::setOrganizationName("N64XR");

    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("N64XR {} starting", QApplication::applicationVersion().toStdString());

    n64xr::MainWindow w;
    w.show();
    return app.exec();
}
