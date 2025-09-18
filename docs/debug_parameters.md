# AQL Debug Parameters Design

## 1. Overview

AQL provides two executable versions to meet different usage requirements:

- **`aql`** - Production Version: Clean, high-performance, zero debug overhead
- **`aqld`** - Debug Version: Complete debugging information, performance analysis, development tools

The debug system uses two parameter categories: Display parameters (`-v*`) show information and continue execution, while Stop parameters (`-s*`) show information and exit early at specific stages.

## 2. Version Positioning

### 2.1 AQL Production Version (`aql`)

**Design Goals:**
- 🚀 **High Performance**: Zero debug overhead, optimized execution
- 🎯 **Simplicity**: Minimal command-line options, focus on core functionality  
- 📦 **Lightweight**: Smaller binary size, faster startup time
- 🔒 **Stability**: Production-ready with robust error handling

**Use Cases:**
- Production deployment
- Performance-critical applications
- Batch processing tasks
- Embedded systems
- CI/CD pipelines

**Available Parameters:**
```bash
aql [OPTIONS] [FILE]

Basic Options:
  -h, --help         Show help information
  -v, --version      Show version information
  -i, --interactive  Start interactive mode (REPL)
  -e "code"          Execute code string directly
  
Performance Options:
  --jit-off          Disable JIT compilation
  --jit-force        Force JIT compilation
  --opt-level=N      Optimization level (0-3, default: 2)
```

### 2.2 AQLD Debug Version (`aqld`)

**Design Goals:**
- 🔍 **Comprehensive Debugging**: Complete compilation and runtime diagnostics
- 🛠️ **Developer Friendly**: Rich development tools and analysis features
- 📊 **Performance Analysis**: Detailed performance monitoring and optimization suggestions
- 🎓 **Educational Tool**: Perfect for learning compiler principles

**Use Cases:**
- Development and debugging
- Performance analysis and optimization
- Compiler development
- Teaching and learning
- Code quality analysis

**Available Parameters:**
All `aql` parameters plus extensive debug options (see sections below).

## 3. Debug Parameter Categories (AQLD Only)

### 3.1 Display Parameters (Verbose Mode)

These parameters show debug information but allow the program to execute normally to completion.

| Parameter | Description | Information Shown |
|-----------|-------------|-------------------|
| `-v` | Show all debug information | Lexical + AST + Bytecode + VM execution + Register state |
| `-vt` | Show lexical analysis | Token stream with types, values, and positions |
| `-va` | Show AST parsing | Abstract syntax tree structure and nodes |
| `-vb` | Show bytecode generation | Generated instructions and constant pool |
| `-ve` | Show VM execution | Virtual machine execution trace |
| `-vr` | Show register state | Register contents during execution |
| `-vm` | Show memory management | Memory allocation and garbage collection |
| `-vg` | Show garbage collection | GC cycles and object cleanup |
| `-vd` | Show debug info | Detailed debugging information |

### 3.2 Stop Parameters (Early Exit Mode)

These parameters show debug information and exit immediately after the specified compilation stage.

| Parameter | Description | Stages Shown | Exit Point |
|-----------|-------------|---------------|------------|
| `-st` | Stop after lexical analysis | Lexical | After tokenization |
| `-sa` | Stop after AST parsing | Lexical + AST | After syntax analysis |
| `-sb` | Stop after bytecode generation | Lexical + AST + Bytecode | After code generation |

## 4. Design Principles

### 4.1 Performance Separation

**Production Version (`aql`):**
- No debug code compilation
- No runtime overhead
- Optimized for speed and size
- Minimal feature set

**Debug Version (`aqld`):**
- Full debug instrumentation
- Rich diagnostic capabilities
- Educational and development focus
- Comprehensive tooling

### 4.2 Logical Naming Convention

- **`-v*`** prefix indicates **V**erbose mode (display + continue)
- **`-s*`** prefix indicates **S**top mode (display + exit)
- Suffix letters maintain consistency:
  - `t` = **T**okens (lexical analysis)
  - `a` = **A**ST (syntax analysis)  
  - `b` = **B**ytecode (code generation)
  - `e` = **E**xecution (virtual machine)
  - `r` = **R**egisters (runtime state)

### 4.3 User Experience Focus

**Intuitive Usage:**
```bash
# Quick lexical check (debug version)
aqld -st test.aql

# View compilation process (debug version)
aqld -vt test.aql

# Production execution (production version)
aql test.aql
```

### 4.4 Backward Compatibility

- All existing `-v` behavior remains unchanged
- New `-s*` parameters are pure additions
- No breaking changes to current workflows

## 5. Usage Examples

### 5.1 Development Workflow

```bash
# Step 1: Quick lexical analysis check
aqld -st fibonacci.aql

# Step 2: Verify AST structure  
aqld -sa fibonacci.aql

# Step 3: Check bytecode generation
aqld -sb fibonacci.aql

# Step 4: Full debug run
aqld -v fibonacci.aql

# Step 5: Production deployment
aql fibonacci.aql
```

### 5.2 Educational Use Cases

```bash
# Teaching compiler phases
aqld -st hello.aql     # "This is lexical analysis"
aqld -sa hello.aql     # "This is syntax analysis"  
aqld -sb hello.aql     # "This is code generation"
aqld -v hello.aql      # "This is the complete process"
```

### 5.3 Performance Analysis

```bash
# Debug version for analysis
aqld -v -vr -vm performance_test.aql

# Production version for benchmarking  
time aql performance_test.aql
```

### 5.4 CI/CD Integration

```bash
# Development builds (with debug info)
aqld -sb src/main.aql > build/debug.bc

# Production builds (optimized)
aql --opt-level=3 src/main.aql > build/release.out
```

## 6. Output Format Examples

### 6.1 Lexical Analysis Output (`aqld -st`)

```
🔍 === LEXICAL ANALYSIS (Tokens) ===
   0: LET          value="let" (line 1, col 1)
   1: NAME         value="fibonacci" (line 1, col 5)
   2: ASSIGN       value="=" (line 1, col 15)
   3: NUMBER       value=42 (line 1, col 17)
   4: EOF          value=<EOF> (line 1, col 19)

📊 Total tokens: 5
✅ Lexical analysis completed successfully
```

### 6.2 AST Analysis Output (`aqld -sa`)

```
🔍 === ABSTRACT SYNTAX TREE (AST) ===
LetDeclaration: fibonacci (1:1-1:19)
├── Variable: fibonacci (type: inferred)
└── Initializer: IntegerLiteral(42) (1:17-1:19)

📊 AST Statistics:
  Total nodes: 3
  Max depth: 2
✅ AST parsing completed successfully
```

### 6.3 Bytecode Analysis Output (`aqld -sb`)

```
🔍 === BYTECODE INSTRUCTIONS ===
📦 Constants Pool:
  CONST[0] = "fibonacci"
  CONST[1] = 42

📝 Instructions:
  PC    OPCODE       A        B        C       
  ---   ------       -        -        -       
  0     LOADI        0        42       0         # R(0) := 42
  1     SETTABUP     0        128      0         # fibonacci = R(0)
  2     RET_VOID     0        0        0         # return

📊 Total instructions: 3
✅ Bytecode generation completed successfully
```

## 7. Implementation Strategy

### 7.1 Build System

```makefile
# Makefile targets
all: aql aqld

# Production version (optimized, no debug)
aql: $(RELEASE_OBJS)
	$(CC) $(RELEASE_OBJS) -o bin/aql $(RELEASE_LDFLAGS)

# Debug version (debug info, instrumentation)  
aqld: $(DEBUG_OBJS)
	$(CC) $(DEBUG_OBJS) -o bin/aqld $(DEBUG_LDFLAGS)

# Separate object files for each version
build/release/%.o: src/%.c
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

build/debug/%.o: src/%.c  
	$(CC) $(DEBUG_CFLAGS) -c $< -o $@
```

### 7.2 Conditional Compilation

```c
// Debug-only features
#ifdef AQL_DEBUG_BUILD
  // Token collection, AST visualization, etc.
  void collect_debug_info(...);
#else
  // No-op in production
  #define collect_debug_info(...) ((void)0)
#endif

// Performance-critical paths
#ifdef AQL_RELEASE_BUILD
  // Aggressive optimizations
  #define INLINE __forceinline
  #define LIKELY(x) __builtin_expect(!!(x), 1)
#else
  // Debug-friendly code
  #define INLINE inline
  #define LIKELY(x) (x)
#endif
```

### 7.3 Parameter Processing

```c
// main.c - Different parameter sets for each version
#ifdef AQL_DEBUG_BUILD
  // Full debug parameter support
  if (strcmp(argv[i], "-st") == 0) {
    debug_flags |= AQL_DEBUG_LEX;
    stop_after_lex = 1;
  }
  // ... all debug parameters
#else
  // Production version - minimal parameters only
  if (strcmp(argv[i], "-v") == 0) {
    printf("AQL Production Version %s\n", AQL_VERSION);
    return 0;
  }
  // Only essential parameters supported
#endif
```

## 8. Benefits of This Design

### 8.1 Clear Separation of Concerns

- **Production users** get a clean, fast tool
- **Developers** get comprehensive debugging capabilities
- **No confusion** about which version to use when

### 8.2 Performance Optimization

- **Zero overhead** in production builds
- **Full instrumentation** available when needed
- **Binary size optimization** for deployment

### 8.3 Development Efficiency

- **Rapid iteration** with debug version
- **Confidence in production** with optimized version
- **Educational value** with detailed output

### 8.4 Maintenance Benefits

- **Separate test suites** for each version
- **Clear feature boundaries** 
- **Easier debugging** of the compiler itself

## 9. Migration Guide

### 9.1 For Existing Users

```bash
# Old usage (still works)
aql -v test.aql

# New recommended usage
aqld -v test.aql    # For development
aql test.aql        # For production
```

### 9.2 For CI/CD Systems

```yaml
# Development pipeline
- name: Debug Analysis
  run: aqld -sb src/main.aql

# Production pipeline  
- name: Production Build
  run: aql --opt-level=3 src/main.aql
```

## 10. Future Extensions

### 10.1 Additional Debug Features (AQLD)

- Interactive debugger (`aqld --debug`)
- Performance profiler (`aqld --profile`)
- Memory analyzer (`aqld --memory`)
- AI workload analysis (`aqld --ai-trace`)

### 10.2 Production Optimizations (AQL)

- Link-time optimization
- Profile-guided optimization
- Cross-compilation support
- Minimal runtime embedding

## 11. Conclusion

This dual-version approach provides:

1. **Production Excellence**: `aql` delivers optimal performance for real-world usage
2. **Development Power**: `aqld` provides comprehensive tooling for development and learning
3. **Clear Purpose**: Each version has a distinct, well-defined role
4. **User Choice**: Developers can choose the right tool for their specific needs

The design ensures AQL remains both a powerful development platform and a production-ready language implementation.