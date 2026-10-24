#include "./Tasker.hpp"

#include <stdexcept>

#include "../Log.h"
#include "./Controller.hpp"
#include "./Resource.hpp"

namespace Maa {
    Tasker::Tasker(const Controller &controller, const Resource &resource) {
        handle_ = MaaTaskerCreate();
        if (handle_ == nullptr) {
            throw std::runtime_error("Failed to create MaaTasker");
        }
        if (!MaaTaskerBindController(handle_, controller.handle()) || !MaaTaskerBindResource(handle_, resource.handle())) {
            throw std::runtime_error("Failed to bind controller and resource");
        }
        if (!MaaTaskerInited(handle_)) {
            throw std::runtime_error("MaaTasker is not inited");
        }
    }

    Tasker::~Tasker() {
        if (handle_ != nullptr) {
            MaaTaskerDestroy(handle_);
        }
    }

    MaaTaskId Tasker::post_task(const char *entry) const {
        LOG_INFO("Tasker", "Posting task %s", entry);
        return MaaTaskerPostTask(handle_, entry, "{}");
    }
}
