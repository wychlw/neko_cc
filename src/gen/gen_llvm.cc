/**
 * @file gen_llvm.cc
 * @author 泠妄 (lingwang@wcysite.com)
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#include "gen.hh"
#include "parse/parse_base.hh"
#include "scan.hh"
#include "out.hh"

namespace neko_cc
{

static size_t vreg_cnt = 0;

string get_vreg()
{
	return "%vr_" + std::to_string(vreg_cnt++);
}

string get_global_name(const string &name)
{
	return "@" + name;
}

string get_local_name(const string &name)
{
	return "%" + name;
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

emit_t get_item_from_arrptr(const var_t &rs, const var_t &arr_offset)
{
	string rd = get_vreg();
	string code = rd + " = getelementptr " +
		      get_type_repr(*rs.type->ptr_to) + ", " +
		      get_type_repr(*rs.type) + " " + rs.name + ", " +
		      get_type_repr(*arr_offset.type) + " " + arr_offset.name;
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->ptr_to;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t get_item_from_structptr(const var_t &rs, const int &offset)
{
	string rd = get_vreg();
	string code = rd + " = getelementptr " +
		      get_type_repr(*rs.type->ptr_to) + ", " + "ptr " +
		      rs.name + ", " + "i32 0, " + "i32 " +
		      std::to_string(offset);
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = rs.type->ptr_to->inner_vars[offset].type;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t get_item_from_structobj(const var_t &rs, const int &offset)
{
	string rd = get_vreg();
	string code = rd + " = extractvalue " + get_type_repr(*rs.type) + " " +
		      rs.name + ", " + std::to_string(offset);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = rs.type->inner_vars[offset].type;
	return { code, ret };
}

emit_t emit_func_begin(const var_t &func, const std::vector<var_t> &args)
{
	string code = "define ";
	if (func.type->is_static) {
		code += "internal ";
	}
	code += get_type_repr(*func.type->ret_type) + " " + func.name + "(";
	for (const auto &i : args) {
		code += get_type_repr(*i.type) + " " + i.name + ", ";
	}
	if (args.size()) {
		code.pop_back();
		code.pop_back();
	}
	code += ") {";
	vreg_cnt = 0;
	return { code, {} };
}

emit_t emit_func_end()
{
	return { "}", {} };
}

emit_t emit_label(const string &label)
{
	return {label + ":", {}};
}

emit_t emit_trunc_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = trunc " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_zext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = zext " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_sext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = sext " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fptosi(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = fptosi " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_sitofp(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = sitofp " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fptrunc_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = fptrunc " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fpext_to(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = fpext " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_inttoptr(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = inttoptr " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	ret.type->ptr_to = rs.type;
	return { code, ret };
}

emit_t emit_ptrtoint(const var_t &rs, const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = ptrtoint " + get_type_repr(*rs.type) + " " +
		      rs.name + " to " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(type);
	ret.type->ptr_to = rs.type;
	return { code, ret };
}

emit_t emit_conv_to(const var_t &rs, const type_t &type)
{
	if (is_type_void(type)) {
		err_msg("emit_conv_to: type is void");
	}

	if (type == *rs.type) {
		return { "", rs };
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
			return { "", rs };
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
			return { "", rs };
		}
	}
	if (is_type_p(type) && is_type_i(*rs.type)) {
		return emit_inttoptr(rs, type);
	}
	if (is_type_i(type) && is_type_p(*rs.type)) {
		return emit_ptrtoint(rs, type);
	}
	if (is_type_p(type) && is_type_p(*rs.type)) {
		return { "", rs };
	}
	err_msg("emit_conv_to: cannot convert type");
	throw std::logic_error("unreachable");
}

emit_t emit_match_type(var_t &v1, var_t &v2)
{
	bool conv_v1 = false;
	string code = "";
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
		return { code, v1 };
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
		auto tmp = emit_conv_to(v1, *v2.type);
		code = tmp.code;
		v1 = tmp.var;
	} else {
		auto tmp = emit_conv_to(v2, *v1.type);
		code = tmp.code;
		v2 = tmp.var;
	}

	if (is_unsigned) {
		v1.type->is_unsigned = true;
		v2.type->is_unsigned = true;
	}
	return { code, {} };
}

emit_t emit_alloca(const type_t &type)
{
	string rd = get_vreg();
	string code = rd + " = alloca " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	type_t ptr_type;
	if (type.type == type_t::type_array) {
		ptr_type = type;
		ptr_type.type = type_t::type_pointer;
	} else {
		ptr_type.name = get_ptr_type_name(type.name);
		ptr_type.type = type_t::type_pointer;
		ptr_type.size = 8;
		ptr_type.ptr_to = std::make_shared<type_t>(type);
	}
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t emit_alloca(const type_t &type, const string &name)
{
	string rd = name;
	string code = rd + " = alloca " + get_type_repr(type);
	var_t ret;
	ret.name = rd;
	type_t ptr_type;
	if (type.type == type_t::type_array) {
		ptr_type = type;
		ptr_type.type = type_t::type_pointer;
	} else {
		ptr_type.name = get_ptr_type_name(type.name);
		ptr_type.type = type_t::type_pointer;
		ptr_type.size = 8;
		ptr_type.ptr_to = std::make_shared<type_t>(type);
	}
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t emit_global_const_decl(const var_t &var, const string &init_val)
{
	string code = var.name + " = private constant " +
		      get_type_repr(*var.type) + " " + init_val;
	return { code, var };
}

emit_t emit_global_decl(const var_t &var)
{
	string code = var.name + " = global " + get_type_repr(*var.type) +
		      " zeroinitializer";
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	ret.name = var.name;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t emit_global_decl(const var_t &var, const string &init_val)
{
	string code = var.name + " = global " + get_type_repr(*var.type) + " " +
		      init_val;
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	ret.name = var.name;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t emit_global_func_decl(const var_t &var)
{
	string code =
		"declare " + get_type_repr(*var.type) + " " + var.name + "(";
	for (const auto &i : var.type->args_type) {
		code += get_type_repr(i) + ", ";
	}
	if (var.type->args_type.size()) {
		code.pop_back();
		code.pop_back();
	}
	code += ")";
	var_t ret;
	type_t ptr_type;
	ptr_type.type = type_t::type_pointer;
	ptr_type.ptr_to = var.type;
	ret.name = var.name;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(ptr_type);
	return { code, ret };
}

emit_t emit_add(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = add " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_sub(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = sub " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_fadd(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fadd " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_fsub(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fsub " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_mul(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = mul " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_fmul(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fmul " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_sdiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = sdiv " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_udiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = udiv " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_fdiv(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fdiv " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = std::make_shared<type_t>(*v1.type);
	return { code, ret };
}

emit_t emit_srem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = srem " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_urem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = urem " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = true;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_frem(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = frem " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_shl(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = shl " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_lshr(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = lshr " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_ashr(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = ashr " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_and(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = and " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	if (v1.is_alloced && v2.is_alloced) {
		ret.is_alloced = true;
	} else {
		ret.is_alloced = false;
	}
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_or(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = or " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	if (v1.is_alloced && v2.is_alloced) {
		ret.is_alloced = true;
	} else {
		ret.is_alloced = false;
	}
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_xor(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = xor " + get_type_repr(*v1.type) + " " + v1.name +
		      ", " + v2.name;
	var_t ret;
	ret.name = rd;
	if (v1.is_alloced && v2.is_alloced) {
		ret.is_alloced = true;
	} else {
		ret.is_alloced = false;
	}
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_load(const var_t &rs)
{
	string rd = get_vreg();
	string code = rd + " = load " + get_type_repr(*rs.type->ptr_to) + ", " +
		      get_type_repr(*rs.type) + " " + rs.name;
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = rs.type->ptr_to;
	return { code, ret };
}

emit_t emit_store(const var_t &rs, const var_t &rd)
{
	string code = "store " + get_type_repr(*rs.type) + " " + rs.name +
		      ", " + get_type_repr(*rd.type) + " " + rd.name;
	return { code, {} };
}

emit_t emit_eq(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp eq " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_ne(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp ne " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_feq(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp oeq " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fne(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp one " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_ult(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp ult " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_slt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp slt " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_flt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp olt " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_ule(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp ule " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_sle(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp sle " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fle(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp ole " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_ugt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp ugt " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_sgt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp sgt " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fgt(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp ogt " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_uge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp uge " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_sge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = icmp sge " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_fge(const var_t &v1, const var_t &v2)
{
	string rd = get_vreg();
	string code = rd + " = fcmp oge " + get_type_repr(*v1.type) + " " +
		      v1.name + ", " + v2.name;
	var_t ret;
	type_t type;
	type.type = type_t::type_basic;
	type.is_bool = true;
	type.size = 1;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = std::make_shared<type_t>(type);
	return { code, ret };
}

emit_t emit_call(const var_t &func, const std::vector<var_t> &args)
{
	string rd = get_vreg();
	string code = rd + " = call " +
		      get_type_repr(*func.type->ptr_to->ret_type) + " " +
		      func.name + "(";
	for (const auto &i : args) {
		code += get_type_repr(*i.type) + " " + i.name + ", ";
	}
	if (args.size()) {
		code.pop_back();
		code.pop_back();
	}
	code += ")";

	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = func.type->ptr_to->ret_type;
	return { code, ret };
}

emit_t emit_ret()
{
	string code = "ret void";
	return { code, {} };
}

emit_t emit_ret(const var_t &v)
{
	string code = "ret " + get_type_repr(*v.type) + " " + v.name;
	return { code, {} };
}

emit_t emit_phi(const var_t &v1, const string &label1, const var_t &v2,
		const string &label2)
{
	string rd = get_vreg();
	string code = rd + " = phi " + get_type_repr(*v1.type) + " [" +
		      v1.name + ", %" + label1 + "], [" + v2.name + ", %" +
		      label2 + "]";
	var_t ret;
	ret.name = rd;
	ret.is_alloced = false;
	ret.type = v1.type;
	return { code, ret };
}

emit_t emit_br(const string &label)
{
	string code = "br label %" + label;
	return { code, {} };
}

emit_t emit_br(const var_t &cond, const string &label_true,
	       const string &label_false)
{
	string code = "br " + get_type_repr(*cond.type) + " " + cond.name +
		      ", label %" + label_true + ", label %" + label_false;
	return { code, {} };
}

}