#!/usr/bin/env python3
import sys
sys.setrecursionlimit(15000)

def sum_recursive(n):
    if n <= 0:
        return 0
    return n + sum_recursive(n - 1)

if __name__ == "__main__":
    result = sum_recursive(10000)
    print(result)
