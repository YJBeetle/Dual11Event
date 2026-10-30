#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

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

        // 设备侧隧道断了（xdb 到手机的 screencapture / input 连接被回收）时框架不会自己
        // 重连，而流水线会一直对着死会话发输入 —— 实测一段刷了 1372 条 touch_move 失败、
        // 白烧十几分钟。控制器句柄从 tasker 取，别用自己的：绑定之后所有权在 tasker 那边。
        bool controller_connected() const {
            auto *controller = MaaTaskerGetController(handle_);
            return controller != nullptr && MaaControllerConnected(controller) != 0;
        }
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

        // 本次任务已经执行过的节点 id 列表。*size 传 0 先问数量，再按数量取数组。
        // 比按名字逐个问 latest 好：MaaTaskerGetLatestNode 对没命中过的名字会打 ERR 日志，
        // 而监控是每半秒问一轮十几个名字，日志会被刷屏。
        std::vector<MaaNodeId> node_ids(MaaTaskId id) const {
            MaaSize count = 0;
            if (MaaTaskerGetTaskDetail(handle_, id, nullptr, nullptr, &count, nullptr) == 0 || count == 0) {
                return {};
            }
            std::vector<MaaNodeId> ids(count);
            MaaSize want = count;
            if (MaaTaskerGetTaskDetail(handle_, id, nullptr, ids.data(), &want, nullptr) == 0) {
                return {};
            }
            ids.resize(std::min<MaaSize>(want, count));
            return ids;
        }

        std::string node_name(MaaNodeId node_id) const {
            auto *buffer = MaaStringBufferCreate();
            if (!buffer) {
                return {};
            }
            MaaRecoId reco_id = 0;
            MaaActId action_id = 0;
            MaaBool completed = false;
            std::string name;
            if (MaaTaskerGetNodeDetail(handle_, node_id, buffer, &reco_id, &action_id, &completed) != 0) {
                if (const char *text = MaaStringBufferGet(buffer)) {
                    name = text;
                }
            }
            MaaStringBufferDestroy(buffer);
            return name;
        }
    };
}
