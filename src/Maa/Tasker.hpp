#pragma once

#include <MaaFramework/MaaAPI.h>

namespace Maa {
    class Controller;
    class Resource;

    class Tasker {
    private:
        MaaTasker *handle_ = nullptr;

    public:
        Tasker(const Controller &controller, const Resource &resource);
        ~Tasker();

        Tasker(const Tasker &) = delete;
        Tasker &operator=(const Tasker &) = delete;

        auto handle() const { return handle_; }

        MaaTaskId post_task(const char *entry) const;
        MaaStatus status(MaaTaskId id) const { return MaaTaskerStatus(handle_, id); }
        MaaStatus wait(MaaTaskId id) const { return MaaTaskerWait(handle_, id); }
        void stop() const { MaaTaskerPostStop(handle_); }
    };
}
