#ifndef BANKING_SYSTEM_TRANSFER_TRANSFER_TASK_H
#define BANKING_SYSTEM_TRANSFER_TRANSFER_TASK_H

#include "banking_system/common/types.h"
#include <cstdint>

// ==================== 转账任务定义 ====================

/**
 * @brief 转账任务类型枚举
 */
enum class TaskType {
    LOCAL_TRANSFER,        ///< 分片内转账（src和dst在同一分片）
    CROSS_SHARD_STEP1,    ///< 跨分片转账第一步（源账户扣款）
    CROSS_SHARD_STEP2     ///< 跨分片转账第二步（目标账户入账）
};

/**
 * @brief 转账任务结构体
 * 
 * 封装一次转账所需的所有信息，支持分片内转账和跨分片转账
 */
struct TransferTask {
    TaskType task_type;           ///< 任务类型
    local_id src_account;         ///< 源账户ID
    local_id dst_account;         ///< 目标账户ID
    balance_t amount;             ///< 转账金额
    
    // 跨分片转账协调信息
    uint64_t correlation_id;      ///< 关联ID，用于匹配跨分片的两步操作
    int src_shard_id;             ///< 源分片ID
    int dst_shard_id;             ///< 目标分片ID
    
    /**
     * @brief 构造函数：分片内转账
     * @param src 源账户ID
     * @param dst 目标账户ID
     * @param amt 转账金额
     */
    TransferTask(local_id src, local_id dst, balance_t amt)
        : task_type(TaskType::LOCAL_TRANSFER)
        , src_account(src)
        , dst_account(dst)
        , amount(amt)
        , correlation_id(0)
        , src_shard_id(-1)
        , dst_shard_id(-1)
    {}
    
    /**
     * @brief 构造函数：跨分片转账
     * @param type 任务类型（CROSS_SHARD_STEP1 或 CROSS_SHARD_STEP2）
     * @param src 源账户ID
     * @param dst 目标账户ID
     * @param amt 转账金额
     * @param corr_id 关联ID
     * @param src_shard 源分片ID
     * @param dst_shard 目标分片ID
     */
    TransferTask(TaskType type, local_id src, local_id dst, balance_t amt,
                 uint64_t corr_id, int src_shard, int dst_shard)
        : task_type(type)
        , src_account(src)
        , dst_account(dst)
        , amount(amt)
        , correlation_id(corr_id)
        , src_shard_id(src_shard)
        , dst_shard_id(dst_shard)
    {}
};

#endif // BANKING_SYSTEM_TRANSFER_TRANSFER_TASK_H