#include "./Controller.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <MaaToolkit/MaaToolkitAPI.h>

#include "../Log.h"

namespace Maa {
    namespace {
        std::string describe(const MaaToolkitIOSDevice *device) {
            return std::string(MaaToolkitIOSDeviceGetUdid(device) ? MaaToolkitIOSDeviceGetUdid(device) : "?") + " ["
                   + (MaaToolkitIOSDeviceGetName(device) ? MaaToolkitIOSDeviceGetName(device) : "?") + ", "
                   + (MaaToolkitIOSDeviceGetState(device) ? MaaToolkitIOSDeviceGetState(device) : "?") + "]";
        }
    }

    std::string Controller::discover_udid(int xdb_port) {
        auto *list = MaaToolkitIOSDeviceListCreate();
        if (list == nullptr) {
            throw std::runtime_error("Failed to create iOS device list");
        }
        struct Owned {
            MaaToolkitIOSDeviceList *value;
            ~Owned() { MaaToolkitIOSDeviceListDestroy(value); }
        } owned{list};

        // 端口要一起给发现用：只传给控制器的话，--xdb-port 指到别的 server 上，
        // 这一步却还在默认端口那台机器上找设备。
        std::string options = R"({"timeout_ms":5000)";
        if (xdb_port > 0) {
            options += R"(,"server_port":)" + std::to_string(xdb_port);
        }
        options += "}";

        if (!MaaToolkitIOSDeviceFindWithOptions(MaaIOSConnectionMethod_Auto, options.c_str(), nullptr, nullptr, list)) {
            std::string reason = MaaToolkitIOSDeviceListGetWarnings(list);
            throw std::runtime_error(
                "No usable iOS device via XDeviceBridge server: " + (reason.empty() ? std::string("unknown") : reason));
        }
        auto size = MaaToolkitIOSDeviceListSize(list);
        if (size == 0) {
            throw std::runtime_error("No iOS device found. Plug the phone in and trust this computer.");
        }

        // 新版发现返回的是 USB/Wi-Fi 快照，记录里有 discovered / paired / connected 三种状态，
        // 「有条目」不等于「连得上」。所以多台的时候先按 connected 收窄，
        // 免得一台插着线、另一台只留着配对记录就被判成歧义。
        std::vector<const MaaToolkitIOSDevice *> candidates;
        for (MaaSize i = 0; i < size; ++i) {
            auto *device = MaaToolkitIOSDeviceListAt(list, i);
            if (device == nullptr) {
                throw std::runtime_error("iOS device list is inconsistent");
            }
            candidates.push_back(device);
        }
        if (candidates.size() > 1) {
            std::vector<const MaaToolkitIOSDevice *> online;
            for (auto *device : candidates) {
                if (std::string(MaaToolkitIOSDeviceGetState(device)) == "connected") {
                    online.push_back(device);
                }
            }
            if (online.size() == 1) {
                candidates = online;
            }
        }
        if (candidates.size() > 1) {
            std::string seen;
            for (auto *device : candidates) {
                seen += (seen.empty() ? " " : ", ") + describe(device);
            }
            throw std::runtime_error("Multiple iOS devices found, pass one with -s <UDID>" + seen);
        }
        return MaaToolkitIOSDeviceGetUdid(candidates.front());
    }

    Controller::Controller(const std::string &udid, int xdb_port) {
        std::string serial = udid.empty() ? discover_udid(xdb_port) : udid;
        std::string config = xdb_port > 0 ? R"({"server_port":)" + std::to_string(xdb_port) + "}" : "{}";

        LOG_INFO("Controller", "Creating iOS controller for %s", serial.c_str());
        handle_ = MaaIOSControllerCreate(
            serial.c_str(),
            MaaIOSScreencapMethod_ScreenshotService,
            MaaIOSConnectionMethod_Auto,
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
