#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace {

QString clipped(QString text)
{
    text.replace(u'\n', QStringLiteral("\\n"));
    text.replace(u'\r', QStringLiteral("\\r"));

    constexpr qsizetype maxLength = 160;
    if (text.size() > maxLength) {
        return text.left(maxLength) + QStringLiteral("...");
    }
    return text;
}

void printSelection(QClipboard *clipboard, const char *reason)
{
    const QString text = clipboard->text(QClipboard::Selection);
    std::cout << reason << ": "
              << "ownsSelection=" << (clipboard->ownsSelection() ? "true" : "false")
              << " text=\"" << clipped(text).toStdString() << "\""
              << std::endl;
}

const char *envValue(const char *name)
{
    const char *value = std::getenv(name);
    return value == nullptr ? "" : value;
}

}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QClipboard *clipboard = QGuiApplication::clipboard();

    std::cout << "QClipboard::Selection POC" << std::endl;
    std::cout << "platformName=" << QGuiApplication::platformName().toStdString() << std::endl;
    std::cout << "XDG_SESSION_TYPE=" << envValue("XDG_SESSION_TYPE") << std::endl;
    std::cout << "WAYLAND_DISPLAY=" << envValue("WAYLAND_DISPLAY") << std::endl;
    std::cout << "DISPLAY=" << envValue("DISPLAY") << std::endl;
    std::cout << "supportsSelection=" << (clipboard->supportsSelection() ? "true" : "false") << std::endl;
    std::cout << "Select text in another application. Press Ctrl+C here to stop." << std::endl;

    printSelection(clipboard, "initial");

    QObject::connect(clipboard, &QClipboard::selectionChanged, clipboard, [clipboard]() {
        printSelection(clipboard, "selectionChanged");
    });

    QObject::connect(clipboard, &QClipboard::changed, clipboard, [clipboard](QClipboard::Mode mode) {
        if (mode == QClipboard::Selection) {
            printSelection(clipboard, "changed(Selection)");
        }
    });

    QString lastPolledText = clipboard->text(QClipboard::Selection);
    QTimer pollTimer;
    pollTimer.setInterval(500);
    QObject::connect(&pollTimer, &QTimer::timeout, clipboard, [&lastPolledText, clipboard]() {
        const QString text = clipboard->text(QClipboard::Selection);
        if (text != lastPolledText) {
            lastPolledText = text;
            printSelection(clipboard, "poll");
        }
    });
    pollTimer.start();

    return QCoreApplication::exec();
}

