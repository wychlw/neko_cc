/**
 * @file gen.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Define basic codegen procedure.
 * @version 0.1.2
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once

#include "parse/parse_base.hh"
#include <cstddef>
#include <string>
#include <vector>

namespace neko_cc
{

using std::size_t;
using std::string;

struct emit_t{
    string code;
    var_t var;
};

string get_vreg();

/**
 * @brief Turn the name into an global format name.
 * 
 * @param name 
 * @return string Glofal var name.
 */
string get_global_name(const string &name);

/**
 * @brief Turn the name into an local format name.
 * 
 * @param name 
 * @return string Local var name.
 */
string get_local_name(const string &name);

/**
 * @brief Get the representation of type.
 * 
 * @param type 
 * @return string 
 */
string get_type_repr(const type_t &type);

/**
 * @brief Emit get item in array pointer.
 */
emit_t get_item_from_arrptr(const var_t &rs, const var_t &arr_offset);

/**
 * @brief Emit get item in struct pointer.
 *
 */
emit_t get_item_from_structptr(const var_t &rs, const int &offset);

/**
 * @brief Emit get item in struct object.
 * 
 */
emit_t get_item_from_structobj(const var_t &rs, const int &offset);

/**
 * @brief Emit the beginning of a function.
 * 
 */
emit_t emit_func_begin(const var_t &func, const std::vector<var_t> &args);

/**
 * @brief Emit the end of a function.
 * 
 */
emit_t emit_func_end();

/**
 * @brief Emit a label.
 * 
 */
emit_t emit_label(const string &label);

/**
 * @brief Trunc int type to target type.
 * 
 */
emit_t emit_trunc_to(const var_t &rs, const type_t &type);

/**
 * @brief Zero extend type to bigger type. 
 * 
 */
emit_t emit_zext_to(const var_t &rs, const type_t &type);

/**
 * @brief Signed extend type to bigger type. 
 * 
 */
emit_t emit_sext_to(const var_t &rs, const type_t &type);

/**
 * @brief Float to int.
 * 
 */
emit_t emit_fptosi(const var_t &rs, const type_t &type);

/**
 * @brief Int to float.
 * 
 */
emit_t emit_sitofp(const var_t &rs, const type_t &type);

/**
 * @brief Trunc float to target type.
 * 
 */
emit_t emit_fptrunc_to(const var_t &rs, const type_t &type);

/**
 * @brief Extend float to target type.
 * 
 */
emit_t emit_fpext_to(const var_t &rs, const type_t &type);

/**
 * @brief Conv int to ptr
 * 
 */
emit_t emit_inttoptr(const var_t &rs, const type_t &type);

/**
 * @brief Conv ptr to int
 * 
 */
emit_t emit_ptrtoint(const var_t &rs, const type_t &type);

/**
 * @brief Conv to target type.
 * 
 */
emit_t emit_conv_to(const var_t &rs, const type_t &type);

/**
 * @brief Match two type.
 * 
 */
emit_t emit_match_type(var_t &v1, var_t &v2);

/**
 * @brief Emit a alloca instruction, reutrn a rvalue.
 * 
 * @param type The type need to alloc
 */
emit_t emit_alloca(const type_t &type);

/**
 * @brief Emit a alloca instruction, reutrn a rvalue.
 * This version will guarantee the name.
 * Use this func carefully, as it may break the SSA.
 * 
 * @param type The type need to alloc
 */
emit_t emit_alloca(const type_t &type, const string &name);

/**
 * @brief Emit a declare of a global const variable.
 * 
 * @param var The variable need to declare
 */
emit_t emit_global_const_decl(const var_t &var, const string &init_val);

/**
 * @brief Emit a declare of a global variable, reutrn a rvalue.
 * 
 * @param var The variable need to declare
 */
emit_t emit_global_decl(const var_t &var);

/**
 * @brief Emit a declare of a global variable, reutrn a rvalue.
 * With init value.
 * 
 * @param var The variable need to declare
 */
emit_t emit_global_decl(const var_t &var, const string &init_val);

/**
 * @brief Emit a declare of a global function.
 * 
 * @param var The variable need to declare
 */
emit_t emit_global_func_decl(const var_t &var);

/**
 * @brief Emit an add instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_add(const var_t &v1, const var_t &v2);

/**
 * @brief Emit an sub instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_sub(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a add instruction of the float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fadd(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a sub instruction of the float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fsub(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a mul instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_mul(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a mul instruction of the float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fmul(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a div instruction of a unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */ 
emit_t emit_udiv(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a div instruction of a signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_sdiv(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a div instruction of the float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fdiv(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a rem instruction of a unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_urem(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a rem instruction of a signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_srem(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a rem instruction of the float.
 * Use this function after you know what the meaning of 
 * the remiander of a float div is.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_frem(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a left shift instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_shl(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a right shift instruction of a unsigned.
 * (i.e. logical shift)
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_lshr(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a right shift instruction of a signed.
 * (i.e. arithmetic shift)
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_ashr(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a and instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_and(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a or instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_or(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a xor instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_xor(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a load instruction.
 * 
 * @param rs The rvalue to load
 */
emit_t emit_load(const var_t &rs);

/**
 * @brief Emit a store instruction.
 * 
 * @param rs The rvalue to store
 * @param rd The lvalue to store
 */
emit_t emit_store(const var_t &rs, const var_t &rd);

/**
 * @brief Emit equal instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_eq(const var_t &v1, const var_t &v2);

/**
 * @brief Emit not equal instruction.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_ne(const var_t &v1, const var_t &v2);

/**
 * @brief Emit equal instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_feq(const var_t &v1, const var_t &v2);

/**
 * @brief Emit not equal instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fne(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than instruction for unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_ult(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than instruction for signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_slt(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_flt(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than or equal instruction for unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_ule(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than or equal instruction for signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_sle(const var_t &v1, const var_t &v2);

/**
 * @brief Emit less than or equal instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fle(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than instruction for unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_ugt(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than instruction for signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_sgt(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */ 
emit_t emit_fgt(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than or equal instruction for unsigned.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_uge(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than or equal instruction for signed.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_sge(const var_t &v1, const var_t &v2);

/**
 * @brief Emit greater than or equal instruction for float.
 * 
 * @param v1 Operand 1
 * @param v2 Operand 2
 */
emit_t emit_fge(const var_t &v1, const var_t &v2);

/**
 * @brief Emit a call instruction.
 * 
 * @param func The function to call
 * @param args The arguments
 */
emit_t emit_call(const var_t &func, const std::vector<var_t> &args);

/**
 * @brief Emit a return instruction, with void ret value.
 * 
 */
emit_t emit_ret();

/**
 * @brief Emit a return instruction, with ret value.
 * 
 * @param v The ret value
 */
emit_t emit_ret(const var_t &v);

/**
 * @brief Emit a phi instruction.
 * 
 * @param v1 Operand 1
 * @param label1 Label 1
 * @param v2 Operand 2
 * @param label2 Label 2
 */
emit_t emit_phi(const var_t &v1, const string &label1, const var_t &v2,
           const string &label2);

/**
 * @brief Emit a branch instruction, with no condition.
 * 
 */
emit_t emit_br(const string &label);

/**
 * @brief Emit a branch instruction, with condition.
 * 
 * @param cond The condition
 * @param label_true The label to jump if true
 * @param label_false The label to jump if false
 */
emit_t emit_br(const var_t &cond, const string &label_true,
         const string &label_false);


}