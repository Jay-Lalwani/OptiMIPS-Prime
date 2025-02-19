#!/usr/bin/env python3
"""
compare_registers.py

This script compares the register states in the last cycle
between pipeline and single-cycle log files for a specific test case.

Usage:
    python compare_registers.py [test_name]
    If test_name is not provided, lists all available tests
"""

import os
import re
import sys

def extract_last_cycle_registers(filepath):
    """
    Extracts register values (a dict mapping register number to value)
    from the final cycle block in the log file.
    
    Assumes that each cycle block begins with a line
      "CYCLE <number>"
    and is followed by several lines of the form:
      "R[<num>]: <value>"
    
    Returns:
        A dict: {register_number: value}
    """
    last_regs = {}
    current_regs = {}
    # regular expressions to match cycle number and register lines
    cycle_re = re.compile(r"^CYCLE\s+(\d+)")
    reg_re = re.compile(r"^R\[(\d+)\]:\s+(-?\d+)")
    
    with open(filepath, "r") as file:
        for line in file:
            line = line.strip()
            # Check for a cycle header.
            cycle_match = cycle_re.match(line)
            if cycle_match:
                # If we already collected register data for a previous cycle,
                # save it as the "last" complete block.
                if current_regs:
                    last_regs = current_regs.copy()
                    current_regs = {}
                continue  # move on to next line
            
            # Check for a register line.
            reg_match = reg_re.match(line)
            if reg_match:
                reg_num = int(reg_match.group(1))
                reg_val = int(reg_match.group(2))
                current_regs[reg_num] = reg_val

    # In case the file did not end with a new cycle header,
    # use the final set of registers read.
    if current_regs:
        last_regs = current_regs.copy()
    return last_regs

def compare_registers(regs_pipeline, regs_single):
    """
    Compares two dictionaries of register values.
    
    Returns a dictionary of differences where key is the register number
    and the value is a tuple (pipeline_value, single_cycle_value).
    """
    differences = {}
    for reg in range(32):
        val_pipeline = regs_pipeline.get(reg, None)
        val_single = regs_single.get(reg, None)
        if val_pipeline != val_single:
            differences[reg] = (val_pipeline, val_single)
    return differences

def list_available_tests():
    """Lists all available test files and returns them."""
    pipeline_dir = os.path.join("logs", "pipeline")
    single_cycle_dir = os.path.join("logs", "single_cycle")
    
    try:
        pipeline_files = set(f for f in os.listdir(pipeline_dir) if f.endswith(".txt"))
        single_cycle_files = set(f for f in os.listdir(single_cycle_dir) if f.endswith(".txt"))
        common_files = pipeline_files & single_cycle_files
        
        if not common_files:
            print("No test files found")
            return []
            
        print("Available tests:")
        for f in sorted(common_files):
            print(f"  {f}")
        return sorted(common_files)
    except FileNotFoundError:
        print("Error: Log directories not found")
        return []

def run_single_test(test_name):
    """Runs a single test comparison."""
    pipeline_dir = os.path.join("logs", "pipeline")
    single_cycle_dir = os.path.join("logs", "single_cycle")
    
    pipeline_path = os.path.join(pipeline_dir, test_name)
    single_cycle_path = os.path.join(single_cycle_dir, test_name)
    
    if not os.path.exists(pipeline_path) or not os.path.exists(single_cycle_path):
        print(f"Error: Test files for {test_name} not found")
        return 1
    
    reg_pipeline = extract_last_cycle_registers(pipeline_path)
    reg_single = extract_last_cycle_registers(single_cycle_path)
    diff = compare_registers(reg_pipeline, reg_single)
    
    print(f"Running test: {test_name}")
    if diff:
        print("Differences found:")
        for reg_num in sorted(diff.keys()):
            pipeline_val, single_val = diff[reg_num]
            print(f"  R[{reg_num}]: pipeline = {pipeline_val}, single_cycle = {single_val}")
        return 1
    else:
        print("Register states match exactly between pipeline and single-cycle.")
        return 0

def main():
    if len(sys.argv) < 2:
        # If no test specified, just list available tests
        return 0 if list_available_tests() else 1
    
    test_name = sys.argv[1]
    return run_single_test(test_name)

if __name__ == "__main__":
    exit(main())
