#pragma once

#include <chrono>
#include <thread>

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
        // 必须等停止作业真正跑完：只 PostStop 就返回的话，析构会在 worker 线程还在
        // 读 RuntimeCache 的时候把它拆掉，表现为 mutex lock failed 的 SIGABRT。
        bool stop_and_wait() const {
            auto stop_id = MaaTaskerPostStop(handle_);
            if (stop_id == 0) {
                return false;
            }
            MaaTaskerWait(handle_, stop_id);
            while (MaaTaskerRunning(handle_)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return true;
        }

        // 该节点本次运行里是否真的被命中执行过。用来区分「面板清空」和「爬不回活动页」。
        bool hit(const char *node_name) const {
            MaaNodeId latest_id = 0;
            return MaaTaskerGetLatestNode(handle_, node_name, &latest_id) != 0;
        }
    };
}
