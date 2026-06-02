#include "waylandprimaryselection.h"

#include <QSocketNotifier>
#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#include <wayland-client.h>

extern "C" {
#include "ext-data-control-v1-client-protocol.h"
#include "primary-selection-unstable-v1-client-protocol.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"
}

struct WaylandPrimarySelection::Offer {
    enum class Protocol {
        ExtDataControl,
        WlrDataControl,
        PrimarySelection,
    };

    Protocol protocol = Protocol::PrimarySelection;
    ext_data_control_offer_v1 *extProxy = nullptr;
    zwlr_data_control_offer_v1 *wlrProxy = nullptr;
    zwp_primary_selection_offer_v1 *proxy = nullptr;
    QStringList mimeTypes;
};

static const wl_registry_listener registryListener = {
    WaylandPrimarySelection::handleRegistryGlobal,
    WaylandPrimarySelection::handleRegistryGlobalRemove,
};

static const ext_data_control_device_v1_listener extDeviceListener = {
    WaylandPrimarySelection::handleExtDeviceDataOffer,
    WaylandPrimarySelection::handleExtDeviceSelection,
    WaylandPrimarySelection::handleExtDeviceFinished,
    WaylandPrimarySelection::handleExtDevicePrimarySelection,
};

static const ext_data_control_offer_v1_listener extOfferListener = {
    WaylandPrimarySelection::handleExtOfferMimeType,
};

static const zwlr_data_control_device_v1_listener wlrDeviceListener = {
    WaylandPrimarySelection::handleWlrDeviceDataOffer,
    WaylandPrimarySelection::handleWlrDeviceSelection,
    WaylandPrimarySelection::handleWlrDeviceFinished,
    WaylandPrimarySelection::handleWlrDevicePrimarySelection,
};

static const zwlr_data_control_offer_v1_listener wlrOfferListener = {
    WaylandPrimarySelection::handleWlrOfferMimeType,
};

static const zwp_primary_selection_device_v1_listener deviceListener = {
    WaylandPrimarySelection::handleDeviceDataOffer,
    WaylandPrimarySelection::handleDeviceSelection,
};

static const zwp_primary_selection_offer_v1_listener offerListener = {
    WaylandPrimarySelection::handleOfferMimeType,
};

WaylandPrimarySelection::WaylandPrimarySelection(QObject *parent)
    : QObject(parent)
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    m_readTimeout = new QTimer(this);
    m_readTimeout->setSingleShot(true);
    m_readTimeout->setInterval(800);
    connect(m_readTimeout, &QTimer::timeout, this, [this]() {
        closeReadPipe();
        Q_EMIT selectionChanged(QString());
    });

    setupDevice();
    wl_display_roundtrip(m_display);

    if (!m_extDevice && !m_wlrDevice && !m_device) {
        cleanupWayland();
        return;
    }

    m_displayNotifier = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Read, this);
    connect(m_displayNotifier, &QSocketNotifier::activated, this, &WaylandPrimarySelection::dispatchDisplayEvents);

    m_valid = true;
}

WaylandPrimarySelection::~WaylandPrimarySelection()
{
    cleanupWayland();
}

bool WaylandPrimarySelection::isValid() const
{
    return m_valid;
}

void WaylandPrimarySelection::handleRegistryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandPrimarySelection *>(data);
    if (std::strcmp(interface, wl_seat_interface.name) == 0 && !self->m_seat) {
        self->m_seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min<uint32_t>(version, 1)));
        return;
    }

    if (std::strcmp(interface, ext_data_control_manager_v1_interface.name) == 0 && !self->m_extDeviceManager) {
        self->m_extDeviceManager = static_cast<ext_data_control_manager_v1 *>(
            wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, 1));
        return;
    }

    if (std::strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0 && !self->m_wlrDeviceManager && version >= 2) {
        self->m_wlrDeviceManager = static_cast<zwlr_data_control_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2));
        return;
    }

    if (std::strcmp(interface, zwp_primary_selection_device_manager_v1_interface.name) == 0 && !self->m_deviceManager) {
        self->m_deviceManager = static_cast<zwp_primary_selection_device_manager_v1 *>(
            wl_registry_bind(registry, name, &zwp_primary_selection_device_manager_v1_interface, 1));
    }
}

void WaylandPrimarySelection::handleRegistryGlobalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

void WaylandPrimarySelection::handleExtDeviceDataOffer(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer)
{
    Q_UNUSED(data)
    Q_UNUSED(device)

    auto *wrappedOffer = new Offer;
    wrappedOffer->protocol = Offer::Protocol::ExtDataControl;
    wrappedOffer->extProxy = offer;
    ext_data_control_offer_v1_add_listener(offer, &extOfferListener, wrappedOffer);
}

void WaylandPrimarySelection::handleExtDeviceSelection(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer)
{
    Q_UNUSED(data)
    Q_UNUSED(device)
    Q_UNUSED(offer)
}

void WaylandPrimarySelection::handleExtDeviceFinished(void *data, ext_data_control_device_v1 *device)
{
    Q_UNUSED(device)

    auto *self = static_cast<WaylandPrimarySelection *>(data);
    self->setCurrentOffer(nullptr);
    Q_EMIT self->selectionChanged(QString());
}

void WaylandPrimarySelection::handleExtDevicePrimarySelection(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer)
{
    Q_UNUSED(device)

    auto *self = static_cast<WaylandPrimarySelection *>(data);
    if (!offer) {
        self->setCurrentOffer(nullptr);
        Q_EMIT self->selectionChanged(QString());
        return;
    }

    self->setCurrentOffer(static_cast<Offer *>(ext_data_control_offer_v1_get_user_data(offer)));
    self->readCurrentOffer();
}

void WaylandPrimarySelection::handleExtOfferMimeType(void *data, ext_data_control_offer_v1 *offer, const char *mimeType)
{
    Q_UNUSED(offer)

    auto *wrappedOffer = static_cast<Offer *>(data);
    if (!wrappedOffer || !mimeType) {
        return;
    }

    wrappedOffer->mimeTypes.push_back(QString::fromUtf8(mimeType));
}

void WaylandPrimarySelection::handleWlrDeviceDataOffer(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer)
{
    Q_UNUSED(data)
    Q_UNUSED(device)

    auto *wrappedOffer = new Offer;
    wrappedOffer->protocol = Offer::Protocol::WlrDataControl;
    wrappedOffer->wlrProxy = offer;
    zwlr_data_control_offer_v1_add_listener(offer, &wlrOfferListener, wrappedOffer);
}

void WaylandPrimarySelection::handleWlrDeviceSelection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer)
{
    Q_UNUSED(data)
    Q_UNUSED(device)
    Q_UNUSED(offer)
}

void WaylandPrimarySelection::handleWlrDeviceFinished(void *data, zwlr_data_control_device_v1 *device)
{
    Q_UNUSED(device)

    auto *self = static_cast<WaylandPrimarySelection *>(data);
    self->setCurrentOffer(nullptr);
    Q_EMIT self->selectionChanged(QString());
}

void WaylandPrimarySelection::handleWlrDevicePrimarySelection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer)
{
    Q_UNUSED(device)

    auto *self = static_cast<WaylandPrimarySelection *>(data);
    if (!offer) {
        self->setCurrentOffer(nullptr);
        Q_EMIT self->selectionChanged(QString());
        return;
    }

    self->setCurrentOffer(static_cast<Offer *>(zwlr_data_control_offer_v1_get_user_data(offer)));
    self->readCurrentOffer();
}

void WaylandPrimarySelection::handleWlrOfferMimeType(void *data, zwlr_data_control_offer_v1 *offer, const char *mimeType)
{
    Q_UNUSED(offer)

    auto *wrappedOffer = static_cast<Offer *>(data);
    if (!wrappedOffer || !mimeType) {
        return;
    }

    wrappedOffer->mimeTypes.push_back(QString::fromUtf8(mimeType));
}

void WaylandPrimarySelection::handleDeviceDataOffer(void *data, zwp_primary_selection_device_v1 *device, zwp_primary_selection_offer_v1 *offer)
{
    Q_UNUSED(data)
    Q_UNUSED(device)

    auto *wrappedOffer = new Offer;
    wrappedOffer->proxy = offer;
    zwp_primary_selection_offer_v1_add_listener(offer, &offerListener, wrappedOffer);
}

void WaylandPrimarySelection::handleDeviceSelection(void *data, zwp_primary_selection_device_v1 *device, zwp_primary_selection_offer_v1 *offer)
{
    Q_UNUSED(device)

    auto *self = static_cast<WaylandPrimarySelection *>(data);
    if (!offer) {
        self->setCurrentOffer(nullptr);
        Q_EMIT self->selectionChanged(QString());
        return;
    }

    self->setCurrentOffer(static_cast<Offer *>(zwp_primary_selection_offer_v1_get_user_data(offer)));
    self->readCurrentOffer();
}

void WaylandPrimarySelection::handleOfferMimeType(void *data, zwp_primary_selection_offer_v1 *offer, const char *mimeType)
{
    Q_UNUSED(offer)

    auto *wrappedOffer = static_cast<Offer *>(data);
    if (!wrappedOffer || !mimeType) {
        return;
    }

    wrappedOffer->mimeTypes.push_back(QString::fromUtf8(mimeType));
}

void WaylandPrimarySelection::dispatchDisplayEvents()
{
    if (!m_display) {
        return;
    }

    wl_display_dispatch(m_display);
    wl_display_flush(m_display);
}

void WaylandPrimarySelection::setupDevice()
{
    if (!m_seat) {
        return;
    }

    if (m_extDeviceManager && !m_extDevice) {
        m_extDevice = ext_data_control_manager_v1_get_data_device(m_extDeviceManager, m_seat);
        ext_data_control_device_v1_add_listener(m_extDevice, &extDeviceListener, this);
        return;
    }

    if (m_wlrDeviceManager && !m_wlrDevice) {
        m_wlrDevice = zwlr_data_control_manager_v1_get_data_device(m_wlrDeviceManager, m_seat);
        zwlr_data_control_device_v1_add_listener(m_wlrDevice, &wlrDeviceListener, this);
        return;
    }

    if (!m_deviceManager || m_device) {
        return;
    }

    m_device = zwp_primary_selection_device_manager_v1_get_device(m_deviceManager, m_seat);
    zwp_primary_selection_device_v1_add_listener(m_device, &deviceListener, this);
}

void WaylandPrimarySelection::setCurrentOffer(Offer *offer)
{
    if (m_currentOffer == offer) {
        return;
    }

    if (m_currentOffer) {
        if (m_currentOffer->protocol == Offer::Protocol::ExtDataControl) {
            ext_data_control_offer_v1_destroy(m_currentOffer->extProxy);
        } else if (m_currentOffer->protocol == Offer::Protocol::WlrDataControl) {
            zwlr_data_control_offer_v1_destroy(m_currentOffer->wlrProxy);
        } else {
            zwp_primary_selection_offer_v1_destroy(m_currentOffer->proxy);
        }
        delete m_currentOffer;
    }

    m_currentOffer = offer;
}

void WaylandPrimarySelection::readCurrentOffer()
{
    closeReadPipe();

    if (!m_currentOffer
        || (m_currentOffer->protocol == Offer::Protocol::ExtDataControl && !m_currentOffer->extProxy)
        || (m_currentOffer->protocol == Offer::Protocol::WlrDataControl && !m_currentOffer->wlrProxy)
        || (m_currentOffer->protocol == Offer::Protocol::PrimarySelection && !m_currentOffer->proxy)) {
        Q_EMIT selectionChanged(QString());
        return;
    }

    const QString mimeType = selectedMimeType(*m_currentOffer);
    if (mimeType.isEmpty()) {
        Q_EMIT selectionChanged(QString());
        return;
    }

    int pipeFds[2] = {-1, -1};
    if (pipe2(pipeFds, O_CLOEXEC | O_NONBLOCK) != 0) {
        Q_EMIT selectionChanged(QString());
        return;
    }

    if (m_currentOffer->protocol == Offer::Protocol::ExtDataControl) {
        ext_data_control_offer_v1_receive(m_currentOffer->extProxy, mimeType.toUtf8().constData(), pipeFds[1]);
    } else if (m_currentOffer->protocol == Offer::Protocol::WlrDataControl) {
        zwlr_data_control_offer_v1_receive(m_currentOffer->wlrProxy, mimeType.toUtf8().constData(), pipeFds[1]);
    } else {
        zwp_primary_selection_offer_v1_receive(m_currentOffer->proxy, mimeType.toUtf8().constData(), pipeFds[1]);
    }
    wl_display_flush(m_display);
    close(pipeFds[1]);

    m_readFd = pipeFds[0];
    m_readBuffer.clear();
    m_readNotifier = new QSocketNotifier(m_readFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, [this]() {
        char buffer[4096];
        while (true) {
            const ssize_t bytesRead = read(m_readFd, buffer, sizeof(buffer));
            if (bytesRead > 0) {
                m_readBuffer.append(buffer, bytesRead);
                continue;
            }
            if (bytesRead == 0) {
                finishRead();
                return;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return;
            }

            closeReadPipe();
            Q_EMIT selectionChanged(QString());
            return;
        }
    });
    m_readTimeout->start();
}

QString WaylandPrimarySelection::selectedMimeType(const Offer &offer) const
{
    if (offer.mimeTypes.contains(QStringLiteral("text/plain;charset=utf-8"))) {
        return QStringLiteral("text/plain;charset=utf-8");
    }
    if (offer.mimeTypes.contains(QStringLiteral("text/plain;charset=UTF-8"))) {
        return QStringLiteral("text/plain;charset=UTF-8");
    }
    if (offer.mimeTypes.contains(QStringLiteral("UTF8_STRING"))) {
        return QStringLiteral("UTF8_STRING");
    }
    if (offer.mimeTypes.contains(QStringLiteral("text/plain"))) {
        return QStringLiteral("text/plain");
    }

    for (const QString &mimeType : offer.mimeTypes) {
        if (mimeType.startsWith(QStringLiteral("text/"))) {
            return mimeType;
        }
    }

    return QString();
}

void WaylandPrimarySelection::finishRead()
{
    const QString text = QString::fromUtf8(m_readBuffer);
    closeReadPipe();
    Q_EMIT selectionChanged(text);
}

void WaylandPrimarySelection::closeReadPipe()
{
    if (m_readTimeout) {
        m_readTimeout->stop();
    }
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if (m_readFd >= 0) {
        close(m_readFd);
        m_readFd = -1;
    }
    m_readBuffer.clear();
}

void WaylandPrimarySelection::cleanupWayland()
{
    m_valid = false;
    closeReadPipe();

    if (m_displayNotifier) {
        m_displayNotifier->setEnabled(false);
        m_displayNotifier->deleteLater();
        m_displayNotifier = nullptr;
    }

    if (m_currentOffer) {
        if (m_currentOffer->protocol == Offer::Protocol::ExtDataControl) {
            ext_data_control_offer_v1_destroy(m_currentOffer->extProxy);
        } else if (m_currentOffer->protocol == Offer::Protocol::WlrDataControl) {
            zwlr_data_control_offer_v1_destroy(m_currentOffer->wlrProxy);
        } else {
            zwp_primary_selection_offer_v1_destroy(m_currentOffer->proxy);
        }
        delete m_currentOffer;
        m_currentOffer = nullptr;
    }
    if (m_extDevice) {
        ext_data_control_device_v1_destroy(m_extDevice);
        m_extDevice = nullptr;
    }
    if (m_wlrDevice) {
        zwlr_data_control_device_v1_destroy(m_wlrDevice);
        m_wlrDevice = nullptr;
    }
    if (m_device) {
        zwp_primary_selection_device_v1_destroy(m_device);
        m_device = nullptr;
    }
    if (m_extDeviceManager) {
        ext_data_control_manager_v1_destroy(m_extDeviceManager);
        m_extDeviceManager = nullptr;
    }
    if (m_wlrDeviceManager) {
        zwlr_data_control_manager_v1_destroy(m_wlrDeviceManager);
        m_wlrDeviceManager = nullptr;
    }
    if (m_deviceManager) {
        zwp_primary_selection_device_manager_v1_destroy(m_deviceManager);
        m_deviceManager = nullptr;
    }
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}
