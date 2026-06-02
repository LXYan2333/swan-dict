#include "clipboardwriter.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

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

void ClipboardWriter::setRichText(const QString &plainText, const QString &htmlText)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return;
    }

    auto *mimeData = new QMimeData();
    mimeData->setText(plainText);
    mimeData->setHtml(htmlText);
    clipboard->setMimeData(mimeData, QClipboard::Clipboard);
}
