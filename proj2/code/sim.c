#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define MEM_WORDS 2048
#define NUM_REGS 32
#define EMPTY_PC 0xFFFFFFFFu
#define NOP 0x00
#define ADD 0x02
#define ADDI 0x03
#define SUB 0x04
#define SUBI 0x05
#define MUL 0x06
#define MULI 0x07
#define DIV 0x08
#define DIVI 0x09
#define LD 0x0B
#define ST 0x0D
#define EXIT 0x0E
#define MOV 0x11
#define B 0x13
#define CBZ 0x15
#define CBNZ 0x17
#define CBNEG 0x19

typedef struct {
	int present;
	int valid;
	unsigned int pc;
	int op;
	int b1;
	int b2;
	int b3;
	int imm8;
	int imm16;
	int32_t a;
	int32_t b;
	int32_t result;
	int target;
	int taken;
} Stage;

const char *opName(int op){
	if(op == ADD)	return "ADD";
	if(op == ADDI)	return "ADDI";
	if(op == SUB)   return "SUB";
	if(op == SUBI)  return "SUBI";
	if(op == MUL)   return "MUL";
	if(op == MULI)  return "MULI";
	if(op == DIV)   return "DIV";
	if(op == DIVI)  return "DIVI";
	if(op == LD)    return "LD";
	if(op == ST)    return "ST";
	if(op == EXIT)  return "EXIT";
	if(op == MOV)   return "MOV";
	if(op == B)     return "B";
	if(op == CBZ)   return "CBZ";
	if(op == CBNZ)  return "CBNZ";
	if(op == CBNEG) return "CBNEG";
	return "NOP";
}

Stage makeStage(unsigned int pc, uint32_t word, int valid){
	Stage s;
	s.present = 1;
	s.valid = valid;
	s.pc = pc;
	s.op = (int)((word >> 24) & 0xFF);
	s.b1 = (int)((word >> 16) & 0xFF);
	s.b2 = (int)((word >> 8) & 0xFF);
	s.b3 = (int)(word & 0xFF);
	s.imm8 = (int)(int8_t)(word & 0xFF);
	s.imm16 = (int)(int16_t)(word & 0xFFFF);
	s.a = 0;
	s.b = 0;
	s.result = 0;
	s.target = 0;
	s.taken = 0;
	return s;
}

Stage emptyStage(void){
	Stage s = makeStage(0, 0, 0);
	s.present = 0;
	return s;
}

int getDest(const Stage *s){
	if(!s->present || !s->valid){
		return -1;
	}
	int op = s->op;
	if(op == ADD || op == ADDI || op == SUB || op == SUBI){
		return s->b1;
	}
	if(op == MUL || op == MULI || op == DIV || op == DIVI){
		return s->b1;
	}
	if(op == LD || op == MOV){
		return s->b1;
	}
	return -1;
}

unsigned int pcOf(const Stage *s){
	if(!s->present){
		return EMPTY_PC;
	}
	return s->pc;
}
/*------------------------------------------------------------------------
	Main Function Definition
------------------------------------------------------------------------*/
int main(int argc,char **argv) {
	// TODO: Code your simulator here
	if(argc != 2){
		printf("Usage: %s <init_mem_file>\n", argv[0]);
		return 1;
	}
	uint32_t memory[MEM_WORDS];
	for(int i = 0; i < MEM_WORDS; i++){
		memory[i] = 0;
	}
	FILE *inFile = fopen(argv[1], "r");
	if(inFile == NULL){
		printf("Error: unable to open %s\n", argv[1]);
		return 1;
	}
	unsigned long long address, value;
	while(fscanf(inFile, "%llx %llx", &address, &value) == 2){
		memory[(address / 4) % MEM_WORDS] = (uint32_t)value;
	}
	fclose(inFile);
	int32_t regs[NUM_REGS];
	for(int i = 0; i < NUM_REGS; i++){
		regs[i] = 0;
	}
	int opCounts[32];
	for(int i = 0; i < 32; i++){
		opCounts[i] = 0;
	}
	int totalInstructions = 0;
	unsigned int pc = 0;
	Stage drStage  = emptyStage();
	Stage ex1Stage = emptyStage();
	Stage ex2Stage = emptyStage();
	Stage memStage = emptyStage();
	Stage wbStage  = emptyStage();
	int cycle = 0;
	printf("== cycle |    fetch |   decode |  exec1   |  exec2   |   memory | writeBack|\n");
	printf("== ------+----------+----------+----------+----------+----------+----------|\n");
	while(1){
		printf("== %5d | %8x | %8x | %8x | %8x | %8x | %8x |\n",
			cycle, pc, pcOf(&drStage), pcOf(&ex1Stage),
			pcOf(&ex2Stage), pcOf(&memStage), pcOf(&wbStage));
		if(wbStage.present && wbStage.valid){
			int dest = getDest(&wbStage);
			if(dest != -1){
				regs[dest] = wbStage.result;
			}
			if(wbStage.op != NOP){
				if(wbStage.op < 32){
					opCounts[wbStage.op]++;
				}
				totalInstructions++;
			}
			if(wbStage.op == EXIT){
				cycle++;
				break;
			}
		}
		if(memStage.present && memStage.valid){
			if(memStage.op == LD){
				memStage.result = (int32_t)memory[((uint32_t)memStage.result / 4) % MEM_WORDS];
			}
			if(memStage.op == ST){
				memory[((uint32_t)memStage.result / 4) % MEM_WORDS] = (uint32_t)memStage.a;
			}
		}
		int branchTaken = 0;
		if(ex2Stage.present && ex2Stage.valid){
			int op = ex2Stage.op;
			if(op == B){
				ex2Stage.taken = 1;
			}
			if(op == CBZ && ex2Stage.a == 0){
				ex2Stage.taken = 1;
			}
			if(op == CBNZ && ex2Stage.a != 0){
				ex2Stage.taken = 1;
			}
			if(op == CBNEG && ex2Stage.a < 0){
				ex2Stage.taken = 1;
			}
			branchTaken = ex2Stage.taken;
		}
		if(ex1Stage.present && ex1Stage.valid){
			int op = ex1Stage.op;
			int32_t a = ex1Stage.a;
			int32_t b = ex1Stage.b;
			int32_t imm8 = ex1Stage.imm8;
			if(op == ADD){
				ex1Stage.result = (int32_t)((uint32_t)a + (uint32_t)b);
			}else if(op == ADDI){
				ex1Stage.result = (int32_t)((uint32_t)a + (uint32_t)imm8);
			}else if(op == SUB){
				ex1Stage.result = (int32_t)((uint32_t)a - (uint32_t)b);
			}else if(op == SUBI){
				ex1Stage.result = (int32_t)((uint32_t)a - (uint32_t)imm8);
			}else if(op == MUL){
				ex1Stage.result = (int32_t)((uint32_t)a * (uint32_t)b);
			}else if(op == MULI){
				ex1Stage.result = (int32_t)((uint32_t)a * (uint32_t)imm8);
			}else if(op == DIV || op == DIVI){
				int32_t divisor = (op == DIV) ? b : imm8;
				if(divisor == 0){
					ex1Stage.result = 0;
				}else if(a == INT32_MIN && divisor == -1){
					ex1Stage.result = INT32_MIN;
				}else{
					ex1Stage.result = a / divisor;
				}
			}else if(op == LD || op == ST){
				ex1Stage.result = (int32_t)((uint32_t)b + (uint32_t)imm8);
			}else if(op == MOV){
				ex1Stage.result = ex1Stage.imm16;
			}else if(op == B || op == CBZ || op == CBNZ || op == CBNEG){
				ex1Stage.target = (int)ex1Stage.pc + 4 * ex1Stage.imm16;
			}
		}
		int stall = 0;
		if(drStage.present){
			int sources[2];
			int numSources = 0;
			int drOp = drStage.op;
			if(!drStage.valid){
				numSources = 0;
			}else if(drOp == ADD || drOp == SUB || drOp == MUL || drOp == DIV){
				sources[0] = drStage.b2;
				sources[1] = drStage.b3;
				numSources = 2;
			}else if(drOp == ADDI || drOp == SUBI || drOp == MULI || drOp == DIVI || drOp == LD){
				sources[0] = drStage.b2;
				numSources = 1;
			}else if(drOp == ST){
				sources[0] = drStage.b1;
				sources[1] = drStage.b2;
				numSources = 2;
			}else if(drOp == CBZ || drOp == CBNZ || drOp == CBNEG){
				sources[0] = drStage.b1;
				numSources = 1;
			}
			for(int i = 0; i < numSources; i++){
				int regNumber = sources[i];
				if(stall){
					break;
				}
				if(getDest(&ex1Stage) == regNumber){
					printf("Data hazard... register X%d will be written by the instruction in the ex1 stage\n", regNumber);
					stall = 1;
				}else if(getDest(&ex2Stage) == regNumber){
					printf("Data hazard... register X%d will be written by the instruction in the ex2 stage\n", regNumber);
					stall = 1;
				}else if(getDest(&memStage) == regNumber){
					printf("Data hazard... register X%d will be written by the instruction in the mem stage\n", regNumber);
					stall = 1;
				}
			}
		}
		if(branchTaken){
			printf("Control hazard... squashing ID, and EX1 and resetting PC\n");
		}
		Stage nextWb = memStage;
		Stage nextMem = ex2Stage;
		Stage nextEx2;
		Stage nextEx1;
		Stage nextDr;
		if(branchTaken){
			nextEx2 = emptyStage();
			nextEx1 = emptyStage();
			nextDr = emptyStage();
			pc = (unsigned int)ex2Stage.target;
		}else if(stall){
			nextEx2 = ex1Stage;
			nextEx1 = makeStage(drStage.pc, 0, 0);
			nextDr = drStage;
		}else{
			nextEx2 = ex1Stage;
			if(drStage.present && drStage.valid){
				int op = drStage.op;
				if(op == ADD || op == SUB || op == MUL || op == DIV){
					drStage.a = regs[drStage.b2 % NUM_REGS];
					drStage.b = regs[drStage.b3 % NUM_REGS];
				}else if(op == ADDI || op == SUBI || op == MULI || op == DIVI){
					drStage.a = regs[drStage.b2 % NUM_REGS];
				}else if(op == LD){
					drStage.b = regs[drStage.b2 % NUM_REGS];
				}else if(op == ST){
					drStage.a = regs[drStage.b1 % NUM_REGS];
					drStage.b = regs[drStage.b2 % NUM_REGS];
				}else if(op == CBZ || op == CBNZ || op == CBNEG){
					drStage.a = regs[drStage.b1 % NUM_REGS];
				}
			}
			nextEx1 = drStage;
			uint32_t word = memory[(pc / 4) % MEM_WORDS];
			nextDr = makeStage(pc, word, 1);
			pc = pc + 4;
		}
		wbStage = nextWb;
		memStage = nextMem;
		ex2Stage = nextEx2;
		ex1Stage = nextEx1;
		drStage = nextDr;
		cycle++;
	}
	printf("=== Simulation Reports ===\n");
	printf("Non-Zero Registers....\n");
	for(int i = 0; i < NUM_REGS; i++){
		if(regs[i] != 0){
			printf("  X%d = %d\n", i, regs[i]);
		}
	}
	printf("Instruction Count Report...\n");
	printf("   OPCODE - CNT\n");
	for(int op = 0; op < 32; op++){
		if(opCounts[op] != 0){
			printf("%9s - %3d\n", opName(op), opCounts[op]);
		}
	}
	printf("\n");
	printf("Total Instructions=%d Cycles=%d IPC=%5.2f\n",
		totalInstructions, cycle, (double)totalInstructions / cycle);
	return 0;
}

