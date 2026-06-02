#include "selectionwatcher.h"

#include "waylandprimaryselection.h"

#include <algorithm>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QStyleHints>

SelectionWatcher::SelectionWatcher(QObject *parent)
    : QObject(parent)
{
  m_clickClearTimer.setSingleShot(true);
  connect(&m_clickClearTimer, &QTimer::timeout, this, [this]() {
    if (m_kWinMouseClearingSuspended) {
      return;
    }
    if (QDateTime::currentMSecsSinceEpoch() <= m_ignoreKWinMouseClicksUntil) {
      return;
    }
    if (m_pendingClickClearSelectionSerial != m_selectionChangeSerial) {
      return;
    }

    suppressCurrentSelection();
  });

  m_emptySelectionTimer.setSingleShot(true);
  connect(&m_emptySelectionTimer, &QTimer::timeout, this, [this]() {
    if (m_pendingEmptySelectionSerial != m_selectionChangeSerial) {
      return;
    }

    applySelectionText(QString());
  });

  QDBusConnection::sessionBus().connect(
      QString(), QStringLiteral("/com/github/LXYan2333/SwanDict/KWinHelper"),
      QStringLiteral("com.github.LXYan2333.SwanDict.KWinHelper"),
      QStringLiteral("mouseClicked"), this,
      SLOT(handleKWinMouseClicked(qlonglong, int)));

  if (useWaylandBackend()) {
    setupWaylandPrimarySelection();
  } else if (QClipboard *clipboard = QGuiApplication::clipboard()) {
    connect(clipboard, &QClipboard::selectionChanged, this,
            &SelectionWatcher::readSelection);

    m_timer.setInterval(m_interval);
    connect(&m_timer, &QTimer::timeout, this, &SelectionWatcher::readSelection);
    m_timer.start();
  }

    readSelection();
}

SelectionWatcher::~SelectionWatcher() {}

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

bool SelectionWatcher::kWinMouseClearingSuspended() const
{
    return m_kWinMouseClearingSuspended;
}

void SelectionWatcher::setKWinMouseClearingSuspended(bool suspended)
{
    if (m_kWinMouseClearingSuspended == suspended) {
        return;
    }

    m_kWinMouseClearingSuspended = suspended;
    Q_EMIT kWinMouseClearingSuspendedChanged();
}

void SelectionWatcher::ignoreNextKWinMouseClick(int msec)
{
    m_ignoreKWinMouseClicksUntil = std::max(m_ignoreKWinMouseClicksUntil, QDateTime::currentMSecsSinceEpoch() + std::max(0, msec));
}

void SelectionWatcher::clearCurrentSelection()
{
    suppressCurrentSelection();
}

bool SelectionWatcher::startKWinMouseHelper()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface *interface = bus.interface();
    if (!interface || !interface->isServiceRegistered(QStringLiteral("org.kde.KWin"))) {
        return false;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/Effects"),
                                                          QStringLiteral("org.kde.kwin.Effects"),
                                                          QStringLiteral("loadEffect"));
    message << QStringLiteral("swandictmousehelper");
    bus.asyncCall(message);
    return true;
}

void SelectionWatcher::readSelection()
{
  if (!useWaylandBackend()) {
    readClipboardSelection();
  }
}

void SelectionWatcher::readClipboardSelection()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard || !clipboard->supportsSelection()) {
        applySelectionText(QString());
        return;
    }

    applySelectionText(clipboard->text(QClipboard::Selection));
}

void SelectionWatcher::handleKWinMouseClicked(qlonglong timestamp, int buttons)
{
    Q_UNUSED(timestamp)
    Q_UNUSED(buttons)

    m_pendingClickClearSelectionSerial = m_selectionChangeSerial;
    m_clickClearTimer.start(clickClearDelay());
}

void SelectionWatcher::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    Q_EMIT textChanged();
}

void SelectionWatcher::applySelectionText(const QString &text)
{
    const QString normalizedText = normalizeSelection(text);
    if (normalizedText.isEmpty()) {
        m_suppressedText.clear();
        setText(QString());
        return;
    }

    if (!m_suppressedText.isEmpty() && normalizedText == m_suppressedText) {
        setText(QString());
        return;
    }

    m_suppressedText.clear();
    setText(normalizedText);
}

void SelectionWatcher::suppressCurrentSelection()
{
    if (m_text.isEmpty()) {
        return;
    }

    m_suppressedText = m_text;
    setText(QString());
}

int SelectionWatcher::clickClearDelay() const
{
    const QStyleHints *styleHints = QGuiApplication::styleHints();
    if (!styleHints) {
        return 350;
    }

    return std::clamp(styleHints->mouseDoubleClickInterval(), 1, 1000);
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

void SelectionWatcher::setupWaylandPrimarySelection() {
  if (m_waylandPrimarySelection) {
    return;
  }

  m_waylandPrimarySelection = new WaylandPrimarySelection(this);
  if (!m_waylandPrimarySelection->isValid()) {
    m_waylandPrimarySelection->deleteLater();
    m_waylandPrimarySelection = nullptr;
    setText(QString());
    return;
  }

  connect(m_waylandPrimarySelection, &WaylandPrimarySelection::selectionChanged,
          this, [this](const QString &text) {
            ++m_selectionChangeSerial;
            if (normalizeSelection(text).isEmpty()) {
              m_pendingEmptySelectionSerial = m_selectionChangeSerial;
              m_emptySelectionTimer.start(std::min(clickClearDelay(), 500));
              return;
            }

            m_emptySelectionTimer.stop();
            m_suppressedText.clear();
            applySelectionText(text);
          });
}
