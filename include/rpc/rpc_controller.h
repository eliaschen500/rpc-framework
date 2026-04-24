#pragma once
#include <google/protobuf/service.h>
#include <string>

namespace rpc {

class RpcController : public google::protobuf::RpcController {
public:
    void Reset() override { failed_ = false; err_text_.clear(); }

    bool Failed()              const override { return failed_; }
    std::string ErrorText()    const override { return err_text_; }

    void StartCancel()               override { /* not implemented */ }
    void SetFailed(const std::string& reason) override {
        failed_   = true;
        err_text_ = reason;
    }
    bool IsCanceled() const override { return false; }
    void NotifyOnCancel(google::protobuf::Closure*) override {}

private:
    bool        failed_{false};
    std::string err_text_;
};

} // namespace rpc
