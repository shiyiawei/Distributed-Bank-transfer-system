#ifndef BANKING_SYSTEM_COMMON_UTILS_H
#define BANKING_SYSTEM_COMMON_UTILS_H

#include "types.h"
#include "labs_headers/banking.h"

// ==================== 辅助工具函数 ====================

/**
 * @brief 更新余额历史记录
 * 
 * 在历史记录中添加从pending_start_time到pending_end_time的余额变化
 * 包括pending状态和最终余额
 * 
 * @param history 余额历史记录指针
 * @param pending_start_time pending状态开始时间
 * @param pending_end_time pending状态结束时间
 * @param amount 最终余额
 * @param pending_money pending中的金额
 */
void update_history(BalanceHistory* history, 
                   timestamp_t pending_start_time, 
                   timestamp_t pending_end_time, 
                   balance_t amount, 
                   balance_t pending_money);

/**
 * @brief 获取当前余额
 * @param history 余额历史记录指针
 * @return 当前余额
 */
inline balance_t now_balance(const BalanceHistory* history) {
    return history->s_history[history->s_history_len - 1].s_balance;
}

/**
 * @brief 返回两个整数中的较大值
 * @param a 第一个整数
 * @param b 第二个整数
 * @return 较大的整数
 */
inline int max(int a, int b) {
    return (a > b) ? a : b;
}

#endif // BANKING_SYSTEM_COMMON_UTILS_H