#include <chrono>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <thread>

#include <MaaFramework/MaaAPI.h>
#include <MaaToolkit/MaaToolkitAPI.h>

#include "./Log.h"
#include "./Maa/Controller.hpp"
#include "./Maa/Resource.hpp"
#include "./Maa/Tasker.hpp"

namespace {
    void print_usage() {
        std::cout
            << "usage: Dual11Event [options]\n"
            << "  -s, --serial UDID    iOS device to drive (default: the only connected one)\n"
            << "  --xdb-port PORT      XDeviceBridge server port (default: XDB_PORT or 17423)\n"
            << "  --assets DIR         resource directory (default: ./assets)\n"
            << "  --entry NAME         pipeline entry node (default: EarnEnergy)\n"
            << "  --budget SECONDS     hard stop for one run (default: 3600)\n"
            << "  -h, --help           show this help\n";
    }

    struct Options {
        std::string udid;
        std::string assets = "./assets";
        std::string entry = "EarnEnergy";
        int xdb_port = 0;
        int budget = 60 * 60;
    };

    bool parse(int argc, char **argv, Options &out) {
        auto take = [&](int &i, const std::string &flag, std::string &dst) {
            if (++i == argc) {
                std::cerr << flag << " requires a value\n";
                return false;
            }
            dst = argv[i];
            return true;
        };
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            std::string value;
            if (arg == "-h" || arg == "--help") {
                print_usage();
                return false;
            }
            if (arg == "-s" || arg == "--serial") {
                if (!take(i, arg, out.udid)) {
                    return false;
                }
                continue;
            }
            if (arg == "--assets") {
                if (!take(i, arg, out.assets)) {
                    return false;
                }
                continue;
            }
            if (arg == "--entry") {
                if (!take(i, arg, out.entry)) {
                    return false;
                }
                continue;
            }
            if (arg == "--xdb-port" || arg == "--budget") {
                if (!take(i, arg, value)) {
                    return false;
                }
                int number = std::atoi(value.c_str());
                if (number <= 0) {
                    std::cerr << arg << " expects a positive number\n";
                    return false;
                }
                if (arg == "--xdb-port") {
                    out.xdb_port = number;
                }
                else {
                    out.budget = number;
                }
                continue;
            }
            std::cerr << "unknown option: " << arg << "\n";
            return false;
        }
        return true;
    }

    // 退出梯子的三段（点左上角、边缘手势、重开应用）全是 DirectHit，永远命中。页面要是
    // 完全不理返回手势，就会一直 EarnEnergy -> CornerBack -> EarnEnergy 原地转圈。
    // 流水线的 max_hit 封的是整轮总次数，调大它就等于没有上限；真正想要的上限是
    // 「连续多少次退出动作没换来任何进展」，MaaFW 里没有这个概念，只能在外面数。
    // node id 单调递增且每执行一次换一个新的，所以 id 变了就是又跑了一次。
    // 一层层往外退的嵌套页面本身就要花掉好几轮（一轮 = 一次盲点 + 一次边缘手势），
    // 所以这个阈值不能贴着「明显卡死」的下限，得留出退 8~10 层的余量。
    constexpr int kMaxEscapesWithoutProgress = 20;
    const std::set<std::string> kProgressNodes = {
        "TaskRow", "TaskFinished", "ClaimReward", "OpenTaskPanel", "PanelDrained2",
    };
    const std::set<std::string> kEscapeNodes = {
        "CornerBack", "EdgeBack",  "LaunchTaobao", "GoMine",      "GoldEntry",
        "BackToTaobao", "CloseCircle", "CloseX",   "DismissDialog", "LeaveDialog",
    };

    // 退出梯子的三段（点左上角、边缘手势、重开应用）全是 DirectHit，永远命中。页面要是
    // 完全不理返回手势，就会一直 EarnEnergy -> CornerBack -> EarnEnergy 原地转圈。
    // 流水线的 max_hit 封的是整轮总次数，调大它就等于没有上限；真正想要的上限是
    // 「连续多少次退出动作没换来任何进展」，MaaFW 里没有这个概念，只能在外面数。
    class ProgressWatch {
    public:
        void poll(const Maa::Tasker &tasker, MaaTaskId task_id) {
            auto ids = tasker.node_ids(task_id);
            for (; cursor_ < ids.size(); ++cursor_) {
                auto name = tasker.node_name(ids[cursor_]);
                if (kProgressNodes.count(name)) {
                    idle_escapes_ = 0;
                }
                else if (kEscapeNodes.count(name) && ++idle_escapes_ >= kMaxEscapesWithoutProgress) {
                    stuck_ = true;
                    return;
                }
            }
        }

        bool stuck() const { return stuck_; }
        int idle_escapes() const { return idle_escapes_; }

    private:
        size_t cursor_ = 0;
        int idle_escapes_ = 0;
        bool stuck_ = false;
    };
}

int main(int argc, char **argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        return 2;
    }

    // ./config/maa_option.json overrides these, e.g. to raise stdout_level while debugging.
    MaaToolkitConfigInitOption("./", R"({"logging": true, "stdout_level": 3})");
    LOG_INFO("main", "MaaFramework %s", MaaVersion());

    try {
        Maa::Controller controller(options.udid, options.xdb_port);
        if (!controller.connect()) {
            throw std::runtime_error("Failed to connect the device");
        }
        LOG_INFO("main", "Connected %s", controller.uuid().c_str());

        Maa::Resource resource;
        resource.load(options.assets);

        Maa::Tasker tasker(controller, resource);
        auto task_id = tasker.post_task(options.entry.c_str());
        if (task_id == 0) {
            throw std::runtime_error("Failed to post task " + options.entry);
        }

        auto status = tasker.status(task_id);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.budget);
        bool timed_out = false;
        ProgressWatch watch;
        while (status == MaaStatus_Pending || status == MaaStatus_Running) {
            if (std::chrono::steady_clock::now() >= deadline) {
                LOG_WARNING("main", "Budget of %d s exhausted, stopping the task", options.budget);
                tasker.stop_and_wait();
                timed_out = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            watch.poll(tasker, task_id);
            if (watch.stuck()) {
                LOG_WARNING(
                    "main",
                    "%d back actions in a row changed nothing — this page ignores the whole escape ladder. "
                    "Stopping instead of spinning",
                    watch.idle_escapes());
                tasker.stop_and_wait();
                return 2;
            }
            status = tasker.status(task_id);
        }
        if (!timed_out) {
            status = tasker.wait(task_id);
        }

        if (timed_out) {
            LOG_WARNING("main", "Stopped at the %d s budget with tasks possibly left", options.budget);
            return 1;
        }
        if (status != MaaStatus_Succeeded) {
            LOG_FATAL("main", "Task did not finish cleanly (status %d)", static_cast<int>(status));
            return 1;
        }
        if (tasker.hit("GiveUp")) {
            LOG_WARNING("main", "Stopped early: either a choice that needs you (paint/team picker) or a page "
                               "the ladder cannot climb out of. Handle it on the phone and run again");
            return 2;
        }
        LOG_INFO("main", "Task succeeded");
    } catch (const std::exception &e) {
        LOG_FATAL("main", "%s", e.what());
        return 1;
    }

    return 0;
}
