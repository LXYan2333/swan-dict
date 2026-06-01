#include "clipboardwriter.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardWriter::ClipboardWriter(QObject *parent)
    : QObject(parent)
{
}

void ClipboardWriter::setText(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return;
    }

    clipboard->setText(text, QClipboard::Clipboard);
}
