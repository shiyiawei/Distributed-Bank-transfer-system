# 分布式银行交易系统

一个基于C++17实现的多分片分布式银行交易系统，支持并发转账和Lamport逻辑时钟。

## 🎯 项目概述

本项目为模块化、可维护的项目结构，包含：
- 4个核心模块（Common、Transfer、Shard、Process）
- 10个头文件 + 7个实现文件
- 完整的构建系统（CMake + Makefile）

## 📁 项目结构

```
DistributedBankingSystem/
│
├── 📄 构建配置文件
│   ├── CMakeLists.txt                          # CMake构建配置
│   ├── Makefile                                # Make构建配置
│   └── build.sh                                # 自动编译脚本 (可执行)
│
├── 📚 头文件目录 (include/)
│   ├── banking_system.h                        # 主头文件（统一入口）
│   └── banking_system/
│       │
│       ├── common/                             # 基础模块 (3个)
│       │   ├── types.h                         # 类型定义
│       │   ├── clock.h                         # Lamport逻辑时钟
│       │   └── utils.h                         # 辅助工具函数
│       │
│       ├── transfer/                           # 转账模块 (2个)
│       │   ├── transfer_task.h                 # 转账任务定义
│       │   └── cross_shard_context.h           # 跨分片上下文
│       │
│       ├── shard/                              # 分片模块 (2个)
│       │   ├── account_shard.h                 # 账户分片类
│       │   └── shard_manager.h                 # 分片管理器类
│       │
│       └── process/                            # 进程模块 (2个)
│           ├── parent_controller.h             # 父进程控制器
│           └── child_worker.h                  # 子进程工作器
│
├── 💻 实现文件目录 (src/)
│   ├── common/                                 # 基础模块实现
│   │   ├── clock.cpp                           # Lamport时钟实现
│   │   └── utils.cpp                           # 工具函数实现
│   │
│   ├── shard/                                  # 分片模块实现
│   │   ├── account_shard.cpp                   # 账户分片实现
│   │   └── shard_manager.cpp                   # 分片管理器实现
│   │
│   ├── process/                                # 进程模块实现
│   │   ├── parent_controller.cpp               # 父进程控制器实现
│   │   └── child_worker.cpp                    # 子进程工作器实现
│   │
│   └── main.cpp                                # 主程序入口
│
├── 🔗 外部依赖目录 (external/) 
│   └── labs_headers/                           # 外部头文件 (需要您提供)
│       ├── message.h                           # 消息定义
│       ├── process.h                           # 进程通信
│       ├── banking.h                           # 银行结构
│       └── log.h                               # 日志功能
│
├── 🏗️ 构建输出目录 (build/) - 自动生成
│   ├── obj/                                    # 目标文件 (.o)
│   │   ├── common/
│   │   ├── shard/
│   │   └── process/
│   │
│   └── bin/                                    # 可执行文件
│       └── banking_system                      # 最终程序
│
└── 📖 文档目录
    └── README.md                               # 项目主说明
```

## 🚀 快速开始

### 编译

```bash
# 使用自动脚本
./build.sh

# 或使用Make
make

# 或使用CMake
mkdir build && cd build
cmake ..
make
```

### 运行

```bash
# Make构建
./build/bin/banking_system

# CMake构建
./build/banking_system
```

## 🔧 依赖要求

- C++17 或更高版本
- pthread 库
- CMake 3.14+ (可选)
- Make

## 📚 核心模块

### Common 模块
- **Lamport逻辑时钟**: 线程安全的分布式时钟
- **辅助工具**: 余额历史管理等工具函数
- **类型定义**: 统一的类型系统

### Transfer 模块
- **转账任务**: 三种任务类型（本地、跨分片步骤1、步骤2）
- **跨分片上下文**: 状态追踪和协调

### Shard 模块
- **账户分片**: 每个分片独立工作线程，任务队列机制
- **分片管理器**: 智能路由和跨分片协调

### Process 模块
- **父进程控制器**: 四阶段流程管理
- **子进程工作器**: 账户进程实现

## 🎯 核心实现概览

### Common 模块

**clock.cpp** (450B)
```cpp
- LamportClock::instance()      // 单例实例
- LamportClock::update()        // 更新时钟
- LamportClock::get_time()      // 获取时间
```

**utils.cpp** (1.4KB)
```cpp
- update_history()              // 更新余额历史
```

### Shard 模块

**account_shard.cpp** (6.7KB)
```cpp
- AccountShard::AccountShard()           // 构造函数
- AccountShard::~AccountShard()          // 析构函数
- AccountShard::submit_task()            // 提交任务
- AccountShard::wait_completion()        // 等待完成
- AccountShard::print_statistics()       // 打印统计
- AccountShard::worker_loop()            // 工作线程
- AccountShard::process_task()           // 处理任务
- AccountShard::handle_local_transfer()  // 本地转账
- AccountShard::handle_cross_shard_step1() // 跨分片步骤1
- AccountShard::handle_cross_shard_step2() // 跨分片步骤2
```

**shard_manager.cpp** (2.9KB)
```cpp
- ShardManager::ShardManager()              // 构造函数
- ShardManager::get_shard_id()              // 计算分片ID
- ShardManager::submit_transfer()           // 提交转账
- ShardManager::submit_cross_shard_step2()  // 提交步骤2
- ShardManager::cleanup_cross_shard_context() // 清理上下文
- ShardManager::wait_all_complete()         // 等待所有完成
- ShardManager::print_statistics()          // 打印统计
- ShardManager::handle_cross_shard_transfer() // 处理跨分片
```

### Process 模块

**parent_controller.cpp** (3.2KB)
```cpp
- ParentController::ParentController()     // 构造函数
- ParentController::run()                  // 运行主流程
- ParentController::phase1_wait_startup()  // 阶段1
- ParentController::phase2_execute_transfers() // 阶段2
- ParentController::phase3_stop_all()      // 阶段3
- ParentController::phase4_collect_history() // 阶段4
- parent_work()                            // 兼容函数
```

**child_worker.cpp** (5.5KB)
```cpp
- ChildWorker::ChildWorker()               // 构造函数
- ChildWorker::run()                       // 运行主流程
- ChildWorker::init_history()              // 初始化历史
- ChildWorker::send_started_and_wait()     // 发送启动消息
- ChildWorker::message_loop()              // 消息循环
- ChildWorker::wait_all_done()             // 等待所有完成
- ChildWorker::send_history()              // 发送历史
- ChildWorker::handle_transfer_as_source() // 作为源账户
- ChildWorker::handle_transfer_as_destination() // 作为目标账户
- child_work()                             // 兼容函数
```

### Main 模块

**main.cpp** (680B)
```cpp
- main()                                   // 程序入口
```

## ✨ 设计特性

- ✅ **模块化分层** - 清晰的层次结构
- ✅ **无循环依赖** - 单向依赖流
- ✅ **线程安全** - 完整的并发保护
- ✅ **RAII管理** - 自动资源清理
- ✅ **异常处理** - 健壮的错误处理
- ✅ **文档完整** - 100%注释覆盖

## 🔍 使用示例

```cpp
#include "banking_system.h"

// 使用Lamport时钟
timestamp_t current = update_lamport_time();

// 创建8个分片的管理器
ShardManager manager(8);

// 提交转账任务
manager.submit_transfer(1, 2, 100);

// 等待所有分片完成
manager.wait_all_complete();

// 打印统计信息
manager.print_statistics();
```

## 📝 许可

本项目为教育和学习目的。

## 🤝 贡献

欢迎提交问题和改进建议。

