#include "ui/MainWindow.h"

#include <QApplication>

// main() does nothing but wire the pieces together and hand control to Qt.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Winnow");
    QApplication::setOrganizationName("Winnow");

    winnow::MainWindow window;
    window.show();

    return app.exec();
}
