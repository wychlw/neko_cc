/**
 * @file gen.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Define basic codegen procedure.
 * @version 0.1
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once

#include "autoconf.h"
#include "parse/parse_base.hh"
#include <cstddef>
#include <string>
#include <ios>
#include <vector>

namespace neko_cc
{
using std::size_t;
using std::string;
using stream = std::basic_iostream<char>;

#ifndef CONFIG_SELECT_CODE_GEN_FORMAT_DUMMY
void init_emit_engine(stream &ss);
#else
void init_emit_engine(stream &ss, bool _need_log = true);
#endif

void release_emit_engine();

void emit(const string &s);
void emit_line(const string &s);

void reset_vreg();
void inc_vreg();
string get_vreg();

string get_type_repr(const type_t &type);

/**
 * Emit get item in array pointer.
 * @return vreg where the val store
 */
var_t get_item_from_arrptr(const var_t &rs, const var_t &arr_offset);

/**
 * Emit get item in struct pointer.
 */
var_t get_item_from_structptr(const var_t &rs, const int &offset);

/**
 * Emit get item in struct object.
 */
var_t get_item_from_structobj(const var_t &rs, const int &offset);

// /**
//  * Emit put item into struct.
//  * Rt should follow with type.
//  */
// void put_item_into_structobj(const type_t &struct_type, const string &rs,
// 			     const string &rt, const size_t &offset);

void emit_func_begin(const var_t &func, const std::vector<var_t> &args);

void emit_func_end();

void emit_label(const string &label);

/**
 * Trunc int type to target type.
 */
var_t emit_trunc_to(const var_t &rs, const type_t &type);

/**
 * Zero extend type to bigger type. 
 */
var_t emit_zext_to(const var_t &rs, const type_t &type);

/**
 * Signed extend type to bigger type. 
 */
var_t emit_sext_to(const var_t &rs, const type_t &type);

/**
 * Float to int.
 */
var_t emit_fptosi(const var_t &rs, const type_t &type);

/**
 * Int to float.
 */
var_t emit_sitofp(const var_t &rs, const type_t &type);

/**
 * Trunc float to target type.
 */
var_t emit_fptrunc_to(const var_t &rs, const type_t &type);

/**
 * Extend float to target type.
 */
var_t emit_fpext_to(const var_t &rs, const type_t &type);

/**
 * Conv int to ptr
 */
var_t emit_inttoptr(const var_t &rs, const type_t &type);

/**
 * Conv ptr to int
 */
var_t emit_ptrtoint(const var_t &rs, const type_t &type);

/**
 * Conv to target type.
 */
var_t emit_conv_to(const var_t &rs, const type_t &type);

/**
 * Match two type.
 */
void emit_match_type(var_t &v1, var_t &v2);

var_t emit_alloc(const type_t &type);

var_t emit_alloc(const type_t &type, const string &name);

void emit_post_const_decl(const var_t &var, const string &init_val);

var_t emit_global_decl(const var_t &var);

var_t emit_global_decl(const var_t &var, const string &init_val);

var_t emit_global_fun_decl(const var_t &var);

var_t emit_add(const var_t &v1, const var_t &v2);

var_t emit_sub(const var_t &v1, const var_t &v2);

var_t emit_fadd(const var_t &v1, const var_t &v2);

var_t emit_fsub(const var_t &v1, const var_t &v2);

var_t emit_mul(const var_t &v1, const var_t &v2);

var_t emit_fmul(const var_t &v1, const var_t &v2);

var_t emit_udiv(const var_t &v1, const var_t &v2);

var_t emit_sdiv(const var_t &v1, const var_t &v2);

var_t emit_fdiv(const var_t &v1, const var_t &v2);

var_t emit_urem(const var_t &v1, const var_t &v2);

var_t emit_srem(const var_t &v1, const var_t &v2);

var_t emit_frem(const var_t &v1, const var_t &v2);

var_t emit_shl(const var_t &v1, const var_t &v2);

var_t emit_lshr(const var_t &v1, const var_t &v2);

var_t emit_ashr(const var_t &v1, const var_t &v2);

var_t emit_and(const var_t &v1, const var_t &v2);

var_t emit_or(const var_t &v1, const var_t &v2);

var_t emit_xor(const var_t &v1, const var_t &v2);

var_t emit_load(const var_t &rs);

void emit_store(const var_t &rd, const var_t &rs);

var_t emit_eq(const var_t &v1, const var_t &v2);

var_t emit_ne(const var_t &v1, const var_t &v2);

var_t emit_ugt(const var_t &v1, const var_t &v2);

var_t emit_uge(const var_t &v1, const var_t &v2);

var_t emit_ult(const var_t &v1, const var_t &v2);

var_t emit_ule(const var_t &v1, const var_t &v2);

var_t emit_sgt(const var_t &v1, const var_t &v2);

var_t emit_sge(const var_t &v1, const var_t &v2);

var_t emit_slt(const var_t &v1, const var_t &v2);

var_t emit_sle(const var_t &v1, const var_t &v2);

var_t emit_feq(const var_t &v1, const var_t &v2);

var_t emit_fne(const var_t &v1, const var_t &v2);

var_t emit_fgt(const var_t &v1, const var_t &v2);

var_t emit_fge(const var_t &v1, const var_t &v2);

var_t emit_flt(const var_t &v1, const var_t &v2);

var_t emit_fle(const var_t &v1, const var_t &v2);

var_t emit_phi(const var_t &v1, const string &label1, const var_t &v2,
	       const string &label2);

var_t emit_select(const var_t &cond, const var_t &v1, const var_t &v2);

void emit_ret();

void emit_ret(const var_t &v);

var_t emit_call(const var_t &func, const std::vector<var_t> &args);

void emit_br(const var_t &cond, const string &label_true,
	     const string &label_false);

void emit_br(const string &label);

}