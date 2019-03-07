#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <sstream>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <queue>

// #include "array.hpp"
#include "type_conversion.hpp"

namespace clcxx
{

/// Wrappers for creating new datatype
CLCXX_API cl_object ecl_defclass(cl_object name, cl_object package,
                                 cl_object super, cl_object options,
                                 std::queue<cl_object> slots,
                                 std::queue<cl_object> types);

/// Some helper functions
namespace detail
{

// Need to treat void specially
template<typename R, typename... Args>
struct ReturnTypeAdapter
{
  using return_type = decltype(ConvertToLisp(std::declval<R>()));

  inline return_type operator()(const void* functor, mapped_lisp_type<Args>... args)
  {
    auto std_func = reinterpret_cast<const std::function<R(Args...)>*>(functor);
    assert(std_func != nullptr);
    return ConvertToLisp((*std_func)(ConvertToCpp<mapped_reference_type<Args>>(args)...));
  }
};

template<typename... Args>
struct ReturnTypeAdapter<void, Args...>
{
  inline void operator()(const void* functor, mapped_lisp_type<Args>... args)
  {
    auto std_func = reinterpret_cast<const std::function<void(Args...)>*>(functor);
    assert(std_func != nullptr);
    (*std_func)(ConvertToCpp<mapped_reference_type<Args>>(args)...);
  }
};

/// Call a C++ std::function, passed as a void pointer since it comes from Lisp
template<typename R, typename... Args>
struct CallFunctor
{
  using return_type = decltype(ReturnTypeAdapter<R, Args...>()(std::declval<const void*>(), std::declval<mapped_lisp_type<Args>>()...));

  static return_type apply(const void* functor, mapped_lisp_type<Args>... args)
  {
    try
    {
      return ReturnTypeAdapter<R, Args...>()(functor, args...);
    }
    catch(const std::exception& err)
    {
      cl_error(1, ecl_make_simple_base_string(err.what()),strlen(err.what()));
    }

    return return_type();
  }
};

/// Make a vector with the types in the variadic template parameter pack
template<typename... Args>
std::vector<cl_object> argtype_vector()
{
  return {lisp_type<dereference_for_mapping<Args>>()...};
}
template<typename... Args>
std::vector<cl_object> reference_argtype_vector()
{
  return {lisp_reference_type<dereference_for_mapping<Args>>()...};
}


template<typename... Args>
struct NeedConvertHelper
{
  bool operator()()
  {
    for(const bool b : {std::is_same<remove_const_ref<mapped_lisp_type<Args>>,remove_const_ref<Args>>::value...})
    {
      if(!b)
        return true;
    }
    return false;
  }
};

template<>
struct NeedConvertHelper<>
{
  bool operator()()
  {
    return false;
  }
};

} // end namespace detail

/// Convenience function to create an object with a finalizer attached
template<typename T, bool finalize=true, typename... ArgsT>
cl_object create(ArgsT&&... args)
{
  cl_object dt = static_type_mapping<T>::lisp_allocated_type();

  T* cpp_obj = new T(std::forward<ArgsT>(args)...);

  return boxed_cpp_pointer(cpp_obj, dt, finalize);
}

/// Safe downcast to base type
template<typename T>
struct DownCast
{
  static inline supertype<T>& apply(T& base)
  {
    return static_cast<supertype<T>&>(base);
  }
};

// The CxxWrap Julia module
extern cl_object g_cxxwrap_module;
extern cl_object g_cppfunctioninfo_type;

class CLCXX_API Module;

/// Abstract base class for storing any function
class CLCXX_API FunctionWrapperBase
{
public:
  FunctionWrapperBase(Module* mod, cl_object return_type) : m_module(mod), m_return_type(return_type)
  {
  }

  /// Types of the arguments (used in the wrapper signature)
  virtual std::vector<cl_object> argument_types() const = 0;

  /// Reference type for the arguments (used in the ccall type list)
  virtual std::vector<cl_object> reference_argument_types() const = 0;

  /// Return type
  cl_object return_type() const { return m_return_type; }

  void set_return_type(cl_object dt) { m_return_type = dt; }

  virtual ~FunctionWrapperBase() {}

  inline void set_name(cl_object name)
  {
    protect_from_gc(name);
    m_name = name;
  }

  inline cl_object name() const
  {
    return m_name;
  }

  inline int_t pointer_index() { return m_pointer_index; }
  inline int_t thunk_index() { return m_thunk_index; }

protected:
  /// Function pointer as void*, since that's what Julia expects
  virtual void* pointer() = 0;

  /// The thunk (i.e. std::function) to pass as first argument to the function pointed to by function_pointer
  virtual void* thunk() = 0;

  void set_pointer_indices();
private:
  cl_object m_name;
  Module* m_module;
  cl_object m_return_type = nullptr;

  int_t m_pointer_index = 0;
  int_t m_thunk_index = 0;
};

/// Implementation of function storage, case of std::function
template<typename R, typename... Args>
class FunctionWrapper : public FunctionWrapperBase
{
public:
  typedef std::function<R(Args...)> functor_t;

  FunctionWrapper(Module* mod, const functor_t &function) : FunctionWrapperBase(mod, lisp_return_type<R>()), m_function(function)
  {
    set_pointer_indices();
  }

  virtual std::vector<cl_object> argument_types() const
  {
    return detail::argtype_vector<Args...>();
  }

  virtual std::vector<cl_object> reference_argument_types() const
  {
    return detail::reference_argtype_vector<Args...>();
  }

protected:
  virtual void* pointer()
  {
    return reinterpret_cast<void*>(detail::CallFunctor<R, Args...>::apply);
  }

  virtual void* thunk()
  {
    return reinterpret_cast<void*>(&m_function);
  }

private:
  functor_t m_function;
};

/// Implementation of function storage, case of a function pointer
template<typename R, typename... Args>
class FunctionPtrWrapper : public FunctionWrapperBase
{
public:
  typedef std::function<R(Args...)> functor_t;

  FunctionPtrWrapper(Module* mod, R (*f)(Args...)) : FunctionWrapperBase(mod, lisp_return_type<R>()), m_function(f)
  {
    set_pointer_indices();
  }

  virtual std::vector<cl_object> argument_types() const
  {
    return detail::argtype_vector<Args...>();
  }

  virtual std::vector<cl_object> reference_argument_types() const
  {
    return detail::reference_argtype_vector<Args...>();
  }

protected:
  virtual void* pointer()
  {
    return reinterpret_cast<void*>(m_function);
  }

  virtual void* thunk()
  {
    return nullptr;
  }

private:
  R(*m_function)(Args...);
};


template<typename T>
class TypeWrapper;

/// Specialise this to instantiate parametric types when first used in
/// a wrapper
template<typename T>
struct InstantiateParametricType
{
  // Returns int to expand parameter packs into an initialization list
  int operator()(Module&) const
  {
    return 0;
  }
};

template<typename... TypesT>
void instantiate_parametric_types(Module& m)
{
  auto unused = {InstantiateParametricType<remove_const_ref<TypesT>>()(m)...};
}

namespace detail
{

template<typename T>
struct GetCLType
{
  cl_object operator()() const
  {
    try
    {
      return lisp_type<remove_const_ref<T>>();
    }
    catch(...)
    {
      // The assumption here is that unmapped types are not needed, i.e. in default argument lists
      return nullptr;
    }
  }
};

template<int I>
struct GetCLType<TypeVar<I>>
{
  cl_object operator()() const
  {
    return TypeVar<I>::tvar();
  }
};

template<typename T, T Val>
struct GetCLType<std::integral_constant<T, Val>>
{
  cl_object operator()() const
   {
    return box(ConvertToLisp(Val));
  }
};

// TODO add constructor
// template<typename... ArgsT>
// inline cl_object make_fname(const std::string& nametype, ArgsT... args)
// {
//   cl_object name = nullptr;
//   name = jl_new_struct((jl_datatype_t*)julia_type(nametype), args...);
//   protect_from_gc(name);

//   return name;
// }

} // namespace detail

// Encapsulate a list of parameters, using types only
template<typename... ParametersT>
struct ParameterList
{
  static constexpr int nb_parameters = sizeof...(ParametersT);

  cl_object operator()(const int n = nb_parameters)
  {
    cl_object result = cl_list(n, detail::GetCLType<ParametersT>()()...);
    for(int i = 0; i != n; ++i)
    {
      if(ecl_nth(i, result) == nullptr)
      {
        throw std::runtime_error("Attempt to use unmapped type in parameter list");
      }
    }
    return result;
  }
};

/// Store all exposed C++ functions associated with a module
class CLCXX_API Module
{
public:

  Module(cl_object cl_mod);

  void append_function(FunctionWrapperBase* f)
  {
    assert(f != nullptr);
    m_functions.push_back(std::shared_ptr<FunctionWrapperBase>(f));
    assert(m_functions.back() != nullptr);
  }
  // TODO: change mecthod name to defun
  /// Define a new function
  template<typename R, typename... Args>
  FunctionWrapperBase& method(const std::string& name,  std::function<R(Args...)> f)
  {
    instantiate_parametric_types<R, Args...>(*this);
    auto* new_wrapper = new FunctionWrapper<R, Args...>(this, f);
    new_wrapper->set_name((cl_object)ecl_read_from_cstring(name.c_str()));
    append_function(new_wrapper);
    return *new_wrapper;
  }

  /// Define a new function. Overload for pointers
  template<typename R, typename... Args>
  FunctionWrapperBase& method(const std::string& name,  R(*f)(Args...), const bool force_convert = false)
  {
    bool need_convert = force_convert || !std::is_same<mapped_lisp_type<R>,remove_const_ref<R>>::value || detail::NeedConvertHelper<Args...>()();

    // Conversion is automatic when using the std::function calling method, so if we need conversion we use that
    if(need_convert)
    {
      return method(name, std::function<R(Args...)>(f));
    }

    instantiate_parametric_types<R, Args...>(*this);

    // No conversion needed -> call can be through a naked function pointer
    auto* new_wrapper = new FunctionPtrWrapper<R, Args...>(this, f);
    new_wrapper->set_name((cl_object)ecl_read_from_cstring(name.c_str()));
    append_function(new_wrapper);
    return *new_wrapper;
  }

  /// Define a new function. Overload for lambda
  template<typename LambdaT>
  FunctionWrapperBase& method(const std::string& name, LambdaT&& lambda)
  {
    return add_lambda(name, std::forward<LambdaT>(lambda), &LambdaT::operator());
  }

  /// Add a constructor with the given argument types for the given datatype (used to get the name)
//   template<typename T, typename... ArgsT>
//   void constructor(cl_object dt, bool finalize=true)
// { //TODO
//     FunctionWrapperBase &new_wrapper = finalize ? method("dummy", [](ArgsT... args) { return create<T, true>(args...); }) : method("dummy", [](ArgsT... args) { return create<T, false>(args...); });
//     new_wrapper.set_name(detail::make_fname("ConstructorFname", dt));
//   }

  /// Loop over the functions
  template<typename F>
  void for_each_function(const F f) const
  {
    auto funcs_copy = m_functions;
    for(const auto &item : funcs_copy)
    {
      assert(item != nullptr);
      f(*item);
    }
    // Account for any new functions added during the loop
    while(funcs_copy.size() != m_functions.size())
    {
      const std::size_t oldsize = funcs_copy.size();
      const std::size_t newsize = m_functions.size();
      funcs_copy = m_functions;
      for(std::size_t i = oldsize; i != newsize; ++i)
      {
        assert(funcs_copy[i] != nullptr);
        f(*funcs_copy[i]);
      }
    }
  }

  /// TODO: change name to defclass
  template<typename T, typename SuperParametersT=ParameterList<>, typename cl_object=cl_object>
  TypeWrapper<T> add_type(const std::string& name, cl_object super = lisp_type<CppAny>());

 /// Set a global constant value at the module level
  template<typename T>
  void set_const(const std::string& name, T&& value)
  {
    if(m_cl_constants.count(name) != 0)
    {
      throw std::runtime_error("Duplicate registration of constant " + name);
    }
    cl_object boxed_const = box(std::forward<T>(value));
    if(gc_index_map().count(boxed_const) == 0)
    {
      protect_from_gc(boxed_const);
    }
    m_cl_constants[name] = boxed_const;
  }

  std::string name() const
  {
    return module_name(m_cl_mod);
  }

  void bind_constants(cl_object mod);

  cl_object get_lisp_type(const char* name)
  {
    // TODO check for type
    if(m_cl_constants.count(name) != 0 && jl_is_datatype(m_cl_constants[name]))
    {
      return (cl_object)m_cl_constants[name];
    }

    return nullptr;
  }

  void register_type_pair(cl_object reference_type, cl_object allocated_type)
  {
    m_reference_types.push_back(reference_type);
    m_allocated_types.push_back(allocated_type);
  }

  const std::vector<cl_object> reference_types() const
  {
    return m_reference_types;
  }

  const std::vector<cl_object> allocated_types() const
  {
    return m_allocated_types;
  }

  cl_object lisp_module() const
  {
    return m_cl_mod;
  }

  int_t store_pointer(void* ptr);

private:

  template<typename T>
  void add_default_constructor(std::true_type, cl_object dt);

  template<typename T>
  void add_default_constructor(std::false_type, cl_object)
  {
  }

  template<typename T>
  void add_copy_constructor(std::true_type, cl_object)
  {
    method("deepcopy_internal", [this](const T& other, ObjectIdDict)
    {
      return create<T>(other);
    });
  }

  template<typename T>
  void add_copy_constructor(std::false_type, cl_object)
  {
  }

  template<typename T, typename SuperParametersT, typename cl_object>
  TypeWrapper<T> add_type_internal(const std::string& name, cl_object super);

  template<typename R, typename LambdaT, typename... ArgsT>
  FunctionWrapperBase& add_lambda(const std::string& name, LambdaT&& lambda, R(LambdaT::*)(ArgsT...) const)
  {
    return method(name, std::function<R(ArgsT...)>(std::forward<LambdaT>(lambda)));
  }

  cl_object m_cl_mod;
  ArrayRef<void*> m_pointer_array;
  std::vector<std::shared_ptr<FunctionWrapperBase>> m_functions;
  std::map<std::string, cl_object> m_cl_constants;
  std::vector<cl_object> m_reference_types;
  std::vector<cl_object> m_allocated_types;

  template<class T> friend class TypeWrapper;
};

template<typename T>
void Module::add_default_constructor(std::true_type, cl_object dt)
{
  this->constructor<T>(dt);
}

// Specialize this to build the correct parameter list, wrapping non-types in integral constants
// There is no way to provide a template here that matchs all possible combinations of type and non-type arguments
template<typename T>
struct BuildParameterList
{
  typedef ParameterList<> type;
};

template<typename T> using parameter_list = typename BuildParameterList<T>::type;

// Match any combination of types only
template<template<typename...> class T, typename... ParametersT>
struct BuildParameterList<T<ParametersT...>>
{
  typedef ParameterList<ParametersT...> type;
};

// Match any number of int parameters
template<template<int...> class T, int... ParametersT>
struct BuildParameterList<T<ParametersT...>>
{
  typedef ParameterList<std::integral_constant<int, ParametersT>...> type;
};

namespace detail
{
  template<typename... Types>
  struct DoApply;

  template<>
  struct DoApply<>
  {
    template<typename WrapperT, typename FunctorT>
    void operator()(WrapperT&, FunctorT&&)
    {
    }
  };

  template<typename AppT>
  struct DoApply<AppT>
  {
    template<typename WrapperT, typename FunctorT>
    void operator()(WrapperT& w, FunctorT&& ftor)
    {
      w.template apply<AppT>(std::forward<FunctorT>(ftor));
    }
  };

  template<typename... Types>
  struct DoApply<ParameterList<Types...>>
  {
    template<typename WrapperT, typename FunctorT>
    void operator()(WrapperT& w, FunctorT&& ftor)
    {
      DoApply<Types...>()(w, std::forward<FunctorT>(ftor));
    }
  };

  template<typename T1, typename... Types>
  struct DoApply<T1, Types...>
  {
    template<typename WrapperT, typename FunctorT>
    void operator()(WrapperT& w, FunctorT&& ftor)
    {
      DoApply<T1>()(w, std::forward<FunctorT>(ftor));
      DoApply<Types...>()(w, std::forward<FunctorT>(ftor));
    }
  };
}

/// Execute a functor on each type
template<typename... Types>
struct ForEachType;

template<>
struct ForEachType<>
{
  template<typename FunctorT>
  void operator()(FunctorT&&)
  {
  }
};

template<typename AppT>
struct ForEachType<AppT>
{
  template<typename FunctorT>
  void operator()(FunctorT&& ftor)
  {
#ifdef _MSC_VER
    ftor.operator()<AppT>();
#else 
    ftor.template operator()<AppT>();
#endif
  }
};

template<typename... Types>
struct ForEachType<ParameterList<Types...>>
{
  template<typename FunctorT>
  void operator()(FunctorT&& ftor)
  {
    ForEachType<Types...>()(std::forward<FunctorT>(ftor));
  }
};

template<typename T1, typename... Types>
struct ForEachType<T1, Types...>
{
  template<typename FunctorT>
  void operator()(FunctorT&& ftor)
  {
    ForEachType<T1>()(std::forward<FunctorT>(ftor));
    ForEachType<Types...>()(std::forward<FunctorT>(ftor));
  }
};

template<typename T, typename FunctorT>
void for_each_type(FunctorT&& f)
{
  ForEachType<T>()(f);
}

/// Trait to allow user-controlled disabling of the default constructor
template<typename T>
struct DefaultConstructible : std::is_default_constructible<T>
{
};

/// Trait to allow user-controlled disabling of the copy constructor
template<typename T>
struct CopyConstructible : std::is_copy_constructible<T>
{
};

template<typename... Types>
struct UnpackedTypeList
{
};

template<typename ApplyT, typename... TypeLists>
struct CombineTypes;

template<typename ApplyT, typename... UnpackedTypes>
struct CombineTypes<ApplyT, UnpackedTypeList<UnpackedTypes...>>
{
  typedef typename ApplyT::template apply<UnpackedTypes...> type;
};

template<typename ApplyT, typename... UnpackedTypes, typename... Types, typename... OtherTypeLists>
struct CombineTypes<ApplyT, UnpackedTypeList<UnpackedTypes...>, ParameterList<Types...>, OtherTypeLists...>
{
  typedef CombineTypes<ApplyT, UnpackedTypeList<UnpackedTypes...>, ParameterList<Types...>, OtherTypeLists...> ThisT;

  template<typename T1>
  struct type_unpack
  {
    typedef UnpackedTypeList<UnpackedTypes..., T1> unpacked_t;
    typedef CombineTypes<ApplyT, unpacked_t, OtherTypeLists...> combined_t;
  };
  typedef ParameterList<typename ThisT::template type_unpack<Types>::combined_t::type...> type;
};

template<typename ApplyT, typename... Types, typename... OtherTypeLists>
struct CombineTypes<ApplyT, ParameterList<Types...>, OtherTypeLists...>
{
  typedef CombineTypes<ApplyT, ParameterList<Types...>, OtherTypeLists...> ThisT;

  template<typename T1>
  struct type_unpack
  {
    typedef UnpackedTypeList<T1> unpacked_t;
    typedef CombineTypes<ApplyT, unpacked_t, OtherTypeLists...> combined_t;
  };

  typedef ParameterList<typename ThisT::template type_unpack<Types>::combined_t::type...> type;
};

// Default ApplyT implementation
template<template<typename...> class TemplateT>
struct ApplyType
{
  template<typename... Types> using apply = TemplateT<Types...>;
};

/// Helper class to wrap type methods
template<typename T>
class TypeWrapper
{
public:
  typedef T type;

  TypeWrapper(Module& mod, cl_object dt, cl_object ref_dt, cl_object alloc_dt) :
    m_module(mod),
    m_dt(dt),
    m_ref_dt(ref_dt),
    m_alloc_dt(alloc_dt)
  {
  }

  /// Add a constructor with the given argument types
  template<typename... ArgsT>
  TypeWrapper<T>& constructor(bool finalize=true)
  {
    m_module.constructor<T, ArgsT...>(m_dt, finalize);
    return *this;
  }

  /// Define a member function
  template<typename R, typename CT, typename... ArgsT>
  TypeWrapper<T>& method(const std::string& name, R(CT::*f)(ArgsT...))
  {
    m_module.method(name, [f](T& obj, ArgsT... args) -> R { return (obj.*f)(args...); } );
    return *this;
  }

  /// Define a member function, const version
  template<typename R, typename CT, typename... ArgsT>
  TypeWrapper<T>& method(const std::string& name, R(CT::*f)(ArgsT...) const)
  {
    m_module.method(name, [f](const T& obj, ArgsT... args) -> R { return (obj.*f)(args...); } );
    return *this;
  }

  /// Define a "member" function using a lambda
  template<typename LambdaT>
  TypeWrapper<T>& method(const std::string& name, LambdaT&& lambda)
  {
    m_module.method(name, std::forward<LambdaT>(lambda));
    return *this;
  }

  template<typename... AppliedTypesT, typename FunctorT>
  TypeWrapper<T>& apply(FunctorT&& apply_ftor)
  {
    static_assert(detail::IsParametric<T>::value, "Apply can only be called on parametric types");
    auto dummy = {this->template apply_internal<AppliedTypesT>(std::forward<FunctorT>(apply_ftor))...};
    return *this;
  }

  /// Apply all possible combinations of the given types (see example)
  template<template<typename...> class TemplateT, typename... TypeLists, typename FunctorT>
  void apply_combination(FunctorT&& ftor);

  template<typename ApplyT, typename... TypeLists, typename FunctorT>
  void apply_combination(FunctorT&& ftor);

  // Access to the module
  Module& module()
  {
    return m_module;
  }

  cl_object dt()
  {
    return m_dt;
  }

private:

  template<typename AppliedT, typename FunctorT>
  int apply_internal(FunctorT&& apply_ftor)
  {
    static_assert(parameter_list<AppliedT>::nb_parameters != 0, "No parameters found when applying type. Specialize clcxx::BuildParameterList for your combination of type and non-type parameters.");
    static_assert(parameter_list<AppliedT>::nb_parameters >= parameter_list<T>::nb_parameters, "Parametric type applied to wrong number of parameters.");

    cl_object app_dt = (cl_object)apply_type((cl_object)m_dt, parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));
    cl_object app_ref_dt = (cl_object)apply_type((cl_object)m_ref_dt, parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));
    cl_object app_alloc_dt = (cl_object)apply_type((cl_object)m_alloc_dt, parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));

    set_lisp_type<AppliedT>(app_dt);
    m_module.add_default_constructor<AppliedT>(DefaultConstructible<AppliedT>(), app_dt);
    m_module.add_copy_constructor<AppliedT>(CopyConstructible<AppliedT>(), app_dt);
    static_type_mapping<AppliedT>::set_reference_type(app_ref_dt);
    static_type_mapping<AppliedT>::set_allocated_type(app_alloc_dt);

    apply_ftor(TypeWrapper<AppliedT>(m_module, app_dt, app_ref_dt, app_alloc_dt));

    m_module.register_type_pair(app_ref_dt, app_alloc_dt);

    if(!std::is_same<supertype<AppliedT>,AppliedT>::value)
    {
      m_module.method("cxxdowncast", DownCast<AppliedT>::apply);
    }

    return 0;
  }
  Module& m_module;
  cl_object m_dt;
  cl_object m_ref_dt;
  cl_object m_alloc_dt;
};

template<typename ApplyT, typename... TypeLists> using combine_types = typename CombineTypes<ApplyT, TypeLists...>::type;

template<typename T>
template<template<typename...> class TemplateT, typename... TypeLists, typename FunctorT>
void TypeWrapper<T>::apply_combination(FunctorT&& ftor)
{
  this->template apply_combination<ApplyType<TemplateT>, TypeLists...>(std::forward<FunctorT>(ftor));
}

template<typename T>
template<typename ApplyT, typename... TypeLists, typename FunctorT>
void TypeWrapper<T>::apply_combination(FunctorT&& ftor)
{
  typedef typename CombineTypes<ApplyT, TypeLists...>::type applied_list;
  detail::DoApply<applied_list>()(*this, std::forward<FunctorT>(ftor));
}

/// Registry containing different modules
class CLCXX_API ModuleRegistry
{
public:
  /// Create a module and register it
  Module& create_module(cl_object jmod);

  Module& get_module(cl_object mod) const
  {
    const auto iter = m_modules.find(mod);
    if(iter == m_modules.end())
    {
      throw std::runtime_error("Module with name " + module_name(mod) + " was not found in registry");
    }

    return *(iter->second);
  }

  bool has_module(cl_object jmod) const
  {
    return m_modules.find(jmod) != m_modules.end();
  }

  bool has_current_module() { return m_current_module != nullptr; }
  Module& current_module();
  void reset_current_module() { m_current_module = nullptr; }

private:
  std::map<cl_object, std::shared_ptr<Module>> m_modules;
  Module* m_current_module = nullptr;
};

CLCXX_API ModuleRegistry& registry();

} // namespace jlcxx

/// Register a new module
extern "C" CLCXX_API void register_lisp_module(cl_object mod, void (*regfunc)(jlcxx::Module&));

#define JLCXX_MODULE extern "C" JLCXX_ONLY_EXPORTS void


