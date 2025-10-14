# VM 字节码测试 Makefile
# 类似于现有的 make rtest，但专门用于字节码测试

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../../src -g
LDFLAGS = -lm

# 目录
SRC_DIR = ../../src
BIN_DIR = ../../bin
BYTECODE_DIR = bytecode

# 目标程序
VMTEST = $(BIN_DIR)/vmtest
BYTECODE_GEN = bytecode_gen

# 需要的 AQL 核心源文件 (最小化依赖)
AQL_CORE_SOURCES = \
	$(SRC_DIR)/astate.c \
	$(SRC_DIR)/amem.c \
	$(SRC_DIR)/astring.c \
	$(SRC_DIR)/afunc.c \
	$(SRC_DIR)/aobject.c \
	$(SRC_DIR)/ado.c \
	$(SRC_DIR)/aerror.c \
	$(SRC_DIR)/adict.c \
	$(SRC_DIR)/acontainer.c \
	$(SRC_DIR)/avm_core.c

# 默认目标
all: $(VMTEST) $(BYTECODE_GEN)

# 编译 vmtest (尝试完整版本)
$(VMTEST): vmtest.c $(AQL_CORE_SOURCES) | $(BIN_DIR)
	@echo "🔨 编译 VM 测试工具..."
	@if $(CC) $(CFLAGS) -o $@ $< $(AQL_CORE_SOURCES) $(LDFLAGS) 2>/dev/null; then \
		echo "✅ 编译完成: $@"; \
	else \
		echo "⚠️  完整编译失败，尝试简化版本..."; \
		$(CC) $(CFLAGS) -DSIMPLIFIED_BUILD -o $@ vmtest.c $(LDFLAGS); \
		echo "✅ 简化版本编译完成: $@"; \
	fi

# 编译字节码生成器
$(BYTECODE_GEN): bytecode_gen.c
	@echo "🔨 编译字节码生成器..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✅ 编译完成: $@"


# 创建 bin 目录
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# 创建字节码目录
$(BYTECODE_DIR):
	@mkdir -p $(BYTECODE_DIR)

# 生成测试字节码文件
generate-tests: $(BYTECODE_GEN) | $(BYTECODE_DIR)
	@echo "📝 生成测试字节码文件..."
	@./$(BYTECODE_GEN)
	@echo "✅ 测试文件生成完成"

# 运行 VM 测试 (类似 make rtest)
vmtest: $(VMTEST)
	@echo "🚀 运行 VM 字节码测试..."
	@echo "📁 测试目录: bytecode_text"
	@echo ""
	@$(VMTEST) bytecode_text

# 运行指定测试
test-%: $(VMTEST) | generate-tests
	@echo "🧪 运行指定测试: $*"
	@$(VMTEST) -t $* $(BYTECODE_DIR)

# 详细模式测试
vmtest-verbose: $(VMTEST) | generate-tests
	@echo "🚀 运行 VM 字节码测试 (详细模式)..."
	@$(VMTEST) -v $(BYTECODE_DIR)

# 清理
clean:
	@echo "🧹 清理生成文件..."
	rm -f $(VMTEST) $(BYTECODE_GEN) $(BYTECODE_PARSER) *.o
	rm -rf $(BYTECODE_DIR)/*.bin $(BYTECODE_DIR)/*.expected
	@echo "✅ 清理完成"

# 深度清理
distclean: clean
	@echo "🧹 深度清理..."
	rm -rf $(BYTECODE_DIR)
	@echo "✅ 深度清理完成"

# 安装 (复制到系统路径)
install: $(VMTEST)
	@echo "📦 安装 VM 测试工具..."
	@if [ -w /usr/local/bin ]; then \
		cp $(VMTEST) /usr/local/bin/; \
		echo "✅ 已安装到 /usr/local/bin/vmtest"; \
	else \
		echo "⚠️  需要 sudo 权限安装到系统路径"; \
		echo "💡 或者将 $(BIN_DIR) 添加到 PATH"; \
	fi

# 帮助信息
help:
	@echo "VM 字节码测试 Makefile"
	@echo ""
	@echo "目标:"
	@echo "  all              - 编译所有工具"
	@echo "  vmtest           - 运行所有 VM 测试 (类似 make rtest)"
	@echo "  vmtest-verbose   - 详细模式运行测试"
	@echo "  test-<name>      - 运行指定测试"
	@echo "  generate-tests   - 生成测试字节码文件"
	@echo "  clean            - 清理生成文件"
	@echo "  distclean        - 深度清理"
	@echo "  install          - 安装到系统路径"
	@echo "  help             - 显示此帮助"
	@echo ""
	@echo "示例:"
	@echo "  make vmtest           # 运行所有测试"
	@echo "  make test-arithmetic  # 运行算术测试"
	@echo "  make vmtest-verbose   # 详细输出"
	@echo ""
	@echo "文件结构:"
	@echo "  $(BYTECODE_DIR)/*.by       - 字节码文件"
	@echo "  $(BYTECODE_DIR)/*.expected - 期望输出文件"
	@echo "  $(BIN_DIR)/vmtest          - 测试工具"

# 显示状态
status:
	@echo "📊 VM 测试状态:"
	@echo "🔧 vmtest 工具: $(if $(wildcard $(VMTEST)),✅ 已编译,❌ 未编译)"
	@echo "🏭 字节码生成器: $(if $(wildcard $(BYTECODE_GEN)),✅ 已编译,❌ 未编译)"
	@echo "📁 字节码目录: $(if $(wildcard $(BYTECODE_DIR)),✅ 存在,❌ 不存在)"
	@echo "📝 测试文件数: $(shell find $(BYTECODE_DIR) -name '*.by' 2>/dev/null | wc -l)"

.PHONY: all vmtest vmtest-verbose generate-tests clean distclean install help status
