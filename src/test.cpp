//#define _GLIBCXX_USE_CXX11_ABI 0
#include <string>

#include "clcxx/clcxx.hpp"

std::string greet() { return "Hello, World"; }
cl_object gr() { return cl_core.libraries; }
cl_object f(cl_object block) { return block->cblock.refs; }
std::string hi(std::string s) { return std::string("hi, " + s); }

CLCXX_PACKAGE SHIT(clcxx::Package &pack) {
  pack.defun("f", &f);
  pack.defun("hi", &hi);
  pack.defun("greet", &greet);
  pack.defun("gr", &gr);
}


