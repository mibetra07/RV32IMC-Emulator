
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>


enum opcode_decode {R = 0x33, I = 0x13, S = 0x23, L = 0x03, B = 0x63, JALR = 0x67, JAL = 0x6F, AUIPC = 0x17, LUI = 0x37};

/* Speicherabgebildete Ausgabe (Memory Mapped IO):
 * Ein per SB auf diese Adresse geschriebenes Byte wird als Zeichen auf der
 * Standardausgabe ausgegeben, statt in den Datenspeicher zu wandern. */
#define MMIO_OUT_ADDR 0x5000u

typedef struct {
    size_t data_mem_size_;
    uint32_t regfile_[32];
    uint32_t pc_;
    uint8_t* instr_mem_;
    uint8_t* data_mem_;
} CPU;

void CPU_open_instruction_mem(CPU* cpu, const char* filename);
void CPU_load_data_mem(CPU* cpu, const char* filename);

CPU* CPU_init(const char* path_to_inst_mem, const char* path_to_data_mem) {
	CPU* cpu = (CPU*) malloc(sizeof(CPU));
	cpu->data_mem_size_ = 0x400000;
    cpu->pc_ = 0x0;
    CPU_open_instruction_mem(cpu, path_to_inst_mem);
    CPU_load_data_mem(cpu, path_to_data_mem);
    return cpu;
}

void CPU_open_instruction_mem(CPU* cpu, const char* filename) {
	uint32_t  instr_mem_size;
	FILE* input_file = fopen(filename, "r");
	if (!input_file) {
			printf("no input\n");
			exit(EXIT_FAILURE);
	}
	struct stat sb;
	if (stat(filename, &sb) == -1) {
			printf("error stat\n");
			perror("stat");
		    exit(EXIT_FAILURE);
	}
	printf("size of instruction memory: %ld Byte\n\n",(long)sb.st_size);
	instr_mem_size =  sb.st_size;
	cpu->instr_mem_ = malloc(instr_mem_size);
	fread(cpu->instr_mem_, sb.st_size, 1, input_file);
	fclose(input_file);
	return;
}

void CPU_load_data_mem(CPU* cpu, const char* filename) {
	FILE* input_file = fopen(filename, "r");
	if (!input_file) {
			printf("no input\n");
			exit(EXIT_FAILURE);
	}
	struct stat sb;
	if (stat(filename, &sb) == -1) {
			printf("error stat\n");
			perror("stat");
		    exit(EXIT_FAILURE);
	}
	printf("read data for data memory: %ld Byte\n\n",(long)sb.st_size);

    cpu->data_mem_ = malloc(cpu->data_mem_size_);
	fread(cpu->data_mem_, sb.st_size, 1, input_file);
	fclose(input_file);
	return;
}


/**
 * Speichert ein Byte (Befehl SB) -- VORGEGEBEN, bitte nicht veraendern.
 *
 * Liegt die Zieladresse auf der speicherabgebildeten Ausgabeadresse
 * MMIO_OUT_ADDR (0x5000), wird das Byte als Zeichen auf der Standardausgabe
 * ausgegeben (Zeichenausgabe / Memory Mapped IO). Andernfalls wird es ganz
 * normal in den Datenspeicher geschrieben.
 *
 * Rufen Sie diese Funktion bei der Implementierung des Befehls SB auf:
 *     CPU_store_byte(cpu, cpu->regfile_[rs1] + imm, (uint8_t)cpu->regfile_[rs2]);
 * Saemtliche Textausgabe der Testprogramme laeuft ueber diese Ausgabe.
 */
void CPU_store_byte(CPU* cpu, uint32_t addr, uint8_t value) {
	if (addr == MMIO_OUT_ADDR) {
		putchar((int)value);
	} else {
		cpu->data_mem_[addr] = value;
	}
}

/**
 * Instruction fetch Instruction decode, Execute, Memory access, Write back
 */
void CPU_execute(CPU* cpu) {

	uint32_t instruction = *(uint32_t*)(cpu->instr_mem_ + (cpu->pc_  & 0xFFFFF));
	uint32_t opcode = instruction & 0x7F;

	switch (opcode) {
		case 0x33: { //(R-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t rs2 = (instruction >> 20) & 0x1F;
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			uint32_t funct7 = (instruction >> 25) & 0x7F; //function 7

			if (rd != 0) //if not r[0] -> read-only
				switch (funct3) {
				case 0x0: { //ADD/SUB
					switch (funct7) {
						case 0x0: { //ADD
							cpu->regfile_[rd] = cpu->regfile_[rs1] + cpu->regfile_[rs2]; //schreibe rs1 + rs2 in rd
							break;
						}
						case 0x20: { //SUB
							cpu->regfile_[rd] = cpu->regfile_[rs1] - cpu->regfile_[rs2]; //schreibe rs1 - rs2 in rd
							break;
						}
						default: break;
					}
					break;
				}
				case 0x1: { //SLL
					cpu->regfile_[rd] = cpu->regfile_[rs1] << (cpu->regfile_[rs2] & 0x1F); //schreibe rs1 << rs2(unterste 5 bits) in rd
					break;
				}
				case 0x2: { //SLT
					if ((int32_t)(cpu->regfile_[rs1]) < (int32_t)(cpu->regfile_[rs2])) //if rs1 < rs2 (signed)
						cpu->regfile_[rd] = 0x1; //rd=1
					else
						cpu->regfile_[rd] = 0x0; //rd=0
					break;
				}
				case 0x3: { //SLTU
					if (cpu->regfile_[rs1] < cpu->regfile_[rs2]) //if rs1 < rs2 (unsigned)
						cpu->regfile_[rd] = 0x1; //rd=1
					else
						cpu->regfile_[rd] = 0x0; //rd=0
					break;
				}
				case 0x4: { //XOR
					cpu->regfile_[rd] = cpu->regfile_[rs1] ^ cpu->regfile_[rs2]; //rd = rs1 ^ rs2
					break;
				}
				case 0x5: { //SRL/SRA
					switch (funct7) {
						case 0x0: { //SRL
							cpu->regfile_[rd] = cpu->regfile_[rs1] >> (cpu->regfile_[rs2] & 0x1F); //logically shift rs1 right rs2(5 lowest bits)
							break;
						}
						case 0x20: { //SRA
							cpu->regfile_[rd] = (uint32_t)((int32_t)cpu->regfile_[rs1] >> (cpu->regfile_[rs2] & 0x1F)); //arithmatically shift rs1 right by rs2(5 lowest bits)
							break;
						}
						default: break;
					}
					break;
				}
				case 0x6: { //OR
					cpu->regfile_[rd] = cpu->regfile_[rs1] | cpu->regfile_[rs2]; //rd = rs1 | rs2
					break;
				}
				case 0x7: { //AND
					cpu->regfile_[rd] = cpu->regfile_[rs1] & cpu->regfile_[rs2]; //rd = rs1 & rs2
					break;
				}
				default: break;
				}
			cpu->pc_ += 4;
			break;
		}
		case 0x13: { //ADDI (I-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			uint32_t imm_u = (instruction >> 20) & 0xFFF; //12 bit immediate
			int32_t imm = (int32_t)instruction >> 20; //signed conversion from 12 to 32 bit

			if (rd != 0) //if not r[0] -> read-only
				switch (funct3) {
				case 0x0: { //ADDI
					cpu->regfile_[rd] = cpu->regfile_[rs1] + imm; //rd = rs1 + imm
				}
				case 0x1: { //SLLI

					break;
				}
				case 0x2: { //SLTI

					break;
				}
				case 0x3: { //SLTIU

					break;
				}
				case 0x4: { //XORI
					cpu->regfile_[rd] = cpu->regfile_[rs1] ^ imm; //rd = rs1 ^ imm
					break;
				}
				case 0x5: { //ORI
					cpu->regfile_[rd] = cpu->regfile_[rs1] | imm; //rd = rs1 | imm
					break;
				}
				case 0x6: { //SRLI/SRAI
					uint32_t funct7 = (instruction >> 25) & 0x7F; //function 7
					break;
				}
				case 0x7: { //ANDI
					cpu->regfile_[rd] = cpu->regfile_[rs1] & imm; //rd = rs1 & imm
					break;
				}
				default: break;
				}

			cpu->pc_ += 4; //zur nächsten instruction springen (4 byte -> pc+4)
			break;
		}
		case 0x37: { // LUI (U-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			if (rd != 0)
				cpu->regfile_[rd] = instruction & 0xFFFFF000;
			cpu->pc_ += 4;
			break;
		}
		case 0x23: { // SB (S-Format, funct3 == 0)
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t rs2 = (instruction >> 20) & 0x1F;
			uint32_t imm_lo = (instruction >> 7) & 0x1F; // imm[4:0]
			uint32_t imm_hi = (instruction >> 25) & 0x7F; // imm[11:5]
			uint32_t imm_u = (imm_hi << 5) | imm_lo;
			int32_t imm = (imm_u & 0x800) ? (int32_t)(imm_u | 0xFFFFF000)
			: (int32_t)imm_u;
			CPU_store_byte(cpu, cpu->regfile_[rs1] + imm,
			(uint8_t)cpu->regfile_[rs2]);
			cpu->pc_ += 4;
			break;
		}
		default:
			// Unbekannter Befehl: pc bleibt stehen -> Programm haelt an
			break;
	}


}

int main(int argc, char* argv[]) {
	printf("C Praktikum\nHU Risc-V  Emulator 2026\n");

	CPU* cpu_inst;

	cpu_inst = CPU_init(argv[1], argv[2]);
    for(uint32_t i = 0; i <1000000; i++) { // Hauptschleife: fuehrt Befehle aus, bis die Obergrenze erreicht ist
    	CPU_execute(cpu_inst);
    }

	printf("\n-----------------------RISC-V program terminate------------------------\nRegfile values:\n");

	//output Regfile
	for(uint32_t i = 0; i <= 31; i++) {
    	printf("%d: %X\n",i,cpu_inst->regfile_[i]);
    }
    fflush(stdout);

	return 0;
}
