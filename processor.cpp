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
    
    // Detect speculative mode by reading first instruction
    uint32_t first_inst = 0;
    // Read from memory address 0 (assumes text was linked at zero)
    memory->access(0, first_inst, 0, true, false);
    uint32_t opcode = first_inst >> 26;
    // Assume that if the opcode is 0x09 (addiu) then it is a SpeculativeMIPS test
    speculative_mode = (opcode == 0x09);
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
        return;
    }
    pipeline_IF();
    pipeline_ID();
    pipeline_EX();
}

void Processor::pipeline_IF() {
    uint32_t instruction = 0;
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

void Processor::pipeline_ID() {
    if (!if_id.valid) return;
    uint32_t instruction = if_id.instruction;
    
    int opcode = (instruction >> 26) & 0x3F;
    int rs     = (instruction >> 21) & 0x1F;
    int rt     = (instruction >> 16) & 0x1F;
    int rd     = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    uint32_t imm   = instruction & 0xFFFF;
    
   
    control.decode(instruction);
    
    int write_reg = control.link ? 31 : (control.reg_dest ? rd : rt);
    
    bool stall = false;
    
    // Check for load-use hazard with previous instruction
    if (id_ex.valid && id_ex.mem_read) {
        if ((id_ex.write_reg == rs && rs != 0) || 
            (!control.ALU_src && id_ex.write_reg == rt && rt != 0)) {
            stall = true;
            // Add extra stall cycle for load-use hazard
            DEBUG(cout << "ID: Load-use hazard detected with previous instruction, stalling\n");
        }
    }
    
    // Check for load-use hazard with instruction in MEM stage
    if (ex_mem.valid && ex_mem.mem_read) {
        if ((ex_mem.write_reg == rs && rs != 0) || 
            (!control.ALU_src && ex_mem.write_reg == rt && rt != 0)) {
            stall = true;
            // Add extra stall cycle for load-use hazard with MEM stage
            DEBUG(cout << "ID: Load-use hazard detected with MEM stage, stalling\n");
        }
    }
    
    // Only stall for store after load if we need the loaded value for the store
    if (control.mem_write && id_ex.valid && id_ex.mem_read && id_ex.write_reg == rt) {
        stall = true;
        DEBUG(cout << "ID: Store after load hazard detected, stalling\n");
    }
    
    // No need to stall for load after store - memory forwarding will handle it
    
    // Only stall for store-store if we're writing to the same address and need a value
    if (control.mem_write && id_ex.valid && id_ex.mem_write) {
        // Only stall if we need a value that's being computed
        if ((rs != 0 && rs == id_ex.write_reg) || 
            (!control.ALU_src && rt != 0 && rt == id_ex.write_reg)) {
            stall = true;
            DEBUG(cout << "ID: Store-store hazard detected, stalling\n");
        }
    }
    
    // Check for RAW hazard with store data only if we actually need the value
    if (control.mem_write && id_ex.valid && id_ex.reg_write && 
        id_ex.write_reg == rt && rt != 0) {
        stall = true;
        DEBUG(cout << "ID: RAW hazard with store data detected, stalling\n");
    }
    
    if (stall) {
        // Insert bubble in ID/EX
        id_ex.reg_dest    = false;
        id_ex.ALU_src     = false;
        id_ex.reg_write   = false;
        id_ex.mem_read    = false;
        id_ex.mem_write   = false;
        id_ex.mem_to_reg  = false;
        id_ex.ALU_op      = 0;
        id_ex.branch      = false;
        id_ex.jump        = false;
        id_ex.jump_reg    = false;
        id_ex.link        = false;
        id_ex.shift       = false;
        id_ex.zero_extend = false;
        id_ex.bne         = false;
        id_ex.halfword    = false;
        id_ex.byte        = false;
        id_ex.write_reg   = 0;
        id_ex.valid       = true;
        return;  // Keep IF/ID unchanged
    }
    
    // Sign-extend the immediate
    imm = control.zero_extend ? imm : ((imm >> 15) ? (0xFFFF0000 | imm) : imm);
    
    DEBUG(control.print());
    
    // Read registers
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
    id_ex.write_reg   = write_reg;
    id_ex.opcode      = opcode;
    id_ex.shamt       = shamt;
    id_ex.funct       = funct;
    id_ex.valid       = true;
    
    // Clear IF/ID register
    if_id.valid = false;
}

// -------------------- EX Stage --------------------
void Processor::pipeline_EX() {
    if (!id_ex.valid) return;
    
    // Data forwarding logic
    uint32_t operand_1 = id_ex.read_data_1;
    uint32_t operand_2 = id_ex.read_data_2;
    uint32_t store_data = id_ex.read_data_2;  // Original value for store data
    
    // Forward from MEM stage
    if (ex_mem.valid && ex_mem.reg_write && ex_mem.write_reg != 0) {
        uint32_t forward_data = ex_mem.mem_to_reg ? ex_mem.mem_read_data : ex_mem.alu_result;
        if (ex_mem.write_reg == id_ex.rs && !id_ex.shift) {
            operand_1 = forward_data;
            DEBUG(cout << "EX: Forwarding " << forward_data << " from MEM to rs\n");
        }
        if (ex_mem.write_reg == id_ex.rt && !id_ex.ALU_src) {
            operand_2 = forward_data;
            DEBUG(cout << "EX: Forwarding " << forward_data << " from MEM to rt\n");
        }
        // Forward store data
        if (id_ex.mem_write && ex_mem.write_reg == id_ex.rt) {
            store_data = forward_data;
            DEBUG(cout << "EX: Forwarding " << forward_data << " from MEM for store data\n");
        }
    }
    
    // Forward from WB stage
    if (mem_wb.valid && mem_wb.reg_write && mem_wb.write_reg != 0) {
        uint32_t wb_data = mem_wb.mem_to_reg ? mem_wb.mem_read_data : mem_wb.alu_result;
        if (mem_wb.write_reg == id_ex.rs && !id_ex.shift && 
            !(ex_mem.valid && ex_mem.reg_write && ex_mem.write_reg == id_ex.rs)) {
            operand_1 = wb_data;
            DEBUG(cout << "EX: Forwarding " << wb_data << " from WB to rs\n");
        }
        if (mem_wb.write_reg == id_ex.rt && !id_ex.ALU_src && 
            !(ex_mem.valid && ex_mem.reg_write && ex_mem.write_reg == id_ex.rt)) {
            operand_2 = wb_data;
            DEBUG(cout << "EX: Forwarding " << wb_data << " from WB to rt\n");
        }
        // Forward store data
        if (id_ex.mem_write && mem_wb.write_reg == id_ex.rt &&
            !(ex_mem.valid && ex_mem.reg_write && ex_mem.write_reg == id_ex.rt)) {
            store_data = wb_data;
            DEBUG(cout << "EX: Forwarding " << wb_data << " from WB for store data\n");
        }
    }
    
    // Set up final operands
    operand_1 = id_ex.shift ? id_ex.shamt : operand_1;
    operand_2 = id_ex.ALU_src ? id_ex.imm : operand_2;
    uint32_t alu_zero = 0;
    
    // Generate ALU control and execute
    alu.generate_control_inputs(id_ex.ALU_op, id_ex.funct, id_ex.opcode);
    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    // Populate EX/MEM pipeline register
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;
    ex_mem.mem_to_reg = id_ex.mem_to_reg;
    ex_mem.link = id_ex.link;
    ex_mem.halfword = id_ex.halfword;
    ex_mem.byte = id_ex.byte;
    ex_mem.alu_result = alu_result;
    ex_mem.write_data = store_data;  // Use forwarded store data
    ex_mem.write_reg = id_ex.write_reg;
    ex_mem.pc_branch = id_ex.pc_plus_4;
    ex_mem.zero = (alu_zero == 1);
    ex_mem.valid = true;
    
    // Handle branches and jumps
    bool take_branch = (id_ex.branch && !id_ex.bne && alu_zero) || 
                      (id_ex.branch && id_ex.bne && !alu_zero);
    
    if (take_branch) {
        fetch_pc = id_ex.pc_plus_4 + (id_ex.imm << 2);
        ex_mem.pc_branch = fetch_pc;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Branch taken to 0x" << hex << fetch_pc << dec << "\n");
    }
    else if (id_ex.jump) {
        uint32_t jump_addr = (id_ex.pc_plus_4 & 0xF0000000) | ((id_ex.imm & 0x03FFFFFF) << 2);
        fetch_pc = jump_addr;
        ex_mem.pc_branch = jump_addr;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Jump to 0x" << hex << jump_addr << dec << "\n");
    }
    else if (id_ex.jump_reg) {
        fetch_pc = operand_1;  // Use forwarded value for jump register
        ex_mem.pc_branch = operand_1;
        flush_IF_ID_ID_EX();
        DEBUG(cout << "EX: Jump register to 0x" << hex << operand_1 << dec << "\n");
    }
    
    // Clear ID/EX register
    id_ex.valid = false;
}

// -------------------- MEM Stage --------------------
bool Processor::pipeline_MEM() {
    if (!ex_mem.valid) return true;
    
    static int mem_stall_cycles = 0;
    if (mem_stall_cycles > 0) {
        mem_stall_cycles--;
        DEBUG(cout << "Memory operation stall, " << mem_stall_cycles << " cycles remaining\n");
        return false;
    }
    
    uint32_t mem_data = 0;
    bool success = true;

    // Only access memory if we need to
    if (ex_mem.mem_write || ex_mem.mem_read) {
        // For stores, first read the current memory value for partial word operations
        if (ex_mem.mem_write) {
            // First read stall
            success = memory->access(ex_mem.alu_result, mem_data, 0, 1, 0);
            if (!success) return false;  // Stall if memory busy

            // Use the forwarded value from WB stage if available
            uint32_t write_data_mem = ex_mem.write_data;
            if (mem_wb.valid && mem_wb.reg_write && mem_wb.write_reg != 0 && 
                mem_wb.write_reg == ex_mem.write_reg) {
                write_data_mem = mem_wb.mem_to_reg ? mem_wb.mem_read_data : mem_wb.alu_result;
                DEBUG(cout << "MEM: Forwarding " << write_data_mem << " from WB for store\n");
            }

            // Apply masking for partial word operations
            if (ex_mem.halfword) {
                write_data_mem = (mem_data & 0xffff0000) | (write_data_mem & 0xffff);
            } else if (ex_mem.byte) {
                write_data_mem = (mem_data & 0xffffff00) | (write_data_mem & 0xff);
            }
            
            // Add store operation stalls - reduced from 2 to 1
            mem_stall_cycles = 1;  // One cycle for store operation
            
            // Second stall for write
            success = memory->access(ex_mem.alu_result, mem_data, write_data_mem, 0, 1);
            if (!success) return false;
            
            // Save both the written value and address for forwarding
            ex_mem.mem_read_data = write_data_mem;
            DEBUG(cout << "MEM: Stored value " << write_data_mem << " at address 0x" << hex << ex_mem.alu_result << dec << "\n");
        }
        
        // For loads, perform the read with stall
        if (ex_mem.mem_read) {
            success = memory->access(ex_mem.alu_result, mem_data, 0, 1, 0);
            if (!success) return false;

            // Add load operation stalls - reduced from 3 to 2
            mem_stall_cycles = 2;  // Two cycles for load operation

            // Check if there's a pending store to the same address in WB stage
            if (mem_wb.valid && mem_wb.mem_write && mem_wb.alu_result == ex_mem.alu_result) {
                mem_data = mem_wb.mem_read_data;  // Forward the stored value
                DEBUG(cout << "MEM: Forwarding stored value " << mem_data << " from WB for load\n");
            }

            // Apply masking and sign extension for loads
            if (ex_mem.halfword) {
                uint16_t half = mem_data & 0xffff;
                mem_data = (half & 0x8000) ? (0xffff0000 | half) : half;
            } else if (ex_mem.byte) {
                uint8_t byte = mem_data & 0xff;
                mem_data = (byte & 0x80) ? (0xffffff00 | byte) : byte;
            }
            
            // Save the read value for forwarding
            ex_mem.mem_read_data = mem_data;
            DEBUG(cout << "MEM: Loaded value " << mem_data << " from address 0x" << hex << ex_mem.alu_result << dec << "\n");
        }
    }
    
    // Populate MEM/WB pipeline register
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.mem_to_reg = ex_mem.mem_to_reg;
    mem_wb.link = ex_mem.link;
    mem_wb.alu_result = ex_mem.alu_result;  // Save ALU result (address) for store-load forwarding
    mem_wb.write_reg = ex_mem.write_reg;
    mem_wb.pc_plus_4 = ex_mem.pc_branch;
    mem_wb.mem_read_data = ex_mem.mem_read_data;
    mem_wb.mem_write = ex_mem.mem_write;  // Save mem_write for store-load forwarding
    mem_wb.valid = true;
    
    // Clear EX/MEM register
    ex_mem.valid = false;
    return true;
}

// -------------------- WB Stage --------------------
void Processor::pipeline_WB() {
    if (!mem_wb.valid) return;
    
    uint32_t write_data = 0;
    if (mem_wb.link) {
        write_data = mem_wb.pc_plus_4 + 4;  // For link instructions, R[31] gets PC+8
    } else if (mem_wb.mem_to_reg) {
        write_data = mem_wb.mem_read_data;
    } else {
        write_data = mem_wb.alu_result;
    }
    
    if (mem_wb.reg_write && mem_wb.write_reg != 0) {  // Added check for non-zero register
        uint32_t dummy1, dummy2;
        regfile.access(0, 0, dummy1, dummy2, mem_wb.write_reg, true, write_data);
        DEBUG(cout << "WB: Writing " << write_data << " to R[" << mem_wb.write_reg << "]\n");
    }
    
    // Update the committed PC here
    regfile.pc = mem_wb.pc_plus_4;
    
    // Clear MEM/WB register
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
