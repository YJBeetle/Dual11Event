#include "./Resource.hpp"

#include <stdexcept>

#include "../Log.h"

namespace Maa {
    Resource::Resource() {
        handle_ = MaaResourceCreate();
        if (handle_ == nullptr) {
            throw std::runtime_error("Failed to create MaaResource");
        }
    }

    Resource::~Resource() {
        if (handle_ != nullptr) {
            MaaResourceDestroy(handle_);
        }
    }

    void Resource::load(const std::string &path) const {
        LOG_INFO("Resource", "Loading %s", path.c_str());
        auto id = MaaResourcePostBundle(handle_, path.c_str());
        if (id == 0 || MaaResourceWait(handle_, id) != MaaStatus_Succeeded) {
            throw std::runtime_error("Failed to load resource: " + path);
        }
        if (!MaaResourceLoaded(handle_)) {
            throw std::runtime_error("Resource is not loaded: " + path);
        }
    }
}
