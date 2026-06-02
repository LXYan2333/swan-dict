#pragma once

#include <QClipboard>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class WaylandPrimarySelection;

class SelectionWatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool kWinMouseClearingSuspended READ kWinMouseClearingSuspended WRITE setKWinMouseClearingSuspended NOTIFY kWinMouseClearingSuspendedChanged)
    QML_ELEMENT

public:
    explicit SelectionWatcher(QObject *parent = nullptr);
    ~SelectionWatcher() override;

    QString text() const;
    int interval() const;
    void setInterval(int interval);
    bool kWinMouseClearingSuspended() const;
    void setKWinMouseClearingSuspended(bool suspended);
    Q_INVOKABLE void ignoreNextKWinMouseClick(int msec = 500);
    Q_INVOKABLE void clearCurrentSelection();
    Q_INVOKABLE bool startKWinMouseHelper();

Q_SIGNALS:
    void textChanged();
    void intervalChanged();
    void kWinMouseClearingSuspendedChanged();

private Q_SLOTS:
    void readSelection();
    void handleKWinMouseClicked(qlonglong timestamp, int buttons);

private:
    void setText(const QString &text);
    void applySelectionText(const QString &text);
    void suppressCurrentSelection();
    int clickClearDelay() const;
    QString normalizeSelection(QString text) const;
    bool useWaylandBackend() const;
    void readClipboardSelection();
    void setupWaylandPrimarySelection();

    QString m_text;
    QString m_suppressedText;
    QTimer m_timer;
    QTimer m_clickClearTimer;
    QTimer m_emptySelectionTimer;
    WaylandPrimarySelection *m_waylandPrimarySelection = nullptr;
    int m_interval = 500;
    qint64 m_ignoreKWinMouseClicksUntil = 0;
    qint64 m_selectionChangeSerial = 0;
    qint64 m_pendingClickClearSelectionSerial = 0;
    qint64 m_pendingEmptySelectionSerial = 0;
    bool m_kWinMouseClearingSuspended = false;
};
