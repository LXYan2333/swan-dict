#include <effect/effect.h>
#include <input.h>

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>

namespace KWin
{

class SwanDictMouseHelperEffect : public Effect
{
    Q_OBJECT

public:
    explicit SwanDictMouseHelperEffect(QObject *parent = nullptr)
        : Effect(parent)
    {
        connect(input(),
                &InputRedirection::pointerButtonStateChanged,
                this,
                &SwanDictMouseHelperEffect::handlePointerButtonStateChanged);
    }

    ~SwanDictMouseHelperEffect() override = default;

    bool isActive() const override
    {
        return true;
    }

private Q_SLOTS:
    void handlePointerButtonStateChanged(uint32_t button, PointerButtonState state)
    {
        if (state != PointerButtonState::Pressed) {
            return;
        }

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/com/github/LXYan2333/SwanDict/KWinHelper"),
            QStringLiteral("com.github.LXYan2333.SwanDict.KWinHelper"),
            QStringLiteral("mouseClicked"));
        message << QDateTime::currentMSecsSinceEpoch();
        message << static_cast<int>(button);
        QDBusConnection::sessionBus().send(message);
    }
};

}

KWIN_EFFECT_FACTORY(KWin::SwanDictMouseHelperEffect, "metadata.json")

#include "swandictmousehelper.moc"
