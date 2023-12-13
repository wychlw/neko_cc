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
    WARN,
    ERROR
};
extern log_level_t log_level;

#define error(msg, ss, on) _error(__func__, __LINE__, msg, ss, on)
#define err_msg(msg) _error(__func__, __LINE__, msg)

void _error(std::string func, int line, std::string msg) __attribute__ ((__noreturn__));
void _error(std::string func, int line, std::string msg, stream &ss, bool output_near = false) __attribute__ ((__noreturn__));
void info(std::string msg);
void warn(std::string msg);


#define debug() _debug(__func__)
#define debug_msg(msg) _debug(__func__, "", msg)
void _debug(std::string pos,std::string tok_name = "", std::string msg = "");

}