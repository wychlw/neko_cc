#pragma once

#include <cstddef>
#include <sstream>
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
	int is_bool;
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
		is_bool = 0;
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

	void output_type(stream *ss, int level = 0) const;
	friend std::ostream &operator<<(std::ostream &ss, type_t &type)
	{
		type.output_type(dynamic_cast<stream *>(&ss));
		return ss;
	}

	friend bool operator==(const type_t &lhs, const type_t &rhs);
};

struct var_t {
	std::string name;
	std::shared_ptr<type_t> type;
	bool is_alloced = false;

	void output_var(stream *ss, int level = 0) const
	{
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "var name: " << name << " alloc: " << is_alloced
		    << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "has type:" << std::endl;
		type->output_type(ss, level + 1);
	}
	std::string str() const
	{
		std::stringstream ss;
		output_var(&ss);
		return ss.str();
	}
	friend std::ostream &operator<<(std::ostream &ss, var_t &var)
	{
		var.output_var(dynamic_cast<stream *>(&ss));
		return ss;
	}
};

struct fun_env_t {
	bool is_func;

	type_t ret_type;

    private:
    public:
	fun_env_t()
		: is_func(false)
		, ret_type()
	{
	}
};

struct context_t {
	context_t *prev_context;

	std::unordered_map<std::string, var_t> vars;
	std::unordered_map<std::string, type_t> types;
	std::unordered_map<std::string, int> enums;
	std::shared_ptr<fun_env_t> fun_env;

	std::string beg_label = "";
	std::string end_label = "";

	context_t(context_t &rhs) = delete;
	context_t &operator=(context_t &rhs) = delete;
	context_t(context_t *_prev = nullptr,
		  std::unordered_map<std::string, var_t> _vars = {},
		  std::unordered_map<std::string, type_t> _types = {})
		: prev_context(_prev)
		, vars(_vars)
		, types(_types)
		, enums()
	{
		if (_prev != nullptr) {
			fun_env = _prev->fun_env;
		} else {
			fun_env = nullptr;
		}
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
		type_t unknown_type;
		unknown_type.type = type_t::type_unknown;
		unknown_var.type = std::make_shared<type_t>(unknown_type);
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
void external_declaration(stream &ss, context_t &ctx);
void function_definition(stream &ss, context_t &ctx, var_t function,
			 std::vector<var_t> args);
int constant_expression(stream &ss, context_t &ctx);

void declaration_specifiers(stream &ss, context_t &ctx, type_t &type);
void init_declarator(stream &ss, context_t &ctx, type_t type);
void strong_class_specfifer(stream &ss, context_t &ctx, type_t &type);
void type_specifier(stream &ss, context_t &ctx, type_t &type);
void struct_or_union_specifier(stream &ss, context_t &ctx, type_t &type);
void struct_declaration_list(stream &ss, context_t &ctx, type_t &type);

void type_qualifier(stream &ss, context_t &ctx, type_t &type);
std::vector<var_t> declarator(stream &ss, context_t &ctx,
			      std::shared_ptr<type_t> type, var_t &var);
std::vector<var_t> direct_declarator(stream &ss, context_t &ctx,
				     std::shared_ptr<type_t> type, var_t &var);

std::shared_ptr<type_t> pointer(stream &ss, context_t &ctx,
				std::shared_ptr<type_t> type);
std::vector<var_t> abstract_declarator(stream &ss, context_t &ctx,
				       std::shared_ptr<type_t> type,
				       var_t &var);
std::vector<var_t> direct_abstract_declarator(stream &ss, context_t &ctx,
					      std::shared_ptr<type_t> type,
					      var_t &var);

std::vector<var_t> parameter_type_list(stream &ss, context_t &ctx);

std::vector<var_t> parameter_type_list(stream &ss, context_t &ctx);
std::vector<var_t> parameter_list(stream &ss, context_t &ctx);
var_t parameter_declaration(stream &ss, context_t &ctx);

UNFIN void initializer(stream &ss, context_t &ctx, var_t &var);

void enum_specifier(stream &ss, context_t &ctx, type_t &type);
void enumerator_list(stream &ss, context_t &ctx);
void enumerator(stream &ss, context_t &ctx, int &val);
void struct_declaration_list(stream &ss, context_t &ctx, type_t &struct_type);
void struct_declaration(stream &ss, context_t &ctx, type_t &struct_type);
void specifier_qualifier_list(stream &ss, context_t &ctx, type_t &type);
std::vector<var_t> struct_declarator_list(stream &ss, context_t &ctx,
					  type_t &type);
void compound_statement(stream &ss, context_t &ctx);
void declaration_list(stream &ss, context_t &ctx);
void statement_list(stream &ss, context_t &ctx);
void statement(stream &ss, context_t &ctx);
var_t unary_expression(stream &ss, context_t &ctx);
var_t cast_expression(stream &ss, context_t &ctx);
type_t type_name(stream &ss, context_t &ctx);
var_t primary_expression(stream &ss, context_t &ctx);
var_t postfix_expression(stream &ss, context_t &ctx);
std::vector<var_t> argument_expression_list(stream &ss, context_t &ctx);
var_t multiplicative_expression(stream &ss, context_t &ctx);
var_t additive_expression(stream &ss, context_t &ctx);
var_t shift_expression(stream &ss, context_t &ctx);
var_t relational_expression(stream &ss, context_t &ctx);
var_t equality_expression(stream &ss, context_t &ctx);
var_t and_expression(stream &ss, context_t &ctx);
var_t exclusive_or_expression(stream &ss, context_t &ctx);
var_t inclusive_or_expression(stream &ss, context_t &ctx);
var_t logical_and_expression(stream &ss, context_t &ctx);
var_t logical_or_expression(stream &ss, context_t &ctx);
var_t conditional_expression(stream &ss, context_t &ctx);
void assignment_operator(stream &ss, var_t rd, var_t rs, tok_t op);
var_t assignment_expression(stream &ss, context_t &ctx);
var_t expression(stream &ss, context_t &ctx);
void expression_statement(stream &ss, context_t &ctx);
void jump_statement(stream &ss, context_t &ctx);
void selection_statement(stream &ss, context_t &ctx);
void iteration_statement(stream &ss, context_t &ctx);

tok_t nxt_tok(stream &ss);
tok_t get_tok(stream &ss);
void unget_tok(tok_t tok);
void match(int tok, stream &ss);
std::string get_label();
std::string get_unnamed_var_name();
std::string get_ptr_type_name(std::string base_name);
std::string get_func_type_name(std::string base_name);

void top_declaration(stream &ss, context_t &ctx, type_t type, var_t var);
bool is_strong_class_specifier(tok_t tok);
bool is_declaration_specifiers(tok_t tok, context_t &ctx);
bool is_type_specifier(tok_t tok, context_t &ctx);
bool is_type_void(const type_t &type);
bool is_type_i(const type_t &type);
bool is_type_f(const type_t &type);
bool is_type_p(const type_t &type);
void try_regulate_basic(stream &ss, type_t &type);
bool is_type_qualifier(tok_t tok);
bool is_selection_statement(tok_t tok);
bool is_iteration_statement(tok_t tok);
bool is_jump_statement(tok_t tok);
bool chk_if_abstract_nested(stream &ss, context_t &ctx);
bool is_assignment_operatior(tok_t tok);
bool is_specifier_qualifier_list(tok_t tok, context_t &ctx);
bool chk_has_assignment_operator(stream &ss);
bool is_unary_operator(tok_t tok);
bool is_postfix_expression_op(tok_t tok);
bool is_mul_op(tok_t tok);
bool is_add_op(tok_t tok);
bool is_shift_op(tok_t tok);
bool is_rel_op(tok_t tok);
bool is_eq_op(tok_t tok);
bool should_conv_to_first(const var_t &v1, const var_t &v2);

}

namespace neko_cc
{
inline void type_t::output_type(stream *ss, int level) const
{
	for (int i = 0; i < level; i++) {
		*ss << "\t";
	}
	*ss << "type name: " << name << " size: " << size << std::endl;

	for (int i = 0; i < level; i++) {
		*ss << "\t";
	}
	*ss << "type: ";
	if (type == type_unknown) {
		*ss << "type_unknown" << std::endl;
	} else if (type == type_basic) {
		*ss << "type_basic" << std::endl;
	} else if (type == type_struct) {
		*ss << "type_struct" << std::endl;
	} else if (type == type_union) {
		*ss << "type_union" << std::endl;
	} else if (type == type_enum) {
		*ss << "type_enum" << std::endl;
	} else if (type == type_typedef) {
		*ss << "type_typedef" << std::endl;
	} else if (type == type_func) {
		*ss << "type_func" << std::endl;
	} else if (type == type_pointer) {
		*ss << "type_pointer" << std::endl;
	} else if (type == type_array) {
		*ss << "type_array" << std::endl;
	}

	for (int i = 0; i < level; i++) {
		*ss << "\t";
	}
	*ss << "properties: ";
	if (type == type_unknown) {
		*ss << "none" << std::endl;
	} else if (type == type_basic) {
		*ss << "has_signed: " << has_signed << " ";
		*ss << "is_unsigned: " << is_unsigned << " ";
		*ss << "is_bool: " << is_bool << " ";
		*ss << "is_char: " << is_char << " ";
		*ss << "is_short: " << is_short << " ";
		*ss << "is_int: " << is_int << " ";
		*ss << "is_long: " << is_long << " ";
		*ss << "is_float: " << is_float << " ";
		*ss << "is_double: " << is_double << " ";
		*ss << std::endl;
	} else if (type == type_struct) {
		*ss << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    inner_vars: ";
		for (auto var : inner_vars) {
			var.output_var(ss, level + 1);
		}
	} else if (type == type_union) {
		*ss << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    inner_vars: ";
		for (auto var : inner_vars) {
			var.output_var(ss, level + 1);
		}
	} else if (type == type_enum) {
		*ss << "none" << std::endl;
	} else if (type == type_typedef) {
		*ss << "none" << std::endl;
	} else if (type == type_func) {
		*ss << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    ret_type: " << std::endl;
		ret_type->output_type(ss, level + 1);
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    args_types: " << std::endl;
		for (auto arg_type : args_type) {
			arg_type.output_type(ss, level + 1);
			for (int i = 0; i < level; i++) {
				*ss << "\t";
			}
			*ss << "---" << std::endl;
		}
	} else if (type == type_pointer) {
		*ss << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    pointer to:" << std::endl;
		ptr_to->output_type(ss, level + 1);
	} else if (type == type_array) {
		*ss << std::endl;
		for (int i = 0; i < level; i++) {
			*ss << "\t";
		}
		*ss << "    array of:" << std::endl;
		ptr_to->output_type(ss, level + 1);
	}
}

inline bool operator==(const type_t &lhs, const type_t &rhs)
{
	if (lhs.type != rhs.type) {
		return false;
	}
	if (lhs.type == type_t::type_unknown || lhs.type == type_t::type_enum) {
		return true;
	}
	if (lhs.type == type_t::type_basic) {
		return lhs.has_signed == rhs.has_signed &&
		       lhs.is_unsigned == rhs.is_unsigned &&
		       lhs.is_bool == rhs.is_bool &&
		       lhs.is_char == rhs.is_char &&
		       lhs.is_short == rhs.is_short &&
		       lhs.is_int == rhs.is_int && lhs.is_long == rhs.is_long &&
		       lhs.is_float == rhs.is_float &&
		       lhs.is_double == rhs.is_double;
	}
	if (lhs.type == type_t::type_struct || lhs.type == type_t::type_union) {
		if (lhs.inner_vars.size() != rhs.inner_vars.size()) {
			return false;
		}
		for (size_t i = 0; i < lhs.inner_vars.size(); i++) {
			if (!(*(lhs.inner_vars[i].type) ==
			      *(rhs.inner_vars[i].type))) {
				return false;
			}
		}
		return true;
	}
	if (lhs.type == type_t::type_typedef) {
		return lhs.target_type == rhs.target_type;
	}
	if (lhs.type == type_t::type_func) {
		if (!(*(lhs.ret_type) == *(rhs.ret_type))) {
			return false;
		}
		if (lhs.args_type.size() != rhs.args_type.size()) {
			return false;
		}
		for (size_t i = 0; i < lhs.args_type.size(); i++) {
			if (!((lhs.args_type[i]) == (rhs.args_type[i]))) {
				return false;
			}
		}
		return true;
	}
	if (lhs.type == type_t::type_pointer) {
		return *(lhs.ptr_to) == *(rhs.ptr_to);
	}
	if (lhs.type == type_t::type_array) {
		return *(lhs.ptr_to) == *(rhs.ptr_to);
	}
	return false;
}

}