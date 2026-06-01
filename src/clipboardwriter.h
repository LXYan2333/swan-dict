#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class ClipboardWriter : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ClipboardWriter(QObject *parent = nullptr);

    Q_INVOKABLE void setText(const QString &text);
};
