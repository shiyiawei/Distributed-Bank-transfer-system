#ifndef BANKING_SYSTEM_COMMON_CLOCK_H
#define BANKING_SYSTEM_COMMON_CLOCK_H

#include "types.h"
#include <mutex>

// ==================== Lamport逻辑时钟管理 ====================

/**
 * @brief Lamport逻辑时钟类
 * 
 * 提供线程安全的分布式逻辑时钟实现
 * 遵循Lamport时钟算法：
 * - 本地事件发生时，时钟+1
 * - 接收消息时，时钟 = max(本地时钟, 消息时钟) + 1
 */
class LamportClock {
public:
    /**
     * @brief 获取单例实例
     */
    static LamportClock& instance();
    
    /**
     * @brief 更新Lamport时钟（线程安全）
     * @param received_time 接收到的时间戳（默认为0表示本地事件）
     * @return 更新后的时间戳
     */
    timestamp_t update(timestamp_t received_time = 0);
    
    /**
     * @brief 获取当前Lamport时钟（线程安全）
     * @return 当前时间戳
     */
    timestamp_t get_time() const;

private:
    LamportClock() : time_(0) {}
    ~LamportClock() = default;
    
    // 禁止拷贝和赋值
    LamportClock(const LamportClock&) = delete;
    LamportClock& operator=(const LamportClock&) = delete;
    
    timestamp_t time_;           // 当前逻辑时间
    mutable std::mutex mutex_;   // 保护时钟的互斥锁
};

// ==================== 全局便利函数 ====================

/**
 * @brief 更新Lamport时钟的全局便利函数
 * @param received_time 接收到的时间戳
 * @return 更新后的时间戳
 */
inline timestamp_t update_lamport_time(timestamp_t received_time = 0) {
    return LamportClock::instance().update(received_time);
}

/**
 * @brief 获取当前Lamport时钟的全局便利函数
 * @return 当前时间戳
 */
inline timestamp_t get_lamport_time() {
    return LamportClock::instance().get_time();
}

#endif // BANKING_SYSTEM_COMMON_CLOCK_H