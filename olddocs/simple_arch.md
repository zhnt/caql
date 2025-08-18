# AI编程工具架构图 - PlantUML版本

## Mermaid格式

```mermaid
graph TB
    subgraph "用户交互层"
        CLI[CLI命令行工具]
        IDE[IDE插件/扩展]
        WebUI[Web界面]
        API[REST API]
    end
    
    subgraph "核心引擎层"
        Router[请求路由器]
        SessionMgr[会话管理器]
        ContextMgr[上下文管理器]
        AgentCore[AI Agent核心]
    end
    
    subgraph "AI服务层"
        LLMGateway[LLM网关]
        CodeAnalyzer[代码分析器]
        CodeGenerator[代码生成器]
        TestGenerator[测试生成器]
        DocGenerator[文档生成器]
    end
    
    subgraph "工具集成层"
        MCP[Model Context Protocol]
        GitIntegration[Git集成]
        FileSystem[文件系统操作]
        DebugTool[调试工具]
        BuildTool[构建工具]
        PackageMgr[包管理器]
    end
    
    subgraph "数据存储层"
        ProjectDB[项目数据库]
        CodeCache[代码缓存]
        ConversationStore[对话存储]
        ConfigStore[配置存储]
    end
    
    subgraph "外部服务"
        OpenAI[OpenAI/Claude]
        Gemini[Google Gemini]
        LocalLLM[本地LLM]
        GitHub[GitHub API]
        Registry[包注册表]
    end
    
    CLI --> Router
    IDE --> Router
    WebUI --> Router
    API --> Router
    
    Router --> SessionMgr
    Router --> ContextMgr
    Router --> AgentCore
    
    AgentCore --> LLMGateway
    AgentCore --> CodeAnalyzer
    AgentCore --> CodeGenerator
    AgentCore --> TestGenerator
    AgentCore --> DocGenerator
    
    LLMGateway --> OpenAI
    LLMGateway --> Gemini
    LLMGateway --> LocalLLM
    
    AgentCore --> MCP
    AgentCore --> GitIntegration
    AgentCore --> FileSystem
    AgentCore --> DebugTool
    AgentCore --> BuildTool
    AgentCore --> PackageMgr
    
    GitIntegration --> GitHub
    PackageMgr --> Registry
    
    SessionMgr --> ProjectDB
    ContextMgr --> CodeCache
    AgentCore --> ConversationStore
    Router --> ConfigStore
```

## PlantUML格式

```plantuml
@startuml AI编程工具架构图

!define LIGHTBLUE #E6F3FF
!define LIGHTGREEN #E6FFE6
!define LIGHTYELLOW #FFFACD
!define LIGHTPINK #FFE6F0
!define LIGHTGRAY #F0F0F0
!define LIGHTCYAN #E0FFFF

package "用户交互层" as UI_Layer LIGHTBLUE {
    component [CLI命令行工具] as CLI
    component [IDE插件/扩展] as IDE
    component [Web界面] as WebUI
    component [REST API] as API
}

package "核心引擎层" as Core_Layer LIGHTGREEN {
    component [请求路由器] as Router
    component [会话管理器] as SessionMgr
    component [上下文管理器] as ContextMgr
    component [AI Agent核心] as AgentCore
}

package "AI服务层" as AI_Layer LIGHTYELLOW {
    component [LLM网关] as LLMGateway
    component [代码分析器] as CodeAnalyzer
    component [代码生成器] as CodeGenerator
    component [测试生成器] as TestGenerator
    component [文档生成器] as DocGenerator
}

package "工具集成层" as Tool_Layer LIGHTPINK {
    component [Model Context Protocol] as MCP
    component [Git集成] as GitIntegration
    component [文件系统操作] as FileSystem
    component [调试工具] as DebugTool
    component [构建工具] as BuildTool
    component [包管理器] as PackageMgr
}

package "数据存储层" as Storage_Layer LIGHTGRAY {
    database [项目数据库] as ProjectDB
    database [代码缓存] as CodeCache
    database [对话存储] as ConversationStore
    database [配置存储] as ConfigStore
}

package "外部服务" as External_Layer LIGHTCYAN {
    cloud [OpenAI/Claude] as OpenAI
    cloud [Google Gemini] as Gemini
    cloud [本地LLM] as LocalLLM
    cloud [GitHub API] as GitHub
    cloud [包注册表] as Registry
}

' 用户交互层到核心引擎层的连接
CLI --> Router
IDE --> Router
WebUI --> Router
API --> Router

' 核心引擎层内部连接
Router --> SessionMgr
Router --> ContextMgr
Router --> AgentCore

' AI Agent核心到AI服务层
AgentCore --> LLMGateway
AgentCore --> CodeAnalyzer
AgentCore --> CodeGenerator
AgentCore --> TestGenerator
AgentCore --> DocGenerator

' LLM网关到外部AI服务
LLMGateway --> OpenAI
LLMGateway --> Gemini
LLMGateway --> LocalLLM

' AI Agent核心到工具集成层
AgentCore --> MCP
AgentCore --> GitIntegration
AgentCore --> FileSystem
AgentCore --> DebugTool
AgentCore --> BuildTool
AgentCore --> PackageMgr

' 工具集成层到外部服务
GitIntegration --> GitHub
PackageMgr --> Registry

' 核心引擎层到数据存储层
SessionMgr --> ProjectDB
ContextMgr --> CodeCache
AgentCore --> ConversationStore
Router --> ConfigStore

@enduml
```

## C4架构图格式 (PlantUML C4扩展)

```plantuml
@startuml
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Container.puml

title AI编程工具系统架构图

Person(user, "开发者", "使用AI编程工具进行开发")

System_Boundary(ai_coding_system, "AI编程工具系统") {
    Container_Boundary(ui_layer, "用户交互层") {
        Container(cli, "CLI工具", "命令行", "命令行交互界面")
        Container(ide_plugin, "IDE插件", "VS Code/JetBrains", "编辑器集成插件")
        Container(web_ui, "Web界面", "React/Vue", "浏览器端界面")
        Container(rest_api, "REST API", "HTTP/JSON", "第三方集成接口")
    }
    
    Container_Boundary(core_layer, "核心引擎层") {
        Container(router, "请求路由器", "Go/Node.js", "请求分发与路由")
        Container(session_mgr, "会话管理器", "Redis/Memory", "用户会话状态管理")
        Container(context_mgr, "上下文管理器", "Vector DB", "代码上下文处理")
        Container(agent_core, "AI Agent核心", "Python/Go", "智能代理控制器")
    }
    
    Container_Boundary(ai_layer, "AI服务层") {
        Container(llm_gateway, "LLM网关", "Python", "大语言模型接入")
        Container(code_analyzer, "代码分析器", "Tree-sitter", "代码结构分析")
        Container(code_generator, "代码生成器", "Template Engine", "智能代码生成")
        Container(test_generator, "测试生成器", "Jest/PyTest", "测试代码生成")
        Container(doc_generator, "文档生成器", "Markdown", "文档自动生成")
    }
    
    Container_Boundary(tool_layer, "工具集成层") {
        Container(mcp, "MCP协议", "JSON-RPC", "模型上下文协议")
        Container(git_integration, "Git集成", "Git CLI", "版本控制集成")
        Container(file_system, "文件系统", "OS API", "文件操作")
        Container(debug_tool, "调试工具", "DAP", "调试功能集成")
        Container(build_tool, "构建工具", "Make/Gradle", "项目构建")
        Container(package_mgr, "包管理器", "npm/pip", "依赖管理")
    }
    
    Container_Boundary(storage_layer, "数据存储层") {
        ContainerDb(project_db, "项目数据库", "PostgreSQL", "项目元数据")
        ContainerDb(code_cache, "代码缓存", "Redis", "代码分析缓存")
        ContainerDb(conversation_store, "对话存储", "MongoDB", "用户对话历史")
        ContainerDb(config_store, "配置存储", "JSON/YAML", "系统配置")
    }
}

System_Boundary(external_systems, "外部服务") {
    System_Ext(openai, "OpenAI/Claude", "大语言模型API")
    System_Ext(gemini, "Google Gemini", "Google AI模型")
    System_Ext(local_llm, "本地LLM", "自部署模型")
    System_Ext(github, "GitHub API", "代码仓库服务")
    System_Ext(registry, "包注册表", "npm/PyPI等")
}

' 关系连接
Rel(user, cli, "使用")
Rel(user, ide_plugin, "使用")
Rel(user, web_ui, "使用")

Rel(cli, router, "HTTP/gRPC")
Rel(ide_plugin, router, "HTTP/gRPC")
Rel(web_ui, router, "HTTP/REST")
Rel(rest_api, router, "HTTP/REST")

Rel(router, session_mgr, "管理会话")
Rel(router, context_mgr, "获取上下文")
Rel(router, agent_core, "转发请求")

Rel(agent_core, llm_gateway, "调用AI服务")
Rel(agent_core, code_analyzer, "分析代码")
Rel(agent_core, code_generator, "生成代码")
Rel(agent_core, test_generator, "生成测试")
Rel(agent_core, doc_generator, "生成文档")

Rel(llm_gateway, openai, "API调用")
Rel(llm_gateway, gemini, "API调用")
Rel(llm_gateway, local_llm, "本地调用")

Rel(agent_core, mcp, "协议通信")
Rel(agent_core, git_integration, "Git操作")
Rel(agent_core, file_system, "文件操作")
Rel(agent_core, debug_tool, "调试功能")
Rel(agent_core, build_tool, "构建项目")
Rel(agent_core, package_mgr, "包管理")

Rel(git_integration, github, "API调用")
Rel(package_mgr, registry, "包查询")

Rel(session_mgr, project_db, "存储/读取")
Rel(context_mgr, code_cache, "缓存/读取")
Rel(agent_core, conversation_store, "存储对话")
Rel(router, config_store, "读取配置")

@enduml
```

## 系统交互时序图

```plantuml
@startuml AI编程工具交互时序图

actor "开发者" as User
participant "IDE插件" as IDE
participant "请求路由器" as Router
participant "AI Agent核心" as Agent
participant "代码分析器" as Analyzer
participant "LLM网关" as Gateway
participant "OpenAI/Claude" as LLM
participant "Git集成" as Git
participant "代码生成器" as Generator

User -> IDE: 请求代码补全
IDE -> Router: 发送补全请求
Router -> Agent: 转发请求
Agent -> Analyzer: 分析当前代码上下文
Analyzer -> Agent: 返回代码结构信息
Agent -> Gateway: 请求AI模型推理
Gateway -> LLM: 调用语言模型API
LLM -> Gateway: 返回补全建议
Gateway -> Agent: 返回AI响应
Agent -> Generator: 格式化代码建议
Generator -> Agent: 返回格式化代码
Agent -> Router: 返回最终结果
Router -> IDE: 返回代码补全
IDE -> User: 显示补全建议

User -> IDE: 提交代码变更
IDE -> Router: 发送提交请求
Router -> Agent: 处理提交请求
Agent -> Git: 执行Git操作
Git -> Agent: 返回操作结果
Agent -> Router: 返回提交结果
Router -> IDE: 确认提交成功
IDE -> User: 显示提交状态

@enduml
```

## 部署架构图

```plantuml
@startuml AI编程工具部署架构

!define LIGHTBLUE #E1F5FE
!define LIGHTGREEN #E8F5E8
!define LIGHTYELLOW #FFF8E1
!define LIGHTPINK #FCE4EC

cloud "客户端环境" {
    node "开发者机器" as DevMachine {
        component [VS Code插件] as VSCode
        component [CLI工具] as CLI
        component [本地Git] as LocalGit
    }
}

cloud "容器化部署环境" as K8s {
    node "Web层" as WebTier {
        component [负载均衡器] as LB
        component [API网关] as Gateway
        component [Web服务] as WebService
    }
    
    node "应用层" as AppTier {
        component [Agent服务集群] as AgentCluster
        component [AI服务集群] as AICluster
        component [工具服务集群] as ToolCluster
    }
    
    node "数据层" as DataTier {
        database [PostgreSQL集群] as PostgreSQL
        database [Redis集群] as Redis
        database [MongoDB集群] as MongoDB
        storage [对象存储] as ObjectStorage
    }
}

cloud "外部服务" as External {
    component [OpenAI API] as OpenAI
    component [GitHub API] as GitHub
    component [包注册表] as Registry
}

cloud "监控运维" as Monitoring {
    component [Prometheus] as Prometheus
    component [Grafana] as Grafana
    component [ELK Stack] as ELK
    component [Jaeger] as Jaeger
}

' 连接关系
VSCode --> LB : HTTPS
CLI --> LB : HTTPS
LB --> Gateway : HTTP
Gateway --> WebService : HTTP
WebService --> AgentCluster : gRPC
AgentCluster --> AICluster : HTTP
AgentCluster --> ToolCluster : HTTP
AgentCluster --> PostgreSQL : SQL
AgentCluster --> Redis : TCP
AgentCluster --> MongoDB : TCP
AICluster --> OpenAI : HTTPS
ToolCluster --> GitHub : HTTPS
ToolCluster --> Registry : HTTPS

' 监控连接
AgentCluster --> Prometheus : Metrics
AICluster --> ELK : Logs
ToolCluster --> Jaeger : Traces
Prometheus --> Grafana : Query

@enduml
```
```

这份文档包含了四种不同格式的架构图：

1. **Mermaid格式** - 适用于GitHub、GitLab等平台的README文档
2. **标准PlantUML格式** - 经典的PlantUML语法，可以在各种支持PlantUML的工具中使用
3. **C4架构图格式** - 使用C4模型的PlantUML扩展，更适合展示系统架构
4. **时序图和部署图** - 补充的交互流程和部署架构图

每种格式都有其适用场景，你可以根据需要选择使用。所有图表都可以直接在支持相应格式的Markdown环境中渲染显示。