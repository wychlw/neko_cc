/**
 * @file gen_dummy.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include <cstddef>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "gen.hh"
#include "parse/parse_top_down.hh"
#include "scan.hh"
#include "out.hh"

namespace neko_cc
{

static stream *out_stream;

static string post_decl;

static bool need_log;

void init_emit_engine(stream &ss, bool _need_log)
{
	out_stream = &ss;
	post_decl = "";

	need_log = true;
	emit_line("Gen code begin.");
	info("Gen code begin.");
	warn("Using dummy emit engine.");
	need_log = _need_log;
}
void release_emit_engine()
{
	*out_stream << post_decl;
	need_log = true;
	emit_line("Gen code end.");
	info("Gen code end.");
	out_stream = nullptr;
}

void emit(const string &s)
{
	if (!need_log)
		return;
	*out_stream << s;
}

void emit_line(const string &s)
{
	if (!need_log)
		return;
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
	info("get_vreg" + std::to_string(vreg));
	return "%vr_" + std::to_string(vreg++);
}

string get_type_repr(const type_t &type)
{
	emit_line("Getting type repr for: \n" + type.str());

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
	emit_line("Getting item from arrptr: ARR, OFFSRT\n");
	emit_line(rs.str());
	emit_line(arr_offset.name);
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->ptr_to;
	ret.name = get_vreg();
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return ret;
}

var_t get_item_from_structptr(const var_t &rs, const int &offset)
{
	emit_line("Getting item from structptr: STRUCT, OFFSRT\n");
	emit_line(rs.str());
	emit_line(std::to_string(offset));
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->inner_vars[offset].type;
	ret.name = get_vreg();
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return ret;
}

var_t get_item_from_structobj(const var_t &rs, const int &offset)
{
	emit_line("Getting item from structobj: STRUCT, OFFSRT\n");
	emit_line(rs.str());
	emit_line(std::to_string(offset));
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->inner_vars[offset].type;
	ret.name = get_vreg();
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return ret;
}

static string func_name;
void emit_func_begin(const var_t &func, const std::vector<var_t> &args)
{
	emit_line("Function begin: FUNC, ARGS\n");
	emit_line(func.name);
	func_name = func.name;
	for (const auto &i : args) {
		emit_line(i.name);
	}
}

void emit_func_end()
{
	emit_line("Function end: FUNC.");
	emit_line(func_name);
}

void emit_label(const string &label)
{
	emit_line("Label: LABEL\n");
	emit_line(label);
}

var_t emit_trunc_to(const var_t &rs, const type_t &type)
{
	emit_line("Try truncing type: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_zext_to(const var_t &rs, const type_t &type)
{
	emit_line("Try zexting: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_sext_to(const var_t &rs, const type_t &type)
{
	emit_line("Try sexting: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fptosi(const var_t &rs, const type_t &type)
{
	emit_line("Try fptosi: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_sitofp(const var_t &rs, const type_t &type)
{
	emit_line("Try sitofp: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fptrunc_to(const var_t &rs, const type_t &type)
{
	emit_line("Try fptrunc: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_fpext_to(const var_t &rs, const type_t &type)
{
	emit_line("Try fpext: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_inttoptr(const var_t &rs, const type_t &type)
{
	emit_line("Try inttoptr: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(type);
	return ret;
}

var_t emit_ptrtoint(const var_t &rs, const type_t &type)
{
	emit_line("Try ptrtoint: RS, TYPE\n");
	emit_line(rs.str());
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
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
	emit_line("Trying match type.");
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

var_t emit_alloc(const type_t &type)
{
	emit_line("Allocating type: TYPE\n");
	emit_line(type.str());
	var_t ret;
	ret.name = get_vreg();
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

var_t emit_alloc(const type_t &type, const string &name)
{
	emit_line("Allocating type: TYPE, NAME\n");
	emit_line(type.str());
	emit_line(name);
	var_t ret;
	ret.name = name;
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

void emit_post_const_decl(const var_t &var, const string &init_val)
{
	emit_line("Post const decl: VAR, INIT_VAL\n");
	emit_line(var.str());
	emit_line(init_val);
	post_decl += var.name + " = " + init_val + "\n";
}

var_t emit_global_decl(const var_t &var)
{
	emit_line("Global decl: VAR\n");
	emit_line(var.str());
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(*var.type);
	ret.is_alloced = true;
	return ret;
}

var_t emit_global_decl(const var_t &var, const string &init_val)
{
	emit_line("Global decl: VAR, INIT_VAL\n");
	emit_line(var.str());
	emit_line(init_val);
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(*var.type);
	ret.is_alloced = true;
	return ret;
}

var_t emit_global_fun_decl(const var_t &var)
{
	emit_line("Global fun decl: VAR\n");
	emit_line(var.str());
	var_t ret;
	ret.name = var.name;
	ret.type = std::make_shared<type_t>(*var.type);
	ret.is_alloced = true;
	return ret;
}

var_t emit_add(const var_t &v1, const var_t &v2)
{
	emit_line("Try adding: " + v1.name + " + " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_sub(const var_t &v1, const var_t &v2)
{
	emit_line("Try subing: " + v1.name + " - " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_fadd(const var_t &v1, const var_t &v2)
{
	emit_line("Try fadding: " + v1.name + " + " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_fsub(const var_t &v1, const var_t &v2)
{
	emit_line("Try fsubing: " + v1.name + " - " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_mul(const var_t &v1, const var_t &v2)
{
	emit_line("Try muling: " + v1.name + " * " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_fmul(const var_t &v1, const var_t &v2)
{
	emit_line("Try fmuling: " + v1.name + " * " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_udiv(const var_t &v1, const var_t &v2)
{
	emit_line("Try udiving: " + v1.name + " / " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_sdiv(const var_t &v1, const var_t &v2)
{
	emit_line("Try sdiving: " + v1.name + " / " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_fdiv(const var_t &v1, const var_t &v2)
{
	emit_line("Try fdiving: " + v1.name + " / " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_urem(const var_t &v1, const var_t &v2)
{
	emit_line("Try ureming: " + v1.name + " % " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_srem(const var_t &v1, const var_t &v2)
{
	emit_line("Try sreming: " + v1.name + " % " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_frem(const var_t &v1, const var_t &v2)
{
	emit_line("Try freming: " + v1.name + " % " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_shl(const var_t &v1, const var_t &v2)
{
	emit_line("Try shling: " + v1.name + " << " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_lshr(const var_t &v1, const var_t &v2)
{
	emit_line("Try lshring: " + v1.name + " >> " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_ashr(const var_t &v1, const var_t &v2)
{
	emit_line("Try ashring: " + v1.name + " >> " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_and(const var_t &v1, const var_t &v2)
{
	emit_line("Try anding: " + v1.name + " & " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_or(const var_t &v1, const var_t &v2)
{
	emit_line("Try oring: " + v1.name + " | " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_xor(const var_t &v1, const var_t &v2)
{
	emit_line("Try xoring: " + v1.name + " ^ " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*v1.type);
	return ret;
}

var_t emit_load(const var_t &rs)
{
	emit_line("Try loading: " + rs.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = rs.type->ptr_to;
	return ret;
}

void emit_store(const var_t &rd, const var_t &rs)
{
	emit_line("Try storing: " + rd.name + " <- " + rs.name);
}

var_t emit_eq(const var_t &v1, const var_t &v2)
{
	emit_line("Try eqing: " + v1.name + " == " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_ne(const var_t &v1, const var_t &v2)
{
	emit_line("Try neing: " + v1.name + " != " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_ugt(const var_t &v1, const var_t &v2)
{
	emit_line("Try uging: " + v1.name + " > " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_uge(const var_t &v1, const var_t &v2)
{
	emit_line("Try ugeing: " + v1.name + " >= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_ult(const var_t &v1, const var_t &v2)
{
	emit_line("Try ulting: " + v1.name + " < " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_ule(const var_t &v1, const var_t &v2)
{
	emit_line("Try uleing: " + v1.name + " <= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_sgt(const var_t &v1, const var_t &v2)
{
	emit_line("Try sging: " + v1.name + " > " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_sge(const var_t &v1, const var_t &v2)
{
	emit_line("Try sgeing: " + v1.name + " >= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_slt(const var_t &v1, const var_t &v2)
{
	emit_line("Try slting: " + v1.name + " < " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_sle(const var_t &v1, const var_t &v2)
{
	emit_line("Try sleing: " + v1.name + " <= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_feq(const var_t &v1, const var_t &v2)
{
	emit_line("Try feqing: " + v1.name + " == " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	ret.type->is_bool = true;
	return ret;
}

var_t emit_fne(const var_t &v1, const var_t &v2)
{
	emit_line("Try fneing: " + v1.name + " != " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fgt(const var_t &v1, const var_t &v2)
{
	emit_line("Try fgt: " + v1.name + " > " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fge(const var_t &v1, const var_t &v2)
{
	emit_line("Try fge: " + v1.name + " >= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_flt(const var_t &v1, const var_t &v2)
{
	emit_line("Try flt: " + v1.name + " < " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_fle(const var_t &v1, const var_t &v2)
{
	emit_line("Try fle: " + v1.name + " <= " + v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>();
	ret.type->type = type_t::type_basic;
	ret.type->is_bool = true;
	ret.type->size = 1;
	return ret;
}

var_t emit_phi(const var_t &v1, const string &label1, const var_t &v2,
	       const string &label2)
{
	emit_line("Select branch between: " + v1.name + " " + label1 + " " +
		  v2.name + " " + label2);
	var_t ret;
	ret.name = get_vreg();
	ret.type = v1.type;
	return ret;
}

var_t emit_select(const var_t &cond, const var_t &v1, const var_t &v2)
{
	emit_line("Select between: " + cond.name + " " + v1.name + " " +
		  v2.name);
	var_t ret;
	ret.name = get_vreg();
	ret.type = v1.type;
	return ret;
}

void emit_ret()
{
	emit_line("Try ret NULL");
}

void emit_ret(const var_t &v)
{
	emit_line("Try ret: " + v.name);
}

var_t emit_call(const var_t &func, const std::vector<var_t> &args)
{
	emit_line("Try call: " + func.name);
	emit_line("ARGS:");
	for (const auto &i : args) {
		emit_line(i.name);
	}
	var_t ret;
	ret.name = get_vreg();
	ret.type = std::make_shared<type_t>(*func.type->ret_type);
	return ret;
}

void emit_br(const var_t &cond, const string &label_true,
	     const string &label_false)
{
	emit_line("Try br: " + cond.name + " " + label_true + " " +
		  label_false);
}

void emit_br(const string &label)
{
	emit_line("Try br: " + label);
}
}