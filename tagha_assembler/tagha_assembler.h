#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "../libharbol/harbol.h"
#include "../tagha.h"

//#define TASM_DEBUG

struct LabelInfo {
	struct HarbolByteBuffer Bytecode;
	uint64_t Addr;
	bool IsFunc : 1;
	bool IsNativeFunc : 1;
};

bool Label_Free(void *);

struct TaghaAsmbler {
	struct HarbolString OutputName, *ActiveFuncLabel, *Lexeme;
	struct HarbolLinkMap
		*LabelTable, *FuncTable, *VarTable,
		*Opcodes, *Registers
	;
	FILE *Src;
	char *Iter;
	size_t SrcSize, ProgramCounter, CurrLine;
	uint32_t Stacksize;
	uint8_t Safemode : 1;
};

bool TaghaAsm_ParseRegRegInstr(struct TaghaAsmbler *, bool);
bool TaghaAsm_ParseOneRegInstr(struct TaghaAsmbler *, bool);
bool TaghaAsm_ParseOneImmInstr(struct TaghaAsmbler *, bool);
bool TaghaAsm_ParseRegMemInstr(struct TaghaAsmbler *, bool);
bool TaghaAsm_ParseMemRegInstr(struct TaghaAsmbler *, bool);
bool TaghaAsm_ParseRegImmInstr(struct TaghaAsmbler *, bool);

bool TaghaAsm_Assemble(struct TaghaAsmbler *);


#ifdef __cplusplus
}
#endif