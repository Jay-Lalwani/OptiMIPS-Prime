#!/usr/bin/env python3
"""
compare_registers.py

This script compares both:
1. The register states in the last cycle between pipeline and single-cycle log files
2. The total cycle count against expected values

Usage:
    python compare_registers.py [test_name] [--check-type=<type>]
    If test_name is not provided, lists all available tests
    If test_name is 'all', runs all available tests
    check-type can be 'registers' or 'cycles' (default: both)
"""

import os
import re
import sys
import argparse

# Expected cycle counts for each test
EXPECTED_CYCLES = {
    "MIPSPipeline-branch.txt": 138,
    "MIPSPipeline-dependent_stores.txt": 142,
    "MIPSPipeline-false_dependency.txt": 78,
    "MIPSPipeline-forward_to_rs.txt": 78,
    "MIPSPipeline-forward_to_rt.txt": 79,
    "MIPSPipeline-hidden.txt": 264,
    "MIPSPipeline-load_use.txt": 150,
    "MIPSPipeline-raw_dependency.txt": 78,
    "MIPSPipeline-reg_file_fowarding.txt": 70,
    "MIPSPipeline-simple_no_branch_no_dep.txt": 154,
    "SpeculativeMIPS-3bit_011.txt": 390,
    "SpeculativeMIPS-3bit_100.txt": 382,
    "SpeculativeMIPS-3bit_double.txt": 510,
    "SpeculativeMIPS-hidden.txt": 16069,
    "SpeculativeMIPS-one_backward_beq.txt": 3074,
    "SpeculativeMIPS-one_backward_bne.txt": 2569,
    "SpeculativeMIPS-one_forward_beq.txt": 3575,
    "SpeculativeMIPS-one_forward_bne.txt": 4083,
    "SpeculativeMIPS-static_branch.txt": 390,
    "SpeculativeMIPS-test_case_local.txt": 4070
}

def extract_last_cycle_registers(filepath):
    """
    Extracts register values (a dict mapping register number to value)
    from the final cycle block in the log file.
    
    Assumes that each cycle block begins with a line
      "CYCLE <number>"
    and is followed by several lines of the form:
      "R[<num>]: <value>"
    
    Returns:
        A tuple: (dict of {register_number: value}, total_cycles)
    """
    last_regs = {}
    current_regs = {}
    total_cycles = 0
    # regular expressions to match cycle number and register lines
    cycle_re = re.compile(r"^CYCLE\s+(\d+)")
    reg_re = re.compile(r"^R\[(\d+)\]:\s+(-?\d+)")
    
    with open(filepath, "r") as file:
        for line in file:
            line = line.strip()
            # Check for a cycle header.
            cycle_match = cycle_re.match(line)
            if cycle_match:
                total_cycles = int(cycle_match.group(1))
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
    return last_regs, total_cycles + 1

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

def check_cycle_count(test_name, actual_cycles):
    """
    Checks if the cycle count matches the expected value.
    
    Returns:
        tuple: (bool indicating success, expected cycles)
    """
    expected = EXPECTED_CYCLES.get(test_name)
    if expected is None:
        print(f"Warning: No expected cycle count found for {test_name}")
        return False, None
    return actual_cycles == expected, expected

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

def run_single_test(test_name, check_type='both'):
    """
    Runs a single test comparison.
    
    Args:
        test_name: Name of the test file
        check_type: Type of check to perform ('registers', 'cycles', or 'both')
    
    Returns:
        dict: Results containing success status for registers and cycles
    """
    pipeline_dir = os.path.join("logs", "pipeline")
    single_cycle_dir = os.path.join("logs", "single_cycle")
    
    pipeline_path = os.path.join(pipeline_dir, test_name)
    single_cycle_path = os.path.join(single_cycle_dir, test_name)
    
    if not os.path.exists(pipeline_path) or not os.path.exists(single_cycle_path):
        print(f"Error: Test files for {test_name} not found")
        return {'registers': False, 'cycles': False}
    
    results = {'registers': True, 'cycles': True}
    print(f"\nRunning test: {test_name}")
    
    # Check registers if requested
    if check_type in ['both', 'registers']:
        reg_pipeline, cycles_pipeline = extract_last_cycle_registers(pipeline_path)
        reg_single, _ = extract_last_cycle_registers(single_cycle_path)
        diff = compare_registers(reg_pipeline, reg_single)
        
        if diff:
            print("Register state differences found:")
            for reg_num in sorted(diff.keys()):
                pipeline_val, single_val = diff[reg_num]
                print(f"  R[{reg_num}]: pipeline = {pipeline_val}, single_cycle = {single_val}")
            results['registers'] = False
        else:
            print("Register states match exactly between pipeline and single-cycle.")
    
    # Check cycle count if requested
    if check_type in ['both', 'cycles']:
        _, cycles_pipeline = extract_last_cycle_registers(pipeline_path)
        cycles_match, expected_cycles = check_cycle_count(test_name, cycles_pipeline)
        
        if not cycles_match:
            print(f"Cycle count mismatch: expected {expected_cycles}, got {cycles_pipeline}")
            results['cycles'] = False
        else:
            print(f"Cycle count matches expected value: {cycles_pipeline}")
    
    return results

def main():
    parser = argparse.ArgumentParser(description='Compare register states and cycle counts between pipeline and single-cycle implementations.')
    parser.add_argument('test_name', nargs='?', help='Name of test to run, "all" for all tests, or omit for list of tests')
    parser.add_argument('--check-type', choices=['registers', 'cycles', 'both'], default='both',
                      help='Type of check to perform (default: both)')
    
    args = parser.parse_args()
    
    if not args.test_name:
        # If no test specified, just list available tests
        return 0 if list_available_tests() else 1
    
    if args.test_name == 'all':
        tests = list_available_tests()
        if not tests:
            return 1
        
        results = {
            'registers': {'passed': 0, 'failed': 0, 'failed_tests': []},
            'cycles': {'passed': 0, 'failed': 0, 'failed_tests': []}
        }
        
        for test in tests:
            test_results = run_single_test(test, args.check_type)
            
            for check_type in ['registers', 'cycles']:
                if args.check_type in [check_type, 'both']:
                    if test_results[check_type]:
                        results[check_type]['passed'] += 1
                    else:
                        results[check_type]['failed'] += 1
                        results[check_type]['failed_tests'].append(test)
        
        print("\nSummary:")
        if args.check_type in ['both', 'registers']:
            print("\nRegister State Comparison:")
            print(f"Passed: {results['registers']['passed']}")
            print(f"Failed: {results['registers']['failed']}")
            if results['registers']['failed_tests']:
                print("\nFailed register state tests:")
                for test in results['registers']['failed_tests']:
                    print(f"  {test}")
        
        if args.check_type in ['both', 'cycles']:
            print("\nCycle Count Comparison:")
            print(f"Passed: {results['cycles']['passed']}")
            print(f"Failed: {results['cycles']['failed']}")
            if results['cycles']['failed_tests']:
                print("\nFailed cycle count tests:")
                for test in results['cycles']['failed_tests']:
                    print(f"  {test}")
        
        return (results['registers']['failed'] if args.check_type in ['both', 'registers'] else 0) + \
               (results['cycles']['failed'] if args.check_type in ['both', 'cycles'] else 0)
    
    test_results = run_single_test(args.test_name, args.check_type)
    return 0 if all(test_results.values()) else 1

if __name__ == "__main__":
    exit(main())
