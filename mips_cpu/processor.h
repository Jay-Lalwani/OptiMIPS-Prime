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
        
        // New: A separate pointer for fetching instructions.
        uint32_t fetch_pc;
        
        // Pipeline register structures
        
        // IF/ID holds the fetched instruction and its PC+4.
        struct IF_ID {
            uint32_t instruction;
            uint32_t pc_plus_4; // Instruction’s PC + 4
            bool valid;
        };
        
        // ID/EX holds the decoded fields, control signals, and register data.
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
            uint32_t pc_plus_4;  // For sequential commit (instruction’s PC + 4)
            uint32_t read_data_1;
            uint32_t read_data_2;
            uint32_t imm;
            int rs;
            int rt;
            int rd;
            int opcode;          // New: store opcode for ALU control generation
            uint32_t shamt;
            uint32_t funct;      // For ALU control (e.g., R-type)
            bool valid;
        };
        
        // EX/MEM holds results from ALU and related control signals.
        struct EX_MEM {
            // Control signals for MEM and WB stages
            bool reg_write;
            bool mem_read;
            bool mem_write;
            bool mem_to_reg;
            bool link;
            bool halfword;
            bool byte;
            // Data fields
            uint32_t alu_result;
            uint32_t write_data;  // Data to be written to memory (for stores)
            int write_reg;        // Destination register number
            uint32_t pc_branch;   // Holds the next-PC value to be committed
            bool zero;            // Zero flag from ALU
            bool valid;
        };
        
        // MEM/WB holds data to be written back.
        struct MEM_WB {
            // Control signals for WB stage
            bool reg_write;
            bool mem_to_reg;
            bool link;
            // Data fields
            uint32_t mem_read_data;
            uint32_t alu_result;
            int write_reg;
            uint32_t pc_plus_4;  // For committing the next sequential PC (or branch/jump target)
            bool valid;
        };
        
        // Pipeline registers (latches)
        IF_ID if_id;
        ID_EX id_ex;
        EX_MEM ex_mem;
        MEM_WB mem_wb;
        
        // Pipeline stage functions (called in reverse order)
        void pipeline_WB();
        bool pipeline_MEM(); // Returns false if a memory stall occurs
        void pipeline_EX();
        void pipeline_ID();
        void pipeline_IF();
        
        // Flush IF/ID and ID/EX (for branch/jump mispredictions)
        void flush_IF_ID_ID_EX();
        
        // Single-cycle processor advance (unchanged baseline)
        void single_cycle_processor_advance();
        
        // Pipelined processor advance (for –O1)
        void pipelined_processor_advance();
 
    public:
        Processor(Memory *mem) { 
            regfile.pc = 0; 
            fetch_pc = 0;   // Initialize the fetch pointer separately.
            memory = mem; 
            // Initialize pipeline registers as invalid.
            if_id.valid = false;
            id_ex.valid = false;
            ex_mem.valid = false;
            mem_wb.valid = false;
        }

        // Get the committed PC from the register file.
        uint32_t getPC() { return regfile.pc; }

        // Prints the register file.
        void printRegFile() { regfile.print(); }
        
        // Initializes the processor (e.g., setting opt_level).
        void initialize(int opt_level);

        // Advances the processor one cycle.
        void advance(); 
};

#endif
