
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

/*
* Erweitert einen n-Bit-Wert auf 32 Bit unter Beruecksichtigung
* des Vorzeichens. Bit n-1 wird als Vorzeichenbit interpretiert.
*/
int32_t sign_extend(uint32_t value, int n) {
	uint32_t mask = 1u << (n - 1);
	return (int32_t)((value ^ mask) - mask);
}

/*
* Expandiert einen 16-Bit-Compressed-Befehl in sein 32-Bit-
* Aequivalent. Gibt 0 zurueck, falls der Befehl ungueltig ist.
*/
uint32_t expand_compressed(uint16_t c) {
	uint8_t op = c & 0x3; // op[1:0] -> Quadrant
	uint8_t funct3 = (c >> 13) & 0x7; // funct3 ueber alle Quadranten
	switch (op) {
		case 0x0: {
			// Quadrant 0
			switch (funct3) {
				case 0x0: {
					// C.ADDI4SPN
					uint32_t nzuimm = (((c >> 7) & 0xF) << 6 |
										((c >> 11) & 0x3) << 4 |
										(c >> 5 & 0x1) << 3 |
										(c >> 6) & 0x1) << 2;
					uint32_t rd = ((c >> 2) & 0x7) + 8;
					//create 32bit inst
					//funct3=000 --> omitted
					return (uint32_t)(nzuimm << 20) | //imm
							(0x2 << 15) | //R2
							(rd << 7) | //rd
							0x13; //opcode
				}
				case 0x2: {
					// C.LW

					return;
				}
				case 0x6: {
					// C.SW

					return;
				}
				default: return 0;
			}
			break;
		}
		case 0x1: {
			// Quadrant 1
			uint8_t funct2 = (c >> 10) & 0x3;
			switch (funct3) {
				case 0x0: { //(C.NOP/C.ADDI

				}
				case 0x1: { //C.JAL

					return;
				}
				case 0x2: { //C.LI

					return;
				}
				case 0x3: { //C.ADDI16SP/C.LUI

					return;
				}
				case 0x4: {
					switch (funct2) {
						case 0x0: { //C.SRLI

							return;
						}
						case 0x1: { //C.SRAI
							return;
						}
						case 0x2: { //C.ANDI
							return;
						}
						case 0x3: {
							funct2 = (c >> 5) & 0x3;
							switch (funct2) {
								case 0x0: { //C.SUB

									return;
								}
								case 0x1: { //C.XOR

									return;
								}
								case 0x2: { //C.OR

									return;
								}
								case 0x3: { //C.AND

									return;
								}
								default: return 0;
							}
						}
						default: return 0;
					}
					return;
				}
				case 0x5: {//C.J

					return;
				}
				case 0x6: { //C.BEQZ

					return;
				}
				case 0x7: { //C.BNEZ

					return;
				}
			}
			break;
		}
		case 0x2: {
			// Quadrant 2
			switch (funct3) {
				case 0x0: { //C.SLLI

					return;
				}
				case 0x2: { //C.LWSP

					return;
				}
				case 0x4: {
					if((c >> 12) & 0x1) { //C.EBREAK, C.JALR, C.ADD

						return;
					}
					//C.JR, C.MV

					return;
				}
				case 0x6: { //C.SWSP

					return;
				}
				default: break;
			}
			// C.ADD, C.SWSP
			break;
		}
	}
	return 0; // Ungueltige Instruktion
}

/**
 * Instruction fetch Instruction decode, Execute, Memory access, Write back
 */
void CPU_execute(CPU* cpu) {
	uint16_t lower = *(uint16_t *)(cpu->instr_mem_ + (cpu->pc_ & 0xFFFFF));
	uint32_t instruction;
	int length;
	if ((lower & 0x3) == 0x3) {
		/* 32-Bit-Befehl */
		instruction = *(uint32_t*)(cpu->instr_mem_ + (cpu->pc_  & 0xFFFFF));
		length = 4;
	} else {
		/* 16-Bit-Befehl: expandieren */
		instruction = expand_compressed(lower);
		length = 2;
	}
	uint32_t opcode = instruction & 0x7F;

	switch (opcode) {
		case 0x33: { //(R-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t rs2 = (instruction >> 20) & 0x1F; //register source 2
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			uint32_t funct7 = (instruction >> 25) & 0x7F; //function 7

			if (rd != 0) //if not r[0] -> read-only
				switch (funct3) {
				case 0x0: { //ADD/SUB/MUL
					switch (funct7) {
						case 0x0: { //ADD
							cpu->regfile_[rd] = cpu->regfile_[rs1] + cpu->regfile_[rs2]; //schreibe rs1 + rs2 in rd
							break;
						}
						case 0x20: { //SUB
							cpu->regfile_[rd] = cpu->regfile_[rs1] - cpu->regfile_[rs2]; //schreibe rs1 - rs2 in rd
							break;
						}
						case 0x1: { //MUL
							cpu->regfile_[rd] = (uint32_t)(((int64_t)(int32_t)cpu->regfile_[rs1] *
															(int64_t)(int32_t)cpu->regfile_[rs2]));
							break;
						}
						default: break;
					}
					break;
				}
				case 0x1: { //SLL/MULH
					switch (funct7) {
						case 0x0: { //SLL
							cpu->regfile_[rd] = cpu->regfile_[rs1] << (cpu->regfile_[rs2] & 0x1F); //schreibe rs1 << rs2(unterste 5 bits) in rd
							break;
						}
						case 0x1: { //MULH
							cpu->regfile_[rd] = ((int64_t)(int32_t)(cpu->regfile_[rs1]) *
												 (int64_t)(int32_t)(cpu->regfile_[rs2])) >> 32;
							break;
						}
						default: break;
					}
					break;
				}
				case 0x2: { //SLT/MULHSU
					switch (funct7) {
						case 0x0: { //SLT
							if ((int32_t)cpu->regfile_[rs1] < (int32_t)cpu->regfile_[rs2]) //if rs1 < rs2 (signed)
								cpu->regfile_[rd] = 0x1; //rd=1
							else
								cpu->regfile_[rd] = 0x0; //rd=0
							break;
						}
						case 0x1: { //MULHSU
							cpu->regfile_[rd] = ((int64_t)(int32_t)cpu->regfile_[rs1] * (uint64_t)cpu->regfile_[rs2]) >> 32;
							break;
						}
						default: break;
					}
					break;
				}
				case 0x3: { //SLTU/MULHU
					switch (funct7) {
						case 0x0: { //SLTU
							if (cpu->regfile_[rs1] < cpu->regfile_[rs2]) //if rs1 < rs2 (unsigned)
								cpu->regfile_[rd] = 0x1; //rd=1
							else
								cpu->regfile_[rd] = 0x0; //rd=0
							break;
						}
						case 0x1: { //MULHU
							cpu->regfile_[rd] = ((uint64_t)cpu->regfile_[rs1] * (uint64_t)cpu->regfile_[rs2]) >> 32;
							break;
						}
						default: break;
					}
					break;
				}
				case 0x4: { //XOR/DIV
					switch (funct7) {
						case 0x0: { //XOR
							cpu->regfile_[rd] = cpu->regfile_[rs1] ^ cpu->regfile_[rs2]; //rd = rs1 ^ rs2
							break;
						}
						case 0x1: { //DIV
							if (cpu->regfile_[rs2] == 0) //cant divide by 0
								cpu->regfile_[rd] = 0xFFFFFFFF; //fill with 1s
							else if ((int32_t)cpu->regfile_[rs1] == INT32_MIN && (int32_t)cpu->regfile_[rs2] == -1) //Integer overflow
								cpu->regfile_[rd] = 0x80000000;
							else
								cpu->regfile_[rd] = (int32_t)cpu->regfile_[rs1] / (int32_t)cpu->regfile_[rs2]; //signed division
						}
						default: break;
					}
					break;
				}
				case 0x5: { //SRL/SRA/DIVU
					switch (funct7) {
						case 0x0: { //SRL
							cpu->regfile_[rd] = cpu->regfile_[rs1] >> (cpu->regfile_[rs2] & 0x1F); //logically shift rs1 right rs2(5 lowest bits)
							break;
						}
						case 0x20: { //SRA
							cpu->regfile_[rd] = (uint32_t)((int32_t)cpu->regfile_[rs1] >> (cpu->regfile_[rs2] & 0x1F)); //arithmetically shift rs1 right by rs2(5 lowest bits)
							break;
						}
						case 0x1: { //DIVU
							if (cpu->regfile_[rs2] == 0) //cant divide by 0
								cpu->regfile_[rd] = 0xFFFFFFFF; //fill with 1s
							else
								cpu->regfile_[rd] = cpu->regfile_[rs1] / cpu->regfile_[rs2]; //unsigned division
							break;
						}
						default: break;
					}
					break;
				}
				case 0x6: { //OR/REM
					switch (funct7) {
						case 0x0: { //OR
							cpu->regfile_[rd] = cpu->regfile_[rs1] | cpu->regfile_[rs2]; //rd = rs1 | rs2
							break;
						}
						case 0x1: { //REM
							if (cpu->regfile_[rs2] == 0) //cant divide by 0
								cpu->regfile_[rd] = cpu->regfile_[rs1];
							else if ((int32_t)cpu->regfile_[rs1] == INT32_MIN && (int32_t)cpu->regfile_[rs2] == -1) //Integer overflow
								cpu->regfile_[rd] = 0;
							else
								cpu->regfile_[rd] = (int32_t)cpu->regfile_[rs1] % (int32_t)cpu->regfile_[rs2]; //signed modulo
							break;
						}
						default: break;
					}
					break;
				}
				case 0x7: { //AND/REMU
					switch (funct7) {
						case 0x0: { //AND
							cpu->regfile_[rd] = cpu->regfile_[rs1] & cpu->regfile_[rs2]; //rd = rs1 & rs2
							break;
						}
						case 0x1: { //REMU
							if (cpu->regfile_[rs2] == 0) //cant divide by 0
								cpu->regfile_[rd] = cpu->regfile_[rs1];
							else
								cpu->regfile_[rd] = cpu->regfile_[rs1] % cpu->regfile_[rs2]; //unsigned modulo
							break;
						}
						default: break;
					}
					break;
				}
				default: break;
				}
			cpu->pc_ += length;
			break;
		}
		case 0x13: { //(I-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			int32_t imm = (int32_t)instruction >> 20; //signed conversion from 12 to 32 bit

			if (rd != 0) //if not r[0] -> read-only
				switch (funct3) {
				case 0x0: { //ADDI
					cpu->regfile_[rd] = cpu->regfile_[rs1] + imm; //rd = rs1 + imm
					break;
				}
				case 0x1: { //SLLI
					cpu->regfile_[rd] = cpu->regfile_[rs1] << (imm & 0x1F); //rd = rs1 << shamt
					break;
				}
				case 0x2: { //SLTI
					if ((int32_t)(cpu->regfile_[rs1]) < imm) //if rs1 < imm (signed)
						cpu->regfile_[rd] = 0x1; //rd=1
					else
						cpu->regfile_[rd] = 0x0; //rd=0
					break;
				}
				case 0x3: { //SLTIU
					if (cpu->regfile_[rs1] < (uint32_t)imm) //if rs1 < imm (unsigned)
						cpu->regfile_[rd] = 0x1; //rd=1
					else
						cpu->regfile_[rd] = 0x0; //rd=0
					break;
				}
				case 0x4: { //XORI
					cpu->regfile_[rd] = cpu->regfile_[rs1] ^ imm; //rd = rs1 ^ imm
					break;
				}
				case 0x5: { //SRLI/SRAI
					uint32_t funct7 = (instruction >> 25) & 0x7F; //function 7
					switch (funct7) {
						case 0x0: { //SRLI
							cpu->regfile_[rd] = cpu->regfile_[rs1] >> (imm & 0x1F); //logically shift rs1 right by shamt
							break;
						}
						case 0x20: { //SRAI
							cpu->regfile_[rd] = (uint32_t)((int32_t)cpu->regfile_[rs1] >> (imm & 0x1F)); //arithmetically shift rs1 right by shamt
							break;
						}
						default: break;
					}
					break;
				}
				case 0x6: { //ORI
					cpu->regfile_[rd] = cpu->regfile_[rs1] | imm; //rd = rs1 | imm
					break;
				}
				case 0x7: { //ANDI
					cpu->regfile_[rd] = cpu->regfile_[rs1] & imm; //rd = rs1 & imm
					break;
				}
				default: break;
				}

			cpu->pc_ += length; //zur nächsten instruction springen (4 byte -> pc+4)
			break;
		}
		case 0x3: { //I-Format
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			int32_t imm = (int32_t)instruction >> 20; //signed conversion from 12 to 32 bit
			if (rd != 0)
				switch (funct3) {
				case 0x0: { //LB
					cpu->regfile_[rd] = (int32_t)(int8_t)(cpu->data_mem_[cpu->regfile_[rs1] + imm]);
					break;
				}
				case 0x1: { //LH
					cpu->regfile_[rd] = (int32_t)(int16_t)*(uint16_t*)(cpu->data_mem_ + cpu->regfile_[rs1] + imm);
					break;
				}
				case 0x2: { //LW
					cpu->regfile_[rd] = *(uint32_t*)(cpu->data_mem_ + cpu->regfile_[rs1] + imm);
					break;
				}
				case 0x4: { //LBU
					cpu->regfile_[rd] = cpu->data_mem_[cpu->regfile_[rs1] + imm];
					break;
				}
				case 0x5: { //LHU
					cpu->regfile_[rd] = *(uint16_t*)(cpu->data_mem_ + cpu->regfile_[rs1] + imm);
					break;
				}
				default: break;
				}
			cpu->pc_ += length;
			break;
		}
		case 0x67: { //JALR (I-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			int32_t imm = (int32_t)instruction >> 20; //signed conversion from 12 to 32 bit
			if (rd != 0)
				cpu->regfile_[rd] = cpu->pc_ + 4;
			cpu->pc_ = (cpu->regfile_[rs1] + imm) & ~1;
			break;
		}
		case 0x23: { //(S-Format)
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t rs2 = (instruction >> 20) & 0x1F;
			uint32_t imm_lo = (instruction >> 7) & 0x1F; // imm[4:0]
			uint32_t imm_hi = (instruction >> 25) & 0x7F; // imm[11:5]
			uint32_t imm_u = (imm_hi << 5) | imm_lo;
			int32_t imm = (imm_u & 0x800) ? (int32_t)(imm_u | 0xFFFFF000)
			: (int32_t)imm_u;
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			switch (funct3) {
				case 0x0: { //SB
					CPU_store_byte(cpu, cpu->regfile_[rs1] + imm,(uint8_t)cpu->regfile_[rs2]);
					break;
				}
				case 0x1: { //SH
					*(uint16_t*)(cpu->data_mem_ + cpu->regfile_[rs1] + imm) = (uint16_t)cpu->regfile_[rs2];
					break;
				}
				case 0x2: { //SW
					*(uint32_t*)(cpu->data_mem_ + cpu->regfile_[rs1] + imm) = (uint32_t)cpu->regfile_[rs2];
					break;
				}
				default: break;
			}
			cpu->pc_ += length;
			break;
		}
		case 0x63: { //(B-Format)
			uint32_t rs1 = (instruction >> 15) & 0x1F; //register source 1
			uint32_t rs2 = (instruction >> 20) & 0x1F; //register source 2
			uint32_t funct3 = (instruction >> 12) & 0x7; //function 3
			//12 bit unsigned immediate
			uint32_t imm_u = ((instruction >> 31) << 12) | //imm 12
							(((instruction >> 7) & 0x1) << 11) | //imm 11
							(((instruction >> 25) & 0x3F) << 5) | //imm 10:5
							(((instruction >> 8) & 0xF) << 1); //imm 4:1
			int32_t imm = (imm_u & 0x1000) ? (int32_t)(imm_u | 0xFFFFE000) : (int32_t)imm_u; //signed conversion from 12 to 32 bit

			switch (funct3) {
				case 0x0: { //BEQ
					if (cpu->regfile_[rs1] == cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				case 0x1: { //BNE
					if (cpu->regfile_[rs1] != cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				case 0x4: { //BLT
					if ((int32_t)cpu->regfile_[rs1] < (int32_t)cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				case 0x5: { //BGE
					if ((int32_t)cpu->regfile_[rs1] >= (int32_t)cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				case 0x6: { //BLTU
					if (cpu->regfile_[rs1] < cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				case 0x7: { //BGEU
					if (cpu->regfile_[rs1] >= cpu->regfile_[rs2])
						cpu->pc_ += imm;
					else
						cpu->pc_ += length;
					break;
				}
				default: break;
			}
			break;
		}
		case 0x37: { // LUI (U-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			if (rd != 0)
				cpu->regfile_[rd] = instruction & 0xFFFFF000; //rd = upper 20 bits of imm
			cpu->pc_ += length;
			break;
		}
		case 0x17: { // AUIPC (U-Format)
			uint32_t rd = (instruction >> 7) & 0x1F; //register destination
			if (rd != 0)
				cpu->regfile_[rd] = cpu->pc_ + (instruction & 0xFFFFF000); //rd = pc + upper 20 bits of imm
			cpu->pc_ += length;
			break;
		}
		case 0x6F: { //JAL (J-Format)
			uint32_t rd = (instruction >> 7) & 0x1F;
			//21 bit unsigned immediate
			uint32_t imm_u = ((instruction >> 31) << 20) | //imm 20
							(((instruction >> 12) & 0xFF) << 12) | //imm 19:12
							(((instruction >> 20) & 0x1) << 11) | //imm 11
							(((instruction >> 21) & 0x3FF) << 1); //imm 10:1
			int32_t imm = (imm_u & 0x100000) ? (int32_t)(imm_u | 0xFFE00000) : (int32_t)imm_u; //signed conversion from 21 to 32 bit
			if (rd != 0)
				cpu->regfile_[rd] = cpu->pc_ + 4;
			cpu->pc_ += imm;
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
