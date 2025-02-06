#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"


// 
struct IF_ID_Reg {
    uint32_t instruction;
    uint32_t pc;
};
struct ID_EX_Reg {
    control_t control;
    uint32_t pc;
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t immediate;
    int rs, rt, rd, shamt, funct;
};

struct EX_MEM_Reg {
    control_t control;
    uint32_t alu_result;
    uint32_t read_data_2; 
    int write_reg;
};

struct MEM_WB_Reg {
    control_t control;
    uint32_t mem_read_data;
    uint32_t alu_result;
    int write_reg;
};


class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        // add other structures as needed

        // pipelined processor

        // add private functions
        void single_cycle_processor_advance();
        void pipelined_processor_advance();
 
    public:
        Processor(Memory *mem) { regfile.pc = 0; memory = mem;}

        // Get PC
        uint32_t getPC() { return regfile.pc; }

        // Prints the Register File
        void printRegFile() { regfile.print(); }
        
        // Initializes the processor appropriately based on the optimization level
        void initialize(int opt_level);

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
