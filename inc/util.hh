/**
 * @file util.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Utility define and functions
 * @version 0.1
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <typeinfo>

#define unused(x) UNUSED_##x __attribute__((__unused__))

class __sym {
    public:
	void *ptr;
	const std::type_info &type;
	__sym(void *_ptr, const std::type_info &_type)
		: ptr(_ptr)
		, type(_type)
	{
	}
	__sym()
		: ptr(nullptr)
		, type(typeid(void))
	{
	}
	__sym(const __sym &sym)
		: ptr(sym.ptr)
		, type(sym.type)
	{
	}
	__sym(__sym &&sym)
		: ptr(sym.ptr)
		, type(sym.type)
	{
	}
};

extern std::unordered_map<std::string, __sym> sym_table;

class __reg_sym {
    public:
	__reg_sym(std::string name, void *ptr, const std::type_info &type)
	{
		if (sym_table.find(name) != sym_table.end()) {
			throw std::runtime_error("Symbol " + name +
						 " already exists");
		}
		sym_table.insert({ name, __sym(ptr, type) });
	}
};

#define reg_sym(name)                                                     \
	__reg_sym __reg_sym_##name(#name, reinterpret_cast<void *>(&name), \
				   typeid(decltype(name)))
#define reg_sym_ptr(name, ptr)                                          \
	__reg_sym __reg_sym_##name(#name, reinterpret_cast<void *>(ptr), \
				   typeid(decltype(*ptr)))
#define get_sym(name) (*reinterpret_cast<sym_table[name].type *>(sym_table[name].ptr))
#define get_sym_type(name, type_name) (typeid(type_name) != sym_table[name].type ? \
				   throw std::runtime_error( \
					   "Type mismatch: " #name \
				   ) : \
				   void() , \
				   *reinterpret_cast<type_name *>(sym_table[name].ptr))

template <typename SizeT> inline void hash_combine(SizeT &seed, SizeT value)
{
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}