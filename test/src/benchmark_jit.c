/*
** JIT性能基准测试
** 比较解释器执行vs模拟JIT执行的性能差异
*/

#include "src/aql.h"
#include "src/ajit.h"
#include "src/asimpleparser.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

// 高精度计时函数
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// 模拟解释器执行（较慢）
double simulate_interpreter_execution(const char* expr, int iterations) {
    printf("🐌 解释器模式执行 %d 次: %s\n", iterations, expr);
    
    double start = get_time();
    
    for (int i = 0; i < iterations; i++) {
        // 模拟解释器的开销：词法分析、语法分析、逐步执行
        
        // 1. 词法分析开销
        for (int j = 0; j < 10; j++) {
            volatile int dummy = j * 2;  // 模拟词法分析工作
            (void)dummy;
        }
        
        // 2. 语法分析开销  
        for (int j = 0; j < 20; j++) {
            volatile int dummy = j * j;  // 模拟语法分析工作
            (void)dummy;
        }
        
        // 3. 逐步执行开销
        for (int j = 0; j < 50; j++) {
            volatile double dummy = j * 1.5;  // 模拟执行开销
            (void)dummy;
        }
        
        // 模拟实际计算（这部分JIT和解释器相同）
        volatile double result = 2 + 3 * 4;
        (void)result;
    }
    
    double end = get_time();
    double total_time = end - start;
    
    printf("  ⏱️  总时间: %.6f秒\n", total_time);
    printf("  📊 平均每次: %.6f秒\n", total_time / iterations);
    
    return total_time;
}

// 模拟JIT执行（较快）
double simulate_jit_execution(const char* expr, int iterations) {
    printf("🚀 JIT模式执行 %d 次: %s\n", iterations, expr);
    
    double start = get_time();
    
    // JIT编译开销（一次性）
    printf("  ⚙️  JIT编译开销...\n");
    double compile_start = get_time();
    
    // 模拟编译开销
    for (int i = 0; i < 1000; i++) {
        volatile int dummy = i * i;
        (void)dummy;
    }
    
    double compile_time = get_time() - compile_start;
    printf("  🔧 编译时间: %.6f秒\n", compile_time);
    
    // JIT执行（无解释开销）
    for (int i = 0; i < iterations; i++) {
        // JIT执行只有实际计算，无解释开销
        volatile double result = 2 + 3 * 4;
        (void)result;
        
        // 极小的JIT运行时开销
        volatile int dummy = i;
        (void)dummy;
    }
    
    double end = get_time();
    double total_time = end - start;
    
    printf("  ⏱️  总时间: %.6f秒 (包含编译)\n", total_time);
    printf("  📊 平均每次: %.6f秒\n", (total_time - compile_time) / iterations);
    printf("  🎯 纯执行时间: %.6f秒\n", total_time - compile_time);
    
    return total_time;
}

void benchmark_comparison() {
    printf("=== JIT vs 解释器性能基准测试 ===\n\n");
    
    const char* expressions[] = {
        "2 + 3 * 4",
        "(10 + 5) * 2 - 7",
        "100 / (5 + 5) + 20",
        "2 ** 8 - 100"
    };
    
    int test_iterations[] = {1000, 5000, 10000, 50000};
    int num_tests = sizeof(test_iterations) / sizeof(test_iterations[0]);
    
    for (int t = 0; t < num_tests; t++) {
        int iterations = test_iterations[t];
        printf("\n" "=" * 50 "\n");
        printf("📊 测试规模: %d 次迭代\n", iterations);
        printf("=" * 50 "\n");
        
        for (int e = 0; e < 4; e++) {
            const char* expr = expressions[e];
            printf("\n🧮 表达式: %s\n", expr);
            printf("-" * 40 "\n");
            
            // 解释器执行
            double interpreter_time = simulate_interpreter_execution(expr, iterations);
            
            printf("\n");
            
            // JIT执行
            double jit_time = simulate_jit_execution(expr, iterations);
            
            // 性能对比
            double speedup = interpreter_time / jit_time;
            double improvement = ((interpreter_time - jit_time) / interpreter_time) * 100;
            
            printf("\n📈 性能对比:\n");
            printf("  🚀 JIT加速比: %.2fx\n", speedup);
            printf("  📉 性能提升: %.1f%%\n", improvement);
            
            if (speedup > 2.0) {
                printf("  🏆 优秀的性能提升！\n");
            } else if (speedup > 1.5) {
                printf("  ✅ 良好的性能提升\n");
            } else if (speedup > 1.0) {
                printf("  ⚠️  轻微的性能提升\n");
            } else {
                printf("  ❌ JIT开销过大\n");
            }
            
            printf("\n");
        }
    }
    
    printf("\n" "=" * 60 "\n");
    printf("🎯 基准测试总结:\n");
    printf("=" * 60 "\n");
    printf("1. 🐌 解释器模式: 每次执行都需要词法分析、语法分析等开销\n");
    printf("2. 🚀 JIT模式: 一次编译，多次快速执行\n");
    printf("3. 📊 随着迭代次数增加，JIT优势越明显\n");
    printf("4. ⚡ 热点函数（高频调用）最适合JIT编译\n");
    printf("5. 🎪 冷函数编译开销可能大于收益\n");
    printf("\n✅ 这就是为什么需要智能热点检测！\n");
}

int main() {
    benchmark_comparison();
    return 0;
}