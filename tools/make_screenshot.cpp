// Renders the real application window to a PNG for the README.
//
// It drives the actual MainWindow through a genuine scan rather than
// assembling a mock-up, so the screenshot cannot drift away from what the
// program does. Regenerating it after a change is one command.
//
//   make_screenshot <folder-to-scan> <output.png>

#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QTextStream out(stdout);

    if (argc < 3) {
        out << "usage: make_screenshot <folder> <output.png>\n";
        return 2;
    }

    const QString folder = QString::fromLocal8Bit(argv[1]);
    const QString output = QString::fromLocal8Bit(argv[2]);

    winnow::MainWindow window;
    window.resize(1180, 760);
    window.show();

    QObject::connect(&window, &winnow::MainWindow::scanFinished, &window, [&] {
        // Thumbnails are decoded as the list is filled, and the widget needs one
        // more turn of the event loop to lay them out before it is worth
        // capturing.
        QTimer::singleShot(400, &window, [&] {
            QDir().mkpath(QFileInfo(output).absolutePath());
            const QPixmap shot = window.grab();
            if (shot.save(output))
                out << "Wrote " << output << "\n";
            else
                out << "Failed to write " << output << "\n";
            QApplication::quit();
        });
    });

    // Give the window one event-loop turn to appear before the scan starts.
    QTimer::singleShot(0, &window, [&] { window.beginScan(folder); });

    return app.exec();
}
