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

# Target executables
TARGET_RELEASE = $(BIN_DIR)/aql
TARGET_DEBUG = $(BIN_DIR)/aqld

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

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "✅ Clean complete"

# Test target (run the executable)
test: mvp1
	@echo "Running AQL MVP1 test..."
	./$(TARGET)

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

# Debug info
info:
	@echo "=== AQL MVP1 Build Configuration ==="
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SRC_DIR: $(SRC_DIR)"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "BIN_DIR: $(BIN_DIR)"
	@echo "TARGET: $(TARGET)"
	@echo ""
	@echo "Core sources:"
	@echo "$(CORE_SOURCES)" | tr ' ' '\n'
	@echo ""
	@echo "Problem sources (excluded):"
	@echo "$(PROBLEM_SOURCES)" | tr ' ' '\n'

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
	@echo "  test         - Build and run AQL MVP1"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Output locations:"
	@echo "  Object files: $(BUILD_DIR)/"
	@echo "  Executable:   $(TARGET)"
	@echo "  Test files:   $(TEST_BIN_DIR)/"