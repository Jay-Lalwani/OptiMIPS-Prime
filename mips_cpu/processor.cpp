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
    control = { .reg_dest    = 0, 
                .jump        = 0,
                .jump_reg    = 0,
                .link        = 0,
                .shift       = 0,
                .branch      = 0,
                .bne         = 0,
                .mem_read    = 0,
                .mem_to_reg  = 0,
                .ALU_op      = 0,
                .mem_write   = 0,
                .halfword    = 0,
                .byte        = 0,
                .ALU_src     = 0,
                .reg_write   = 0,
                .zero_extend = 0 };
    opt_level = level;
    // Clear pipeline registers.
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
        case 1: {
            // Cycle-accurate update: compute next states based on current state.
            IF_ID next_if = compute_IF();
            ID_EX next_id = compute_ID(if_id);
            // If a load–use hazard is detected in the current ID/EX stage, then stall:
            // Do not update next_id (bubble) and keep if_id unchanged.
            bool hazard = false;
            if (id_ex.valid && id_ex.mem_read) {
                int loadDest = id_ex.rt; // For load, destination is rt.
                int src1 = (if_id.instruction >> 21) & 0x1F;
                int src2 = (if_id.instruction >> 16) & 0x1F;
                if (loadDest != 0 && (src1 == loadDest || src2 == loadDest))
                    hazard = true;
            }
            if (hazard) {
                DEBUG(cout << "ID: Load-use hazard detected. Stalling (inserting bubble).\n");
                next_id.valid = false;
                // Do not update IF/ID; keep it for the next cycle.
                next_if = if_id;
            }
            EX_MEM next_ex = compute_EX(next_id);
            MEM_WB next_mw = compute_MEM(next_ex);
            do_WB(mem_wb); // Use current MEM/WB for WB.
            // Now update pipeline registers simultaneously.
            if_id = next_if;
            id_ex = next_id;
            ex_mem = next_ex;
            mem_wb = next_mw;
            break;
        }
        default:
            break;
    }
}

// -------------------- IF Stage --------------------
Processor::IF_ID Processor::compute_IF() {
    IF_ID next;
    uint32_t instr = 0;
    bool fetched = memory->access(fetch_pc, instr, 0, 1, 0);
    if (fetched) {
        next.instruction = instr;
        next.pc_plus_4 = fetch_pc + 4;
        next.valid = true;
        DEBUG(cout << "IF: Fetched 0x" << hex << instr << " from PC 0x" << fetch_pc << dec << "\n");
        fetch_pc += 4;
    } else {
        next.valid = false;
    }
    return next;
}

// -------------------- ID Stage --------------------
Processor::ID_EX Processor::compute_ID(const IF_ID &if_reg) {
    ID_EX next;
    if (!if_reg.valid) {
        next.valid = false;
        return next;
    }
    uint32_t instruction = if_reg.instruction;
    int opcode = (instruction >> 26) & 0x3F;
    int rs = (instruction >> 21) & 0x1F;
    int rt = (instruction >> 16) & 0x1F;
    int rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    uint32_t imm = instruction & 0xFFFF;
    // Decode control signals.
    control.decode(instruction);
    // For sign-extension, use control.zero_extend.
    imm = control.zero_extend ? imm : ((imm >> 15) ? (0xFFFF0000 | imm) : imm);
    // Read registers.
    uint32_t rd1 = 0, rd2 = 0;
    regfile.access(rs, rt, rd1, rd2, 0, false, 0);
    next.reg_dest    = control.reg_dest;
    next.ALU_src     = control.ALU_src;
    next.reg_write   = control.reg_write;
    next.mem_read    = control.mem_read;
    next.mem_write   = control.mem_write;
    next.mem_to_reg  = control.mem_to_reg;
    next.ALU_op      = control.ALU_op;
    next.branch      = control.branch;
    next.jump        = control.jump;
    next.jump_reg    = control.jump_reg;
    next.link        = control.link;
    next.shift       = control.shift;
    next.zero_extend = control.zero_extend;
    next.bne         = control.bne;
    next.halfword    = control.halfword;
    next.byte        = control.byte;
    next.pc_plus_4   = if_reg.pc_plus_4;
    next.read_data_1 = rd1;
    next.read_data_2 = rd2;
    next.imm         = imm;
    next.rs          = rs;
    next.rt          = rt;
    next.rd          = rd;
    next.opcode      = opcode;
    next.shamt       = shamt;
    next.funct       = funct;
    next.valid       = true;
    return next;
}

// -------------------- EX Stage --------------------
Processor::EX_MEM Processor::compute_EX(const ID_EX &id_reg) {
    EX_MEM next;
    if (!id_reg.valid) {
        next.valid = false;
        return next;
    }
    // Compute ALU operands with forwarding.
    uint32_t op1 = id_reg.shift ? id_reg.shamt : id_reg.read_data_1;
    uint32_t op2 = id_reg.ALU_src ? id_reg.imm : id_reg.read_data_2;
    // Forwarding for op1.
    if (!id_reg.shift) {
        int rs = id_reg.rs;
        if (rs != 0) {
            if (ex_mem.valid && ex_mem.reg_write && (ex_mem.write_reg == rs))
                op1 = ex_mem.alu_result;
            else if (mem_wb.valid && mem_wb.reg_write && (mem_wb.write_reg == rs))
                op1 = mem_wb.mem_to_reg ? mem_wb.mem_read_data : mem_wb.alu_result;
        }
    }
    // Forwarding for op2 (ALU operand when not using immediate).
    if (!id_reg.ALU_src) {
        int rt = id_reg.rt;
        if (rt != 0) {
            if (ex_mem.valid && ex_mem.reg_write && (ex_mem.write_reg == rt))
                op2 = ex_mem.alu_result;
            else if (mem_wb.valid && mem_wb.reg_write && (mem_wb.write_reg == rt))
                op2 = mem_wb.mem_to_reg ? mem_wb.mem_read_data : mem_wb.alu_result;
        }
    }
    uint32_t alu_zero = 0;
    alu.generate_control_inputs(id_reg.ALU_op, id_reg.funct, id_reg.opcode);
    uint32_t alu_result = alu.execute(op1, op2, alu_zero);
    uint32_t branch_target = id_reg.pc_plus_4 + (id_reg.imm << 2);
    next.reg_write = id_reg.reg_write;
    next.mem_read = id_reg.mem_read;
    next.mem_write = id_reg.mem_write;
    next.mem_to_reg = id_reg.mem_to_reg;
    next.link = id_reg.link;
    next.halfword = id_reg.halfword;
    next.byte = id_reg.byte;
    next.alu_result = alu_result;
    
    // *** Added forwarding for store data ***
    // For store instructions, the data to be written (from rt) must be forwarded if needed.
    uint32_t store_data = id_reg.read_data_2;
    if (id_reg.mem_write) {
        int store_reg = id_reg.rt;
        if (store_reg != 0) {
            if (ex_mem.valid && ex_mem.reg_write && (ex_mem.write_reg == store_reg))
                store_data = ex_mem.alu_result;
            else if (mem_wb.valid && mem_wb.reg_write && (mem_wb.write_reg == store_reg))
                store_data = mem_wb.mem_to_reg ? mem_wb.mem_read_data : mem_wb.alu_result;
        }
    }
    next.write_data = store_data;
    
    next.write_reg = id_reg.link ? 31 : (id_reg.reg_dest ? id_reg.rd : id_reg.rt);
    next.pc_branch = id_reg.pc_plus_4;  // default sequential.
    next.zero = (alu_zero == 1);
    next.valid = true;
    // --- Control Hazard Resolution ---
    if ((id_reg.branch && !id_reg.bne && next.zero) || (id_reg.bne && !next.zero)) {
        fetch_pc = branch_target;
        next.pc_branch = branch_target;
        flush_IF_ID_ID_EX();
    }
    else if (id_reg.jump) {
        uint32_t jump_addr = (id_reg.pc_plus_4 & 0xF0000000) | ((id_reg.imm & 0x03FFFFFF) << 2);
        fetch_pc = jump_addr;
        next.pc_branch = jump_addr;
        flush_IF_ID_ID_EX();
    }
    else if (id_reg.jump_reg) {
        fetch_pc = id_reg.read_data_1;
        next.pc_branch = id_reg.read_data_1;
        flush_IF_ID_ID_EX();
    }
    return next;
}

// -------------------- MEM Stage --------------------
Processor::MEM_WB Processor::compute_MEM(const EX_MEM &ex_reg) {
    MEM_WB next;
    if (!ex_reg.valid) {
        next.valid = false;
        return next;
    }
    uint32_t mem_data = 0;
    bool success = memory->access(ex_reg.alu_result, mem_data, 0, ex_reg.mem_read || ex_reg.mem_write, 0);
    if (success) {
        if (ex_reg.mem_write) {
            uint32_t wd = ex_reg.write_data;
            if (ex_reg.halfword)
                wd = (mem_data & 0xffff0000) | (ex_reg.write_data & 0xffff);
            else if (ex_reg.byte)
                wd = (mem_data & 0xffffff00) | (ex_reg.write_data & 0xff);
            success = memory->access(ex_reg.alu_result, mem_data, wd, ex_reg.mem_read, ex_reg.mem_write);
        }
        if (ex_reg.mem_read) {
            if (ex_reg.halfword)
                mem_data &= 0xffff;
            else if (ex_reg.byte)
                mem_data &= 0xff;
        }
        next.reg_write = ex_reg.reg_write;
        next.mem_to_reg = ex_reg.mem_to_reg;
        next.link = ex_reg.link;
        next.alu_result = ex_reg.alu_result;
        next.write_reg = ex_reg.write_reg;
        next.pc_plus_4 = ex_reg.pc_branch;
        next.mem_read_data = mem_data;
        next.valid = true;
    } else {
        next.valid = false;
    }
    return next;
}

// -------------------- WB Stage --------------------
void Processor::do_WB(const MEM_WB &mw_reg) {
    if (!mw_reg.valid) return;
    uint32_t write_data = 0;
    if (mw_reg.link)
        write_data = mw_reg.pc_plus_4 + 4;
    else if (mw_reg.mem_to_reg)
        write_data = mw_reg.mem_read_data;
    else
        write_data = mw_reg.alu_result;
    if (mw_reg.reg_write) {
        uint32_t dummy1, dummy2;
        regfile.access(0, 0, dummy1, dummy2, mw_reg.write_reg, true, write_data);
        DEBUG(cout << "WB: Writing " << write_data << " to R[" << mw_reg.write_reg << "]\n");
    }
    // Update committed PC.
    regfile.pc = mw_reg.pc_plus_4;
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
    int funct = (instruction & 0x3F);
    uint32_t imm = instruction & 0xFFFF;
    int addr = instruction & 0x3FFFFFF;
    
    uint32_t rd1 = 0, rd2 = 0;
    regfile.access(rs, rt, rd1, rd2, 0, false, 0);
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
    imm = control.zero_extend ? imm : ((imm >> 15) ? (0xFFFF0000 | imm) : imm);
    uint32_t operand_1 = control.shift ? shamt : rd1;
    uint32_t operand_2 = control.ALU_src ? imm : rd2;
    uint32_t alu_zero = 0;
    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    uint32_t mem_rd = 0, wd = 0;
    memory->access(alu_result, mem_rd, 0, control.mem_read | control.mem_write, 0);
    wd = control.halfword ? (mem_rd & 0xffff0000) | (rd2 & 0xffff) :
         control.byte ? (mem_rd & 0xffffff00) | (rd2 & 0xff) : rd2;
    memory->access(alu_result, mem_rd, wd, control.mem_read, control.mem_write);
    mem_rd &= control.halfword ? 0xFFFF : control.byte ? 0xFF : 0xFFFFFFFF;
    
    int wr = control.link ? 31 : control.reg_dest ? rd : rt;
    uint32_t write_data = control.link ? regfile.pc + 8 : control.mem_to_reg ? mem_rd : alu_result;
    regfile.access(0, 0, rd2, rd2, wr, control.reg_write, write_data);
    regfile.pc += ((control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero)) ? (imm << 2) : 0;
    regfile.pc = control.jump_reg ? rd1 : control.jump ? ((regfile.pc & 0xF0000000) | (addr << 2)) : regfile.pc;
}
