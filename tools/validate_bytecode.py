#!/usr/bin/env python3
"""
AQL字节码验证脚本
检查.by文件是否遵循最佳实践
"""

import re
import sys
import os
from pathlib import Path

class BytecodeValidator:
    def __init__(self):
        self.errors = []
        self.warnings = []
        
    def validate_file(self, filepath):
        """验证单个.by文件"""
        self.errors = []
        self.warnings = []
        
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        for i, line in enumerate(lines, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
                
            self._check_add_instruction(line, i)
            self._check_mmbin_instruction(line, i)
            self._check_jmp_instruction(line, i)
            self._check_call_instruction(line, i)
            
        return len(self.errors) == 0
        
    def _check_add_instruction(self, line, line_num):
        """检查ADD指令是否正确使用"""
        # 匹配 ADD Rx, Ry, Kz 模式（错误）
        if re.match(r'ADD\s+R\d+,\s*R\d+,\s*K\d+', line):
            self.errors.append(f"行{line_num}: ADD指令不能直接使用常量 '{line}' - 应该先LOADK")
            
    def _check_mmbin_instruction(self, line, line_num):
        """检查MMBIN指令参数是否正确"""
        # MMBIN应该对结果寄存器进行绑定
        mmbin_match = re.match(r'MMBIN\s+([RK]\d+),\s*([RK]\d+),\s*(\d+)', line)
        if mmbin_match:
            first_arg = mmbin_match.group(1)
            if first_arg.startswith('K'):
                self.errors.append(f"行{line_num}: MMBIN第一个参数不应该是常量 '{line}'")
                
    def _check_jmp_instruction(self, line, line_num):
        """检查JMP指令格式"""
        # JMP应该只有一个参数
        if re.match(r'JMP\s+\d+,\s*\d+', line):
            self.errors.append(f"行{line_num}: JMP指令只应该有一个参数 '{line}' - 使用 'JMP offset'")
            
    def _check_call_instruction(self, line, line_num):
        """检查CALL指令参数"""
        # 这里可以添加更复杂的CALL指令验证
        pass
        
    def print_results(self, filepath):
        """打印验证结果"""
        print(f"📋 验证文件: {filepath}")
        
        if not self.errors and not self.warnings:
            print("✅ 验证通过！")
            return True
            
        if self.errors:
            print("❌ 发现错误:")
            for error in self.errors:
                print(f"  {error}")
                
        if self.warnings:
            print("⚠️  警告:")
            for warning in self.warnings:
                print(f"  {warning}")
                
        return len(self.errors) == 0

def main():
    if len(sys.argv) < 2:
        print("用法: python3 validate_bytecode.py <file.by> [file2.by ...]")
        print("或者: python3 validate_bytecode.py <directory>")
        return 1
        
    validator = BytecodeValidator()
    all_passed = True
    
    for arg in sys.argv[1:]:
        path = Path(arg)
        
        if path.is_file() and path.suffix == '.by':
            files = [path]
        elif path.is_dir():
            files = list(path.rglob('*.by'))
        else:
            print(f"❌ 无效路径: {arg}")
            continue
            
        for file_path in files:
            passed = validator.validate_file(file_path)
            validator.print_results(file_path)
            print()
            
            if not passed:
                all_passed = False
                
    return 0 if all_passed else 1

if __name__ == '__main__':
    sys.exit(main())

