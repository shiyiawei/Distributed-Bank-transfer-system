#ifndef BANKING_SYSTEM_COMMON_TYPES_H
#define BANKING_SYSTEM_COMMON_TYPES_H

#include <cstdint>

// ==================== 基础类型定义 ====================

/**
 * @brief 时间戳类型（Lamport逻辑时钟）
 */
using timestamp_t = int;

/**
 * @brief 余额类型
 */
using balance_t = int;

/**
 * @brief 本地进程ID类型
 */
using local_id = uint8_t;

/**
 * @brief 缓冲区大小常量
 */
constexpr int BUF_SIZE = 256;

/**
 * @brief 最大时间戳
 */
constexpr int MAX_T = 1024;

#endif // BANKING_SYSTEM_COMMON_TYPES_H