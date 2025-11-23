#include "banking_system/common/clock.h"
#include <algorithm>

LamportClock& LamportClock::instance() {
    static LamportClock instance;
    return instance;
}

timestamp_t LamportClock::update(timestamp_t received_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    time_ = std::max(time_, received_time) + 1;
    return time_;
}

timestamp_t LamportClock::get_time() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return time_;
}