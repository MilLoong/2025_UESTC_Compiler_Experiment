#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import subprocess
import sys
from difflib import unified_diff

# 颜色代码
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
NC = '\033[0m'

def extract_tests(filename, pattern, is_c_file=False):
    """从文件中提取测试用例"""
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 分割测试用例
    tests = re.split(pattern, content)
    # 移除第一个空元素
    tests = [t.strip() for t in tests if t.strip()]
    
    # 如果是C文件，过滤掉注释行
    if is_c_file:
        cleaned_tests = []
        for test in tests:
            lines = test.split('\n')
            # 过滤掉以 // 或 # 开头的行
            code_lines = [line for line in lines if not line.strip().startswith('//') and not line.strip().startswith('#')]
            cleaned_test = '\n'.join(code_lines).strip()
            if cleaned_test:
                # 只添加非空测试
                cleaned_tests.append(cleaned_test)
        return cleaned_tests
    
    return tests

def normalize_llvm(text):
    """规范化 LLVM IR"""
    lines = text.split('\n')
    normalized = []
    
    for line in lines:
        line = line.strip()
        # 跳过空行和注释
        if not line or line.startswith(';') or line.startswith('#'):
            continue
        # 规范化空白字符
        line = re.sub(r'\s+', ' ', line)
        normalized.append(line)
    
    return '\n'.join(normalized)

def compare_outputs(expected, actual):
    """对比两个输出"""
    exp_norm = normalize_llvm(expected)
    act_norm = normalize_llvm(actual)
    
    return exp_norm == act_norm, exp_norm, act_norm

def main():
    print("测试所有样例")
    print()
    
    # 检查文件
    if not os.path.exists('lab4-example/test.c'):
        print(f"{RED}错误: test.c 不存在{NC}")
        sys.exit(1)
    
    if not os.path.exists('lab4-example/answer.ll'):
        print(f"{RED}错误: answer.ll 不存在{NC}")
        sys.exit(1)
    
    if not os.path.exists('./calc'):
        print(f"{YELLOW}警告: ./calc 不存在{NC}")
        sys.exit(1)
    
    # 提取测试用例
    print("正在提取测试用例...")
    c_tests = extract_tests('lab4-example/test.c', r'// Test \d+:', is_c_file=True)
    expected_tests = extract_tests('lab4-example/answer.ll', r'# Test \d+:', is_c_file=False)
    
    print(f"提取了 {len(c_tests)} 个 C 测试用例")
    print(f"提取了 {len(expected_tests)} 个期望输出")
    print()
    
    if len(c_tests) != len(expected_tests):
        print(f"{YELLOW}警告: 测试数量不匹配{NC}")
    
    # 运行测试
    total = 0
    passed = 0
    failed = 0
    failed_list = []
    
    test_count = min(len(c_tests), len(expected_tests))
    
    for i in range(test_count):
        total += 1
        test_num = i + 1
        
        # 保存测试用例到临时文件
        with open('lab4-example/temp_test.c', 'w', encoding='utf-8') as f:
            f.write(c_tests[i])
        
        # 运行编译器
        try:
            result = subprocess.run(
                ['./calc'],
                stdin=open('lab4-example/temp_test.c', 'r'),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=5,
                encoding='utf-8'
            )
            
            actual_output = result.stdout
            
            # 对比输出
            is_same, exp_norm, act_norm = compare_outputs(
                expected_tests[i], 
                actual_output
            )
            
            if is_same:
                print(f"{GREEN}通过: Test {test_num}{NC}")
                passed += 1
            else:
                print(f"{RED}失败: Test {test_num}{NC}")
                failed += 1
                failed_list.append(test_num)
                
                # 显示差异
                exp_lines = exp_norm.split('\n')
                act_lines = act_norm.split('\n')
                
                print("  期望输出:")
                for line in exp_lines:
                    print(f"    {line}")
                
                print("  实际输出:")
                for line in act_lines:
                    print(f"    {line}")
                print()
        
        except subprocess.TimeoutExpired:
            print(f"{RED}失败, Test {test_num}: 超时{NC}")
            failed += 1
            failed_list.append(test_num)
        except Exception as e:
            print(f"{RED}失败, Test {test_num}: {str(e)}{NC}")
            failed += 1
            failed_list.append(test_num)
    
    # 清理临时文件
    if os.path.exists('lab4-example/temp_test.c'):
        os.remove('lab4-example/temp_test.c')
    
    # 输出统计
    print()
    print("测试结果:")
    print(f"总测试数: {total}")
    print(f"{GREEN}通过: {passed}{NC}")
    print(f"{RED}失败: {failed}{NC}")
    
    if failed > 0:
        print(f"\n失败的测试: {', '.join(map(str, failed_list))}")
    
    if failed == 0:
        print(f"\n{GREEN}所有测试通过 {NC}")
        sys.exit(0)
    else:
        print(f"\n{RED}有测试失败, 请检查{NC}")
        sys.exit(1)

if __name__ == '__main__':
    main()