#include "selectionwatcher.h"

#include <algorithm>

#include <QGuiApplication>
#include <QRegularExpression>

SelectionWatcher::SelectionWatcher(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(m_interval);
    connect(&m_timer, &QTimer::timeout, this, &SelectionWatcher::readSelection);
    m_timer.start();

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        connect(clipboard, &QClipboard::selectionChanged, this, &SelectionWatcher::readSelection);
    }

    readSelection();
}

SelectionWatcher::~SelectionWatcher()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
}

QString SelectionWatcher::text() const
{
    return m_text;
}

int SelectionWatcher::interval() const
{
    return m_interval;
}

void SelectionWatcher::setInterval(int interval)
{
    const int boundedInterval = std::max(200, interval);
    if (m_interval == boundedInterval) {
        return;
    }

    m_interval = boundedInterval;
    m_timer.setInterval(m_interval);
    Q_EMIT intervalChanged();
}

void SelectionWatcher::readSelection()
{
    if (useWaylandBackend()) {
        readWaylandSelection();
    } else {
        readClipboardSelection();
    }
}

void SelectionWatcher::readClipboardSelection()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard || !clipboard->supportsSelection()) {
        setText(QString());
        return;
    }

    setText(normalizeSelection(clipboard->text(QClipboard::Selection)));
}

void SelectionWatcher::readWaylandSelection()
{
    if (m_process) {
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(QStringLiteral("wl-paste"));
    m_process->setArguments({
        QStringLiteral("--primary"),
        QStringLiteral("--no-newline"),
        QStringLiteral("--type"),
        QStringLiteral("text/plain"),
    });

    connect(m_process, &QProcess::finished, this, &SelectionWatcher::finishWaylandRead);
    connect(m_process, &QProcess::errorOccurred, this, [this]() {
        QProcess *process = m_process;
        m_process = nullptr;
        setText(QString());
        if (process) {
            process->deleteLater();
        }
    });

    QTimer::singleShot(800, this, &SelectionWatcher::abortWaylandRead);
    m_process->start(QIODevice::ReadOnly);
}

void SelectionWatcher::finishWaylandRead(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = m_process;
    m_process = nullptr;

    if (!process) {
        return;
    }

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        setText(normalizeSelection(QString::fromUtf8(process->readAllStandardOutput())));
    } else {
        setText(QString());
    }

    process->deleteLater();
}

void SelectionWatcher::abortWaylandRead()
{
    if (!m_process) {
        return;
    }

    m_process->kill();
}

void SelectionWatcher::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    Q_EMIT textChanged();
}

QString SelectionWatcher::normalizeSelection(QString text) const
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return text.replace(whitespace, QStringLiteral(" "));
}

bool SelectionWatcher::useWaylandBackend() const
{
    return QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}
