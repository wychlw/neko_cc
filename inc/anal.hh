#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <ios>
#include <memory>

#include "scan.hh"

namespace neko_cc
{
struct type_t;
struct conpond_type_t;
struct basic_type_t;
struct func_type_t;
struct var_t;
struct context_t;

using stream = std::basic_iostream<char>;
}

namespace neko_cc
{

struct type_t {
	std::string name;
	size_t size;
	bool is_extern;
	bool is_static;
	bool is_register;
	bool is_const;
	bool is_volatile;
	bool is_unnamed;

	enum {
		type_unknown = 0,
		type_norm,
		type_basic,
		type_struct,
		type_union,
		type_enum,
		type_typedef,
		type_func,
		type_pointer,
		type_array
	} type = type_unknown;

	// type_struct/type_union
	std::vector<var_t> inner_vars;

	// type_basic
	bool has_signed;
	bool is_unsigned;
	int is_char;
	int is_short;
	int is_int;
	int is_long;
	int is_float;
	int is_double;

	// type_func
	std::shared_ptr<type_t> ret_type;
	std::vector<type_t> args_type;

	// type_typedef
	std::string target_type;

	// type_pointer type_array
	std::shared_ptr<type_t> ptr_to;

	type_t()
	{
		name = "";
		size = 0;
		is_extern = false;
		is_static = false;
		is_register = false;
		is_const = false;
		is_volatile = false;
		is_unnamed = false;
		type = type_unknown;
		has_signed = false;
		is_unsigned = false;
		is_char = 0;
		is_short = 0;
		is_int = 0;
		is_long = 0;
		is_float = 0;
		is_double = 0;
		ret_type = nullptr;
		args_type = {};
		target_type = "";
		ptr_to = nullptr;
	}

	void output_type(std::ostream &ss, int level = 0);
	friend std::ostream &operator<<(std::ostream &ss, type_t &type)
	{
		type.output_type(ss);
		return ss;
	}
};

struct var_t {
	std::string name;
	type_t type;

	void output_var(std::ostream &ss, int level = 0)
	{
		for (int i = 0; i < level; i++) {
			ss << "\t";
		}
		ss << "var name: " << name << std::endl;
		for (int i = 0; i < level; i++) {
			ss << "\t";
		}
		ss << "has type:" << std::endl;
		type.output_type(ss, level + 1);
	}
	friend std::ostream &operator<<(std::ostream &ss, var_t &var)
	{
		var.output_var(ss);
		return ss;
	}
};

struct context_t {
	context_t *prev_context;

	std::unordered_map<std::string, var_t> vars;
	std::unordered_map<std::string, type_t> types;
	std::unordered_map<std::string, int> enums;
	size_t label;

	context_t(context_t &rhs) = delete;
	context_t &operator=(context_t &rhs) = delete;
	context_t(context_t *prev, size_t _label)
		: prev_context(prev)
		, label(_label)
		, vars()
		, types()
	{
	}
	context_t(context_t *_prev = nullptr,
		  std::unordered_map<std::string, var_t> _vars = {},
		  std::unordered_map<std::string, type_t> _types = {},
		  size_t _label = 0)
		: prev_context(_prev)
		, vars(_vars)
		, types(_types)
		, label(_label)
	{
	}

	type_t get_type(std::string type_name)
	{
		context_t *ctx_now = this;
		while (ctx_now != nullptr) {
			if (ctx_now->types.find(type_name) !=
			    ctx_now->types.end()) {
				return ctx_now->types[type_name];
			}
			ctx_now = ctx_now->prev_context;
		}
		type_t unknown_type;
		unknown_type.type = type_t::type_unknown;
		return unknown_type;
	}

	var_t get_var(std::string var_name)
	{
		context_t *ctx_now = this;
		while (ctx_now != nullptr) {
			if (ctx_now->vars.find(var_name) !=
			    ctx_now->vars.end()) {
				return ctx_now->vars[var_name];
			}
			ctx_now = ctx_now->prev_context;
		}
		var_t unknown_var;
		unknown_var.type.type = type_t::type_unknown;
		unknown_var.name = "";
		return unknown_var;
	}

	int get_enum(std::string enum_name)
	{
		context_t *ctx_now = this;
		while (ctx_now != nullptr) {
			if (ctx_now->enums.find(enum_name) !=
			    ctx_now->enums.end()) {
				return ctx_now->enums[enum_name];
			}
			ctx_now = ctx_now->prev_context;
		}
		return -1;
	}
};
}

namespace neko_cc
{
#define UNFIN

// procedure define
void translation_unit(stream &ss);
UNFIN void external_declaration(stream &ss, context_t &ctx);
void function_definition(stream &ss, context_t &ctx, std::vector<var_t> args);

int constant_expression(stream &ss, context_t &ctx);

void declaration_specifiers(stream &ss, context_t &ctx, type_t &type);
UNFIN void init_declarator(stream &ss, context_t &ctx, type_t type);
void strong_class_specfifer(stream &ss, context_t &ctx, type_t &type);
void type_specifier(stream &ss, context_t &ctx, type_t &type);
void struct_or_union_specifier(stream &ss, context_t &ctx, type_t &type);
UNFIN void struct_declaration_list(stream &ss, context_t &ctx, type_t &type);

void type_qualifier(stream &ss, context_t &ctx, type_t &type);
std::vector<var_t> declarator(stream &ss, context_t &ctx,
			      std::shared_ptr<type_t> type, var_t &var);
std::vector<var_t> direct_declarator(stream &ss, context_t &ctx,
				     std::shared_ptr<type_t> type, var_t &var);

std::shared_ptr<type_t> pointer(stream &ss, context_t &ctx,
				std::shared_ptr<type_t> type);

UNFIN std::vector<var_t> parameter_type_list(stream &ss, context_t &ctx);

UNFIN void initializer(stream &ss, context_t &ctx, var_t &var);

UNFIN void enum_specifier(stream &ss, context_t &ctx, type_t &type);
UNFIN void compound_statement(stream &ss, context_t &ctx);

static tok_t nxt_tok(stream &ss);
static tok_t get_tok(stream &ss);
static void unget_tok(tok_t tok);
static void match(int tok, stream &ss);
static size_t get_label();
static std::string get_unnamed_var_name();
static std::string get_ptr_type_name(std::string base_name);
static std::string get_func_type_name(std::string base_name);

static void top_declaration(stream &ss, context_t &ctx, type_t type, var_t var);
static bool is_strong_class_specifier(tok_t tok);
static bool is_type_specifier(tok_t tok, const type_t type = type_t());
static void try_regulate_basic(stream &ss, type_t &type);
static bool is_type_qualifier(tok_t tok);

}

namespace neko_cc
{
inline void type_t::output_type(std::ostream &ss, int level)
{
	for (int i = 0; i < level; i++) {
		ss << "\t";
	}
	ss << "type name: " << name << " size: " << size << std::endl;

	for (int i = 0; i < level; i++) {
		ss << "\t";
	}
	ss << "type: ";
	if (type == type_unknown) {
		ss << "type_unknown" << std::endl;
	} else if (type == type_norm) {
		ss << "type_norm" << std::endl;
	} else if (type == type_basic) {
		ss << "type_basic" << std::endl;
	} else if (type == type_struct) {
		ss << "type_struct" << std::endl;
	} else if (type == type_union) {
		ss << "type_union" << std::endl;
	} else if (type == type_enum) {
		ss << "type_enum" << std::endl;
	} else if (type == type_typedef) {
		ss << "type_typedef" << std::endl;
	} else if (type == type_func) {
		ss << "type_func" << std::endl;
	} else if (type == type_pointer) {
		ss << "type_pointer" << std::endl;
	} else if (type == type_array) {
		ss << "type_array" << std::endl;
	}

	for (int i = 0; i < level; i++) {
		ss << "\t";
	}
	ss << "properties: ";
	if (type == type_unknown) {
		ss << "none" << std::endl;
	} else if (type == type_norm) {
		ss << "none" << std::endl;
	} else if (type == type_basic) {
		ss << "has_signed: " << has_signed << " ";
		ss << "is_unsigned: " << is_unsigned << " ";
		ss << "is_char: " << is_char << " ";
		ss << "is_short: " << is_short << " ";
		ss << "is_int: " << is_int << " ";
		ss << "is_long: " << is_long << " ";
		ss << "is_float: " << is_float << " ";
		ss << "is_double: " << is_double << " ";
		ss << std::endl;
	} else if (type == type_struct) {
		ss << "inner_vars: ";
		for (auto var : inner_vars) {
			var.output_var(ss, level + 1);
		}
	} else if (type == type_union) {
		ss << "inner_vars: ";
		for (auto var : inner_vars) {
			var.output_var(ss, level + 1);
		}
	} else if (type == type_enum) {
		ss << "none" << std::endl;
	} else if (type == type_typedef) {
		ss << "none" << std::endl;
	} else if (type == type_func) {
		ss << "ret_type: " << std::endl;
		ret_type->output_type(ss, level + 1);
		for (int i = 0; i < level; i++) {
			ss << "\t";
		}
		ss << "args_types: " << std::endl;
		for (auto arg_type : args_type) {
			arg_type.output_type(ss, level + 1);
			for (int i = 0; i < level; i++) {
				ss << "\t";
			}
			ss << "---" << std::endl;
		}
	} else if (type == type_pointer) {
		ss << "pointer to:" << std::endl;
		ptr_to->output_type(ss, level + 1);
	} else if (type == type_array) {
		ss << "array of:" << std::endl;
		ptr_to->output_type(ss, level + 1);
	}
}

}