#!/bin/bash

# 分布式银行交易系统 - 编译脚本

set -e

echo "==================================="
echo "  分布式银行交易系统 - 构建脚本"
echo "==================================="
echo ""

# 检查构建方式
if [ "$1" == "cmake" ]; then
    echo "使用 CMake 构建..."
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    echo ""
    echo "✓ 构建完成！可执行文件: build/banking_system"
    
elif [ "$1" == "make" ] || [ -z "$1" ]; then
    echo "使用 Make 构建..."
    make -j$(nproc)
    echo ""
    echo "✓ 构建完成！可执行文件: build/bin/banking_system"
    
elif [ "$1" == "clean" ]; then
    echo "清理构建文件..."
    rm -rf build
    echo "✓ 清理完成！"
    
else
    echo "用法:"
    echo "  ./build.sh          - 使用 Make 构建 (默认)"
    echo "  ./build.sh make     - 使用 Make 构建"
    echo "  ./build.sh cmake    - 使用 CMake 构建"
    echo "  ./build.sh clean    - 清理构建文件"
    exit 1
fi