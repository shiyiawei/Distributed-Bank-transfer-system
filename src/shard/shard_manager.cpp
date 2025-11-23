#include "banking_system/shard/shard_manager.h"
#include <iostream>

ShardManager::ShardManager(int num_shards)
    : num_shards_(num_shards)
    , next_correlation_id_(1)
{
    std::cout << "\n=== 初始化分片管理器 ===" << std::endl;
    std::cout << "分片数量: " << num_shards_ << std::endl;
    
    for (int i = 0; i < num_shards_; ++i) {
        shards_.push_back(std::make_unique<AccountShard>(i, this));
    }
    
    std::cout << "所有分片已启动\n" << std::endl;
}

int ShardManager::get_shard_id(local_id account_id) const {
    return account_id % num_shards_;
}

void ShardManager::submit_transfer(local_id src, local_id dst, balance_t amount) {
    int src_shard = get_shard_id(src);
    int dst_shard = get_shard_id(dst);
    
    if (src_shard == dst_shard) {
        TransferTask task(src, dst, amount);
        shards_[src_shard]->submit_task(task);
    } else {
        handle_cross_shard_transfer(src, dst, amount, src_shard, dst_shard);
    }
}

void ShardManager::submit_cross_shard_step2(uint64_t correlation_id) {
    CrossShardContext context(TransferTask(0, 0, 0));
    
    {
        std::lock_guard<std::mutex> lock(context_mutex_);
        auto it = cross_shard_contexts_.find(correlation_id);
        if (it == cross_shard_contexts_.end()) {
            std::cerr << "错误: 找不到correlation_id=" << correlation_id << std::endl;
            return;
        }
        context = it->second;
        context.step1_completed = true;
    }
    
    TransferTask step2_task(
        TaskType::CROSS_SHARD_STEP2,
        context.task.src_account,
        context.task.dst_account,
        context.task.amount,
        correlation_id,
        context.task.src_shard_id,
        context.task.dst_shard_id
    );
    
    shards_[context.task.dst_shard_id]->submit_task(step2_task);
}

void ShardManager::cleanup_cross_shard_context(uint64_t correlation_id) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    cross_shard_contexts_.erase(correlation_id);
}

void ShardManager::wait_all_complete() {
    for (auto& shard : shards_) {
        shard->wait_completion();
    }
}

void ShardManager::print_statistics() {
    std::cout << "\n=== 分片统计信息 ===" << std::endl;
    for (auto& shard : shards_) {
        shard->print_statistics();
    }
}

void ShardManager::handle_cross_shard_transfer(local_id src, local_id dst, balance_t amount,
                                               int src_shard, int dst_shard) {
    uint64_t correlation_id = next_correlation_id_.fetch_add(1);
    
    TransferTask step1_task(
        TaskType::CROSS_SHARD_STEP1,
        src, dst, amount,
        correlation_id,
        src_shard, dst_shard
    );
    
    {
        std::lock_guard<std::mutex> lock(context_mutex_);
        cross_shard_contexts_.emplace(correlation_id, CrossShardContext(step1_task));
    }
    
    shards_[src_shard]->submit_task(step1_task);
}