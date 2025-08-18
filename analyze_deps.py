#!/usr/bin/env python3
"""
Lua/AQL 依赖关系分析工具
分析头文件和源文件的包含关系，生成依赖图和建议
"""

import os
import re
import json
from collections import defaultdict, deque
from pathlib import Path

class DependencyAnalyzer:
    def __init__(self, source_dir):
        self.source_dir = Path(source_dir)
        self.headers = {}  # filename -> {includes: [], content: str}
        self.sources = {}  # filename -> {includes: [], content: str}
        self.dependencies = defaultdict(set)  # file -> set of dependencies
        
    def scan_files(self):
        """扫描所有.h和.c文件"""
        print(f"扫描目录: {self.source_dir}")
        
        for file_path in self.source_dir.glob("*.h"):
            self._analyze_file(file_path, self.headers)
            
        for file_path in self.source_dir.glob("*.c"):
            self._analyze_file(file_path, self.sources)
            
        print(f"找到 {len(self.headers)} 个头文件")
        print(f"找到 {len(self.sources)} 个源文件")
        
    def _analyze_file(self, file_path, storage):
        """分析单个文件的包含关系"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except:
            try:
                with open(file_path, 'r', encoding='latin-1') as f:
                    content = f.read()
            except Exception as e:
                print(f"无法读取文件 {file_path}: {e}")
                return
                
        filename = file_path.name
        includes = self._extract_includes(content)
        
        storage[filename] = {
            'includes': includes,
            'content': content,
            'path': str(file_path)
        }
        
        # 记录依赖关系
        for inc in includes:
            self.dependencies[filename].add(inc)
    
    def _extract_includes(self, content):
        """提取#include指令"""
        includes = []
        # 匹配 #include "xxx.h" 和 #include <xxx.h>
        pattern = r'#include\s+[<"]([^>"]+)[>"]'
        matches = re.findall(pattern, content)
        
        for match in matches:
            # 只关心本地头文件（.h结尾）
            if match.endswith('.h') and not match.startswith('/') and not match.startswith('sys/'):
                includes.append(match)
                
        return includes
    
    def build_dependency_levels(self):
        """构建依赖层级（拓扑排序）"""
        # 只考虑本项目内的文件
        all_files = set(self.headers.keys()) | set(self.sources.keys())
        local_deps = {}
        
        for file, deps in self.dependencies.items():
            if file in all_files:
                local_deps[file] = {d for d in deps if d in all_files}
        
        # 拓扑排序
        levels = []
        remaining = set(all_files)
        
        while remaining:
            # 找到没有依赖的文件（或依赖都已解决）
            no_deps = {f for f in remaining 
                      if not (local_deps.get(f, set()) & remaining)}
            
            if not no_deps:
                # 检测循环依赖
                print("警告：检测到循环依赖!")
                print("剩余文件:", remaining)
                for f in remaining:
                    deps = local_deps.get(f, set()) & remaining
                    if deps:
                        print(f"  {f} -> {deps}")
                break
                
            levels.append(sorted(no_deps))
            remaining -= no_deps
            
        return levels
    
    def analyze_lua(self):
        """分析Lua的架构特点"""
        print("\n=== Lua 架构分析 ===")
        
        # 基础配置文件
        config_files = [f for f in self.headers.keys() 
                       if 'conf' in f.lower() or 'limit' in f.lower()]
        print(f"配置文件: {config_files}")
        
        # 核心对象系统
        object_files = [f for f in self.headers.keys() 
                       if 'object' in f.lower() or 'value' in f.lower()]
        print(f"对象系统: {object_files}")
        
        # 状态管理
        state_files = [f for f in self.headers.keys() 
                      if 'state' in f.lower()]
        print(f"状态管理: {state_files}")
        
        # VM相关
        vm_files = [f for f in self.headers.keys() 
                   if 'vm' in f.lower() or 'code' in f.lower() or 'do' in f.lower()]
        print(f"虚拟机: {vm_files}")
        
        # 分析每个重要文件的依赖
        important = ['lua.h', 'lstate.h', 'lobject.h', 'lvm.h', 'ldo.h']
        for file in important:
            if file in self.headers:
                deps = self.dependencies.get(file, set())
                print(f"\n{file} 依赖:")
                for dep in sorted(deps):
                    print(f"  -> {dep}")
    
    def suggest_aql_architecture(self):
        """基于Lua分析建议AQL架构"""
        print("\n=== AQL 架构建议 ===")
        
        levels = self.build_dependency_levels()
        
        print("\n依赖层级 (从底层到高层):")
        for i, level in enumerate(levels):
            print(f"Level {i}: {level}")
            
        # 基于Lua的模式建议AQL架构
        aql_mapping = {
            'luaconf.h': 'aconf.h - 基础配置，无依赖',
            'llimits.h': 'aconf.h - 基础限制和宏定义',
            'lua.h': 'aql.h - 公共API，依赖aconf.h',
            'lobject.h': 'aobject.h - 对象系统，依赖aql.h',
            'lstate.h': 'astate.h - 状态管理，依赖aobject.h',
            'lmem.h': 'amem.h - 内存管理，依赖astate.h',
            'lvm.h': 'avm.h - 虚拟机，依赖astate.h',
            'ldo.h': 'ado.h - 执行控制，依赖avm.h'
        }
        
        print("\nAQL架构映射建议:")
        for lua_file, aql_desc in aql_mapping.items():
            print(f"  {lua_file:12} -> {aql_desc}")
            
        return levels
    
    def generate_report(self):
        """生成完整的依赖报告"""
        print("\n=== 完整依赖矩阵 ===")
        
        all_files = sorted(set(self.headers.keys()) | set(self.sources.keys()))
        
        # 生成依赖矩阵
        matrix = []
        for file in all_files:
            deps = self.dependencies.get(file, set())
            row = []
            for target in all_files:
                row.append(1 if target in deps else 0)
            matrix.append(row)
            
        # 打印简化的依赖列表
        print("\n文件依赖关系:")
        for file in all_files:
            deps = sorted(self.dependencies.get(file, set()))
            if deps:
                print(f"{file:15} -> {', '.join(deps)}")
                
        return matrix, all_files

def main():
    # 分析Lua
    print("=== 分析 Lua 5.4.8 ===")
    lua_analyzer = DependencyAnalyzer("lua548/src")
    lua_analyzer.scan_files()
    lua_analyzer.analyze_lua()
    levels = lua_analyzer.suggest_aql_architecture()
    lua_analyzer.generate_report()
    
    # 分析当前AQL
    print("\n" + "="*50)
    print("=== 分析当前 AQL ===")
    aql_analyzer = DependencyAnalyzer("src")
    aql_analyzer.scan_files()
    
    print("\n当前AQL依赖关系:")
    aql_levels = aql_analyzer.build_dependency_levels()
    for i, level in enumerate(aql_levels):
        print(f"Level {i}: {level}")
        
    # 生成AQL优化建议
    print("\n=== AQL 优化建议 ===")
    print("基于Lua分析，建议的AQL编译顺序:")
    
    suggested_order = [
        "Level 0: aconf.h (配置基础)",
        "Level 1: aql.h (公共API) -> aconf.h", 
        "Level 2: aobject.h (对象系统) -> aql.h",
        "Level 3: astate.h (状态管理) -> aobject.h",
        "Level 4: amem.h, astring.h, azio.h -> astate.h",
        "Level 5: avm.h, ado.h (VM和执行) -> 上述所有",
        "Level 6: alex.h, aparser.h (词法和语法) -> avm.h",
        "Level 7: 所有.c文件按对应.h的层级顺序"
    ]
    
    for suggestion in suggested_order:
        print(f"  {suggestion}")

if __name__ == "__main__":
    main()