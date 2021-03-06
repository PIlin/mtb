#include "common.hpp"

#include <cstdarg>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void dbg_msg1(cstr format) {
	::OutputDebugStringA(format);
}

void dbg_msg(cstr format, ...) {
	 char msg[1024];
	 va_list va;
	 va_start(va, format);
	 ::vsprintf_s(msg, format, va);
	 va_end(va);
	 ::OutputDebugStringA(msg);
}

// See http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// http://www.isthe.com/chongo/tech/comp/fnv/
uint32_t hash_fnv_1a_cstr(char const* p) {
	if (!p) { return 0; }
	const uint32_t prime = 16777619U;
	const uint32_t offset = 2166136261U;

	uint32_t val = offset;
	for (char c = *p; c; c = *(++p)) {
		val ^= (uint32_t)c;
		val *= prime;
	}

	return val;
}

std::hash<cstr>::result_type std::hash<cstr>::operator()(argument_type const& s) const {
	return hash_fnv_1a_cstr(s.p);
}
