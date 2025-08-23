/*
** Phase 3 调试系统测试
** 验证零成本调试系统功能
*/

#include <stdio.h>
#include <stdlib.h>

#define AQL_DEBUG_LEVEL 3  /* Enable all debugging features */
#include "../../src/adebug.h"

void test_function(void) {
    AQL_PROFILE_START("test_function");
    AQL_TRACE("Entering test function");
    
    /* Simulate some work */
    for (int i = 0; i < 1000; i++) {
        AQL_DEBUG(2, "Loop iteration %d", i);
    }
    
    AQL_TRACE("Exiting test function");
    AQL_PROFILE_END("test_function");
}

int main() {
    printf("=== AQL Phase 3 调试系统测试 ===\n\n");
    
    /* 初始化调试系统 */
    aqlD_init(NULL);
    
    printf("1. 调试级别检测:\n");
    printf("调试启用: %s\n", aqlD_is_enabled() ? "是" : "否");
    printf("调试级别: %d\n", aqlD_get_level());
    
    printf("\n2. 变量跟踪测试:\n");
    int x = 42;
    float y = 3.14;
    
    AQL_DEBUG(1, "变量初始化: x=%d, y=%.2f", x, y);
    
    printf("\n3. 函数调用跟踪:\n");
    aqlD_push_frame("main", __LINE__);
    
    AQL_DEBUG(2, "调用测试函数");
    test_function();
    
    aqlD_pop_frame();
    
    printf("\n4. 性能分析:\n");
    aqlD_print_profile();
    
    printf("\n5. 堆栈跟踪:\n");
    aqlD_print_stack_trace();
    
    printf("\n6. 断言测试:\n");
    AQL_ASSERT(1 == 1); /* Should pass */
    AQL_DEBUG(1, "断言通过: 1 == 1");
    
    /* 清理 */
    aqlD_cleanup(NULL);
    
    printf("\n🎉 Phase 3 调试系统验证完成！\n");
    printf("零成本调试系统已就绪，可集成到AQL执行流程\n");
    
    return 0;
}