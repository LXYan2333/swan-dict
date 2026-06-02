#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"
#include "primary-selection-unstable-v1-client-protocol.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"

#include <csignal>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool g_running = true;

struct OfferInfo {
    std::vector<std::string> mimeTypes;
};

struct State {
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_seat *seat = nullptr;
    zwp_primary_selection_device_manager_v1 *primaryManager = nullptr;
    zwp_primary_selection_device_v1 *primaryDevice = nullptr;
    ext_data_control_manager_v1 *dataControlManager = nullptr;
    ext_data_control_device_v1 *dataControlDevice = nullptr;
    zwlr_data_control_manager_v1 *wlrDataControlManager = nullptr;
    zwlr_data_control_device_v1 *wlrDataControlDevice = nullptr;
};

std::string now()
{
    std::time_t t = std::time(nullptr);
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&t));
    return buffer;
}

void printOffer(const char *protocol, const OfferInfo *offer)
{
    std::cout << now() << " " << protocol << " primary_selection: ";
    if (!offer) {
        std::cout << "NULL/CLEARED\n";
        std::cout.flush();
        return;
    }

    if (offer->mimeTypes.empty()) {
        std::cout << "offer with no MIME types yet\n";
    } else {
        std::cout << "offer";
        for (const std::string &mimeType : offer->mimeTypes) {
            std::cout << " [" << mimeType << "]";
        }
        std::cout << '\n';
    }
    std::cout.flush();
}

void handleSignal(int)
{
    g_running = false;
}

void primaryOffer(void *data, zwp_primary_selection_offer_v1 *, const char *mimeType)
{
    auto *offer = static_cast<OfferInfo *>(data);
    offer->mimeTypes.emplace_back(mimeType ? mimeType : "");
}

const zwp_primary_selection_offer_v1_listener primaryOfferListener = {
    primaryOffer,
};

void primaryDataOffer(void *, zwp_primary_selection_device_v1 *, zwp_primary_selection_offer_v1 *offer)
{
    auto *info = new OfferInfo;
    zwp_primary_selection_offer_v1_set_user_data(offer, info);
    zwp_primary_selection_offer_v1_add_listener(offer, &primaryOfferListener, info);
}

void primarySelection(void *, zwp_primary_selection_device_v1 *, zwp_primary_selection_offer_v1 *offer)
{
    auto *info = offer ? static_cast<OfferInfo *>(zwp_primary_selection_offer_v1_get_user_data(offer)) : nullptr;
    printOffer("zwp_primary_selection", info);
}

const zwp_primary_selection_device_v1_listener primaryDeviceListener = {
    primaryDataOffer,
    primarySelection,
};

void dataControlOffer(void *data, ext_data_control_offer_v1 *, const char *mimeType)
{
    auto *offer = static_cast<OfferInfo *>(data);
    offer->mimeTypes.emplace_back(mimeType ? mimeType : "");
}

const ext_data_control_offer_v1_listener dataControlOfferListener = {
    dataControlOffer,
};

void dataControlDataOffer(void *, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer)
{
    auto *info = new OfferInfo;
    ext_data_control_offer_v1_set_user_data(offer, info);
    ext_data_control_offer_v1_add_listener(offer, &dataControlOfferListener, info);
}

void dataControlSelection(void *, ext_data_control_device_v1 *, ext_data_control_offer_v1 *)
{
}

void dataControlFinished(void *, ext_data_control_device_v1 *)
{
    std::cout << now() << " ext_data_control: finished\n";
    std::cout.flush();
    g_running = false;
}

void dataControlPrimarySelection(void *, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer)
{
    auto *info = offer ? static_cast<OfferInfo *>(ext_data_control_offer_v1_get_user_data(offer)) : nullptr;
    printOffer("ext_data_control", info);
}

const ext_data_control_device_v1_listener dataControlDeviceListener = {
    dataControlDataOffer,
    dataControlSelection,
    dataControlFinished,
    dataControlPrimarySelection,
};

void wlrDataControlOffer(void *data, zwlr_data_control_offer_v1 *, const char *mimeType)
{
    auto *offer = static_cast<OfferInfo *>(data);
    offer->mimeTypes.emplace_back(mimeType ? mimeType : "");
}

const zwlr_data_control_offer_v1_listener wlrDataControlOfferListener = {
    wlrDataControlOffer,
};

void wlrDataControlDataOffer(void *, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *offer)
{
    auto *info = new OfferInfo;
    zwlr_data_control_offer_v1_set_user_data(offer, info);
    zwlr_data_control_offer_v1_add_listener(offer, &wlrDataControlOfferListener, info);
}

void wlrDataControlSelection(void *, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *)
{
}

void wlrDataControlFinished(void *, zwlr_data_control_device_v1 *)
{
    std::cout << now() << " zwlr_data_control: finished\n";
    std::cout.flush();
    g_running = false;
}

void wlrDataControlPrimarySelection(void *, zwlr_data_control_device_v1 *, zwlr_data_control_offer_v1 *offer)
{
    auto *info = offer ? static_cast<OfferInfo *>(zwlr_data_control_offer_v1_get_user_data(offer)) : nullptr;
    printOffer("zwlr_data_control", info);
}

const zwlr_data_control_device_v1_listener wlrDataControlDeviceListener = {
    wlrDataControlDataOffer,
    wlrDataControlSelection,
    wlrDataControlFinished,
    wlrDataControlPrimarySelection,
};

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto *state = static_cast<State *>(data);
    const std::string iface = interface ? interface : "";

    if (iface == wl_seat_interface.name) {
        state->seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
        std::cout << now() << " bound wl_seat\n";
    } else if (iface == zwp_primary_selection_device_manager_v1_interface.name) {
        state->primaryManager = static_cast<zwp_primary_selection_device_manager_v1 *>(
            wl_registry_bind(registry, name, &zwp_primary_selection_device_manager_v1_interface, std::min(version, 1u)));
        std::cout << now() << " bound zwp_primary_selection_device_manager_v1\n";
    } else if (iface == ext_data_control_manager_v1_interface.name) {
        state->dataControlManager = static_cast<ext_data_control_manager_v1 *>(
            wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, std::min(version, 1u)));
        std::cout << now() << " bound ext_data_control_manager_v1\n";
    } else if (iface == zwlr_data_control_manager_v1_interface.name) {
        state->wlrDataControlManager = static_cast<zwlr_data_control_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, std::min(version, 2u)));
        std::cout << now() << " bound zwlr_data_control_manager_v1\n";
    }
}

void registryGlobalRemove(void *, wl_registry *, uint32_t)
{
}

const wl_registry_listener registryListener = {
    registryGlobal,
    registryGlobalRemove,
};

} // namespace

int main()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    State state;
    state.display = wl_display_connect(nullptr);
    if (!state.display) {
        std::cerr << "Failed to connect to Wayland display.\n";
        return 1;
    }

    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registryListener, &state);
    wl_display_roundtrip(state.display);

    if (!state.seat) {
        std::cerr << "No wl_seat advertised by compositor.\n";
        return 1;
    }

    if (state.primaryManager) {
        state.primaryDevice = zwp_primary_selection_device_manager_v1_get_device(state.primaryManager, state.seat);
        zwp_primary_selection_device_v1_add_listener(state.primaryDevice, &primaryDeviceListener, &state);
    } else {
        std::cout << now() << " zwp_primary_selection_device_manager_v1 not advertised\n";
    }

    if (state.dataControlManager) {
        state.dataControlDevice = ext_data_control_manager_v1_get_data_device(state.dataControlManager, state.seat);
        ext_data_control_device_v1_add_listener(state.dataControlDevice, &dataControlDeviceListener, &state);
    } else {
        std::cout << now() << " ext_data_control_manager_v1 not advertised\n";
    }

    if (state.wlrDataControlManager) {
        state.wlrDataControlDevice = zwlr_data_control_manager_v1_get_data_device(state.wlrDataControlManager, state.seat);
        zwlr_data_control_device_v1_add_listener(state.wlrDataControlDevice, &wlrDataControlDeviceListener, &state);
    } else {
        std::cout << now() << " zwlr_data_control_manager_v1 not advertised\n";
    }

    std::cout << now() << " watching primary selection; press Ctrl+C to stop\n";
    std::cout.flush();

    wl_display_roundtrip(state.display);
    while (g_running && wl_display_dispatch(state.display) != -1) {
    }

    if (state.dataControlDevice) {
        ext_data_control_device_v1_destroy(state.dataControlDevice);
    }
    if (state.dataControlManager) {
        ext_data_control_manager_v1_destroy(state.dataControlManager);
    }
    if (state.wlrDataControlDevice) {
        zwlr_data_control_device_v1_destroy(state.wlrDataControlDevice);
    }
    if (state.wlrDataControlManager) {
        zwlr_data_control_manager_v1_destroy(state.wlrDataControlManager);
    }
    if (state.primaryDevice) {
        zwp_primary_selection_device_v1_destroy(state.primaryDevice);
    }
    if (state.primaryManager) {
        zwp_primary_selection_device_manager_v1_destroy(state.primaryManager);
    }
    if (state.seat) {
        wl_seat_destroy(state.seat);
    }
    if (state.registry) {
        wl_registry_destroy(state.registry);
    }
    wl_display_disconnect(state.display);
    return 0;
}
