# AQL AI工作流语法设计

## 1. PPT生成场景的形式化描述

### 1.1 意图驱动的声明式语法

```aql
@ai_workflow("ppt_generation")
async fn generate_ppt(topic: string, audience: string) -> PPT {
    // 意图声明：生成PPT的整体目标
    @intent("创建专业的演示文稿，面向${audience}，主题为${topic}")
    
    // 阶段1：生成故事线大纲
    let storyline = ai_generate! {
        @model("gpt-4")
        @prompt_template("storyline") 
        @context(topic, audience)
        
        "为主题'${topic}'创建PPT大纲，目标受众：${audience}
         要求：逻辑清晰，5-8个主要章节，每章节包含2-3个要点"
    } -> Storyline
    
    // 阶段2：并行生成各页面
    let slides = storyline.chapters 
        |> parallel_map(chapter -> {
            generate_slide(chapter, storyline.context)
        })
        |> await_all()
    
    // 阶段3：整合和优化
    let ppt = PPT::new(storyline.title, slides)
        |> ai_optimize! {
            @model("gpt-4")
            "检查PPT整体一致性，优化页面转换，确保逻辑流畅"
        }
    
    return ppt
}

// 单页生成的递归工作流
@ai_workflow("slide_generation") 
async fn generate_slide(chapter: Chapter, context: Context) -> Slide {
    // 页面结构生成
    let structure = ai_generate! {
        @model("gpt-4")
        @temperature(0.7)
        
        "为章节'${chapter.title}'设计页面结构：
         - 主标题
         - 2-3个子标题
         - 每个子标题下的内容要点
         - 需要的图片类型和位置"
    } -> SlideStructure
    
    // 并行生成各部分内容
    let content_parts = structure.sections
        |> parallel_map(section -> {
            // 文字内容生成
            let text = ai_generate! {
                @model("gpt-4")
                @context(chapter, context)
                "为'${section.title}'生成具体内容，要求简洁专业"
            } -> Text
            
            // 图片生成（如果需要）
            let image = when section.needs_image {
                true => ai_generate! {
                    @model("dall-e-3")
                    @style("professional, clean, ${context.style}")
                    "生成图片：${section.image_description}"
                } -> Image
                false => nil
            }
            
            return ContentPart { text, image, layout: section.layout }
        })
        |> await_all()
    
    return Slide {
        title: structure.title,
        subtitle: structure.subtitle,
        parts: content_parts,
        layout: structure.layout
    }
}
```

### 1.2 管道式数据流语法

```aql
@ai_pipeline("ppt_generation_pipeline")
fn generate_ppt_pipeline(topic: string, audience: string) -> PPT {
    topic
        |> ai_analyze!("分析主题深度和复杂度") -> TopicAnalysis
        |> ai_generate!("基于分析结果生成故事线大纲") -> Storyline  
        |> expand!(chapter -> {
            chapter
                |> ai_generate!("生成页面结构") -> SlideStructure
                |> fork! {
                    // 并行分支1：文字内容
                    text_branch: ai_generate!("生成文字内容") -> Text
                    
                    // 并行分支2：图片内容  
                    image_branch: ai_generate!("生成图片描述") -> ImageDesc
                        |> ai_generate!(@model("dall-e-3"), "生成图片") -> Image
                }
                |> merge!(text_branch, image_branch) -> ContentParts
                |> compose! -> Slide
        })
        |> collect! -> slides: slice<Slide>
        |> ai_optimize!("整体优化和一致性检查") -> PPT
}
```

### 1.3 状态机式迭代语法

```aql
@ai_state_machine("iterative_ppt_generation")
state PPTGenerator {
    // 状态定义
    states {
        Initial -> Planning
        Planning -> Outlining  
        Outlining -> ContentGeneration
        ContentGeneration -> Review
        Review -> Refinement | Completed
        Refinement -> ContentGeneration
    }
    
    // 状态转换和动作
    state Planning {
        entry {
            @ai_action("分析需求，制定生成策略")
            let strategy = ai_plan! {
                @context(topic, audience, constraints)
                "制定PPT生成策略，包括风格、结构、重点"
            }
        }
        
        transitions {
            to Outlining when strategy.ready
        }
    }
    
    state Outlining {
        entry {
            let outline = ai_generate! {
                @model("gpt-4")
                @iterate_until(quality_score > 0.8)
                "生成详细大纲"
            }
        }
        
        transitions {
            to ContentGeneration when outline.approved
            to Planning when outline.needs_revision
        }
    }
    
    state ContentGeneration {
        entry {
            // 迭代生成内容
            for chapter in outline.chapters {
                let slide = ai_generate_slide!(chapter)
                    |> ai_review! -> feedback
                    |> when feedback.score < 0.8 {
                        ai_refine!(slide, feedback) -> slide
                    }
                slides.add(slide)
            }
        }
        
        transitions {
            to Review when all_slides_generated
        }
    }
    
    state Review {
        entry {
            let review = ai_review! {
                @model("gpt-4")
                @criteria(["consistency", "flow", "quality"])
                "全面评估PPT质量"
            }
        }
        
        transitions {
            to Completed when review.overall_score > 0.85
            to Refinement when review.needs_improvement
        }
    }
    
    state Refinement {
        entry {
            // 基于反馈优化
            slides = slides
                |> filter!(slide -> review.get_feedback(slide).needs_work)
                |> parallel_map!(slide -> ai_refine!(slide, review.feedback))
        }
        
        transitions {
            to ContentGeneration
        }
    }
}
```

### 1.4 声明式配置语法

```aql
@ai_config
configuration PPTGenerationConfig {
    // 模型配置
    models {
        primary: "gpt-4" {
            temperature: 0.7
            max_tokens: 2000
            context_window: 8000
        }
        
        image: "dall-e-3" {
            style: "professional"
            quality: "hd"
            size: "1024x1024"
        }
        
        review: "gpt-4" {
            temperature: 0.3  // 更保守，适合评估
            max_tokens: 1000
        }
    }
    
    // 质量标准
    quality_criteria {
        content_relevance: weight(0.3)
        logical_flow: weight(0.25) 
        visual_appeal: weight(0.2)
        audience_appropriate: weight(0.25)
        
        minimum_score: 0.8
        max_iterations: 3
    }
    
    // 工作流参数
    workflow {
        max_slides: 20
        parallel_workers: 4
        timeout_per_slide: 120s
        retry_limit: 2
        
        enable_caching: true
        cache_ttl: 1h
    }
    
    // 提示词模板
    prompts {
        storyline: """
        为主题"{topic}"创建PPT大纲，目标受众：{audience}
        
        要求：
        1. 逻辑清晰的5-8个主要章节
        2. 每章节包含2-3个要点
        3. 符合{audience}的知识水平
        4. 总体时长控制在{duration}分钟
        
        输出格式：JSON结构，包含title, chapters数组
        """
        
        slide_content: """
        为章节"{chapter_title}"生成具体内容
        
        上下文：{context}
        目标受众：{audience}
        
        要求：
        1. 内容简洁专业，要点明确
        2. 适合{audience}的理解水平  
        3. 支持演讲者展开说明
        4. 包含必要的数据或例子
        """
    }
}
```

### 1.5 简化的语法糖

```aql
// 最简洁的使用方式
@ai_magic  // 启用AI语法糖
fn quick_ppt(topic: string) -> PPT {
    // 一行生成整个PPT
    ai! "为'${topic}'生成专业PPT，包含大纲、内容和图片" -> PPT
}

// 稍微详细的控制
@ai_magic
fn controlled_ppt(topic: string, audience: string) -> PPT {
    let outline = ai! "为'${topic}'生成面向${audience}的PPT大纲"
    
    let slides = outline |> ai_expand! {
        "为每个章节生成页面，包含标题、内容和配图"
    }
    
    slides |> ai_optimize! "整体优化PPT流畅性和一致性" -> PPT
}

// 交互式生成
@ai_interactive  
fn interactive_ppt(topic: string) -> PPT {
    let outline = ai! "生成PPT大纲" 
        |> user_review!("请检查大纲是否合适")  // 用户确认点
        |> ai_refine! when user.feedback.exists
    
    let slides = outline |> ai_generate_slides!
        |> user_preview!("预览生成的页面")     // 用户预览点
        |> ai_adjust! based_on user.feedback
    
    return PPT::finalize(slides)
}
```

## 2. 语法特性总结

### 2.1 核心语法元素
- **`ai!`**: AI调用的语法糖
- **`|>`**: 管道操作符，数据流传递
- **`|>>`**: 异步管道操作符
- **`parallel_map!`**: 并行处理
- **`await_all()`**: 等待所有异步操作完成
- **`@ai_workflow`**: 工作流装饰器
- **`@intent`**: 意图声明
- **`@model`**: 模型指定

### 2.2 AI专用操作符
- **`ai_generate!`**: AI内容生成
- **`ai_review!`**: AI质量评估
- **`ai_optimize!`**: AI优化改进
- **`ai_refine!`**: AI精细调整
- **`user_review!`**: 用户交互确认

### 2.3 控制流扩展
- **`iterate_until`**: 基于条件的智能迭代
- **`fork!/merge!`**: 并行分支和合并
- **`when/match`**: 智能条件判断
- **状态机**: 复杂流程的状态管理

这套语法让AI工作流的描述变得既简洁又强大，真正实现了"意图驱动"的编程范式！ 