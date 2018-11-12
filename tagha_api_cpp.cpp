#include "tagha.h"

TAGHA_EXPORT CTagha::CTagha(void *script)
{
	Tagha_Init((struct Tagha *)this, script);
}

TAGHA_EXPORT CTagha::CTagha(void *script, const struct CNativeInfo natives[])
{
	Tagha_Init((struct Tagha *)this, script);
	Tagha_RegisterNatives((struct Tagha *)this, (const struct NativeInfo *)natives);
}

TAGHA_EXPORT CTagha::~CTagha()
{
	
}

TAGHA_EXPORT bool CTagha::RegisterNatives(const struct CNativeInfo natives[])
{
	return Tagha_RegisterNatives((struct Tagha *)this, (const struct NativeInfo *)natives);
}

TAGHA_EXPORT void *CTagha::GetGlobalVarByName(const char varname[])
{
	return Tagha_GetGlobalVarByName((struct Tagha *)this, varname);
}

TAGHA_EXPORT int32_t CTagha::CallFunc(const char funcname[], const size_t args, union TaghaVal params[])
{
	return Tagha_CallFunc((struct Tagha *)this, funcname, args, params);
}

TAGHA_EXPORT union TaghaVal CTagha::GetReturnValue()
{
	return Tagha_GetReturnValue((struct Tagha *)this);
}

TAGHA_EXPORT int32_t CTagha::RunScript(int32_t argc, char *argv[])
{
	return Tagha_RunScript((struct Tagha *)this, argc, argv);
}

TAGHA_EXPORT const char *CTagha::GetError()
{
	return Tagha_GetError((struct Tagha *)this);
}

TAGHA_EXPORT void CTagha::PrintVMState()
{
	Tagha_PrintVMState((struct Tagha *)this);
}

TAGHA_EXPORT void *CTagha::GetRawScriptPtr()
{
	return Tagha_GetRawScriptPtr((struct Tagha *)this);
}

TAGHA_EXPORT void CTagha::ThrowError(const int32_t err)
{
	Tagha_ThrowError((struct Tagha *)this, err);
}

/////////////////////////////////////////////////////////////////////////////////
