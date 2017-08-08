
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <iso646.h>
#include <inttypes.h>
#include <assert.h>
#include "vm.h"

/*	here's the deal ok? make an opcode for each and erry n-bytes!
 * 'q' = int64
 * 'l' - int32
 * 's' - int16
 * 'b' - byte | push and pop do not take bytes
 * 'f' - float32
 * 'df' - float64
 * 'a' - address
 * 'sp' - takes or uses the current stack pointer address.
 * 'ip' - takes/uses the current instruction pointer address.
*/

// this vm is designed to run C programs. Vastly, if not all C expressions are int32, uint32 if bigger than int
// this is why the arithmetic and bit operations are all int32 sized.
// there's 2 byte and single byte memory storage for the sake of dealing with structs and unions.
// expressions are int or uint then truncated to a variable's byte-width.
#define INSTR_SET	\
	X(halt) \
	X(pushl) X(pushs) X(pushb) X(pushsp) X(puship) X(pushbp) \
	X(pushspadd) X(pushspsub) X(pushbpadd) X(pushbpsub) \
	X(popl) X(pops) X(popb) X(popsp) X(popip) X(popbp) \
	X(wrtl) X(wrts) X(wrtb) \
	X(storel) X(stores) X(storeb) \
	X(storela) X(storesa) X(storeba) \
	X(storespl) X(storesps) X(storespb) \
	X(loadl) X(loads) X(loadb) \
	X(loadla) X(loadsa) X(loadba) \
	X(loadspl) X(loadsps) X(loadspb) \
	X(copyl) X(copys) X(copyb) \
	X(addl) X(uaddl) X(addf) \
	X(subl) X(usubl) X(subf) \
	X(mull) X(umull) X(mulf) \
	X(divl) X(udivl) X(divf) \
	X(modl) X(umodl) \
	X(andl) X(orl) X(xorl) \
	X(notl) X(shll) X(shrl) \
	X(incl) X(incf) X(decl) X(decf) X(negl) X(negf) \
	X(ltl) X(ultl) X(ltf) \
	X(gtl) X(ugtl) X(gtf) \
	X(cmpl) X(ucmpl) X(compf) \
	X(leql) X(uleql) X(leqf) \
	X(geql) X(ugeql) X(geqf) \
	X(jmp) X(jzl) X(jnzl) \
	X(call) X(calls) X(calla) X(ret) X(retx) X(reset) \
	X(callnat) \
	X(nop) \

#define X(x) x,
enum InstrSet { INSTR_SET };
#undef X

/* prototype ==> void PrintHelloWorld(void); */
static void NativePrintHelloWorld(TaghaVM_t *restrict vm)
{
	if( !vm )
		return;
	
	printf("hello world from bytecode!\n");
}

/* prototype ==> void TestArgs(int, short, char, float); */
static void NativeTestArgs(TaghaVM_t *restrict vm)
{
	if( !vm )
		return;
	
	int iInt = tagha_pop_long(vm);
	printf("NativeTestArgs Int: %i\n", iInt); 
	ushort sShort = tagha_pop_short(vm);
	printf("NativeTestArgs uShort: %u\n", sShort); 
	char cChar = tagha_pop_byte(vm);
	printf("NativeTestArgs Char: %i\n", cChar); 
	float fFloat = tagha_pop_float32(vm);
	printf("NativeTestArgs Float: %f\n", fFloat); 
}

/* prototype ==> float TestArgs(void); */
static void NativeTestRet(TaghaVM_t *restrict vm)
{
	if( !vm )
		return;
	
	float f = 100.f;
	printf("NativeTestRet: returning %f\n", f);
	tagha_push_float32(vm, f);
}

static inline int is_bigendian()
{
	const int i=1;
	return( (*(char *)&i) == 0 );
}

static inline uint _tagha_get_imm4(TaghaVM_t *restrict vm)
{
	if( !vm )
		return 0;
		
	union conv_union conv;
	//	0x0A,0x0B,0x0C,0x0D,
	conv.c[3] = vm->pInstrStream[++vm->ip];
	conv.c[2] = vm->pInstrStream[++vm->ip];
	conv.c[1] = vm->pInstrStream[++vm->ip];
	conv.c[0] = vm->pInstrStream[++vm->ip];
	return conv.ui;
}

static inline ushort _tagha_get_imm2(TaghaVM_t *restrict vm)
{
	if( !vm )
		return 0;
	union conv_union conv;
	conv.c[1] = vm->pInstrStream[++vm->ip];
	conv.c[0] = vm->pInstrStream[++vm->ip];
	return conv.us;
}

static inline void _tagha_push_long(TaghaVM_t *restrict vm, const uint val)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp+4) >= STK_SIZE ) {
		printf("tagha_push_long reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+4);
		exit(1);
	}
	union conv_union conv;
	conv.ui = val;
	vm->pbStack[++vm->sp] = conv.c[0];
	vm->pbStack[++vm->sp] = conv.c[1];
	vm->pbStack[++vm->sp] = conv.c[2];
	vm->pbStack[++vm->sp] = conv.c[3];
}
static inline uint _tagha_pop_long(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-4) >= STK_SIZE ) {	// we're subtracting, did we integer underflow?
		printf("tagha_pop_long reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-4);
		exit(1);
	}
	
	union conv_union conv;
	conv.c[3] = vm->pbStack[vm->sp--];
	conv.c[2] = vm->pbStack[vm->sp--];
	conv.c[1] = vm->pbStack[vm->sp--];
	conv.c[0] = vm->pbStack[vm->sp--];
	return conv.ui;
}

static inline void _tagha_push_float32(TaghaVM_t *restrict vm, const float val)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp+4) >= STK_SIZE ) {
		printf("tagha_push_float32 reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+4);
		exit(1);
	}
	union conv_union conv;
	conv.f = val;
	vm->pbStack[++vm->sp] = conv.c[0];
	vm->pbStack[++vm->sp] = conv.c[1];
	vm->pbStack[++vm->sp] = conv.c[2];
	vm->pbStack[++vm->sp] = conv.c[3];
}
static inline float _tagha_pop_float32(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-4) >= STK_SIZE ) {
		printf("tagha_pop_float32 reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-4);
		exit(1);
	}
	
	union conv_union conv;
	conv.c[3] = vm->pbStack[vm->sp--];
	conv.c[2] = vm->pbStack[vm->sp--];
	conv.c[1] = vm->pbStack[vm->sp--];
	conv.c[0] = vm->pbStack[vm->sp--];
	return conv.f;
}

static inline void _tagha_push_short(TaghaVM_t *restrict vm, const ushort val)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp+2) >= STK_SIZE ) {
		printf("tagha_push_short reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+2);
		exit(1);
	}
	union conv_union conv;
	conv.us = val;
	vm->pbStack[++vm->sp] = conv.c[0];
	vm->pbStack[++vm->sp] = conv.c[1];
}
static inline ushort _tagha_pop_short(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-2) >= STK_SIZE ) {
		printf("tagha_pop_short reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-2);
		exit(1);
	}
	union conv_union conv;
	conv.c[1] = vm->pbStack[vm->sp--];
	conv.c[0] = vm->pbStack[vm->sp--];
	return conv.us;
}

static inline void _tagha_push_byte(TaghaVM_t *restrict vm, const uchar val)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp+1) >= STK_SIZE ) {
		printf("tagha_push_byte reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+1);
		exit(1);
	}
	vm->pbStack[++vm->sp] = val;
}
static inline uchar _tagha_pop_byte(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-1) >= STK_SIZE ) {
		printf("tagha_pop_byte reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
		exit(1);
	}
	
	return vm->pbStack[vm->sp--];
}

static inline void _tagha_push_nbytes(TaghaVM_t *restrict vm, void *restrict pItem, uint bytesize)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp+bytesize) >= STK_SIZE ) {
		printf("tagha_push_nbytes reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+1);
		exit(1);
	}
	uint i=0;
	for( i=0 ; i<bytesize ; i++ )
		vm->pbStack[++vm->sp] = ((uchar *)pItem)[i];
}
static inline void _tagha_pop_nbytes(TaghaVM_t *restrict vm, void *restrict pBuffer, const uint bytesize)
{
	if( !vm )
		return;
	if( vm->bSafeMode and (vm->sp-bytesize) >= STK_SIZE ) {
		printf("tagha_pop_nbytes reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+1);
		exit(1);
	}
	uint i=0;
	// should stop when the integer underflows
	for( i=bytesize-1 ; i<bytesize ; i-- )
		((uchar *)pBuffer)[i] = vm->pbStack[vm->sp--];
}

static inline uint _tagha_read_long(TaghaVM_t *restrict vm, const Word_t address)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and address > MEM_SIZE-4 ) {
		printf("tagha_read_long reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	union conv_union conv;
	conv.c[0] = vm->pbMemory[address];
	conv.c[1] = vm->pbMemory[address+1];
	conv.c[2] = vm->pbMemory[address+2];
	conv.c[3] = vm->pbMemory[address+3];
	return conv.ui;
}
static inline void _tagha_write_long(TaghaVM_t *restrict vm, const uint val, const Word_t address)
{
	if( !vm )
		return;
	if( vm->bSafeMode and address > MEM_SIZE-4 ) {
		printf("tagha_write_long reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	
	union conv_union conv;
	conv.ui = val;
	vm->pbMemory[address] = conv.c[0];
	vm->pbMemory[address+1] = conv.c[1];
	vm->pbMemory[address+2] = conv.c[2];
	vm->pbMemory[address+3] = conv.c[3];
	//printf("wrote %" PRIu32 " to address: %" PRIu32 "\n" );
}

static inline void _tagha_write_short(TaghaVM_t *restrict vm, const ushort val, const Word_t address)
{
	if( !vm )
		return;
	if( vm->bSafeMode and address > MEM_SIZE-2 ) {
		printf("tagha_write_short reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	union conv_union conv;
	conv.us = val;
	vm->pbMemory[address] = conv.c[0];
	vm->pbMemory[address+1] = conv.c[1];
}

static inline ushort _tagha_read_short(TaghaVM_t *restrict vm, const Word_t address)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and address > MEM_SIZE-2 ) {
		printf("tagha_read_short reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	union conv_union conv;
	conv.c[0] = vm->pbMemory[address];
	conv.c[1] = vm->pbMemory[address+1];
	return conv.us;
}

static inline uchar _tagha_read_byte(TaghaVM_t *restrict vm, const Word_t address)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and address >= MEM_SIZE ) {
		printf("tagha_read_byte reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	return vm->pbMemory[address];
}
static inline void _tagha_write_byte(TaghaVM_t *restrict vm, const uchar val, const Word_t address)
{
	if( !vm )
		return;
	if( vm->bSafeMode and address >= MEM_SIZE ) {
		printf("tagha_write_byte reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	vm->pbMemory[address] = val;
}

static inline float _tagha_read_float32(TaghaVM_t *restrict vm, const Word_t address)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and address > MEM_SIZE-4 ) {
		printf("tagha_read_float32 reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	union conv_union conv;
	conv.c[0] = vm->pbMemory[address];
	conv.c[1] = vm->pbMemory[address+1];
	conv.c[2] = vm->pbMemory[address+2];
	conv.c[3] = vm->pbMemory[address+3];
	return conv.f;
}
static inline void _tagha_write_float32(TaghaVM_t *restrict vm, const float val, const Word_t address)
{
	if( !vm )
		return;
	if( vm->bSafeMode and address > MEM_SIZE-4 ) {
		printf("tagha_write_float32 reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, address);
		exit(1);
	}
	union conv_union conv;
	conv.f = val;
	vm->pbMemory[address] = conv.c[0];
	vm->pbMemory[address+1] = conv.c[1];
	vm->pbMemory[address+2] = conv.c[2];
	vm->pbMemory[address+3] = conv.c[3];
}

static inline void _tagha_read_nbytes(TaghaVM_t *restrict vm, void *restrict pBuffer, const uint bytesize, const Word_t address)
{
	if( !vm )
		return;
	
	Word_t	addr = address;
	uint	i=0;
	while( i<bytesize ) {
		if( vm->bSafeMode and addr >= MEM_SIZE-i ) {
			printf("tagha_read_array reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, addr);
			exit(1);
		}
		((uchar *)pBuffer)[i++] = vm->pbMemory[addr++];
		//buffer[i++] = vm->pbMemory[addr++];
	}
}
static inline void _tagha_write_nbytes(TaghaVM_t *restrict vm, void *restrict pItem, const uint bytesize, const Word_t address)
{
	if( !vm )
		return;
	
	Word_t	addr = address;
	uint	i=0;
	while( i<bytesize ) {
		if( vm->bSafeMode and addr >= MEM_SIZE+i ) {
			printf("tagha_write_array reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, addr);
			exit(1);
		}
		//vm->pbMemory[addr++] = val[i++];
		vm->pbMemory[addr++] = ((uchar *)pItem)[i++];
	}
}

static inline uint _tagha_peek_long(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-3) >= STK_SIZE ) {
		printf("Tagha_peek_long reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-3);
		exit(1);
	}
	union conv_union conv;
	conv.c[3] = vm->pbStack[vm->sp];
	conv.c[2] = vm->pbStack[vm->sp-1];
	conv.c[1] = vm->pbStack[vm->sp-2];
	conv.c[0] = vm->pbStack[vm->sp-3];
	return conv.ui;
}

static inline float _tagha_peek_float32(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-3) >= STK_SIZE ) {	// we're subtracting, did we integer underflow?
		printf("Tagha_peek_float32 reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-3);
		exit(1);
	}
	union conv_union conv;
	conv.c[3] = vm->pbStack[vm->sp];
	conv.c[2] = vm->pbStack[vm->sp-1];
	conv.c[1] = vm->pbStack[vm->sp-2];
	conv.c[0] = vm->pbStack[vm->sp-3];
	return conv.f;
}

static inline ushort _tagha_peek_short(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp-1) >= STK_SIZE ) {
		printf("Tagha_peek_short reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
		exit(1);
	}
	union conv_union conv;
	conv.c[1] = vm->pbStack[vm->sp];
	conv.c[0] = vm->pbStack[vm->sp-1];
	return conv.us;
}

static inline uchar _tagha_peek_byte(TaghaVM_t *vm)
{
	if( !vm )
		return 0;
	if( vm->bSafeMode and (vm->sp) >= STK_SIZE ) {
		printf("Tagha_peek_byte reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp);
		exit(1);
	}
	return vm->pbStack[vm->sp];
}


//#include <unistd.h>	// sleep() func
void tagha_exec(TaghaVM_t *restrict vm)
{
	if( !vm )
		return;
	else if( !vm->pInstrStream )
		return;
	
	union conv_union conv;
	uint b,a;
	float fb,fa;
	ushort sb,sa;
	uchar cb,ca;
	
#define X(x) #x ,
	const char *opcode2str[] = { INSTR_SET };
#undef X

#define X(x) &&exec_##x ,
	static const void *dispatch[] = { INSTR_SET };
#undef X
#undef INSTR_SET
	
	
#ifdef _UNISTD_H
	//#define DISPATCH()	sleep(1); INC(); goto *dispatch[ vm->pInstrStream[++vm->ip] ]
	//#define JUMP()		sleep(1); INC(); goto *dispatch[ vm->pInstrStream[vm->ip] ]
	#define DISPATCH()	sleep(1); ++vm->ip; continue
#else
	//#define DISPATCH()	INC(); goto *dispatch[ vm->pInstrStream[++vm->ip] ]
	//#define JUMP()		INC(); goto *dispatch[ vm->pInstrStream[vm->ip] ]
	#define DISPATCH()	++vm->ip; continue
#endif
	
	while( 1 ) {
		vm->uiMaxInstrs--;
		if( !vm->uiMaxInstrs )
			break;
		
		if( vm->pInstrStream[vm->ip] > nop) {
			printf("illegal instruction exception! instruction == \'%" PRIu32 "\' @ %" PRIu32 "\n", vm->pInstrStream[vm->ip], vm->ip);
			goto *dispatch[halt];
		}
		//printf( "current instruction == \"%s\" @ ip == %" PRIu32 "\n", opcode2str[vm->pInstrStream[vm->ip]], vm->ip );
		goto *dispatch[ vm->pInstrStream[vm->ip] ];
		
		exec_nop:;
			DISPATCH();
		
		exec_halt:;
			printf("========================= [vm done] =========================\n\n");
			return;
		
		// opcodes for longs
		exec_pushl:;	// push 4 bytes onto the stack
			conv.ui = _tagha_get_imm4(vm);
			printf("pushl: pushed %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_pushs:;	// push 2 bytes onto the stack
			conv.us = _tagha_get_imm2(vm);
			_tagha_push_short(vm, conv.us);
			//vm->pbStack[++vm->sp] = conv.c[0];
			//vm->pbStack[++vm->sp] = conv.c[1];
			printf("pushs: pushed %" PRIu32 "\n", conv.us);
			DISPATCH();
		
		exec_pushb:;	// push a byte onto the stack
			//vm->pbStack[++vm->sp] = vm->pInstrStream[++vm->ip];
			_tagha_push_byte(vm, vm->pInstrStream[++vm->ip]);
			printf("pushb: pushed %" PRIu32 "\n", vm->pbStack[vm->sp]);
			DISPATCH();
		
		exec_pushsp:;	// push sp onto the stack, uses 4 bytes since 'sp' is uint32
			conv.ui = vm->sp;
			_tagha_push_long(vm, conv.ui);
			printf("pushsp: pushed sp index: %" PRIu32 "\n", conv.ui);
			DISPATCH();
		
		exec_puship:;
			conv.ui = vm->ip;
			_tagha_push_long(vm, conv.ui);
			printf("puship: pushed ip index: %" PRIu32 "\n", conv.ui);
			DISPATCH();
		
		exec_pushbp:;
			_tagha_push_long(vm, vm->bp);
			printf("pushbp: pushed bp index: %" PRIu32 "\n", vm->bp);
			DISPATCH();
		
		exec_pushspadd:;
			a = vm->sp;
			b = _tagha_pop_long(vm);
			_tagha_push_long(vm, a+b);
			printf("pushspadd: added sp with %" PRIu32 ", result: %" PRIu32 "\n", b, a+b);
			DISPATCH();
			
		exec_pushspsub:;
			a = vm->sp;
			b = _tagha_pop_long(vm);
			_tagha_push_long(vm, a-b);
			printf("pushspsub: subbed sp with %" PRIu32 ", result: %" PRIu32 "\n", b, a-b);
			DISPATCH();
		
		exec_pushbpadd:;
			a = vm->bp;
			b = _tagha_pop_long(vm);
			_tagha_push_long(vm, a-b);
			printf("pushbpadd: added bp with %" PRIu32 ", result: %" PRIu32 "\n", b, a-b);
			DISPATCH();
		
		exec_pushbpsub:;
			a = vm->bp;
			b = _tagha_pop_long(vm);
			_tagha_push_long(vm, a-b);
			printf("pushbpsub: subbed bp with %" PRIu32 ", result: %" PRIu32 "\n", b, a-b);
			DISPATCH();
		
		exec_popl:;		// pop 4 bytes to eventually be overwritten
			if( vm->bSafeMode and (vm->sp-4) >= STK_SIZE ) {
				printf("exec_popl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-4);
				goto *dispatch[halt];
			}
			vm->sp -= 4;
			printf("popl\n");
			DISPATCH();
		
		exec_pops:;		// pop 2 bytes
			if( vm->bSafeMode and (vm->sp-2) >= STK_SIZE ) {
				printf("exec_pops reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-2);
				goto *dispatch[halt];
			}
			vm->sp -= 2;
			printf("pops\n");
			DISPATCH();
		
		exec_popb:;		// pop a byte
			if( vm->bSafeMode and (vm->sp-1) >= STK_SIZE ) {
				printf("exec_popb reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
				goto *dispatch[halt];
			}
			vm->sp--;
			printf("popb\n");
			DISPATCH();
			
		exec_popsp:;
			vm->sp = _tagha_pop_long(vm);
			printf("popsp: sp is now %" PRIu32 " bytes.\n", vm->sp);
			DISPATCH();
			
		exec_popbp:;
			vm->bp = _tagha_pop_long(vm);
			printf("popbp: bp is now %" PRIu32 " bytes.\n", vm->bp);
			DISPATCH();
			
		exec_popip:;
			vm->ip = _tagha_pop_long(vm);
			printf("popip: ip is now at address: %" PRIu32 ".\n", vm->ip);
			continue;
		
		exec_wrtl:;	// writes an int to memory, First operand is the memory address as 4 byte number, second is the int of data.
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a > MEM_SIZE-4 ) {
				printf("exec_wrtl reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a+3);
				goto *dispatch[halt];
			}
			// TODO: replace the instr stream with _tagha_get_imm4(vm)
			vm->pbMemory[a+3] = vm->pInstrStream[++vm->ip];
			vm->pbMemory[a+2] = vm->pInstrStream[++vm->ip];
			vm->pbMemory[a+1] = vm->pInstrStream[++vm->ip];
			vm->pbMemory[a+0] = vm->pInstrStream[++vm->ip];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[2] = vm->pbMemory[a+2];
			conv.c[3] = vm->pbMemory[a+3];
			printf("wrote int data - %" PRIu32 " @ address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_wrts:;	// writes a short to memory. First operand is the memory address as 4 byte number, second is the short of data.
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a > MEM_SIZE-2 ) {
				printf("exec_wrts reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			vm->pbMemory[a+1] = vm->pInstrStream[++vm->ip];
			vm->pbMemory[a+0] = vm->pInstrStream[++vm->ip];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			printf("wrote short data - %" PRIu32 " @ address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_wrtb:;	// writes a byte to memory. First operand is the memory address as 32-bit number, second is the byte of data.
			conv.ui = _tagha_get_imm4(vm);
			if( vm->bSafeMode and conv.ui >= MEM_SIZE ) {
				printf("exec_wrtb reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, conv.ui);
				goto *dispatch[halt];
			}
			vm->pbMemory[conv.ui] = vm->pInstrStream[++vm->ip];
			printf("wrote byte data - %" PRIu32 " @ address 0x%x\n", vm->pbMemory[conv.ui], conv.ui);
			DISPATCH();
		
		exec_storel:;	// pops 4-byte value off stack and into a memory address.
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a >= MEM_SIZE-4 ) {
				printf("exec_storel reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-4) >= STK_SIZE ) {
				printf("exec_storel reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-4);
				goto *dispatch[halt];
			}
			vm->pbMemory[a+3] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+2] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+1] = vm->pbStack[vm->sp--];
			vm->pbMemory[a] = vm->pbStack[vm->sp--];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[2] = vm->pbMemory[a+2];
			conv.c[3] = vm->pbMemory[a+3];
			printf("stored int data - %" PRIu32 " @ address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_stores:;	// pops 2-byte value off stack and into a memory address.
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a > MEM_SIZE-2 ) {
				printf("exec_stores reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-2) >= STK_SIZE ) {
				printf("exec_stores reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-2);
				goto *dispatch[halt];
			}
			vm->pbMemory[a+1] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+0] = vm->pbStack[vm->sp--];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			printf("stored short data - %" PRIu32 " @ address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_storeb:;	// pops byte value off stack and into a memory address.
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a >= MEM_SIZE ) {
				printf("exec_storeb reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-1) >= STK_SIZE ) {
				printf("exec_storeb reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
				goto *dispatch[halt];
			}
			vm->pbMemory[a] = vm->pbStack[vm->sp--];
			printf("stored byte data - %" PRIu32 " @ address 0x%x\n", vm->pbMemory[a], a);
			DISPATCH();
		
		/*
		 * pushl <value to store>
		 * loadl <ptr address>
		 * storela
		*/
		exec_storela:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE-4 ) {
				printf("exec_storela reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-4) >= STK_SIZE ) {
				printf("exec_storela reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-4);
				goto *dispatch[halt];
			}
			vm->pbMemory[a+3] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+2] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+1] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+0] = vm->pbStack[vm->sp--];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[2] = vm->pbMemory[a+2];
			conv.c[3] = vm->pbMemory[a+3];
			printf("stored 4 byte data - %" PRIu32 " to pointer address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_storesa:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE-2 ) {
				printf("exec_storesa reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-2) >= STK_SIZE ) {
				printf("exec_storesa reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-2);
				goto *dispatch[halt];
			}
			vm->pbMemory[a+1] = vm->pbStack[vm->sp--];
			vm->pbMemory[a+0] = vm->pbStack[vm->sp--];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			printf("stored 2 byte data - %" PRIu32 " to pointer address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_storeba:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a >= MEM_SIZE ) {
				printf("exec_storeba reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp-1) >= STK_SIZE ) {
				printf("exec_storeba reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
				goto *dispatch[halt];
			}
			vm->pbMemory[a] = vm->pbStack[vm->sp--];
			printf("stored byte - %" PRIu32 " to pointer address 0x%x\n", vm->pbMemory[a], a);
			DISPATCH();
		
		exec_loadl:;
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a > MEM_SIZE-4 ) {
				printf("exec_loadl reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+4) >= STK_SIZE ) {
				printf("exec_loadl reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+4);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a+0];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+1];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+2];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+3];
			conv.c[3] = vm->pbMemory[a+3];
			conv.c[2] = vm->pbMemory[a+2];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[0] = vm->pbMemory[a+0];
			printf("loaded int data to T.O.S. - %" PRIu32 " from address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_loads:;
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a > MEM_SIZE-2 ) {
				printf("exec_loads reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+2) >= STK_SIZE ) {
				printf("exec_loads reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+2);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a+0];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+1];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[0] = vm->pbMemory[a+0];
			printf("loaded short data to T.O.S. - %" PRIu32 " from address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_loadb:;
			a = _tagha_get_imm4(vm);
			if( vm->bSafeMode and a >= MEM_SIZE ) {
				printf("exec_loadb reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+1) >= STK_SIZE ) {
				printf("exec_loadb reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+1);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a];
			printf("loaded byte data to T.O.S. - %" PRIu32 " from address 0x%x\n", vm->pbStack[vm->sp], a);
			DISPATCH();
		
		exec_loadla:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE-4 ) {
				printf("exec_loadla reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+4) >= STK_SIZE ) {
				printf("exec_loadla reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+4);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a+0];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+1];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+2];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+3];
			conv.c[0] = vm->pbMemory[a+3];
			conv.c[1] = vm->pbMemory[a+2];
			conv.c[2] = vm->pbMemory[a+1];
			conv.c[3] = vm->pbMemory[a+0];
			printf("loaded 4 byte data to T.O.S. - %" PRIu32 " from pointer address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_loadsa:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE-2 ) {
				printf("exec_loadsa reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+2) >= STK_SIZE ) {
				printf("exec_loadsa reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+2);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a+0];
			vm->pbStack[++vm->sp] = vm->pbMemory[a+1];
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			printf("loaded 2 byte data to T.O.S. - %" PRIu32 " from pointer address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_loadba:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE ) {
				printf("exec_loadba reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and (vm->sp+1) >= STK_SIZE ) {
				printf("exec_loadba reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+1);
				goto *dispatch[halt];
			}
			vm->pbStack[++vm->sp] = vm->pbMemory[a];
			printf("loaded byte data to T.O.S. - %" PRIu32 " from pointer address 0x%x\n", vm->pbStack[vm->sp], a);
			DISPATCH();
		
		exec_loadspl:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and (a-3) >= STK_SIZE ) {
				printf("exec_loadspl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a-3);
				goto *dispatch[halt];
			}
			conv.c[3] = vm->pbStack[a];
			conv.c[2] = vm->pbStack[a-1];
			conv.c[1] = vm->pbStack[a-2];
			conv.c[0] = vm->pbStack[a-3];
			_tagha_push_long(vm, conv.ui);
			printf("loaded 4-byte SP address data to T.O.S. - %" PRIu32 " from sp address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_loadsps:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and (a-1) >= STK_SIZE ) {
				printf("exec_loadsps reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a-1);
				goto *dispatch[halt];
			}
			conv.c[1] = vm->pbStack[a];
			conv.c[0] = vm->pbStack[a-1];
			_tagha_push_short(vm, conv.us);
			printf("loaded 2-byte SP address data to T.O.S. - %" PRIu32 " from sp address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_loadspb:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a >= STK_SIZE ) {
				printf("exec_loadspb reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a);
				goto *dispatch[halt];
			}
			conv.c[0] = vm->pbStack[a];
			_tagha_push_byte(vm, conv.c[0]);
			printf("loaded byte SP address data to T.O.S. - %" PRIu32 " from sp address 0x%x\n", conv.c[0], a);
			DISPATCH();
		
		exec_storespl:;		// store TOS into another part of the data stack.
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a-3 >= STK_SIZE ) {
				printf("exec_storespl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a-3);
				goto *dispatch[halt];
			}
			conv.ui = _tagha_pop_long(vm);
			vm->pbStack[a] = conv.c[3];
			vm->pbStack[a-1] = conv.c[2];
			vm->pbStack[a-2] = conv.c[1];
			vm->pbStack[a-3] = conv.c[0];
			printf("stored 4-byte data from T.O.S. - %" PRIu32 " to sp address 0x%x\n", conv.ui, a);
			DISPATCH();
		
		exec_storesps:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a-1 >= STK_SIZE ) {
				printf("exec_storesps reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a-1);
				goto *dispatch[halt];
			}
			conv.us = _tagha_pop_short(vm);
			vm->pbStack[a] = conv.c[1];
			vm->pbStack[a-1] = conv.c[0];
			printf("stored 2-byte data from T.O.S. - %" PRIu32 " to sp address 0x%x\n", conv.us, a);
			DISPATCH();
		
		exec_storespb:;
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a >= STK_SIZE ) {
				printf("exec_storespb reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, a);
				goto *dispatch[halt];
			}
			vm->pbStack[a] = _tagha_pop_byte(vm);
			printf("stored byte data from T.O.S. - %" PRIu32 " to sp address 0x%x\n", vm->pbStack[a], a);
			DISPATCH();
		
		exec_copyl:;	// copy 4 bytes of top of stack and put as new top of stack.
			if( vm->bSafeMode and vm->sp-3 >= STK_SIZE ) {
				printf("exec_copyl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-3);
				goto *dispatch[halt];
			}
			conv.c[0] = vm->pbStack[vm->sp-3];
			conv.c[1] = vm->pbStack[vm->sp-2];
			conv.c[2] = vm->pbStack[vm->sp-1];
			conv.c[3] = vm->pbStack[vm->sp];
			printf("copied int data from T.O.S. - %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_copys:;
			if( vm->bSafeMode and vm->sp-1 >= STK_SIZE ) {
				printf("exec_copys reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-1);
				goto *dispatch[halt];
			}
			else if( vm->bSafeMode and vm->sp+2 >= STK_SIZE ) {
				printf("exec_copys reported: stack overflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp+2);
				goto *dispatch[halt];
			}
			conv.c[0] = vm->pbStack[vm->sp-1];
			conv.c[1] = vm->pbStack[vm->sp];
			_tagha_push_short(vm, conv.us);
			//vm->pbStack[++vm->sp] = conv.c[0];
			//vm->pbStack[++vm->sp] = conv.c[1];
			printf("copied short data from T.O.S. - %" PRIu32 "\n", conv.us);
			DISPATCH();
		
		exec_copyb:;
			//conv.c[0] = vm->pbStack[vm->sp];
			_tagha_push_byte(vm, vm->pbStack[vm->sp]);
			//vm->pbStack[++vm->sp] = conv.c[0];
			//printf("copied byte data from T.O.S. - %" PRIu32 "\n", conv.c[0]);
			DISPATCH();
		
		exec_addl:;		// pop 8 bytes, signed addition, and push 4 byte result to top of stack
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.i = (int)a + (int)b;
			printf("signed 4 byte addition result: %i == %i + %i\n", conv.i, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_uaddl:;	// In C, all integers in an expression are promoted to int32, if number is bigger then uint32 or int64
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a+b;
			printf("unsigned 4 byte addition result: %u == %u + %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_addf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.f = fa+fb;
			printf("float addition result: %f == %f + %f\n", conv.f, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_subl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.i = (int)a - (int)b;
			printf("signed 4 byte subtraction result: %i == %i - %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_usubl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a-b;
			printf("unsigned 4 byte subtraction result: %" PRIu32 " == %" PRIu32 " - %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_subf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.f = fa-fb;
			printf("float subtraction result: %f == %f - %f\n", conv.f, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_mull:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.i = (int)a * (int)b;
			printf("signed 4 byte mult result: %i == %i * %i\n", conv.i, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_umull:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a*b;
			printf("unsigned 4 byte mult result: %" PRIu32 " == %" PRIu32 " * %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_mulf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.f = fa*fb;
			printf("float mul result: %f == %f * %f\n", conv.f, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_divl:;
			b = _tagha_pop_long(vm);
			if( !b ) {
				printf("divl: divide by 0 error.\n");
				goto *dispatch[halt];
			}
			a = _tagha_pop_long(vm);
			conv.i = (int)a / (int)b;
			printf("signed 4 byte division result: %i == %i / %i\n", conv.i, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_udivl:;
			b = _tagha_pop_long(vm);
			if( !b ) {
				printf("udivl: divide by 0 error.\n");
				goto *dispatch[halt];
			}
			a = _tagha_pop_long(vm);
			conv.ui = a/b;
			printf("unsigned 4 byte division result: %" PRIu32 " == %" PRIu32 " / %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_divf:;
			fb = _tagha_pop_float32(vm);
			if( !fb ) {
				printf("divf: divide by 0.0 error.\n");
				goto *dispatch[halt];
			}
			fa = _tagha_pop_float32(vm);
			conv.f = fa/fb;
			printf("float division result: %f == %f / %f\n", conv.f, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_modl:;
			b = _tagha_pop_long(vm);
			if( !b ) {
				printf("modl: divide by 0 error.\n");
				goto *dispatch[halt];
			}
			a = _tagha_pop_long(vm);
			conv.i = (int)a % (int)b;
			printf("signed 4 byte modulo result: %i == %i %% %i\n", conv.i, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_umodl:;
			b = _tagha_pop_long(vm);
			if( !b ) {
				printf("umodl: divide by 0 error.\n");
				goto *dispatch[halt];
			}
			a = _tagha_pop_long(vm);
			conv.ui = a % b;
			printf("unsigned 4 byte modulo result: %" PRIu32 " == %" PRIu32 " %% %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_andl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a & b;
			printf("4 byte AND result: %" PRIu32 " == %u & %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_orl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a | b;
			printf("4 byte OR result: %" PRIu32 " == %u | %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_xorl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a ^ b;
			printf("4 byte XOR result: %" PRIu32 " == %u ^ %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_notl:;
			a = _tagha_pop_long(vm);
			conv.ui = ~a;
			printf("4 byte NOT result: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_shll:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a << b;
			printf("4 byte Shift Left result: %" PRIu32 " == %u << %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_shrl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a >> b;
			printf("4 byte Shift Right result: %" PRIu32 " == %u >> %u\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_incl:;
			a = _tagha_pop_long(vm);
			conv.ui = ++a;
			printf("4 byte Increment result: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_incf:;
			fa = _tagha_pop_float32(vm);
			conv.f = ++fa;
			printf("float Increment result: %f\n", conv.f);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_decl:;
			a = _tagha_pop_long(vm);
			conv.ui = --a;
			printf("4 byte Decrement result: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_decf:;
			fa = _tagha_pop_float32(vm);
			conv.f = --fa;
			printf("float Decrement result: %f\n", conv.f);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_negl:;
			a = _tagha_pop_long(vm);
			conv.ui = -a;
			printf("4 byte Negate result: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
			
		exec_negf:;
			fa = _tagha_pop_float32(vm);
			conv.f = -fa;
			printf("float Negate result: %f\n", conv.f);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_ltl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = (int)a < (int)b;
			printf("4 byte Signed Less Than result: %" PRIu32 " == %i < %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_ultl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a < b;
			printf("4 byte Unsigned Less Than result: %" PRIu32 " == %" PRIu32 " < %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_ltf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.ui = fa < fb;
			printf("4 byte Less Than Float result: %" PRIu32 " == %f < %f\n", conv.ui, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_gtl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = (int)a > (int)b;
			printf("4 byte Signed Greater Than result: %" PRIu32 " == %i > %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_ugtl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a > b;
			printf("4 byte Signed Greater Than result: %" PRIu32 " == %" PRIu32 " > %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_gtf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.ui = fa > fb;
			printf("4 byte Greater Than Float result: %" PRIu32 " == %f > %f\n", conv.ui, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_cmpl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = (int)a == (int)b;
			printf("4 byte Signed Compare result: %" PRIu32 " == %i == %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_ucmpl:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a == b;
			printf("4 byte Unsigned Compare result: %" PRIu32 " == %" PRIu32 " == %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_compf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.ui = fa == fb;
			printf("4 byte Compare Float result: %" PRIu32 " == %f == %f\n", conv.ui, fa,fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_leql:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = (int)a <= (int)b;
			printf("4 byte Signed Less Equal result: %" PRIu32 " == %i <= %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_uleql:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a <= b;
			printf("4 byte Unsigned Less Equal result: %" PRIu32 " == %" PRIu32 " <= %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_leqf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.ui = fa <= fb;
			printf("4 byte Less Equal Float result: %" PRIu32 " == %f <= %f\n", conv.ui, fa, fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_geql:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = (int)a >= (int)b;
			printf("4 byte Signed Greater Equal result: %" PRIu32 " == %i >= %i\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_ugeql:;
			b = _tagha_pop_long(vm);
			a = _tagha_pop_long(vm);
			conv.ui = a >= b;
			printf("4 byte Unsigned Greater Equal result: %" PRIu32 " == %" PRIu32 " >= %" PRIu32 "\n", conv.ui, a,b);
			_tagha_push_long(vm, conv.ui);
			DISPATCH();
		
		exec_geqf:;
			fb = _tagha_pop_float32(vm);
			fa = _tagha_pop_float32(vm);
			conv.ui = fa >= fb;
			printf("4 byte Greater Equal Float result: %" PRIu32 " == %f >= %f\n", conv.ui, fa, fb);
			_tagha_push_float32(vm, conv.f);
			DISPATCH();
		
		exec_jmp:;		// addresses are word sized bytes.
			conv.ui = _tagha_get_imm4(vm);
			vm->ip = conv.ui;
			printf("jmping to instruction address: %" PRIu32 "\n", vm->ip);
			continue;
		
		exec_jzl:;		// check if the first 4 bytes on stack are zero, if yes then jump it.
			if( vm->bSafeMode and vm->sp-3 >= STK_SIZE ) {
				printf("exec_jzl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-3);
				goto *dispatch[halt];
			}
			conv.c[3] = vm->pbStack[vm->sp];
			conv.c[2] = vm->pbStack[vm->sp-1];
			conv.c[1] = vm->pbStack[vm->sp-2];
			conv.c[0] = vm->pbStack[vm->sp-3];
			a = conv.ui;
			conv.ui = _tagha_get_imm4(vm);
			vm->ip = (!a) ? conv.ui : vm->ip+1 ;
			printf("jzl'ing to instruction address: %" PRIu32 "\n", vm->ip);	//opcode2str[vm->pInstrStream[vm->ip]]
			continue;
		
		exec_jnzl:;
			if( vm->bSafeMode and vm->sp-3 >= STK_SIZE ) {
				printf("exec_jnzl reported: stack underflow! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\n", vm->ip, vm->sp-3);
				goto *dispatch[halt];
			}
			conv.c[3] = vm->pbStack[vm->sp];
			conv.c[2] = vm->pbStack[vm->sp-1];
			conv.c[1] = vm->pbStack[vm->sp-2];
			conv.c[0] = vm->pbStack[vm->sp-3];
			a = conv.ui;
			conv.ui = _tagha_get_imm4(vm);
			vm->ip = (a) ? conv.ui : vm->ip+1 ;
			printf("jnzl'ing to instruction address: %" PRIu32 "\n", vm->ip);
			continue;
		
		exec_call:;		// support functions
			conv.ui = _tagha_get_imm4(vm);	// get func address
			printf("call: calling address: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, vm->ip+1);	// save return address.
			printf("call return addr: %" PRIu32 "\n", _tagha_peek_long(vm));
			_tagha_push_long(vm, vm->bp);	// push ebp;
			vm->bp = vm->sp;	// mov ebp, esp;
			vm->ip = conv.ui;	// jump to instruction
			printf("vm->bp: %" PRIu32 "\n", vm->sp);
			continue;
		
		exec_calls:;	// support local function pointers
			conv.ui = _tagha_pop_long(vm);	// get func address
			printf("calls: calling address: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, vm->ip+1);	// save return address.
			printf("call return addr: %" PRIu32 "\n", _tagha_peek_long(vm));
			_tagha_push_long(vm, vm->bp);	// push ebp
			vm->bp = vm->sp;	// mov ebp, esp
			vm->ip = conv.ui;	// jump to instruction
			printf("vm->bp: %" PRIu32 "\n", vm->sp);
			continue;
		
		exec_calla:;	// support globally allocated function pointers
			a = _tagha_pop_long(vm);
			if( vm->bSafeMode and a > MEM_SIZE-4 ) {
				printf("exec_calla reported: Invalid Memory Access! Current instruction address: %" PRIu32 " | Stack index: %" PRIu32 "\nInvalid Memory Address: %" PRIu32 "\n", vm->ip, vm->sp, a);
				goto *dispatch[halt];
			}
			conv.ui = _tagha_read_long(vm, a);
			/*
			conv.c[0] = vm->pbMemory[a+0];
			conv.c[1] = vm->pbMemory[a+1];
			conv.c[2] = vm->pbMemory[a+2];
			conv.c[3] = vm->pbMemory[a+3];
			*/
			printf("calla: calling address: %" PRIu32 "\n", conv.ui);
			_tagha_push_long(vm, vm->ip+1);	// save return address.
			printf("call return addr: %" PRIu32 "\n", _tagha_peek_long(vm));
			_tagha_push_long(vm, vm->bp);	// push ebp
			vm->bp = vm->sp;	// mov ebp, esp
			vm->ip = conv.ui;	// jump to instruction
			printf("vm->bp: %" PRIu32 "\n", vm->sp);
			continue;
		
		exec_ret:;
			vm->sp = vm->bp;	// mov esp, ebp
			printf("sp set to bp, sp == %" PRIu32 "\n", vm->sp);
			vm->bp = _tagha_pop_long(vm);	// pop ebp
			vm->ip = _tagha_pop_long(vm);	// pop return address.
			printf("returning to address: %" PRIu32 "\n", vm->ip);
			continue;
		
		exec_retx:; {		// for functions that return something.
			a = _tagha_get_imm4(vm);
			uchar bytebuffer[a];
			/* This opcode assumes all the data for return
			 * is on the near top of stack. In theory, you can
			 * use this method to return multiple pieces of data.
			 */ 
			_tagha_pop_nbytes(vm, bytebuffer, a);	// store our needed data to a buffer.
			// do our usual return code.
			vm->sp = vm->bp;	// mov esp, ebp
			printf("sp set to bp, sp == %" PRIu32 "\n", vm->sp);
			vm->bp = _tagha_pop_long(vm);	// pop ebp
			vm->ip = _tagha_pop_long(vm);	// pop return address.
			_tagha_push_nbytes(vm, bytebuffer, a);
			printf("retxurning to address: %" PRIu32 "\n", vm->ip);
			continue;
		}
		exec_callnat:;	// call a native
			// pop data and arg count possibly from VM?
			vm->fnpNative(vm);
			DISPATCH();
		
		exec_reset:;
			tagha_reset(vm);
			DISPATCH();
	}
	printf("tagha_exec :: max instructions reached\n");
}

int main(int argc, char **argv)
{
	if( !argv[1] ) {
		printf("[TaghaVM Usage]: './TaghaVM' '.tagha file' \n");
		return 1;
	}
	
	TaghaVM_t *vm = &(TaghaVM_t){ 0 };
	tagha_init(vm);
	tagha_load_code(vm, argv[1]);
	//tagha_register_func(vm, NativePrintHelloWorld);
	//tagha_register_func(vm, NativeTestArgs);
	tagha_register_func(vm, NativeTestRet);
	tagha_exec(vm);	//tagha_free(vm);
	
	//printf("instruction set amount == %u\n", nop);
	/*
	// Hello World is approximately 12 chars if u count NULL-term
	
	char buffer[12];
	tagha_write_nbytes(vm, "Hello World", 12, 0x0);
	tagha_read_nbytes(vm, buffer, 12, 0x0);
	printf("read/write array test == %s\n", buffer);
	*/
	
	/*
	struct kek {
		long long int lli;
		short shrt;
		char ic;
	};
	struct kek *test = &(struct kek){500, 6436, 127};
	
	tagha_push_nbytes(vm, test, sizeof(struct kek));
	struct kek test2;
	tagha_pop_nbytes(vm, &test2, sizeof(struct kek));
	printf("test2 data: lli:%lli , shrt: %i , ic:%i\n", test2.lli, test2.shrt, test2.ic);
	*/
	tagha_debug_print_memory(vm);
	tagha_debug_print_stack(vm);
	tagha_free(vm);
	//tagha_debug_print_ptrs(p_vm);
	//free(program); program=NULL;
	return 0;
}

