#include "clcxx/functions.hpp"
#include "clcxx/module.hpp"

// This header provides helper functions to call Julia functions from C++

namespace clcxx
{

LispFunction::LispFunction(const std::string& name, const std::string& module_name)
{
  cl_object mod = nullptr;
  cl_object current_mod = nullptr;
  if(registry().has_current_module())
  {
    current_mod = registry().current_module().lisp_module();
  }
  if(!module_name.empty())
  {
    mod = (cl_object)cl_find_symbol(2, ecl_read_from_cstring(module_name.c_str()), ecl_read_from_cstring("CL-USER"));
    if(mod == nullptr && current_mod != nullptr)
    {
      mod = (cl_object)cl_find_symbol(ecl_read_from_cstring(module_name.c_str()), current_mod);
    }
    if(mod == nullptr)
    {
      throw std::runtime_error("Could not find module " + module_name + " when looking up function " + name);
    }
  }
  if(mod == nullptr)
  {
    mod = current_mod == nullptr ? ecl_read_from_cstring("CL-USER") : current_mod;
  }

  // intern modify
  cl_object sym = ecl_make_symbol(ecl_read_from_cstring(name.c_str()), ecl_base_string_pointer_safe(mod));
  m_function = cl_fboundp(sym);
  if(m_function == nullptr)
  {
    throw std::runtime_error("Could not find function " + name);
  }
}

LispFunction::LispFunction(cl_object fpointer)
{
  if(fpointer == nullptr)
  {
    throw std::runtime_error("Storing a null function pointer in a LispFunction is not allowed");
  }
  m_function = fpointer;
}

}
