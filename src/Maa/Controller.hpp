#pragma once

#include <string>

#include <MaaFramework/MaaAPI.h>

namespace Maa {
    class Controller {
    private:
        MaaController *handle_ = nullptr;

        static std::string discover_udid();

    public:
        // udid 为空时自动挑选唯一在线设备。xdb_port 为 0 时用 XDB_PORT 或默认端口。
        explicit Controller(const std::string &udid, int xdb_port = 0);

        ~Controller();

        Controller(const Controller &) = delete;
        Controller &operator=(const Controller &) = delete;

        auto handle() const { return handle_; }

        bool connect() const;
        std::string uuid() const;
    };
}
