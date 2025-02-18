#include <cstdint>
#include <iostream>
#include "processor.h"
using namespace std;

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

// Initializes the processor.
void Processor::initialize(int level) {
    // Initialize the baseline control signals (they will be generated per instruction).
    control = {.reg_dest = 0, 
               .jump = 0,
               .jump_reg = 0,
               .link = 0,
               .shift = 0,
               .branch = 0,
               .bne = 0,
               .mem_read = 0,
               .mem_to_reg = 0,
               .ALU_op = 0,
               .mem_write = 0,
               .halfword = 0,
               .byte = 0,
               .ALU_src = 0,
               .reg_write = 0,
               .zero_extend = 0};
   
    opt_level = level;
    // Reset pipeline registers.
    if_id.valid = false;
    id_ex.valid = false;
    ex_mem.valid = false;
    mem_wb.valid = false;
}

// Advances the processor one cycle.
void Processor::advance() {
    switch (opt_level) {
        case 0: 
            single_cycle_processor_advance();
            break;
        case 1:
            pipelined_processor_advance();
            break;
        // Other optimization levels can be added here.
        default: break;
    }
}

// Pipelined processor: simulate one full cycle (WB, MEM, EX, ID, IF) in order.
void Processor::pipelined_processor_advance() {
    // Write-Back stage (commits results and updates regfile.pc).
    pipeline_WB();
    
    // Memory stage: perform load/store; stall if a cache miss occurs.
    bool memStageCompleted = pipeline_MEM();
    if (!memStageCompleted) {
        DEBUG(cout << "Memory stall encountered. Pipeline is stalled.\n");
        return; // Stall the pipeline this cycle.
    }
    
    // Execution stage.
    pipeline_EX();
    
    // Instruction Decode stage.
    pipeline_ID();
    
    // Instruction Fetch stage.
    pipeline_IF();
}

// -------------------- WB Stage --------------------
// Write back the result from MEM/WB into the register file and update the committed PC.
void Processor::pipeline_WB() {
    if (mem_wb.valid) {
        uint32_t write_data = 0;
        if (mem_wb.link) {
            // For link instructions, R[31] gets PC+8.
            write_data = mem_wb.pc_plus_4 + 4;
        } else if (mem_wb.mem_to_reg) {
            write_data = mem_wb.mem_read_data;
        } else {
            write_data = mem_wb.alu_result;
        }
        if (mem_wb.reg_write) {
            uint32_t dummy1, dummy2;
            regfile.access(0, 0, dummy1, dummy2, mem_wb.write_reg, true, write_data);
            DEBUG(cout << "WB: Writing " << write_data << " to R[" << mem_wb.write_reg << "]\n");
        }
        // Update committed PC here during WB.
        regfile.pc = mem_wb.pc_plus_4;
    }
    // Clear MEM/WB after write-back.
    mem_wb.valid = false;
}

// -------------------- MEM Stage --------------------
// Perform memory read or write using data from EX/MEM. Mimics the single-cycle memory sequence.
bool Processor::pipeline_MEM() {
    if (ex_mem.valid) {
        uint32_t mem_data = 0;
        // First, perform a read to get the original memory contents.
        bool success = memory->access(ex_mem.alu_result, mem_data, 0, ex_mem.mem_read || ex_mem.mem_write, false);
        if (!success) {
            return false; // Stall if the memory is busy.
        }
        // For store instructions, compute the data to be written using masking (as in single-cycle code).
        if (ex_mem.mem_write) {
            uint32_t write_data_mem = ex_mem.write_data;
            if (ex_mem.halfword) {
                write_data_mem = (mem_data & 0xffff0000) | (ex_mem.write_data & 0xffff);
            } else if (ex_mem.byte) {
                write_data_mem = (mem_data & 0xffffff00) | (ex_mem.write_data & 0xff);
            }
            success = memory->access(ex_mem.alu_result, mem_data, write_data_mem, ex_mem.mem_read, ex_mem.mem_write);
            if (!success) {
                return false;
            }
        }
        // For loads, apply masking if needed.
        if (ex_mem.mem_read) {
            if (ex_mem.halfword) {
                mem_data &= 0xffff;
            } else if (ex_mem.byte) {
                mem_data &= 0xff;
            }
        }
        // Populate the MEM/WB pipeline register.
        mem_wb.reg_write     = ex_mem.reg_write;
        mem_wb.mem_to_reg    = ex_mem.mem_to_reg;
        mem_wb.link          = ex_mem.link;
        mem_wb.alu_result    = ex_mem.alu_result;
        mem_wb.write_reg     = ex_mem.write_reg;
        mem_wb.pc_plus_4     = ex_mem.pc_branch; // Either sequential PC or branch/jump target.
        mem_wb.mem_read_data = mem_data;
        mem_wb.valid         = true;
        // Clear EX/MEM after transfer.
        ex_mem.valid = false;
    }
    return true;
}

// -------------------- EX Stage --------------------
// Execute the ALU operation and compute branch targets. Mirrors the single-cycle execution.
void Processor::pipeline_EX() {
    if (id_ex.valid) {
        // Set up operands.
        uint32_t operand_1 = id_ex.shift ? id_ex.shamt : id_ex.read_data_1;
        uint32_t operand_2 = id_ex.ALU_src ? id_ex.imm : id_ex.read_data_2;
        uint32_t alu_zero = 0;
        
        // Generate ALU control signals and execute the operation.
        alu.generate_control_inputs(id_ex.ALU_op, id_ex.funct, id_ex.opcode);
        uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
        
        // Compute branch target: PC+4 + (imm << 2)
        uint32_t branch_target = id_ex.pc_plus_4 + (id_ex.imm << 2);
        
        // Populate the EX/MEM pipeline register.
        ex_mem.reg_write   = id_ex.reg_write;
        ex_mem.mem_read    = id_ex.mem_read;
        ex_mem.mem_write   = id_ex.mem_write;
        ex_mem.mem_to_reg  = id_ex.mem_to_reg;
        ex_mem.link        = id_ex.link;
        ex_mem.halfword    = id_ex.halfword;
        ex_mem.byte        = id_ex.byte;
        ex_mem.alu_result  = alu_result;
        ex_mem.write_data  = id_ex.read_data_2; // For store instructions.
        ex_mem.write_reg   = id_ex.link ? 31 : (id_ex.reg_dest ? id_ex.rd : id_ex.rt);
        // By default, the committed PC will be sequential (PC+4).
        ex_mem.pc_branch   = id_ex.pc_plus_4;
        ex_mem.zero        = (alu_zero == 1);
        ex_mem.valid       = true;
        
        // Handle branch/jump hazards, as in the single-cycle code.
        if (id_ex.branch && ex_mem.zero) {
            // For a taken branch, update fetch_pc and commit the branch target.
            fetch_pc = branch_target;
            ex_mem.pc_branch = branch_target;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Branch taken to 0x" << std::hex << branch_target << std::dec << "\n");
        }
        else if (id_ex.jump) {
            uint32_t jump_addr = (id_ex.pc_plus_4 & 0xF0000000) | ((id_ex.imm & 0x03FFFFFF) << 2);
            fetch_pc = jump_addr;
            ex_mem.pc_branch = jump_addr;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Jump to 0x" << std::hex << jump_addr << std::dec << "\n");
        }
        else if (id_ex.jump_reg) {
            fetch_pc = id_ex.read_data_1;
            ex_mem.pc_branch = id_ex.read_data_1;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Jump register to 0x" << std::hex << id_ex.read_data_1 << std::dec << "\n");
        }
    }
    // Clear ID/EX after execution.
    id_ex.valid = false;
}

// -------------------- ID Stage --------------------
// Decode the instruction (like the single-cycle version): extract opcode, registers, immediate, etc.  
// Then read the register file and populate the ID/EX pipeline register.
void Processor::pipeline_ID() {
    if (if_id.valid) {
        uint32_t instruction = if_id.instruction;
        // Decode fields as in the single-cycle processor.
        int opcode = (instruction >> 26) & 0x3F;
        int rs     = (instruction >> 21) & 0x1F;
        int rt     = (instruction >> 16) & 0x1F;
        int rd     = (instruction >> 11) & 0x1F;
        uint32_t shamt = (instruction >> 6) & 0x1F;
        uint32_t funct = instruction & 0x3F;
        uint32_t imm   = instruction & 0xFFFF;
        
        // Sign-extend the immediate (unless zero extension is required, e.g., for ori/andi).
        if (!(opcode == 0xc || opcode == 0xd)) {
            if (imm & 0x8000) {
                imm |= 0xFFFF0000;
            }
        }
        
        // Decode control signals.
        control.decode(instruction);
        DEBUG(control.print());
        
        // Read register file.
        uint32_t read_data_1 = 0, read_data_2 = 0;
        regfile.access(rs, rt, read_data_1, read_data_2, 0, false, 0);
        
        // Populate ID/EX pipeline register in the same order as the single-cycle code.
        id_ex.reg_dest    = control.reg_dest;
        id_ex.ALU_src     = control.ALU_src;
        id_ex.reg_write   = control.reg_write;
        id_ex.mem_read    = control.mem_read;
        id_ex.mem_write   = control.mem_write;
        id_ex.mem_to_reg  = control.mem_to_reg;
        id_ex.ALU_op      = control.ALU_op;
        id_ex.branch      = control.branch;
        id_ex.jump        = control.jump;
        id_ex.jump_reg    = control.jump_reg;
        id_ex.link        = control.link;
        id_ex.shift       = control.shift;
        id_ex.zero_extend = control.zero_extend;
        id_ex.bne         = control.bne;
        id_ex.halfword    = control.halfword;
        id_ex.byte        = control.byte;
        id_ex.pc_plus_4   = if_id.pc_plus_4;  // PC+4 as in the single-cycle processor.
        id_ex.read_data_1 = read_data_1;
        id_ex.read_data_2 = read_data_2;
        id_ex.imm         = imm;
        id_ex.rs          = rs;
        id_ex.rt          = rt;
        id_ex.rd          = rd;
        id_ex.opcode      = opcode;       // Save opcode for ALU control.
        id_ex.shamt       = shamt;
        id_ex.funct       = funct;
        id_ex.valid       = true;
        
        // Clear IF/ID after using it.
        if_id.valid = false;
    }
}

// -------------------- IF Stage --------------------
// Fetch the instruction using fetch_pc (like the single-cycle fetch, but without updating regfile.pc).
void Processor::pipeline_IF() {
    uint32_t instruction = 0;
    bool fetchSuccess = memory->access(fetch_pc, instruction, 0, true, false);
    if (!fetchSuccess) {
        DEBUG(cout << "IF: Memory stall during fetch at PC 0x" << std::hex << fetch_pc << std::dec << "\n");
        return;
    }
    if_id.instruction = instruction;
    if_id.pc_plus_4  = fetch_pc + 4;
    if_id.valid      = true;
    DEBUG(cout << "IF: Fetched instruction 0x" << std::hex << instruction 
               << " from PC 0x" << fetch_pc << std::dec << "\n");
    // Increment fetch_pc for the next instruction.
    fetch_pc += 4;
}

// Flush the IF/ID and ID/EX pipeline registers (used when a branch or jump is taken).
void Processor::flush_IF_ID_ID_EX() {
    if_id.valid = false;
    id_ex.valid = false;
}

// -------------------- Single-Cycle Processor (Unchanged) --------------------
void Processor::single_cycle_processor_advance() {
    // Existing single-cycle implementation (not shown here).
}
