# VM bytecode test helpers

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I../../src -g
LDFLAGS = -lm

ROOT_DIR = ../..
BYTECODE_DIR = bytecode
GENERATED_DIR = $(BYTECODE_DIR)/generated
BYTECODE_GEN = bytecode_gen
AQLM = $(ROOT_DIR)/bin/aqlm
VMTEST = $(ROOT_DIR)/bin/vmtest

.PHONY: all generate-tests vmtest vmtest-verbose clean help status

all: $(BYTECODE_GEN)

$(BYTECODE_GEN): bytecode_gen.c
	@echo "🔨 编译字节码生成器..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✅ 编译完成: $@"

generate-tests: $(BYTECODE_GEN)
	@echo "📝 生成文本字节码测试..."
	@./$(BYTECODE_GEN)
	@echo "✅ 测试文件生成完成"

vmtest: generate-tests
	@echo "🔨 构建 aqlm 与 vmtest runner..."
	@$(MAKE) -C $(ROOT_DIR) --no-print-directory aqlm bin/vmtest
	@echo "🚀 运行 VM 字节码测试..."
	@$(VMTEST) $(GENERATED_DIR) $(AQLM)

vmtest-verbose: generate-tests
	@echo "🔨 构建 aqlm 与 vmtest runner..."
	@$(MAKE) -C $(ROOT_DIR) --no-print-directory aqlm bin/vmtest
	@echo "🚀 运行 VM 字节码测试 (详细模式)..."
	@$(VMTEST) $(GENERATED_DIR) $(AQLM) -v

clean:
	@echo "🧹 清理 VM 测试辅助产物..."
	rm -f $(BYTECODE_GEN)
	@echo "✅ 清理完成"

help:
	@echo "VM 字节码测试辅助 Makefile"
	@echo ""
	@echo "目标:"
	@echo "  all              - 编译文本字节码生成器"
	@echo "  generate-tests   - 生成/更新示例 .by 与 .expected"
	@echo "  vmtest           - 运行生成的 smoke bytecode/generated/"
	@echo "  vmtest-verbose   - 详细模式运行生成的 smoke fixtures"
	@echo "  clean            - 清理本目录生成器"
	@echo "  status           - 显示当前状态"

status:
	@echo "📊 VM 测试状态:"
	@echo "🏭 生成器: $(if $(wildcard $(BYTECODE_GEN)),✅ 已编译,❌ 未编译)"
	@echo "🔧 aqlm: $(if $(wildcard $(AQLM)),✅ 已编译,❌ 未编译)"
	@echo "🧪 vmtest runner: $(if $(wildcard $(VMTEST)),✅ 已编译,❌ 未编译)"
	@echo "📝 文本测试数: $(shell find $(BYTECODE_DIR) -name '*.by' 2>/dev/null | wc -l)"
