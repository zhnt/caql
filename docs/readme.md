# AQL OAP 范式设计文档

## 📚 文档概览

本目录包含了 AQL (AI Query Language) OAP 范式设计的完整文档集合：

### 核心设计文档

1. **[AQL OAP 范式设计文档](aql_oap_paradigm_design.md)**
   - AQL 面向AI编程的核心范式设计
   - Agent 构造体、原生AI集成、智能工作流
   - 语言架构、类型系统、性能规范
   - 实现路线图和生态系统规划

2. **[AQL 自进化语言生成架构设计](aql_self_evolution_architecture.md)**
   - 自进化机制的技术架构
   - 元模型系统、代码生成引擎、进化策略引擎
   - 性能监控与优化系统
   - 技术挑战与解决方案

3. **[AQL 自进化机制实现示例](aql_self_evolution_examples.md)**
   - 具体的代码实现示例
   - 元模型系统实现、代码生成引擎实现
   - 进化策略引擎实现、性能监控系统实现
   - 完整工作流示例

4. **[AQL OAP 范式设计总结](aql_oap_summary.md)**
   - 所有设计文档的整合总结
   - 核心愿景、架构、特性、规范
   - 实现路线图、应用场景、未来展望

## 🎯 核心概念

### OAP 范式 (Ontology/AI Programming)
- **从 OOP 到 OAP**：面向对象编程到面向AI编程的范式革命
- **AI-First**：AI 能力作为语言的一等公民
- **自进化**：语言能够自我改进和扩展

### Agent 构造体
```aql
agent DataScientist {
    entity Dataset { /* 数据模型 */ }
    role Analyst { /* 行为模式 */ }
    task AnalyzeData { /* 执行逻辑 */ }
    artifact Report { /* 输出产物 */ }
}
```

### 原生 AI 集成
- **LLM 集成**：`llm! "分析数据" -> Insights`
- **MCP 协议**：`mcp! "database_query" -> Result`
- **A2A 通信**：`a2a! "Agent.Task" -> Result`
- **HTTP 调用**：`http! "POST" -> Response`

## 🚀 技术优势

- **高性能**：比 Python 快 3-10 倍，比 Go 快 1.5-3 倍
- **类型安全**：编译时类型检查，运行时零开销
- **原生集成**：AI 能力原生集成，不是包装
- **自进化**：语言能够自我改进和优化

## 📖 阅读建议

1. **初学者**：从 [AQL OAP 范式设计总结](aql_oap_summary.md) 开始
2. **架构师**：重点关注 [AQL OAP 范式设计文档](aql_oap_paradigm_design.md)
3. **开发者**：参考 [AQL 自进化机制实现示例](aql_self_evolution_examples.md)
4. **技术专家**：深入研究 [AQL 自进化语言生成架构设计](aql_self_evolution_architecture.md)

## 🔗 相关文档

- [AQL 需求文档](../req.md) - 项目需求规范
- [Lua 5.5 对齐原则](lua55-alignment.md) - VM 基线与 AQL 扩展边界
- [栈帧设计文档](stackframe-design.md) - VM 栈帧设计

---

**AQL 将成为 AI 时代最重要的编程语言之一，推动整个编程范式的发展！** 🚀
