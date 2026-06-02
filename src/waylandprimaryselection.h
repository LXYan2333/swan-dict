#pragma once

#include <QObject>

class QSocketNotifier;
class QTimer;
struct wl_display;
struct wl_registry;
struct wl_seat;
struct ext_data_control_manager_v1;
struct ext_data_control_device_v1;
struct ext_data_control_offer_v1;
struct zwlr_data_control_manager_v1;
struct zwlr_data_control_device_v1;
struct zwlr_data_control_offer_v1;
struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_v1;
struct zwp_primary_selection_offer_v1;

class WaylandPrimarySelection : public QObject
{
    Q_OBJECT

public:
    explicit WaylandPrimarySelection(QObject *parent = nullptr);
    ~WaylandPrimarySelection() override;

    bool isValid() const;

    static void handleRegistryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void handleRegistryGlobalRemove(void *data, wl_registry *registry, uint32_t name);
    static void handleExtDeviceDataOffer(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer);
    static void handleExtDeviceSelection(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer);
    static void handleExtDeviceFinished(void *data, ext_data_control_device_v1 *device);
    static void handleExtDevicePrimarySelection(void *data, ext_data_control_device_v1 *device, ext_data_control_offer_v1 *offer);
    static void handleExtOfferMimeType(void *data, ext_data_control_offer_v1 *offer, const char *mimeType);
    static void handleWlrDeviceDataOffer(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer);
    static void handleWlrDeviceSelection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer);
    static void handleWlrDeviceFinished(void *data, zwlr_data_control_device_v1 *device);
    static void handleWlrDevicePrimarySelection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *offer);
    static void handleWlrOfferMimeType(void *data, zwlr_data_control_offer_v1 *offer, const char *mimeType);
    static void handleDeviceDataOffer(void *data, zwp_primary_selection_device_v1 *device, zwp_primary_selection_offer_v1 *offer);
    static void handleDeviceSelection(void *data, zwp_primary_selection_device_v1 *device, zwp_primary_selection_offer_v1 *offer);
    static void handleOfferMimeType(void *data, zwp_primary_selection_offer_v1 *offer, const char *mimeType);

Q_SIGNALS:
    void selectionChanged(const QString &text);

private:
    struct Offer;

    void dispatchDisplayEvents();
    void setupDevice();
    void setCurrentOffer(Offer *offer);
    void readCurrentOffer();
    QString selectedMimeType(const Offer &offer) const;
    void finishRead();
    void closeReadPipe();
    void cleanupWayland();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    ext_data_control_manager_v1 *m_extDeviceManager = nullptr;
    ext_data_control_device_v1 *m_extDevice = nullptr;
    zwlr_data_control_manager_v1 *m_wlrDeviceManager = nullptr;
    zwlr_data_control_device_v1 *m_wlrDevice = nullptr;
    zwp_primary_selection_device_manager_v1 *m_deviceManager = nullptr;
    zwp_primary_selection_device_v1 *m_device = nullptr;
    QSocketNotifier *m_displayNotifier = nullptr;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_readTimeout = nullptr;
    Offer *m_currentOffer = nullptr;
    int m_readFd = -1;
    QByteArray m_readBuffer;
    bool m_valid = false;
};
