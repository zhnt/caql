cmake_minimum_required(VERSION 3.12)
project(AQL VERSION 1.0.0 LANGUAGES C)

# C标准设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DAQL_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# 检测平台和SIMD支持
include(CheckCSourceCompiles)
check_c_source_compiles("
    #include <immintrin.h>
    int main() { __m256 v = _mm256_setzero_ps(); return 0; }
" HAS_AVX)

check_c_source_compiles("
    #include <arm_neon.h>
    int main() { float32x4_t v = vdupq_n_f32(0.0f); return 0; }
" HAS_NEON)

# 配置选项
option(AQL_USE_SIMD "Enable SIMD optimizations" ON)
option(AQL_USE_JIT "Enable JIT compilation" OFF)
option(AQL_BUILD_TESTS "Build test suite" ON)
option(AQL_BUILD_EXAMPLES "Build example programs" ON)

# 源文件列表
set(AQL_CORE_SOURCES
    # 基础层
    src/aconf.c
    src/aopcodes.c
    
    # 对象系统层
    src/aobject.c
    
    # 虚拟机层
    src/amem.c
    src/agc.c
    src/avm.c
    src/ado.c
    
    # 运行时层
    src/astate.c
    src/astring.c
    
    # 容器层
    src/aarray.c
    src/aslice.c
    src/adict.c
    src/avector.c
    
    # 编译器层
    src/alex.c
    src/aparser.c
    src/acode.c
    
    # 标准库层
    src/azio.c
    src/aundump.c
    
    # API层
    src/aql.c
    src/aapi.c
    src/aauxlib.c
)

# 性能优化层（可选）
set(AQL_OPT_SOURCES)
if(AQL_USE_SIMD)
    list(APPEND AQL_OPT_SOURCES src/ai_simd.c)
    add_definitions(-DAQL_USE_SIMD=1)
endif()

if(AQL_USE_JIT)
    list(APPEND AQL_OPT_SOURCES src/ai_jit.c)
    add_definitions(-DAQL_USE_JIT=1)
endif()

# AQL静态库
add_library(aql_static STATIC ${AQL_CORE_SOURCES} ${AQL_OPT_SOURCES})
target_include_directories(aql_static PUBLIC src)

# AQL动态库
add_library(aql_shared SHARED ${AQL_CORE_SOURCES} ${AQL_OPT_SOURCES})
target_include_directories(aql_shared PUBLIC src)
set_target_properties(aql_shared PROPERTIES OUTPUT_NAME aql)

# SIMD编译选项
if(AQL_USE_SIMD AND HAS_AVX)
    target_compile_options(aql_static PRIVATE -mavx -mavx2)
    target_compile_options(aql_shared PRIVATE -mavx -mavx2)
    add_definitions(-DAQL_HAS_X86_SIMD=1)
elseif(AQL_USE_SIMD AND HAS_NEON)
    add_definitions(-DAQL_HAS_ARM_NEON=1)
endif()

# AQL解释器可执行文件
add_executable(aql cmd/aql.c)
target_link_libraries(aql aql_static)

# 测试套件
if(AQL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 示例程序
if(AQL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装配置
install(TARGETS aql_static aql_shared aql
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY src/
    DESTINATION include/aql
    FILES_MATCHING PATTERN "*.h"
)

# 打包配置
set(CPACK_PACKAGE_NAME "AQL")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "AI Query Language - High-performance AI-native scripting language")
set(CPACK_PACKAGE_CONTACT "AQL Team")
include(CPack) 