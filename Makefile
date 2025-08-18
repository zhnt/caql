# Makefile for AQL MVP1
# Target: mvp1
# Build directory: build/
# Binary directory: bin/

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -I./src
LDFLAGS = -lm

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/aql

# Core source files (working modules)
CORE_SOURCES = \
    $(SRC_DIR)/aconf.c \
    $(SRC_DIR)/aopcodes.c \
    $(SRC_DIR)/aobject.c \
    $(SRC_DIR)/amem.c \
    $(SRC_DIR)/alex.c \
    $(SRC_DIR)/aarray.c \
    $(SRC_DIR)/aslice.c \
    $(SRC_DIR)/adict.c \
    $(SRC_DIR)/avector.c \
    $(SRC_DIR)/adatatype.c \
    $(SRC_DIR)/avm.c \
    $(SRC_DIR)/astate.c \
    $(SRC_DIR)/aql.c \
    $(SRC_DIR)/asimpleparser.c \
    $(SRC_DIR)/main.c

# Object files (replace .c with .o and move to build dir)
CORE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Problematic sources (to be fixed later)
PROBLEM_SOURCES = \
    $(SRC_DIR)/aql.c

# Header dependencies
HEADERS = $(wildcard $(SRC_DIR)/*.h)

# Default target
.PHONY: all mvp1 clean dirs test

all: mvp1

mvp1: dirs $(TARGET)

# Create necessary directories
dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Link the executable
$(TARGET): $(CORE_OBJECTS)
	@echo "Linking AQL MVP1 executable..."
	$(CC) $(CORE_OBJECTS) -o $@ $(LDFLAGS)
	@echo "✅ Successfully built AQL MVP1: $@"

# Compile core source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✅ Clean complete"

# Test target (run the executable)
test: mvp1
	@echo "Running AQL MVP1 test..."
	./$(TARGET)

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
help:
	@echo "=== AQL MVP1 Makefile Help ==="
	@echo "Available targets:"
	@echo "  mvp1     - Build AQL MVP1 executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Build and run AQL MVP1"
	@echo "  info     - Show build configuration"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Output locations:"
	@echo "  Object files: $(BUILD_DIR)/"
	@echo "  Executable:   $(TARGET)"