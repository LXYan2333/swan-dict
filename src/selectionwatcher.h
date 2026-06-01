#pragma once

#include <QClipboard>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class SelectionWatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    QML_ELEMENT

public:
    explicit SelectionWatcher(QObject *parent = nullptr);
    ~SelectionWatcher() override;

    QString text() const;
    int interval() const;
    void setInterval(int interval);

Q_SIGNALS:
    void textChanged();
    void intervalChanged();

private Q_SLOTS:
    void readSelection();
    void finishWaylandRead(int exitCode, QProcess::ExitStatus exitStatus);
    void abortWaylandRead();

private:
    void setText(const QString &text);
    QString normalizeSelection(QString text) const;
    bool useWaylandBackend() const;
    void readClipboardSelection();
    void readWaylandSelection();

    QString m_text;
    QTimer m_timer;
    QProcess *m_process = nullptr;
    int m_interval = 500;
};
