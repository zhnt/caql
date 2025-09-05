#!/bin/bash

# 批量替换 DEBUG printf 语句为新的调试系统调用

# 备份所有文件
echo "Creating backups..."
for file in src/alex.c src/aparser.c src/avm.c src/acode.c src/aapi.c src/ado.c src/arepl.c src/astate.c; do
    if [ -f "$file" ]; then
        cp "$file" "$file.debug_backup"
        echo "Backed up $file"
    fi
done

echo "Adding debug headers..."

# 添加调试头文件到需要的文件
files_need_user_debug="src/alex.c src/aparser.c src/avm.c src/arepl.c"
files_need_internal_debug="src/acode.c src/aapi.c src/ado.c src/astate.c"

for file in $files_need_user_debug; do
    if [ -f "$file" ] && ! grep -q "adebug_user.h" "$file"; then
        # 在第一个 #include 后添加
        sed -i '' '/^#include.*<.*>/a\
\
#include "adebug_user.h"
' "$file"
        echo "Added adebug_user.h to $file"
    fi
done

for file in $files_need_internal_debug; do
    if [ -f "$file" ] && ! grep -q "adebug_internal.h" "$file"; then
        # 在第一个 #include 后添加
        sed -i '' '/^#include.*<.*>/a\
\
#include "adebug_internal.h"
' "$file"
        echo "Added adebug_internal.h to $file"
    fi
done

echo "Replacing DEBUG statements..."

# 简单的替换策略：将所有 printf("[DEBUG] 替换为注释，稍后手动处理关键的
for file in src/*.c; do
    if [ -f "$file" ]; then
        # 注释掉所有 DEBUG printf 语句
        sed -i '' 's/printf("\[DEBUG\]/\/\/ DEBUG: printf("[DEBUG]/g' "$file"
        sed -i '' 's/fflush(stdout);.*DEBUG/\/\/ fflush(stdout); \/\/ DEBUG/g' "$file"
        echo "Commented DEBUG statements in $file"
    fi
done

echo "Debug replacement completed!"
echo "Files backed up with .debug_backup extension"
echo "Next step: manually add key debug calls using new system"
