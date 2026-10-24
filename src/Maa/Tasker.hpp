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

        // 该节点本次运行里是否真的被命中执行过。用来区分「面板清空」和「爬不回活动页」。
        bool hit(const char *node_name) const {
            MaaNodeId latest_id = 0;
            return MaaTaskerGetLatestNode(handle_, node_name, &latest_id) != 0;
        }
    };
}
