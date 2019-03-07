#pragma once

#include <sstream>
#include <vector>

// #include "array.hpp"
#include "type_conversion.hpp"

// This header provides helper functions to call Julia functions from C++

namespace clcxx
{

/// Wrap a Julia function for easy calling
class CLCXX_API LispFunction
{
public:
  /// Construct using a function name and module name. Searches the current module by default. Throws if the function was not found.
  LispFunction(const std::string& name, const std::string& module_name = "");
  /// Construct directly from a pointer (throws if pointer is null)
  LispFunction(cl_object fpointer);

  /// Access to the raw pointer
  jl_function_t* pointer() const
  {
    return m_function;
  }

  /// Call a julia function, converting the arguments to the corresponding Julia types
  template<typename... ArgumentsT>
  cl_object operator()(ArgumentsT&&... args) const;

private:
  struct StoreArgs
  {
    StoreArgs(cl_object* arg_array) : m_arg_array(arg_array)
    {
    }

    template<typename ArgT>
    cl_object BoxArg(ArgT&& a)
    {
      auto temp =  box(a);
      if (temp == nullptr) {
          std::stringstream sstr;
          sstr << "Unsupported Julia function argument type";
          throw std::runtime_error(sstr.str());
      }
      return temp;
    }

    template<typename ArgT, typename... ArgsT>
    void push(ArgT&& a, ArgsT... args)
    {
      push(a);
      push(args...);
    }

    template<typename ArgT>
    void push(ArgT&& a)
    {
      m_arg_array[m_i++] = box(a);
    }

    void push() {}

    cl_object* m_arg_array;
    int m_i = 0;
  };
  cl_object m_function;
};

template<typename... ArgumentsT>
cl_object LispFunction::operator()(ArgumentsT&&... args) const
{
  const int nb_args = sizeof...(args);

  cl_object result = nullptr;
  cl_object* lisp_args;

  // Process arguments
  StoreArgs store_args(lisp_args);
  store_args.push(args...);
  for(int i = 0; i != nb_args; ++i)
  {
    if(lisp_args[i] == nullptr)
    {
      std::stringstream sstr;
      sstr << "Unsupported Julia function argument type at position " << i;
      throw std::runtime_error(sstr.str());
    }
  }

  // Do the call
  // result = jl_call(m_function, julia_args, nb_args);
  result = cl_funcall(++nb_args, m_function, store_args.BoxArg(args...));
  // TODO: check for exceptions
  return result;
}

// done
/// Data corresponds to immutable with the same name on the Julia side
struct SafeCFunction
{
  void* fptr;
  cl_object return_type;
  cl_object argtypes;
};


// Direct conversion
template<> struct static_type_mapping<SafeCFunction>
{
  typedef SafeCFunction type;
  static cl_object lisp_type() { return jlcxx::lisp_type("SafeCFunction"); }
};

template<>
struct ConvertToCpp<SafeCFunction, false, true, false>
{
  SafeCFunction operator()(const SafeCFunction& lisp_value) const
  {
    return lisp_value;
  }

  SafeCFunction operator()(cl_object lisp_value) const
  {
    return *reinterpret_cast<SafeCFunction*>(jl_data_ptr(lisp_value));
  }
};

namespace detail
{
  template<typename SignatureT>
  struct SplitSignature;

  template<typename R, typename... ArgsT>
  struct SplitSignature<R(ArgsT...)>
  {
    typedef R return_type;
    typedef R(*fptr_t)(ArgsT...);

    std::vector<cl_object> operator()()
    {
      return std::vector<cl_object>({julia_reference_type<ArgsT>()...});
    }

    fptr_t cast_ptr(void* ptr)
    {
      return reinterpret_cast<fptr_t>(ptr);
    }
  };
}

/// Type-checking on return type and arguments of a cfunction (void* pointer)
template<typename SignatureT>
typename detail::SplitSignature<SignatureT>::fptr_t make_function_pointer(SafeCFunction data)
{
  typedef detail::SplitSignature<SignatureT> SplitterT;

  // Check return type
  cl_object expected_rt = lisp_type<typename SplitterT::return_type>();
  if(expected_rt != data.return_type)
  {
    throw std::runtime_error("Incorrect datatype for cfunction return type, expected " + lisp_type_name(expected_rt) + " but got " + lisp_type_name(data.return_type));
  }

  // Check arguments
  const std::vector<cl_object> expected_argstypes = SplitterT()();
  ArrayRef<cl_object> argtypes(data.argtypes);
  const int nb_args = expected_argstypes.size();
  if(nb_args != static_cast<int>(argtypes.size()))
  {
    std::stringstream err_sstr;
    err_sstr << "Incorrect number of arguments for cfunction, expected: " << nb_args << ", obtained: " << argtypes.size();
    throw std::runtime_error(err_sstr.str());
  }
  for(int i = 0; i != nb_args; ++i)
  {
    cl_object argt = (cl_object)argtypes[i];
    if(argt != expected_argstypes[i])
    {
      std::stringstream err_sstr;
      err_sstr << "Incorrect argument type for cfunction at position " << i+1 << ", expected: " << lisp_type_name(expected_argstypes[i]) << ", obtained: " << lisp_type_name(argt);
      throw std::runtime_error(err_sstr.str());
    }
  }
  return SplitterT().cast_ptr(data.fptr);
}

/// Implicit conversion to pointer type
template<typename R, typename...ArgsT> struct static_type_mapping<R(*)(ArgsT...)>
{
  typedef SafeCFunction type;
  static cl_object lisp_type() { return (cl_object)jlcxx::lisp_type("SafeCFunction"); }
};

template<typename R, typename...ArgsT>
struct ConvertToCpp<R(*)(ArgsT...), false, false, false>
{
  typedef R(*fptr_t)(ArgsT...);
  fptr_t operator()(const SafeCFunction& lisp_value) const
  {
    return make_function_pointer<R(ArgsT...)>(lisp_value);
  }
};

}


