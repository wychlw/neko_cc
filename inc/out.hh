/**
 * @file out.hh
 * @author 泠妄 (lingwang@wcysite.com)
 * @brief Define utility error and expetation reporting functions.
 * 
 * @copyright Copyright (c) 2023 lingwang with MIT License.
 * 
 */

#pragma once

#include <string>
#include <iostream>
#include <sstream>

namespace neko_cc
{

using stream = std::basic_iostream<char>;

class not_implemented : public std::logic_error
{
    public:
    not_implemented() : std::logic_error("not implemented") {}
};

enum log_level_t
{
    INFO,
    DEBUG,
    ERROR
};
extern log_level_t log_level;

#define error(msg, ss, on) _error(__func__, __LINE__, msg, ss, on)
#define err_msg(msg) _error(__func__, __LINE__, msg)

void _error(std::string func, int line, std::string msg);
void _error(std::string func, int line, std::string msg, stream &ss, bool output_near = false);
void info(std::string msg);

#define anal_debug() _anal_debug(__func__)
#define anal_debug_tok(tok) _anal_debug(__func__, tok)
#define anal_debug_msg(msg) _anal_debug(__func__, "", msg)
#define anal_debug_tok_msg(tok, msg) _anal_debug(__func__, tok, msg)
void _anal_debug(std::string pos,std::string tok_name = "", std::string msg = "");

}