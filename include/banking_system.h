#ifndef BANKING_SYSTEM_H
#define BANKING_SYSTEM_H

/**
 * @file banking_system.h
 * @brief 分布式银行交易系统主头文件
 * 
 * 包含所有公共API，方便外部使用
 */

// ==================== 通用组件 ====================
#include "banking_system/common/types.h"
#include "banking_system/common/clock.h"
#include "banking_system/common/utils.h"

// ==================== 转账组件 ====================
#include "banking_system/transfer/transfer_task.h"
#include "banking_system/transfer/cross_shard_context.h"

// ==================== 分片组件 ====================
#include "banking_system/shard/account_shard.h"
#include "banking_system/shard/shard_manager.h"

// ==================== 进程管理组件 ====================
#include "banking_system/process/parent_controller.h"
#include "banking_system/process/child_worker.h"

// ==================== 外部依赖 ====================
#include "labs_headers/message.h"
#include "labs_headers/log.h"
#include "labs_headers/process.h"
#include "labs_headers/banking.h"

/**
 * @namespace banking_system
 * @brief 分布式银行系统命名空间
 * 
 * 提供多分片并发转账功能，支持：
 * - Lamport逻辑时钟
 * - 分片内/跨分片转账
 * - 并发处理
 * - 历史记录追踪
 */

#endif // BANKING_SYSTEM_H