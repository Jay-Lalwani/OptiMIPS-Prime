#!/usr/bin/env python3
"""
compare_registers.py

This script compares the register states in the last cycle
between pipeline and single-cycle log files. It looks in the
directories "logs/pipeline" and "logs/single_cycle" for files with
matching names and prints out any differences between the registers
(assuming registers are printed as "R[0]: <value>" lines in each cycle block).

Usage:
    python compare_registers.py

Author: Your Name
Date: YYYY-MM-DD
"""

import os
import re

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

def main():
    pipeline_dir = os.path.join("logs", "pipeline")
    single_cycle_dir = os.path.join("logs", "single_cycle")
    
    # List all .txt files in each directory.
    try:
        pipeline_files = [f for f in os.listdir(pipeline_dir) if f.endswith(".txt")]
        single_cycle_files = [f for f in os.listdir(single_cycle_dir) if f.endswith(".txt")]
    except FileNotFoundError as e:
        print("Error: One of the log directories does not exist.")
        return
    
    # Determine common files (those with the same filename in both folders)
    common_files = set(pipeline_files) & set(single_cycle_files)
    if not common_files:
        print("No common log files found in pipeline and single_cycle directories.")
        return

    passed = 0
    total = 0
    # Compare register states for each common file.
    for filename in sorted(common_files):
        pipeline_path = os.path.join(pipeline_dir, filename)
        single_cycle_path = os.path.join(single_cycle_dir, filename)
        
        reg_pipeline = extract_last_cycle_registers(pipeline_path)
        reg_single = extract_last_cycle_registers(single_cycle_path)
        diff = compare_registers(reg_pipeline, reg_single)
        
        total += 1
        print(f"--- Comparing file: {filename} ---")
        if diff:
            print("Differences found:")
            for reg_num in sorted(diff.keys()):
                pipeline_val, single_val = diff[reg_num]
                print(f"  R[{reg_num}]: pipeline = {pipeline_val}, single_cycle = {single_val}")
            print(f"File {filename}: FAILED")
        else:
            print("Register states match exactly between pipeline and single-cycle.")
            passed += 1
            print(f"File {filename}: passed")
        print()
    
    print(f"\n{passed}/{total} tests passed.")

        
if __name__ == "__main__":
    main()
