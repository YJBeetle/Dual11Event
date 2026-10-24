#pragma once

#include <string>

#include <MaaFramework/MaaAPI.h>

namespace Maa {
    class Resource {
    private:
        MaaResource *handle_ = nullptr;

    public:
        Resource();
        ~Resource();

        Resource(const Resource &) = delete;
        Resource &operator=(const Resource &) = delete;

        auto handle() const { return handle_; }

        // 阻塞加载资源包：pipeline / image / model/ocr 都在里面。
        void load(const std::string &path) const;
    };
}
