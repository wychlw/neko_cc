/**
 * @file parse_top_down.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Define parse procedure for top-down parser.
 * @version 0.1
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once
#include "parse.hh"

namespace neko_cc
{

void top_declaration(stream &ss, context_t &ctx, type_t type, var_t var);

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

void initializer(stream &ss, context_t &ctx, var_t &var);

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

}