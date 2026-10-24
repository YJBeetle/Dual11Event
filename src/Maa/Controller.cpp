#include "./Controller.hpp"

#include <stdexcept>
#include <utility>

#include <MaaToolkit/MaaToolkitAPI.h>

#include "../Log.h"

namespace Maa {
    std::string Controller::discover_udid() {
        auto *list = MaaToolkitIOSDeviceListCreate();
        if (list == nullptr) {
            throw std::runtime_error("Failed to create iOS device list");
        }
        struct Owned {
            MaaToolkitIOSDeviceList *value;
            ~Owned() { MaaToolkitIOSDeviceListDestroy(value); }
        } owned{list};

        if (!MaaToolkitIOSDeviceFindWithTimeout(5000, list)) {
            std::string reason = MaaToolkitIOSDeviceListGetWarnings(list);
            throw std::runtime_error(
                "No usable iOS device via XDeviceBridge server: " + (reason.empty() ? std::string("unknown") : reason));
        }
        auto size = MaaToolkitIOSDeviceListSize(list);
        if (size == 0) {
            throw std::runtime_error("No iOS device found. Plug the phone in and trust this computer.");
        }
        if (size > 1) {
            throw std::runtime_error("Multiple iOS devices found, pass one with -s <UDID>");
        }
        auto *device = MaaToolkitIOSDeviceListAt(list, 0);
        if (device == nullptr) {
            throw std::runtime_error("iOS device list is inconsistent");
        }
        return MaaToolkitIOSDeviceGetUdid(device);
    }

    Controller::Controller(const std::string &udid, int xdb_port) {
        std::string serial = udid.empty() ? discover_udid() : udid;
        std::string config = xdb_port > 0 ? R"({"server_port":)" + std::to_string(xdb_port) + "}" : "{}";

        LOG_INFO("Controller", "Creating iOS controller for %s", serial.c_str());
        handle_ = MaaIOSControllerCreate(
            serial.c_str(),
            MaaIOSScreencapMethod_ScreenshotService,
            MaaIOSConnectionMethod_Auto,
            "",
            config.c_str());
        if (handle_ == nullptr) {
            LOG_ERROR("Controller", "Failed to create iOS controller");
            throw std::runtime_error("Failed to create iOS controller");
        }
    }

    Controller::~Controller() {
        if (handle_ != nullptr) {
            MaaControllerDestroy(handle_);
        }
    }

    bool Controller::connect() const {
        auto id = MaaControllerPostConnection(handle_);
        if (id == 0) {
            LOG_ERROR("Controller", "Failed to post connection");
            return false;
        }
        if (MaaControllerWait(handle_, id) != MaaStatus_Succeeded) {
            LOG_ERROR("Controller", "Connection failed");
            return false;
        }
        return MaaControllerConnected(handle_) != 0;
    }

    std::string Controller::uuid() const {
        auto *buffer = MaaStringBufferCreate();
        if (buffer == nullptr) {
            return {};
        }
        struct Owned {
            MaaStringBuffer *value;
            ~Owned() { MaaStringBufferDestroy(value); }
        } owned{buffer};

        if (!MaaControllerGetUuid(handle_, buffer)) {
            return {};
        }
        return MaaStringBufferGet(buffer);
    }
}
