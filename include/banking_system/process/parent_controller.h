#ifndef BANKING_SYSTEM_PROCESS_PARENT_CONTROLLER_H
#define BANKING_SYSTEM_PROCESS_PARENT_CONTROLLER_H

#include "banking_system/common/types.h"

// ==================== 父进程控制器 ====================

/**
 * @brief 父进程控制器类
 * 
 * 负责协调整个分布式银行系统的运行流程：
 * 1. 等待所有账户进程启动
 * 2. 使用分片管理器并发执行转账
 * 3. 通知所有账户停止
 * 4. 收集并打印历史记录
 */
class ParentController {
public:
    /**
     * @brief 构造函数
     * @param count_nodes 节点总数（包括父进程）
     * @param num_shards 分片数量（默认8）
     */
    explicit ParentController(int count_nodes, int num_shards = 8);
    
    /**
     * @brief 执行父进程主控流程
     * 
     * 包含四个阶段：
     * - 阶段1: 启动同步
     * - 阶段2: 并发转账
     * - 阶段3: 结束同步
     * - 阶段4: 收集历史
     */
    void run();

private:
    int count_nodes_;     ///< 节点总数
    int num_shards_;      ///< 分片数量
    
    /**
     * @brief 阶段1：等待所有账户启动
     */
    void phase1_wait_startup();
    
    /**
     * @brief 阶段2：并发执行转账
     */
    void phase2_execute_transfers();
    
    /**
     * @brief 阶段3：通知所有账户停止
     */
    void phase3_stop_all();
    
    /**
     * @brief 阶段4：收集余额历史记录
     */
    void phase4_collect_history();
};

// ==================== 兼容性函数 ====================

/**
 * @brief 父进程主控流程（函数版本，保持向后兼容）
 * 
 * @param count_nodes 节点总数（包括父进程）
 * @param num_shards 分片数量（默认8）
 */
void parent_work(int count_nodes, int num_shards = 8);

#endif // BANKING_SYSTEM_PROCESS_PARENT_CONTROLLER_H