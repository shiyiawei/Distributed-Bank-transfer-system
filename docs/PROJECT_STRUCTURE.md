# 项目完整结构

## 实现文件统计

### C++ 源文件 (.cpp)
1. src/common/clock.cpp
2. src/common/utils.cpp
3. src/shard/account_shard.cpp
4. src/shard/shard_manager.cpp
5. src/process/parent_controller.cpp
6. src/process/child_worker.cpp
7. src/main.cpp

**总计**: 7 个

### C++ 头文件 (.h)
1. include/banking_system.h
2. include/banking_system/common/types.h
3. include/banking_system/common/clock.h
4. include/banking_system/common/utils.h
5. include/banking_system/transfer/transfer_task.h
6. include/banking_system/transfer/cross_shard_context.h
7. include/banking_system/shard/account_shard.h
8. include/banking_system/shard/shard_manager.h
9. include/banking_system/process/parent_controller.h
10. include/banking_system/process/child_worker.h

**总计**: 10 个

### 构建配置文件
1. CMakeLists.txt (CMake配置)
2. Makefile (Make配置)

**总计**: 2 个

### 文档文件 (.md)
1. BUILD.md
2. DEPENDENCY_GRAPH.md
3. EXTRACTION_SUMMARY.md
4. FILE_INDEX.md
5. HEADERS_README.md

**总计**: 5 个

## 完整目录树

```
outputs/
├── CMakeLists.txt
├── Makefile
├── include/
│   ├── banking_system.h
│   └── banking_system/
│       ├── common/
│       │   ├── types.h
│       │   ├── clock.h
│       │   └── utils.h
│       ├── transfer/
│       │   ├── transfer_task.h
│       │   └── cross_shard_context.h
│       ├── shard/
│       │   ├── account_shard.h
│       │   └── shard_manager.h
│       └── process/
│           ├── parent_controller.h
│           └── child_worker.h
│
└── src/
    ├── common/
    │   ├── clock.cpp
    │   └── utils.cpp
    ├── shard/
    │   ├── account_shard.cpp
    │   └── shard_manager.cpp
    ├── process/
    │   ├── parent_controller.cpp
    │   └── child_worker.cpp
    └── main.cpp
```

## 文件映射关系

| 头文件 | 对应实现文件 |
|--------|-------------|
| common/clock.h | common/clock.cpp |
| common/utils.h | common/utils.cpp |
| shard/account_shard.h | shard/account_shard.cpp |
| shard/shard_manager.h | shard/shard_manager.cpp |
| process/parent_controller.h | process/parent_controller.cpp |
| process/child_worker.h | process/child_worker.cpp |
| transfer/transfer_task.h | (内联实现) |
| transfer/cross_shard_context.h | (内联实现) |
| common/types.h | (类型定义) |

## 代码行数估算

| 模块 | 头文件 | 实现文件 | 总计 |
|------|--------|---------|------|
| Common | ~135 | ~50 | ~185 |
| Transfer | ~100 | 0 | ~100 |
| Shard | ~260 | ~280 | ~540 |
| Process | ~140 | ~220 | ~360 |
| Main | ~50 | ~20 | ~70 |
| **总计** | **~685** | **~570** | **~1255** |

## 编译流程

```
.cpp 源文件
    ↓
编译器 (g++ -c)
    ↓
.o 目标文件
    ↓
链接器 (g++)
    ↓
可执行文件 (banking_system)
```

## 库依赖关系

```
banking_system (可执行文件)
    ├── banking_process (静态库)
    │   ├── banking_shard (静态库)
    │   │   └── banking_common (静态库)
    │   └── banking_common (静态库)
    └── pthread (系统库)
```