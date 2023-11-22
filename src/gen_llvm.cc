/**
 * @file gen_llvm.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include <cstddef>
#include <memory>
#include <sstream>

#include "gen.hh"
#include "parse/parse/parse_top_down.hh"
#include "scan.hh"
#include "out.hh"
#include <stdexcept>
#include <string>

namespace neko_cc
{
static stream *out_stream;

static string post_decl;

void init_emit_engine(stream &ss)
{
	out_stream = &ss;
	post_decl = "";

	emit_line(
		"target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
	emit_line("target triple = \"x86_64-pc-linux-gnu\"");
}
void release_emit_engine()
{
	*out_stream << post_decl;
	out_stream = nullptr;
}

void emit(const string &s)
{
	*out_stream << s;
}

void emit_line(const string &s)
{
	*out_stream << s << std::endl;
}

static size_t vreg = 0;
void reset_vreg()
{
	vreg = 0;
}
void inc_vreg()
{
	vreg++;
}
string get_vreg()
{
	return "%vr_" + std::to_string(vreg++);
}

string get_type_repr(const type_t &type)
{
	if (type.type == type_t::type_unknown ||
	    type.type == type_t::type_typedef) {
		std::stringstream ss;
		type.output_type(&ss);
		err_msg(string("Cannot emit type in ir, with type: \n" +
			       ss.str()));
	}

	if (type.type == type_t::type_func) {
		string ret = "";
		ret += get_type_repr(*type.ret_type);
		ret += " (";
		for (const auto &i : type.args_type) {
			ret += get_type_repr(i);
			ret += ", ";
		}
		if (type.args_type.size()) {
			ret.pop_back();
			ret.pop_back();
		}
		ret += ')';
		return ret;
	}
	if (type.type == type_t::type_basic) {
		if (type.is_bool) {
			return "i1";
		} else if (type.is_char) {
			return "i8";
		} else if (type.is_short) {
			return "i16";
		} else if (type.is_int) {
			return "i32";
		} else if (type.is_long == 1 && !type.is_double) {
			return "i64";
		} else if (type.is_long > 1) {
			return "i64";
		} else if (type.is_float) {
			return "float";
		} else if (type.is_double && !type.is_long) {
			return "double";
		} else if (type.is_double && type.is_long) {
			return "fp128";
		} else {
			return "void";
		}
	}
	if (type.type == type_t::type_struct) {
		string ret = "";
		ret += '{';
		for (const auto &i : type.inner_vars) {
			ret += get_type_repr(*i.type);
			ret += ", ";
		}
		if (type.inner_vars.size()) {
			ret.pop_back();
			ret.pop_back();
		}
		ret += '}';
		return ret;
	}
	if (type.type == type_t::type_union) {
		size_t max_sz = 0;
		for (const auto &i : type.inner_vars) {
			max_sz = std::max(max_sz, i.type->size);
		}
		return 'i' + std::to_string(max_sz);
	}
	if (type.type == type_t::type_enum) {
		return "i32";
	}
	if (type.type == type_t::type_pointer) {
		return "ptr";
	}
	if (type.type == type_t::type_array) {
		string ret = "";
		ret += "[ ";
		size_t len = type.size / (type.ptr_to->size);
		ret += std::to_string(len);
		ret += " x ";
		ret += get_type_repr(*type.ptr_to);
		ret += " ]";
		return ret;
	}
	throw std::logic_error("unreachable");
}

var_t get_item_from_arrptr(const var_t &rs, const var_t &arr_offset)
{
	string rd = get_vreg();
	string prt = rd + " = getelementptr " +
		     get_type_repr(*rs.type->ptr_to) + ", " +
		     get_type_repr(*rs.type) + " " + rs.name + ", " +
		     get_type_repr(*arr_offset.type) + " " + arr_offset.name;
	emit_line(prt);
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->ptr_to;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return ret;
}

var_t get_item_from_structptr(const var_t &rs, const int &offset)
{
	string rd = get_vreg();

	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->ptr_to->inner_vars[offset].type;

	string prt = rd + " = getelementptr " +
		     get_type_repr(*rs.type->ptr_to) + ", ptr " + rs.name +
		     ", i32 0, i32 " + std::to_string(offset);
	emit_line(prt);

	var_t ret;

	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return ret;
}

var_t get_item_from_structobj(const var_t &rs, const int &offset)
{
	string rd = get_vreg();
	string prt = rd + " = extractvalue " + get_type_repr(*rs.type) + " " +
		     rs.name + ", " + std::to_string(offset);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = rs.type->inner_vars[offset].type;
	return ret;
}

void put_item_into_structobj(const type_t &struct_type, const string &rs,
			     const string &rt, const size_t &offset)
{
	string prt = "insertvalue " + get_type_repr(struct_type) + " " + rs +
		     ", " + rt + ", " + std::to_string(offset);
	emit_line(prt);
}

void emit_func_begin(const var_t &func, const std::vector<var_t> &args)
{
	string prt = "";
	prt += "define ";
	if (func.type->is_static) {
		prt += "internal ";
	}
	prt += get_type_repr(*func.type->ret_type);
	prt += ' ';
	prt += func.name + "(";
	for (const auto &i : args) {
		prt += get_type_repr(*i.type);
		prt += " " + i.name + ", ";
	}
	if (args.size()) {
		prt.pop_back();
		prt.pop_back();
	}
	prt += ") {";
	emit_line(prt);
}

void emit_func_end()
{
	emit_line("}");
}

var_t emit_trunc_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = trunc " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_zext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = zext " + get_type_repr(*rs.type) + " " + rs.name +
		     " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_sext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = sext " + get_type_repr(*rs.type) + " " + rs.name +
		     " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fptosi(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = fptosi " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_sitofp(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = sitofp " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fptrunc_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = fptrunc " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fpext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = fpext " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_inttoptr(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = inttoptr " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_ptrtoint(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string prt = rd + " = ptrtoint " + get_type_repr(*rs.type) + " " +
		     rs.name + " to " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_conv_to(const var_t &rs, const type_t &type)
{
	if (is_type_void(type)) {
		err_msg("emit_conv_to: type is void");
	}

	if (type == *rs.type) {
		return rs;
	}

	if (is_type_i(type) && is_type_i(*rs.type)) {
		if (type.size < rs.type->size) {
			return emit_trunc_to(rs, type);
		} else if (type.size > rs.type->size) {
			if (type.is_unsigned) {
				return emit_zext_to(rs, type);
			} else {
				return emit_sext_to(rs, type);
			}
		} else {
			return rs;
		}
	}
	if (is_type_i(type) && is_type_f(*rs.type)) {
		return emit_fptosi(rs, type);
	}
	if (is_type_f(type) && is_type_i(*rs.type)) {
		return emit_sitofp(rs, type);
	}
	if (is_type_f(type) && is_type_f(*rs.type)) {
		if (type.size < rs.type->size) {
			return emit_fptrunc_to(rs, type);
		} else if (type.size > rs.type->size) {
			return emit_fpext_to(rs, type);
		} else {
			return rs;
		}
	}
	if (is_type_p(type) && is_type_i(*rs.type)) {
		return emit_inttoptr(rs, type);
	}
	if (is_type_i(type) && is_type_p(*rs.type)) {
		return emit_ptrtoint(rs, type);
	}
	if (is_type_p(type) && is_type_p(*rs.type)) {
		return rs;
	}
	err_msg("emit_conv_to: cannot convert type");
	throw std::logic_error("unreachable");
}

void emit_match_type(var_t &v1, var_t &v2)
{
	bool conv_v1 = false;
	if (is_type_i(*v1.type) && is_type_i(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_v1 = true;
		} else {
			conv_v1 = false;
		}
	} else if (is_type_f(*v1.type) && is_type_f(*v2.type)) {
		if (v1.type->size < v2.type->size) {
			conv_v1 = true;
		} else {
			conv_v1 = false;
		}
	} else if (is_type_p(*v1.type) && is_type_p(*v2.type)) {
		return;
	} else if (is_type_p(*v1.type) && is_type_i(*v2.type)) {
		conv_v1 = true;
	} else if (is_type_i(*v1.type) && is_type_p(*v2.type)) {
		conv_v1 = false;
	} else if (is_type_f(*v1.type) && is_type_i(*v2.type)) {
		conv_v1 = false;
	} else if (is_type_i(*v1.type) && is_type_f(*v2.type)) {
		conv_v1 = true;
	} else {
		err_msg("emit_match_type: cannot match type");
	}

	bool is_unsigned = conv_v1 ? v2.type->is_unsigned :
				     v1.type->is_unsigned;

	if (conv_v1) {
		v1 = emit_conv_to(v1, *v2.type);
	} else {
		v2 = emit_conv_to(v2, *v1.type);
	}

	if (is_unsigned) {
		v1.type->is_unsigned = true;
		v2.type->is_unsigned = true;
	}
}

var_t emit_load(const var_t &rs)
{
	if (rs.type->type != type_t::type_pointer) {
		err_msg("emit_load: rs is not a pointer");
	}
	string rd = get_vreg();
	string prt = rd + " = load " + get_type_repr(*rs.type->ptr_to) + ", " +
		     get_type_repr(*rs.type) + " " + rs.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = rs.type->ptr_to;
	return ret;
}

void emit_store(const var_t &rd, const var_t &rs)
{
	if (rd.type->type != type_t::type_pointer) {
		err_msg("emit_store: rd is not a pointer");
	}
	string prt = "store " + get_type_repr(*rs.type) + " " + rs.name + ", " +
		     get_type_repr(*rd.type) + " " + rd.name;
	emit_line(prt);
}

var_t emit_alloc(const type_t &type, const string &name)
{
	string rd = name;
	string prt = rd + " = alloca " + get_type_repr(type);
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	type_t new_type;
	if (type.type != type_t::type_array) {
		new_type.name = get_ptr_type_name(type.name);
		new_type.size = 8;
		new_type.type = type_t::type_pointer;
		new_type.ptr_to = std::make_shared<type_t>(type);
	} else {
		new_type = type;
		new_type.type = type_t::type_pointer;
	}
	ret.type = std::make_shared<type_t>(new_type);
	ret.is_alloced = true;
	return ret;
}

var_t emit_alloc(const type_t &type)
{
	string rd = get_vreg();
	return emit_alloc(type, rd);
}

void emit_post_const_decl(const var_t &var, const string &init_val)
{
	string prt = "";
	prt += var.name + " = private constant ";
	prt += get_type_repr(*var.type);
	prt += " " + init_val;
	post_decl += prt + '\n';
}

var_t emit_global_decl(const var_t &var)
{
	string prt = "";
	prt += var.name + " = global ";
	prt += get_type_repr(*var.type);
	prt += " zeroinitializer";
	emit_line(prt);
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(ptr_type);
	ret.is_alloced = 1;
	return ret;
}

var_t emit_global_decl(const var_t &var, const string &init_val)
{
	string prt = "";
	prt += var.name + " = global ";
	prt += get_type_repr(*var.type);
	prt += " " + init_val;
	emit_line(prt);
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(ptr_type);
	ret.is_alloced = 1;
	return ret;
}

var_t emit_global_fun_decl(const var_t &var)
{
	string prt = "";
	prt += "declare ";
	prt += get_type_repr(*var.type->ret_type);
	prt += ' ';
	prt += var.name + "(";
	for (const auto &i : var.type->args_type) {
		prt += get_type_repr(i);
		prt += ", ";
	}
	if (var.type->args_type.size()) {
		prt.pop_back();
		prt.pop_back();
	}
	prt += ")";
	emit_line(prt);
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(ptr_type);
	ret.is_alloced = 1;
	return ret;
}

var_t emit_add(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = add " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_sub(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = sub " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_fadd(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fadd " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_fsub(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fsub " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_xor(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = xor " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_eq(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp eq " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_call(const var_t &func, const std::vector<var_t> &args)
{
	string rd = get_vreg();
	string prt = rd + " = call " +
		     get_type_repr(*func.type->ptr_to->ret_type) + " " +
		     func.name + "(";
	for (const auto &i : args) {
		prt += get_type_repr(*i.type);
		prt += " " + i.name + ", ";
	}
	if (args.size()) {
		prt.pop_back();
		prt.pop_back();
	}
	prt += ")";
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = func.type->ptr_to->ret_type;
	return ret;
}

var_t emit_mul(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = mul " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_fmul(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fmul " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_udiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = udiv " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_sdiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = sdiv " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_fdiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fdiv " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_urem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = urem " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_srem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = srem " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_frem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = frem " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_shl(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = shl " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_lshr(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = lshr " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_unsigned = true;
	return ret;
}

var_t emit_ashr(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = ashr " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_unsigned = false;
	return ret;
}

var_t emit_feq(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp oeq " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_ne(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp ne " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_ugt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp ugt " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_uge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp uge " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_ult(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp ult " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_ule(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp ule " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_sgt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp sgt " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_sge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp sge " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_slt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp slt " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_sle(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = icmp sle " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fne(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp one " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	return ret;
}

var_t emit_fgt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp ogt " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp oge " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_flt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp olt " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fle(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = fcmp ole " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = std::make_shared<type_t>(*v1.type);
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_and(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = and " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_or(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = or " + get_type_repr(*v1.type) + " " + v1.name +
		     ", " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

var_t emit_select(const var_t &cond, const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string prt = rd + " = select " + get_type_repr(*cond.type) + " " +
		     cond.name + ", " + get_type_repr(*v1.type) + " " +
		     v1.name + ", " + get_type_repr(*v2.type) + " " + v2.name;
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

void emit_br(const var_t &cond, const string &label_true,
	     const string &label_false)
{
	string prt = "br " + get_type_repr(*cond.type) + " " + cond.name +
		     ", " + "label %" + label_true + ", label %" + label_false;
	emit_line(prt);
}

void emit_br(const string &label)
{
	string prt = "br label %" + label;
	emit_line(prt);
}

void emit_label(const string &label)
{
	string prt = label + ":";
	emit_line(prt);
}

var_t emit_phi(const var_t &v1, const string &label1, const var_t &v2,
	       const string &label2)
{
	string rd = get_vreg();
	string prt = rd + " = phi " + get_type_repr(*v1.type) + " [" + v1.name +
		     ", %" + label1 + "], [" + v2.name + ", %" + label2 + "]";
	emit_line(prt);
	var_t ret;
	ret.name = rd;
	ret.type = v1.type;
	return ret;
}

void emit_ret()
{
	string prt = "ret void";
	emit_line(prt);
}

void emit_ret(const var_t &v)
{
	string prt = "ret " + get_type_repr(*v.type) + " " + v.name;
	emit_line(prt);
}

}