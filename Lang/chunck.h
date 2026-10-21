#ifndef CELEX_SCRIPT_CHUNCK
#define CELEX_SCRIPT_CHUNCK

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define debug

typedef char byte;

typedef struct {
	byte *op_codes;
	uint32_t len;
	uint32_t size;
} Chunck;

typedef enum {
	OP_UPN, OP_UPS, OP_UPB, OP_UPV, OP_UPNIL,
	OP_GETVAR, OP_SETVAR, OP_GETREF, OP_SETREF,
	OP_ADD, OP_SUB, OP_MULT, OP_DIV,
	OP_EQ, OP_NE, OP_GT, OP_LT, OP_GE, OP_LE,
	OP_JMP, OP_JF,
	OP_PRINT, OP_INPUT,
	OP_DEF, OP_ASIGN, OP_IF, OP_GOTO, OP_GOTOB,
	OP_RETURN, OP_POP, OP_HALT
} op_code;

void addOpCode(op_code type);
void addUInt(uint32_t number);
void addInt32(int32_t number);
void addBytecode(byte code);
void setInt32(uint32_t pos, int32_t number);

void addData(byte *data, uint32_t len);

Chunck getChunck();
uint32_t chunkLength();

void initChunck();
void freeChunck();

#ifdef debug
void printChunck();
void printChunckSea(uint32_t start);
#endif

#endif
