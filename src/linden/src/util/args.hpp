#pragma once
#include <deque>
#include <string>
#include <optional>

namespace linden {

class Args {
public:
    explicit Args(std::deque<std::string> args)
    :   args_(args)
    {}

    std::optional<std::string> next() noexcept {
        if (args_.empty()) {
            return std::nullopt;
        } else {
            std::string res = args_.front();
            args_.pop_front();
            return std::optional(res);
        }
    }

private:
    std::deque<std::string> args_;
};

} // namespace linden
