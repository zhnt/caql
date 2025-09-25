# Makefile for AQL MVP1
# Target: mvp1
# Build directory: build/
# Binary directory: bin/

# Compiler and flags
CC = gcc
BASE_CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -I./src
DEBUG_CFLAGS = $(BASE_CFLAGS) -DAQL_DEBUG_BUILD -g -O0 -DDEBUG_DISABLED=0
RELEASE_CFLAGS = $(BASE_CFLAGS) -O2 -DNDEBUG -DDEBUG_DISABLED=1
LDFLAGS = -lm

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Test directories
TEST_DIR = test
TEST_FRAMEWORK_DIR = $(TEST_DIR)/framework
TEST_BUILD_DIR = build/test
TEST_BIN_DIR = bin/test

# Target executables
TARGET_RELEASE = $(BIN_DIR)/aql
TARGET_DEBUG = $(BIN_DIR)/aqld

# Test executables
TEST_RUNNER = $(TEST_BIN_DIR)/test_runner

# Core source files (working modules)
CORE_SOURCES = \
    $(SRC_DIR)/aconf.c \
    $(SRC_DIR)/aopcodes.c \
    $(SRC_DIR)/aobject.c \
    $(SRC_DIR)/amem.c \
    $(SRC_DIR)/agc.c \
    $(SRC_DIR)/alex.c \
    $(SRC_DIR)/azio.c \
    $(SRC_DIR)/acontainer.c \
    $(SRC_DIR)/aarray.c \
    $(SRC_DIR)/aslice.c \
    $(SRC_DIR)/adict.c \
    $(SRC_DIR)/avector.c \
    $(SRC_DIR)/adatatype.c \
    $(SRC_DIR)/atype.c \
    $(SRC_DIR)/astring.c \
    $(SRC_DIR)/arange.c \
    $(SRC_DIR)/adebug_internal.c \
    $(SRC_DIR)/adebug_user.c \
    $(SRC_DIR)/aperf.c \
    $(SRC_DIR)/atypeinfer.c \
    $(SRC_DIR)/avm.c \
    $(SRC_DIR)/astate.c \
    $(SRC_DIR)/afunc.c \
    $(SRC_DIR)/ajit.c \
    $(SRC_DIR)/acodegen.c \
    $(SRC_DIR)/acodegen_templates.c \
    $(SRC_DIR)/aregalloc.c \
    $(SRC_DIR)/aoptimizer.c \
    $(SRC_DIR)/aql.c \
    $(SRC_DIR)/aapi.c \
    $(SRC_DIR)/ado.c \
    $(SRC_DIR)/aparser.c \
    $(SRC_DIR)/acode.c \
    $(SRC_DIR)/aerror.c \
    $(SRC_DIR)/arepl.c \
    $(SRC_DIR)/main.c

# Object files (replace .c with .o and move to build dir)
# Object files for different build types
DEBUG_BUILD_DIR = $(BUILD_DIR)/debug
RELEASE_BUILD_DIR = $(BUILD_DIR)/release
DEBUG_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(RELEASE_BUILD_DIR)/%.o)
# Keep old name for compatibility with tests
CORE_OBJECTS = $(DEBUG_OBJECTS)

# Problematic sources (to be fixed later)
PROBLEM_SOURCES = \
    $(SRC_DIR)/aql.c

# Header dependencies
HEADERS = $(wildcard $(SRC_DIR)/*.h)

# Default target
.PHONY: all both debug release clean dirs test test_phase1 test_phase2 test_phase3 test_phase4

all: both

# Build both debug and release versions
both: dirs $(TARGET_DEBUG) $(TARGET_RELEASE)

# Build only debug version
debug: dirs $(TARGET_DEBUG)

# Build only release version  
release: dirs $(TARGET_RELEASE)

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link the executable
# Link the release executable
$(TARGET_RELEASE): $(RELEASE_OBJECTS)
	@echo "Linking AQL Release executable..."
	$(CC) $(RELEASE_OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built AQL Release: $@"

# Link the debug executable
$(TARGET_DEBUG): $(DEBUG_OBJECTS)
	@echo "Linking AQL Debug executable..."
	$(CC) $(DEBUG_OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built AQL Debug: $@"

# Compile debug version
$(DEBUG_BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $< (debug)..."
	@mkdir -p $(DEBUG_BUILD_DIR)
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

# Compile release version  
$(RELEASE_BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $< (release)..."
	@mkdir -p $(RELEASE_BUILD_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

# Keep old rule for compatibility with tests
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@

# Smart clean: remove main executables but keep object files (for faster rebuild)
clean:
	@echo "Smart cleaning: removing main executables but keeping object files..."
	rm -f $(TARGET_DEBUG) $(TARGET_RELEASE)
	@echo "✅ Smart clean complete (objects preserved for faster rebuild)"

# Test target (run the executable)
test: debug
	@echo "Running AQL debug test..."
	./$(TARGET_DEBUG) --test

# Test Phase 1
TEST_SRC_DIR = test/src
TEST_BUILD_DIR = test/build
TEST_BIN_DIR = test/bin

TEST_SOURCES = $(TEST_SRC_DIR)/simple_test.c $(TEST_SRC_DIR)/type_test.c $(TEST_SRC_DIR)/debug_test.c
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_SRC_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_TARGET = $(TEST_BIN_DIR)/test_phase1
TEST_PHASE2_TARGET = $(TEST_BIN_DIR)/test_phase2
TEST_PHASE3_TARGET = $(TEST_BIN_DIR)/test_phase3
TEST_PHASE4_TARGET = $(TEST_BIN_DIR)/test_phase4_simple

test_phase1: dirs $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(filter-out build/main.o,$(CORE_OBJECTS))
	@echo "Linking Phase 1 test executable..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_OBJECTS) $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 1 test: $@"

$(TEST_BUILD_DIR)/%.o: $(TEST_SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Phase 2 test target
test_phase2: dirs $(TEST_PHASE2_TARGET)

$(TEST_PHASE2_TARGET): $(TEST_BUILD_DIR)/type_test.o $(filter-out build/main.o,$(CORE_OBJECTS))
	@echo "Linking Phase 2 type inference test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/type_test.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 2 test: $@"

$(TEST_BUILD_DIR)/type_test.o: $(TEST_SRC_DIR)/type_test.c $(HEADERS)
	@echo "Compiling Phase 2 test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Phase 3 test target
test_phase3: dirs $(TEST_PHASE3_TARGET)

TEST_PHASE3_TARGET = $(TEST_BIN_DIR)/test_phase3

$(TEST_PHASE3_TARGET): $(TEST_BUILD_DIR)/debug_test.o $(filter-out build/main.o,$(CORE_OBJECTS)) build/adebug.o
	@echo "Linking Phase 3 debug system test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/debug_test.o build/adebug.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 3 test: $@"

$(TEST_BUILD_DIR)/debug_test.o: $(TEST_SRC_DIR)/debug_test.c $(HEADERS)
	@echo "Compiling Phase 3 test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Phase 4 test targets
test_phase4: dirs $(TEST_PHASE4_TARGET) $(TEST_BIN_DIR)/phase4_jit_functional_test

$(TEST_PHASE4_TARGET): $(TEST_BUILD_DIR)/phase4_simple_test.o $(filter-out build/main.o,$(CORE_OBJECTS)) build/ai_jit.o
	@echo "Linking Phase 4 JIT system test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/phase4_simple_test.o build/ai_jit.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 4 test: $@"

$(TEST_BUILD_DIR)/phase4_simple_test.o: $(TEST_SRC_DIR)/phase4_simple_test.c $(HEADERS)
	@echo "Compiling Phase 4 test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAQL_USE_JIT=1 -c $< -o $@

$(TEST_BIN_DIR)/phase4_jit_functional_test: $(TEST_BUILD_DIR)/phase4_jit_functional_test.o $(filter-out build/main.o,$(CORE_OBJECTS)) build/ai_jit.o
	@echo "Linking Phase 4 JIT functional test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/phase4_jit_functional_test.o build/ai_jit.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 4 functional test: $@"

$(TEST_BUILD_DIR)/phase4_jit_functional_test.o: $(TEST_SRC_DIR)/phase4_jit_functional_test.c $(HEADERS)
	@echo "Compiling Phase 4 functional test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAQL_USE_JIT=1 -c $< -o $@

# Clean test artifacts
clean_test:
	@echo "Cleaning test artifacts..."
	rm -rf $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	@echo "✅ Test clean complete"

# Individual module targets for debugging
$(BUILD_DIR)/aconf.o: $(SRC_DIR)/aconf.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aopcodes.o: $(SRC_DIR)/aopcodes.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/alex.o: $(SRC_DIR)/alex.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aarray.o: $(SRC_DIR)/aarray.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aslice.o: $(SRC_DIR)/aslice.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/adict.o: $(SRC_DIR)/adict.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/avector.o: $(SRC_DIR)/avector.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/adatatype.o: $(SRC_DIR)/adatatype.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/avm.o: $(SRC_DIR)/avm.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

# Help
$(TEST_BIN_DIR)/phase4_jit_simple_test: $(TEST_BUILD_DIR)/phase4_jit_simple_test.o $(filter-out build/main.o,$(CORE_OBJECTS)) build/ai_jit.o
	@echo "Linking Phase 4 JIT simple test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/phase4_jit_simple_test.o build/ai_jit.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 4 simple test: $@"

$(TEST_BUILD_DIR)/phase4_jit_simple_test.o: $(TEST_SRC_DIR)/phase4_jit_simple_test.c $(HEADERS)
	@echo "Compiling Phase 4 simple test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAQL_USE_JIT=1 -c $< -o $@

$(TEST_BIN_DIR)/phase4_jit_final_proof: $(TEST_BUILD_DIR)/phase4_jit_final_proof.o $(filter-out build/main.o,$(CORE_OBJECTS)) build/ai_jit.o
	@echo "Linking Phase 4 JIT final proof test..."
	@mkdir -p $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	$(CC) $(TEST_BUILD_DIR)/phase4_jit_final_proof.o build/ai_jit.o $(filter-out build/main.o,$(CORE_OBJECTS)) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built Phase 4 final proof test: $@"

$(TEST_BUILD_DIR)/phase4_jit_final_proof.o: $(TEST_SRC_DIR)/phase4_jit_final_proof.c $(HEADERS)
	@echo "Compiling Phase 4 final proof test $<..."
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DAQL_USE_JIT=1 -c $< -o $@

final_proof_jit: $(TEST_BIN_DIR)/phase4_jit_final_proof

proof_jit: $(TEST_BIN_DIR)/phase4_jit_proof

help:
	@echo "=== AQL MVP1 Makefile Help ==="
	@echo "Available targets:"
	@echo "  mvp1         - Build AQL MVP1 executable (default)"
	@echo "  test_phase1  - Build Phase 1 test executable"
	@echo "  test_phase2  - Build Phase 2 type inference test"
	@echo "  test_phase3  - Build Phase 3 debug system test"
	@echo "  test_phase4  - Build Phase 4 JIT system test"
	@echo "  test_phase4_simple - Build Phase 4 JIT simple test"
	@echo "  proof_jit    - Build Phase 4 JIT proof test"
	@echo "  clean        - Remove build artifacts"
	@echo "  clean_test   - Remove test artifacts only"
	@echo "  test         - Build debug version and run tests"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Debug System targets:"
	@echo "  debug        - Build AQL debug version (aqld)"
	@echo "  release      - Build AQL release version (aql)"
	@echo "  both         - Build both debug and release versions"
	@echo "  dev          - Quick development mode (build debug + test)"
	@echo "  quick-debug  - Build debug and run with -v examples/hello.aql"
	@echo "  benchmark    - Compare performance of debug vs release"
	@echo "  test-debug   - Test debug version only"
	@echo "  test-release - Test release version only"
	@echo "  test-both    - Test both versions"
	@echo ""
	@echo "Test Framework Commands (Release Version):"
	@echo "  alltest      - Run all tests (unit + regression + integration)"
	@echo "  unittest     - Run unit tests only"
	@echo "  rtest        - Run regression tests (fast, production-like)"
	@echo "  itest        - Run integration tests"
	@echo "  quick-test   - Fast regression test (minimal rebuild)"
	@echo "  dev-test     - Development cycle (incremental build + test)"
	@echo "  test-file    - Test single file: make test-file FILE=path/to/test.aql"
	@echo ""
	@echo "Debug Test Commands (Debug Version):"
	@echo "  debug-test   - Run regression tests with debug output"
	@echo "  debug-file   - Debug single file: make debug-file FILE=path/to/test.aql"
	@echo ""
	@echo "Test Utilities:"
	@echo "  coverage     - Generate test coverage report"
	@echo "  clean-test   - Clean test artifacts"
	@echo ""
	@echo "Cleaning Commands:"
	@echo "  clean        - Smart clean (remove executables, keep objects for faster rebuild)"
	@echo "  clean-all    - Complete clean (remove everything, use when headers change)"
	@echo "  clean-bin    - Clean only executables (keep objects)"
	@echo "  clean-test   - Clean only test artifacts"
	@echo ""
	@echo "Output locations:"
	@echo "  Object files: $(BUILD_DIR)/"
	@echo "  Debug exe:    $(TARGET_DEBUG)"
	@echo "  Release exe:  $(TARGET_RELEASE)"
	@echo "  Test files:   $(TEST_BIN_DIR)/"

# ===== 新增调试友好的测试目标 =====

# 开发模式：快速构建调试版本并运行测试
dev: debug
	@echo "🚀 Development mode: building and testing..."
	./$(TARGET_DEBUG) --test

# 快速调试：构建并使用-v参数运行示例  
quick-debug: debug
	@echo "🔍 Quick debug mode..."
	./$(TARGET_DEBUG) -v examples/hello.aql

# 性能对比：同时构建并比较执行时间
benchmark: both
	@echo "⚡ Performance benchmark..."
	@echo "Debug version:"
	time ./$(TARGET_DEBUG) --test
	@echo "\nRelease version:"
	time ./$(TARGET_RELEASE) --test

# 双版本测试
test-both: debug release
	@echo "=== Testing Debug Version ==="
	./$(TARGET_DEBUG) --test
	@echo "\n=== Testing Release Version ==="
	./$(TARGET_RELEASE) --test

# 调试版本测试
test-debug: debug
	@echo "Testing AQL Debug version..."
	./$(TARGET_DEBUG) --test

# 生产版本测试  
test-release: release
	@echo "Testing AQL Release version..."
	./$(TARGET_RELEASE) --test

# 增强清理目标 (已移动到测试框架部分)

# 仅清理可执行文件（保留对象文件，加快重新构建）
clean-bin:
	@echo "Cleaning executables..."
	rm -f $(TARGET_DEBUG) $(TARGET_RELEASE)
	rm -f $(TEST_TARGET) $(TEST_PHASE2_TARGET) $(TEST_PHASE3_TARGET) $(TEST_PHASE4_TARGET)
	@echo "✅ Executables cleaned"

# ===== 测试框架集成 =====

# Test framework source files
TEST_FRAMEWORK_SOURCES = \
    $(TEST_FRAMEWORK_DIR)/test_utils.c \
    $(TEST_FRAMEWORK_DIR)/test_runner.c

# Test framework object files
TEST_FRAMEWORK_OBJS = $(TEST_FRAMEWORK_SOURCES:$(TEST_FRAMEWORK_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)

# Test framework compilation flags
TEST_CFLAGS = $(DEBUG_CFLAGS) -I$(TEST_FRAMEWORK_DIR)

# Build test framework objects
$(TEST_BUILD_DIR)/%.o: $(TEST_FRAMEWORK_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling test framework: $<"
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Build test runner (standalone, only depends on test framework sources)
$(TEST_RUNNER): $(TEST_FRAMEWORK_OBJS)
	@mkdir -p $(dir $@)
	@echo "Linking test runner (standalone)..."
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(LDFLAGS)

# ===== 主要测试命令 =====

# 执行所有测试 (All Tests)
.PHONY: alltest
alltest: unittest rtest itest
	@echo "========================================="
	@echo "All tests completed!"
	@echo "========================================="

# 执行单元测试 (Unit Tests)
.PHONY: unittest
unittest: $(TEST_RUNNER)
	@echo "========================================="
	@echo "Running Unit Tests..."
	@echo "========================================="
	@$(TEST_RUNNER) unit $(TEST_DIR)/unit
	@echo ""

# 执行回归测试 (Regression Tests) - 默认使用release版本
.PHONY: rtest
rtest: $(TEST_RUNNER) $(TARGET_RELEASE)
	@echo "========================================="
	@echo "Running Regression Tests (Release)..."
	@echo "========================================="
	@$(TEST_RUNNER) regression $(TEST_DIR)/regression $(TARGET_RELEASE)
	@echo ""

# 执行集成测试 (Integration Tests) - 默认使用release版本
.PHONY: itest
itest: $(TEST_RUNNER) $(TARGET_RELEASE)
	@echo "========================================="
	@echo "Running Integration Tests (Release)..."
	@echo "========================================="
	@$(TEST_RUNNER) integration $(TEST_DIR)/integration $(TARGET_RELEASE)
	@echo ""

# 测试覆盖率
.PHONY: coverage
coverage: CFLAGS += --coverage
coverage: alltest
	@echo "Generating coverage report..."
	@gcov $(SRC_DIR)/*.c
	@lcov --capture --directory . --output-file coverage.info 2>/dev/null || true
	@genhtml coverage.info --output-directory coverage_html 2>/dev/null || true
	@echo "Coverage report generated in coverage_html/"

# 清理测试文件
.PHONY: clean-test
clean-test:
	@echo "Cleaning test artifacts..."
	rm -rf $(TEST_BUILD_DIR) $(TEST_BIN_DIR)
	rm -f coverage.info
	rm -rf coverage_html
	@echo "✅ Test artifacts cleaned"


# 快速测试：跳过不必要的重新编译
.PHONY: quick-test
quick-test: $(TEST_RUNNER) $(TARGET_RELEASE)
	@echo "🚀 Quick regression test (minimal rebuild)..."
	@$(TEST_RUNNER) regression $(TEST_DIR)/regression $(TARGET_RELEASE)

# 开发者友好的增量构建测试
.PHONY: dev-test
dev-test:
	@echo "🔧 Development test cycle..."
	@echo "1. Checking if rebuild needed..."
	@$(MAKE) --no-print-directory $(TARGET_RELEASE)
	@echo "2. Running regression tests..."
	@$(MAKE) --no-print-directory quick-test
	@echo "✅ Development test cycle complete!"

# 调试测试：使用debug版本进行详细调试
.PHONY: debug-test
debug-test: $(TEST_RUNNER) $(TARGET_DEBUG)
	@echo "🐛 Debug regression test (with verbose output)..."
	@$(TEST_RUNNER) regression $(TEST_DIR)/regression $(TARGET_DEBUG)

# 调试单个测试文件
.PHONY: debug-file
debug-file:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make debug-file FILE=path/to/test.aql"; \
		exit 1; \
	fi
	@echo "🎯 Debug testing single file: $(FILE)"
	@$(MAKE) --no-print-directory $(TARGET_DEBUG)
	@./$(TARGET_DEBUG) $(FILE)

# 仅测试特定文件（使用release版本）
.PHONY: test-file
test-file:
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make test-file FILE=path/to/test.aql"; \
		exit 1; \
	fi
	@echo "🎯 Testing single file: $(FILE)"
	@$(MAKE) --no-print-directory $(TARGET_RELEASE)
	@./$(TARGET_RELEASE) $(FILE)

# Complete clean: remove everything (use when headers change or for fresh build)
.PHONY: clean-all
clean-all: clean-test
	@echo "Complete cleaning: removing all build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "✅ Complete clean finished (full rebuild required)"
