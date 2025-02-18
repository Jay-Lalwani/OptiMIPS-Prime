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
    // Initialize the baseline control signals (they will be generated per instruction in the pipeline).
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
    // In pipelined mode, mark all pipeline registers as invalid.
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
        // Other optimization levels could be added here.
        default: break;
    }
}

// Pipelined processor: simulate one full cycle (WB, MEM, EX, ID, IF).
void Processor::pipelined_processor_advance() {
    // Process WB stage.
    pipeline_WB();
    
    // Process MEM stage.
    bool memStageCompleted = pipeline_MEM();
    if (!memStageCompleted) {
        DEBUG(cout << "Memory stall encountered. Pipeline is stalled.\n");
        return; // Stall the pipeline this cycle.
    }
    
    // Process EX stage.
    pipeline_EX();
    
    // Process ID stage.
    pipeline_ID();
    
    // Process IF stage.
    pipeline_IF();
}

// WB Stage: Write back the result from MEM/WB register to the register file.
void Processor::pipeline_WB() {
    if (mem_wb.valid) {
        uint32_t write_data = 0;
        if (mem_wb.link) {
            // For link instructions, write R[31] = PC+4+4.
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
    }
    // Clear MEM/WB after write-back.
    mem_wb.valid = false;
}

// MEM Stage: Perform memory read or write using data from EX/MEM.
bool Processor::pipeline_MEM() {
    if (ex_mem.valid) {
        uint32_t mem_data = 0;
        // Access memory for loads/stores.
        bool mem_access_success = memory->access(ex_mem.alu_result, mem_data, ex_mem.write_data,
                                                   ex_mem.mem_read, ex_mem.mem_write);
        if (!mem_access_success) {
            // A cache miss causes a stall.
            return false;
        }
        // Fill the MEM/WB pipeline register.
        mem_wb.reg_write    = ex_mem.reg_write;
        mem_wb.mem_to_reg   = ex_mem.mem_to_reg;
        mem_wb.link         = ex_mem.link;
        mem_wb.alu_result   = ex_mem.alu_result;
        mem_wb.write_reg    = ex_mem.write_reg;
        mem_wb.pc_plus_4    = ex_mem.pc_branch; // Saved from EX stage.
        mem_wb.mem_read_data = mem_data;
        mem_wb.valid = true;
        // Clear EX/MEM after transfer.
        ex_mem.valid = false;
    }
    return true;
}

// EX Stage: Execute the ALU operation using data from ID/EX.
void Processor::pipeline_EX() {
    if (id_ex.valid) {
        // Select ALU operands.
        uint32_t operand1 = id_ex.shift ? id_ex.shamt : id_ex.read_data_1;
        uint32_t operand2 = id_ex.ALU_src ? id_ex.imm : id_ex.read_data_2;
        uint32_t alu_zero = 0;
        
        // Set ALU control signals and perform the operation.
        alu.generate_control_inputs(id_ex.ALU_op, id_ex.funct, id_ex.opcode);
        uint32_t alu_result = alu.execute(operand1, operand2, alu_zero);
        
        // Compute branch target: PC+4 + (imm << 2)
        uint32_t branch_target = id_ex.pc_plus_4 + (id_ex.imm << 2);
        
        // Populate the EX/MEM pipeline register.
        ex_mem.reg_write   = id_ex.reg_write;
        ex_mem.mem_read    = id_ex.mem_read;
        ex_mem.mem_write   = id_ex.mem_write;
        ex_mem.mem_to_reg  = id_ex.mem_to_reg;
        ex_mem.branch      = id_ex.branch;
        ex_mem.jump        = id_ex.jump;
        ex_mem.jump_reg    = id_ex.jump_reg;
        ex_mem.link        = id_ex.link;
        ex_mem.alu_result  = alu_result;
        ex_mem.write_data  = id_ex.read_data_2; // For store instructions.
        // Determine destination: if link then R[31], else choose between rd and rt.
        ex_mem.write_reg   = id_ex.link ? 31 : (id_ex.reg_dest ? id_ex.rd : id_ex.rt);
        ex_mem.pc_branch   = id_ex.pc_plus_4; // Save PC+4 (used for link).
        ex_mem.zero        = (alu_zero == 1);
        ex_mem.valid       = true;
        
        // Handle control hazards:
        // If a branch is taken, update PC and flush IF/ID and ID/EX.
        if (id_ex.branch && ex_mem.zero) {
            regfile.pc = branch_target;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Branch taken to 0x" << std::hex << branch_target << std::dec << "\n");
        }
        else if (id_ex.jump) {
            // For jump instructions, compute target from the lower 26 bits.
            uint32_t jump_addr = (id_ex.pc_plus_4 & 0xF0000000) | ((id_ex.imm & 0x03FFFFFF) << 2);
            regfile.pc = jump_addr;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Jump to 0x" << std::hex << jump_addr << std::dec << "\n");
        }
        else if (id_ex.jump_reg) {
            regfile.pc = id_ex.read_data_1;
            flush_IF_ID_ID_EX();
            DEBUG(cout << "EX: Jump register to 0x" << std::hex << id_ex.read_data_1 << std::dec << "\n");
        }
    }
    // Clear ID/EX after execution.
    id_ex.valid = false;
}

// ID Stage: Decode the instruction from IF/ID, generate control signals, and read registers.
void Processor::pipeline_ID() {
    if (if_id.valid) {
        uint32_t instruction = if_id.instruction;
        // Decode fields.
        int opcode = (instruction >> 26) & 0x3F;
        int rs = (instruction >> 21) & 0x1F;
        int rt = (instruction >> 16) & 0x1F;
        int rd = (instruction >> 11) & 0x1F;
        uint32_t shamt = (instruction >> 6) & 0x1F;
        uint32_t funct = instruction & 0x3F;
        uint32_t imm = instruction & 0xFFFF;
        // Sign-extend immediate (except for instructions that require zero extension).
        if (!(opcode == 0xc || opcode == 0xd)) {
            if (imm & 0x8000) {
                imm |= 0xFFFF0000;
            }
        }
        
        // Decode control signals.
        control.decode(instruction);
        DEBUG(control.print());
        
        // Read registers.
        uint32_t read_data_1 = 0, read_data_2 = 0;
        regfile.access(rs, rt, read_data_1, read_data_2, 0, false, 0);
        
        // Populate ID/EX.
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
        id_ex.pc_plus_4   = if_id.pc_plus_4;
        id_ex.read_data_1 = read_data_1;
        id_ex.read_data_2 = read_data_2;
        id_ex.imm         = imm;
        id_ex.rs          = rs;
        id_ex.rt          = rt;
        id_ex.rd          = rd;
        id_ex.shamt       = shamt;
        id_ex.funct       = funct;
        id_ex.opcode      = opcode;
        id_ex.valid       = true;
        
        // Clear IF/ID after use.
        if_id.valid = false;
    }
}

// IF Stage: Fetch the instruction at the current PC.
void Processor::pipeline_IF() {
    if (extra_nop_triggered && extra_nop_count > 0) {
        if_id.instruction = 0x0;  // Force a NOP.
        if_id.pc_plus_4 = regfile.pc + 4; // PC remains unchanged.
        if_id.valid = true;
        extra_nop_count--;
        DEBUG(cout << "IF: Inserting extra NOP (" << extra_nop_count 
                   << " remaining). PC remains at 0x" << std::hex << regfile.pc << std::dec << "\n");
        return; // Do not advance PC.
    }

    uint32_t instruction = 0;
    bool fetchSuccess = memory->access(regfile.pc, instruction, 0, true, false);
    if (!fetchSuccess) {
        DEBUG(cout << "IF: Memory stall during fetch at PC 0x" << std::hex << regfile.pc << std::dec << "\n");
        return;
    }

    // If we see a 0x0 for the first time, trigger extra NOP mode.
    if (instruction == 0x0 && !extra_nop_triggered) {
        extra_nop_triggered = true;
        extra_nop_count = 5;
        DEBUG(cout << "IF: Fetched first NOP at PC 0x" << std::hex << regfile.pc 
                   << std::dec << ", triggering 5 extra NOP cycles (PC frozen).\n");
        // Insert this fetched NOP into the IF/ID latch without advancing PC.
        if_id.instruction = 0x0;
        if_id.pc_plus_4 = regfile.pc + 4;
        if_id.valid = true;
        return; // Do not update PC.
    }
    
    if_id.instruction = instruction;
    if_id.pc_plus_4 = regfile.pc + 4;
    if_id.valid = true;
    // Increment PC for the next fetch.
    regfile.pc += 4;
    DEBUG(cout << "IF: Fetched instruction 0x" << std::hex << instruction 
               << " from PC 0x" << (regfile.pc - 4) << std::dec << "\n");
}

// Flush the IF/ID and ID/EX pipeline registers (e.g., on branch or jump).
void Processor::flush_IF_ID_ID_EX() {
    if_id.valid = false;
    id_ex.valid = false;
}

// Single-cycle processor advance
void Processor::single_cycle_processor_advance() {
    // fetch
    uint32_t instruction;
    memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    // increment pc
    regfile.pc += 4;
    
    // decode into contol signals
    control.decode(instruction);
    DEBUG(control.print());

    // extract rs, rt, rd, imm, funct 
    int opcode = (instruction >> 26) & 0x3f;
    int rs = (instruction >> 21) & 0x1f;
    int rt = (instruction >> 16) & 0x1f;
    int rd = (instruction >> 11) & 0x1f;
    int shamt = (instruction >> 6) & 0x1f;
    int funct = instruction & 0x3f;
    uint32_t imm = (instruction & 0xffff);
    int addr = instruction & 0x3ffffff;
    // Variables to read data into
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    
    // Read from reg file
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    
    // Execution 
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    imm = control.zero_extend ? imm : (imm >> 15) ? 0xffff0000 | imm : imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type
    uint32_t operand_1 = control.shift ? shamt : read_data_1;
    uint32_t operand_2 = control.ALU_src ? imm : read_data_2;
    uint32_t alu_zero = 0;

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    
    uint32_t read_data_mem = 0;
    uint32_t write_data_mem = 0;

    // Memory
    // First read no matter whether it is a load or a store
    memory->access(alu_result, read_data_mem, 0, control.mem_read | control.mem_write, 0);
    // Stores: sb or sh mask and preserve original leftmost bits
    write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (read_data_2 & 0xff): read_data_2;
    // Write to memory only if mem_write is 1, i.e store
    memory->access(alu_result, read_data_mem, write_data_mem, control.mem_read, control.mem_write);
    // Loads: lbu or lhu modify read data by masking
    read_data_mem &= control.halfword ? 0xffff : control.byte ? 0xff : 0xffffffff;

    int write_reg = control.link ? 31 : control.reg_dest ? rd : rt;

    uint32_t write_data = control.link ? regfile.pc+8 : control.mem_to_reg ? read_data_mem : alu_result;  

    // Write Back
    regfile.access(0, 0, read_data_2, read_data_2, write_reg, control.reg_write, write_data);
    
    // Update PC
    regfile.pc += (control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero) ? imm << 2 : 0; 
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? (regfile.pc & 0xf0000000) & (addr << 2): regfile.pc;
}
