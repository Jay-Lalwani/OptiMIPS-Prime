#include <cstdint>
#include <iostream>
#include "processor.h"
using namespace std;

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

// -------------------- Initialization --------------------
void Processor::initialize(int level) {
    // Initialize the baseline control signals.
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

// -------------------- Advance --------------------
void Processor::advance() {
    switch (opt_level) {
        case 0:
            single_cycle_processor_advance();
            break;
        case 1:
            pipelined_processor_advance();
            break;
        default:
            break;
    }
}

// -------------------- Pipelined Advance --------------------
void Processor::pipelined_processor_advance() {
    pipeline_WB();
    if (!pipeline_MEM()) {
        DEBUG(cout << "Memory stall encountered. Pipeline is stalled.\n");
        return; // Stall the pipeline.
    }
    pipeline_EX();
    pipeline_ID();
    pipeline_IF();
}

// -------------------- IF Stage --------------------
void Processor::pipeline_IF() {
    uint32_t instruction = 0;
    // Fetch instruction from memory using fetch_pc
    bool fetchSuccess = memory->access(fetch_pc, instruction, 0, 1, 0);
    if (!fetchSuccess) {
        DEBUG(cout << "IF: Memory stall during fetch at PC 0x" << hex << fetch_pc << dec << "\n");
        return;
    }
    if_id.instruction = instruction;
    if_id.pc_plus_4 = fetch_pc + 4;
    if_id.valid = true;
    DEBUG(cout << "IF: Fetched instruction 0x" << hex << instruction << " from PC 0x" << fetch_pc << dec << "\n");
    fetch_pc += 4;
}

// -------------------- ID Stage --------------------
void Processor::pipeline_ID() {
    if (!if_id.valid) return;
    uint32_t instruction = if_id.instruction;
    
    // Decode fields
    int opcode = (instruction >> 26) & 0x3F;
    int rs     = (instruction >> 21) & 0x1F;
    int rt     = (instruction >> 16) & 0x1F;
    int rd     = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    uint32_t imm   = instruction & 0xFFFF;
    
    // Sign-extend the immediate
    imm = control.zero_extend ? imm : ((imm >> 15) ? (0xFFFF0000 | imm) : imm);
    
    // Decode control signals.
    control.decode(instruction);
    DEBUG(control.print());
    
    // Read registers.
    uint32_t read_data_1 = 0, read_data_2 = 0;
    regfile.access(rs, rt, read_data_1, read_data_2, 0, false, 0);
    
    // Populate ID/EX pipeline register
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
    id_ex.pc_plus_4   = if_id.pc_plus_4;
    id_ex.read_data_1 = read_data_1;
    id_ex.read_data_2 = read_data_2;
    id_ex.imm         = imm;
    id_ex.rs          = rs;
    id_ex.rt          = rt;
    id_ex.rd          = rd;
    id_ex.opcode      = opcode;
    id_ex.shamt       = shamt;
    id_ex.funct       = funct;
    id_ex.valid       = true;
    
    // Clear IF/ID register.
    if_id.valid = false;
}

// -------------------- EX Stage --------------------
void Processor::pipeline_EX() {
    if (!id_ex.valid) return;
    
    // Set up operands
    uint32_t operand_1 = id_ex.shift ? id_ex.shamt : id_ex.read_data_1;
    uint32_t operand_2 = id_ex.ALU_src ? id_ex.imm : id_ex.read_data_2;
    uint32_t alu_zero = 0;
    
    // Generate ALU control and execute.
    alu.generate_control_inputs(id_ex.ALU_op, id_ex.funct, id_ex.opcode);
    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    // Compute branch target: PC+4 + (imm << 2)
    uint32_t branch_target = id_ex.pc_plus_4 + (id_ex.imm << 2);
    
    // Populate EX/MEM pipeline register.
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
    ex_mem.pc_branch   = id_ex.pc_plus_4;  // Default: sequential.
    ex_mem.zero        = alu_zero;  // Store actual ALU zero flag
    ex_mem.valid       = true;
    
    // Handle branch/jump hazards
    bool take_branch = (id_ex.branch && !id_ex.bne && alu_zero) || (id_ex.bne && !alu_zero);
    if (take_branch) {
        fetch_pc = branch_target;
        ex_mem.pc_branch = branch_target;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Branch taken to 0x" << hex << branch_target << dec << "\n");
    }
    else if (id_ex.jump) {
        uint32_t jump_addr = (id_ex.pc_plus_4 & 0xF0000000) | ((id_ex.imm & 0x03FFFFFF) << 2);
        fetch_pc = jump_addr;
        ex_mem.pc_branch = jump_addr;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Jump to 0x" << hex << jump_addr << dec << "\n");
    }
    else if (id_ex.jump_reg) {
        fetch_pc = id_ex.read_data_1;
        ex_mem.pc_branch = id_ex.read_data_1;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Jump register to 0x" << hex << id_ex.read_data_1 << dec << "\n");
    }
    
    // Clear ID/EX register.
    id_ex.valid = false;
}

// -------------------- MEM Stage --------------------
bool Processor::pipeline_MEM() {
    if (!ex_mem.valid) return true;
    
    uint32_t mem_data = 0;
    // First, perform a memory access to read current data.
    bool success = memory->access(ex_mem.alu_result, mem_data, 0, ex_mem.mem_read || ex_mem.mem_write, 0);
    if (!success) return false; // Stall if memory busy.
    
    if (ex_mem.mem_write) {
        uint32_t write_data_mem = ex_mem.write_data;
        if (ex_mem.halfword) {
            write_data_mem = (mem_data & 0xffff0000) | (ex_mem.write_data & 0xffff);
        } else if (ex_mem.byte) {
            write_data_mem = (mem_data & 0xffffff00) | (ex_mem.write_data & 0xff);
        }
        success = memory->access(ex_mem.alu_result, mem_data, write_data_mem, ex_mem.mem_read, ex_mem.mem_write);
        if (!success) return false;
    }
    
    // For load instructions, apply masking.
    if (ex_mem.mem_read) {
        if (ex_mem.halfword) {
            mem_data &= 0xffff;
        } else if (ex_mem.byte) {
            mem_data &= 0xff;
        }
    }
    
    // Populate MEM/WB pipeline register.
    mem_wb.reg_write     = ex_mem.reg_write;
    mem_wb.mem_to_reg    = ex_mem.mem_to_reg;
    mem_wb.link          = ex_mem.link;
    mem_wb.alu_result    = ex_mem.alu_result;
    mem_wb.write_reg     = ex_mem.write_reg;
    mem_wb.pc_plus_4     = ex_mem.pc_branch; // This holds the PC to commit.
    mem_wb.mem_read_data = mem_data;
    mem_wb.valid         = true;
    
    // Clear EX/MEM register.
    ex_mem.valid = false;
    return true;
}

// -------------------- WB Stage --------------------
void Processor::pipeline_WB() {
    if (!mem_wb.valid) return;
    
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
    // Update the committed PC here.
    regfile.pc = mem_wb.pc_plus_4;
    
    // Clear MEM/WB register.
    mem_wb.valid = false;
}

// -------------------- Flush --------------------
void Processor::flush_IF_ID_ID_EX() {
    if_id.valid = false;
    id_ex.valid = false;
}

// -------------------- Single-Cycle Processor --------------------
void Processor::single_cycle_processor_advance() {
    uint32_t instruction;
    memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << hex << regfile.pc << dec << "\n");
    regfile.pc += 4;
    
    control.decode(instruction);
    DEBUG(control.print());

    int opcode = (instruction >> 26) & 0x3F;
    int rs = (instruction >> 21) & 0x1F;
    int rt = (instruction >> 16) & 0x1F;
    int rd = (instruction >> 11) & 0x1F;
    int shamt = (instruction >> 6) & 0x1F;
    int funct = instruction & 0x3F;
    uint32_t imm = instruction & 0xFFFF;
    int addr = instruction & 0x3FFFFFF;
    
    uint32_t read_data_1 = 0, read_data_2 = 0;
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
    imm = control.zero_extend ? imm : ((imm >> 15) ? (0xFFFF0000 | imm) : imm);
    
    uint32_t operand_1 = control.shift ? shamt : read_data_1;
    uint32_t operand_2 = control.ALU_src ? imm : read_data_2;
    uint32_t alu_zero = 0;
    
    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    uint32_t read_data_mem = 0, write_data_mem = 0;
    memory->access(alu_result, read_data_mem, 0, control.mem_read | control.mem_write, 0);
    write_data_mem = control.halfword ? (read_data_mem & 0xFFFF0000) | (read_data_2 & 0xFFFF) :
                     control.byte ? (read_data_mem & 0xFFFFFF00) | (read_data_2 & 0xFF) :
                     read_data_2;
    memory->access(alu_result, read_data_mem, write_data_mem, control.mem_read, control.mem_write);
    read_data_mem &= control.halfword ? 0xFFFF : control.byte ? 0xFF : 0xFFFFFFFF;
    
    int write_reg = control.link ? 31 : control.reg_dest ? rd : rt;
    uint32_t write_data = control.link ? regfile.pc + 8 : control.mem_to_reg ? read_data_mem : alu_result;
    
    regfile.access(0, 0, read_data_2, read_data_2, write_reg, control.reg_write, write_data);
    
    regfile.pc += ((control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero)) ? (imm << 2) : 0;
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? ((regfile.pc & 0xF0000000) | (addr << 2)) : regfile.pc;
}
