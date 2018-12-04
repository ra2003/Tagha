
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * File Ops
 */

/* int remove(const char *filename); */
static void native_remove(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = remove(params[0].Ptr);
}

/* int rename(const char *oldname, const char *newname); */
static void native_rename(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = rename(params[0].Ptr, params[1].Ptr);
}

/* FILE *tmpfile(void); */
static void native_tmpfile(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Ptr = tmpfile();
}

/* char *tmpnam(char *str); */
static void native_tmpnam(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Ptr = tmpnam(params[0].Ptr);
}


/*
 * File Access
 */

/* int fclose(FILE *stream); */
static void native_fclose(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	if( params[0].Ptr ) {
		retval->Int32 = fclose(params[0].Ptr);
		return;
	}
	retval->Int32 = -1;
}

/* int fflush(FILE *stream); */
static void native_fflush(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = fflush(params[0].Ptr);
}

/* FILE *fopen(const char *filename, const char *modes); */
static void native_fopen(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	const char *mode = params[1].Ptr;
	if( !mode ) {
		return;
	}
	retval->Ptr = fopen(params[0].Ptr, mode);
}

/* FILE *freopen(const char *filename, const char *mode, FILE *stream); */
static void native_freopen(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	const char *filename = params[0].Ptr;
	const char *mode = params[1].Ptr;
	FILE *pStream = params[2].Ptr;
	
	if( !filename or !mode or !pStream ) {
		return; // retval data is already zeroed out.
	}
	retval->Ptr = freopen(filename, mode, pStream);
}

/* void setbuf(FILE *stream, char *buffer); */
static void native_setbuf(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	FILE *pStream = params[0].Ptr;
	if( !pStream ) {
		return;
	}
	setbuf(pStream, params[1].Ptr);
}


/*
 * File Access
 */

/* int fprintf(FILE *stream, const char *format, ...); */
static void native_fprintf(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	FILE *stream = params[0].Ptr;
	if( !stream ) {
		retval->Int32 = -1;
		return;
	}
	
	const char *format = params[1].Ptr;
	if( !format ) {
		retval->Int32 = -1;
		return;
	}
	
	retval->Int32 = 0;
}

/* int fscanf(FILE *stream, const char *format, ...); */
static void native_fscanf(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	FILE *stream = params[0].Ptr;
	if( !stream ) {
		retval->Int32 = -1;
		return;
	}
	
	const char *fmt = params[1].Ptr;
	if( !fmt ) {
		retval->Int32 = -1;
		return;
	}
	// flags
#define LONGADJ  0x00000001  /* adjust for longer values like "lli" */
#define LONGDBLADJ  0x00000002  /* adjust for long double like "Lf" */
#define HALFADJ  0x00000004  /* adjust for long double like "Lf" */
#define HALFHALFADJ  0x00000008  /* adjust for long double like "Lf" */
	
	// %[*][width][length]specifier
	size_t width=0;
	
	for( ; *fmt ; fmt++ ) {
		if( *fmt != '%' )
			continue;
		// found a width, parse its number.
		if( *fmt>='0' and *fmt<='9' )
			while( *fmt>='0' and *fmt<='9' )
				width = width * 10 + *fmt++ - '0';
		
		if( *fmt=='d' or *fmt=='i' ) {
			
		}
		else if( *fmt=='u' or *fmt=='x' or *fmt=='o' ) {
			
		}
	}
	
	retval->Int32 = -1;
	// Need some help with this one...
}

/* int printf(const char *fmt, ...); */
static void native_printf(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	const char *str = params[0].Ptr;
	if( !str ) {
		retval->Int32 = -1;
		return;
	}
	retval->Int32 = 0;
}

/* int puts(const char *s); */
static void native_puts(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	const char *str = params[0].Ptr;
	if( !str ) {
		retval->Int32 = -1;
		return;
	}
	// push back the value of the return val of puts.
	retval->Int32 = puts(str);
}

/* int setvbuf(FILE *stream, char *buffer, int mode, size_t size); */
static void native_setvbuf(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	FILE *stream = params[0].Ptr;
	char *buffer = params[1].Ptr;
	if( !stream ) {
		retval->Int32 = -1;
		return;
	}
	else if( !buffer ) {
		retval->Int32 = -1;
		return;
	}
	retval->Int32 = setvbuf(stream, buffer, params[2].Int32, params[3].UInt64);
}

/* int fgetc(FILE *stream); */
static void native_fgetc(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = fgetc(params[0].Ptr);
}

/* char *fgets(char *str, int num, FILE *stream); */
static void native_fgets(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Ptr = fgets(params[0].Ptr, params[1].Int32, params[2].Ptr);
}

/* int fputc(int character, FILE *stream); */
static void native_fputc(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = fputc(params[0].Int32, params[1].Ptr);
}

/* int fputs(const char *str, FILE *stream); */
static void native_fputs(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = fputs(params[0].Ptr, params[1].Ptr);
}

/* int getc(FILE *stream); */
static void native_getc(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = getc(params[0].Ptr);
}

/* int getchar(void); */
static void native_getchar(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = getchar();
}

/* int putc(int character, FILE *stream); */
static void native_putc(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = putc(params[0].Int32, params[1].Ptr);
}

/* int putchar(int character); */
static void native_putchar(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = putchar(params[0].Int32);
}

/* int ungetc(int character, FILE *stream); */
static void native_ungetc(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = ungetc(params[0].Int32, params[1].Ptr);
}

/* size_t fread(void *ptr, size_t size, size_t count, FILE *stream); */
static void native_fread(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->UInt64 = fread(params[0].Ptr, params[1].UInt64, params[2].UInt64, params[3].Ptr);
}

/* size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream); */
static void native_fwrite(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->UInt64 = fwrite(params[0].Ptr, params[1].UInt64, params[2].UInt64, params[3].Ptr);
}

/* int fseek(FILE *stream, long int offset, int origin); */
static void native_fseek(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = fseek(params[0].Ptr, params[1].UInt64, params[2].Int32);
}

/* long int ftell(FILE *stream); */
static void native_ftell(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int64 = ftell(params[0].Ptr);
}

/* void rewind(FILE *stream); */
static void native_rewind(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	rewind(params[0].Ptr);
}

/* void clearerr(FILE *stream); */
static void native_clearerr(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	clearerr(params[0].Ptr);
}

/* int feof(FILE *stream); */
static void native_feof(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = feof(params[0].Ptr);
}

/* int ferror(FILE *stream); */
static void native_ferror(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	retval->Int32 = ferror(params[0].Ptr);
}

/* void perror(const char *str); */
static void native_perror(struct TaghaModule *const restrict module, union TaghaVal *const restrict retval, const size_t argc, union TaghaVal params[restrict static argc])
{
	perror(params[0].Ptr);
}


bool tagha_module_load_stdio_natives(struct TaghaModule *const restrict module)
{
	const struct TaghaNative libc_stdio_natives[] = {
		{"remove", native_remove},
		{"rename", native_rename},
		{"tmpfile", native_tmpfile},
		{"printf", native_printf},
		{"puts", native_puts},
		{"fopen", native_fopen},
		{"fclose", native_fclose},
		{"freopen", native_freopen},
		{"setbuf", native_setbuf},
		{"fflush", native_fflush},
		{"setvbuf", native_setvbuf},
		{"fgetc", native_fgetc},
		{"fgets", native_fgets},
		{"fputc", native_fputc},
		{"fputs", native_fputs},
		{"getc", native_getc},
		{"getchar", native_getchar},
		{"putc", native_putc},
		{"putchar", native_putchar},
		{"ungetc", native_ungetc},
		{"fread", native_fread},
		{"fwrite", native_fwrite},
		{"fseek", native_fseek},
		{"ftell", native_ftell},
		{"rewind", native_rewind},
		{"clearerr", native_clearerr},
		{"feof", native_feof},
		{"ferror", native_ferror},
		{"perror", native_perror},
		{NULL, NULL}
	};
	return module ? tagha_module_register_natives(module, libc_stdio_natives) : false;
}
