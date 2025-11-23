#ifndef BANKING_SYSTEM_TRANSFER_CROSS_SHARD_CONTEXT_H
#define BANKING_SYSTEM_TRANSFER_CROSS_SHARD_CONTEXT_H

#include "transfer_task.h"
#include <chrono>

// ==================== 跨分片上下文 ====================

/**
 * @brief 跨分片转账的上下文信息
 * 
 * 用于跟踪跨分片转账的状态，确保两步操作的协调
 * 包含原始任务信息、完成状态和时间戳
 */
struct CrossShardContext {
    TransferTask task;                                     ///< 原始转账任务
    bool step1_completed;                                  ///< 第一步是否完成
    std::chrono::steady_clock::time_point timestamp;       ///< 创建时间（用于超时检测）
    
    /**
     * @brief 构造函数
     * @param t 转账任务
     */
    explicit CrossShardContext(const TransferTask& t)
        : task(t)
        , step1_completed(false)
        , timestamp(std::chrono::steady_clock::now())
    {}
};

#endif // BANKING_SYSTEM_TRANSFER_CROSS_SHARD_CONTEXT_H