#include "waylandprimaryselection.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    WaylandPrimarySelection primarySelection;
    if (!primarySelection.isValid()) {
        qWarning() << "Wayland primary-selection client is not valid.";
        return 1;
    }

    QObject::connect(&primarySelection, &WaylandPrimarySelection::selectionChanged, &app, [](const QString &text) {
        if (text.isEmpty()) {
            qInfo() << "selection: <empty>";
            return;
        }

        qInfo().noquote() << "selection:" << text;
    });

    qInfo() << "Watching Wayland primary selection. Select text in another window; press Ctrl+C here to quit.";
    return app.exec();
}
