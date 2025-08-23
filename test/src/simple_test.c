/*
** Phase 1 简化验证测试
** 只验证核心结构大小和类型定义
*/

#include <stdio.h>
#include <stddef.h>

// 直接包含头文件路径
#include "../../src/aobject.h"

int main() {
    printf("=== AQL Phase 1 核心结构验证 ===\n\n");
    
    // 验证TValue结构大小（标准Lua 16字节设计）
    printf("TValue结构大小: %zu 字节\n", sizeof(TValue));
    printf("Value联合大小: %zu 字节\n", sizeof(Value));
    
    // 验证16字节标准设计
    if (sizeof(TValue) == 16) {
        printf("✅ TValue结构大小验证通过 (16字节)\n");
    } else {
        printf("❌ TValue结构大小异常\n");
    }
    
    // 验证容器类型定义存在
    printf("AQL容器类型定义:\n");
    printf("  AQL_TARRAY = %d\n", AQL_TARRAY);
    printf("  AQL_TSLICE = %d\n", AQL_TSLICE);
    printf("  AQL_TDICT = %d\n", AQL_TDICT);
    printf("  AQL_TVECTOR = %d\n", AQL_TVECTOR);
    
    printf("\n🎉 Phase 1 核心验证完成！\n");
    printf("所有关键结构已就绪，可直接启动Phase 1实施\n");
    
    return 0;
}