#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"

class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        
        // A separate pointer for fetching instructions.
        uint32_t fetch_pc;
        
        // Pipeline register structures
        
        // IF/ID: Holds the fetched instruction and its PC+4.
        struct IF_ID {
            uint32_t instruction;
            uint32_t pc_plus_4;
            bool valid;
        };
        
        // ID/EX: Holds the decoded fields, control signals, and register data.
        struct ID_EX {
            // Control signals
            bool reg_dest;
            bool ALU_src;
            bool reg_write;
            bool mem_read;
            bool mem_write;
            bool mem_to_reg;
            unsigned ALU_op; // 2-bit value
            bool branch;
            bool jump;
            bool jump_reg;
            bool link;
            bool shift;
            bool zero_extend;
            bool bne;
            bool halfword;
            bool byte;
            // Data fields
            uint32_t pc_plus_4;  // For sequential commit
            uint32_t read_data_1;
            uint32_t read_data_2;
            uint32_t imm;
            int rs;
            int rt;
            int rd;
            int opcode;          // Saved for ALU control generation.
            uint32_t shamt;
            uint32_t funct;
            bool valid;
        };
        
        // EX/MEM: Holds results from ALU and related signals.
        struct EX_MEM {
            // Control signals for MEM and WB
            bool reg_write;
            bool mem_read;
            bool mem_write;
            bool mem_to_reg;
            bool link;
            bool halfword;
            bool byte;
            // Data fields
            uint32_t alu_result;
            uint32_t write_data;  // For store instructions.
            int write_reg;        // Destination register.
            uint32_t pc_branch;   // The PC to be committed (either sequential or branch target).
            bool zero;            // ALU zero flag.
            bool valid;
        };
        
        // MEM/WB: Holds data to be written back.
        struct MEM_WB {
            // Control signals for WB stage.
            bool reg_write;
            bool mem_to_reg;
            bool link;
            // Data fields.
            uint32_t mem_read_data;
            uint32_t alu_result;
            int write_reg;
            uint32_t pc_plus_4;   // Next sequential PC (or branch target).
            bool valid;
        };
        
        // Pipeline registers.
        IF_ID if_id;
        ID_EX id_ex;
        EX_MEM ex_mem;
        MEM_WB mem_wb;
        
        // Pipeline stage functions.
        void pipeline_IF();
        void pipeline_ID();
        void pipeline_EX();
        bool pipeline_MEM(); // Returns false to stall.
        void pipeline_WB();
        
        // Flush IF/ID and ID/EX (used on branch/jump mispredictions).
        void flush_IF_ID_ID_EX();
        
        // Single-cycle processor.
        void single_cycle_processor_advance();
        
        // Pipelined processor advance.
        void pipelined_processor_advance();
 
    public:
        Processor(Memory *mem) { 
            regfile.pc = 0; 
            fetch_pc = 0;   // Separate fetch pointer.
            memory = mem; 
            if_id.valid = false;
            id_ex.valid = false;
            ex_mem.valid = false;
            mem_wb.valid = false;
        }

        // Get the committed PC.
        uint32_t getPC() { return regfile.pc; }

        // Print the register file.
        void printRegFile() { regfile.print(); }
        
        // Initialize the processor.
        void initialize(int opt_level);

        // Advance one cycle.
        void advance(); 
};

#endif
