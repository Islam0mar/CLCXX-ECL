#pragma once

#include <cassert>
#include <ecl/ecl.h>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include "array.hpp"
#include "type_conversion.hpp"

namespace clcxx {

class CLCXX_API Package;

namespace detail {
// Need to treat void specially
template <typename R, typename... Args> struct ReturnTypeAdapter {
  using return_type = decltype(convert_to_lisp(std::declval<R>()));
  // using return_type = decltype(ConvertToLisp<typename
  // StrippedConversionType<R>::type,
  // IsFundamental<remove_const_ref<R>>::value>());

  inline return_type operator()(const void *functor,
                                mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<R(Args...)> *>(functor);
    assert(std_func != nullptr);
    ecl_process_env()->nvalues = 1;
    return convert_to_lisp(
        (*std_func)(convert_to_cpp<mapped_reference_type<Args>>(args)...));
  }
};

template <typename... Args> struct ReturnTypeAdapter<void, Args...> {
  inline void operator()(const void *functor, mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<void(Args...)> *>(functor);
    assert(std_func != nullptr);
    (*std_func)(convert_to_cpp<mapped_reference_type<Args>>(args)...);
    ecl_process_env()->nvalues = 0;
  }
};

template <typename T> cl_object gen_args(int &i) {
  return ecl_read_from_cstring(std::string("V" + std::to_string(i++)).c_str());
}

/// Make a vector with the types in the variadic template parameter pack
template <typename... Args> std::vector<cl_object> argtype_vector() {
  return {lisp_type<dereference_for_mapping<Args>>()...};
}

template <typename... Args> struct NeedConvertHelper {
  bool operator()() {
    for (const bool b : {std::is_same<remove_const_ref<mapped_lisp_type<Args>>,
                                      remove_const_ref<Args>>::value...}) {
      if (!b)
        return true;
    }
    return false;
  }
};

template <> struct NeedConvertHelper<> {
  bool operator()() { return false; }
};

} // namespace detail

/// Convenience function to create an object with a finalizer attached
template <typename T, bool finalize = true, typename... ArgsT>
cl_object create(ArgsT &&... args) {
  cl_object dt = lisp_type<T>();

  T *cpp_obj = new T(std::forward<ArgsT>(args)...);

  return boxed_cpp_pointer(cpp_obj, dt, finalize);
}

/// Registry containing different packages
class CLCXX_API PackageRegistry {
public:
  /// Create a package and register it
  Package &create_package(cl_object lpack);

  Package &get_package(cl_object pack) const {
    const auto iter = p_packages.find(pack);
    if (iter == p_packages.end()) {
      throw std::runtime_error("Pack with name " + package_name(pack) +
                               " was not found in registry");
    }

    return *(iter->second);
  }

  bool has_package(cl_object lpack) const {
    return p_packages.find(lpack) != p_packages.end();
  }

  bool has_current_package() { return p_current_package != nullptr; }
  Package &current_package();
  void reset_current_package() { p_current_package = nullptr; }
  std::vector<std::shared_ptr<const void *>> &functions() {
    return p_functions;
  }

private:
  std::map<cl_object, std::shared_ptr<Package>> p_packages;
  std::vector<std::shared_ptr<const void *>> p_functions;
  Package *p_current_package = nullptr;
};

CLCXX_API PackageRegistry &registry();

namespace detail {
template <typename R, typename... Args> struct CallFunctor {
  using return_type = decltype(ReturnTypeAdapter<R, Args...>()(
      std::declval<const void *>(), std::declval<mapped_lisp_type<Args>>()...));

  static cl_object apply(cl_object index, mapped_lisp_type<Args>... args) {
    try {
      const void *f =
          *registry().functions().at(ecl_to_unsigned_integer(index));
      return ReturnTypeAdapter<R, Args...>()(f, args...);
    } catch (const std::exception &err) {
      FEerror(err.what(), 0);
    }
    return return_type();
  }
};
} // namespace detail

template <typename T> class ClassWrapper;

/// Used to define cfun and init code.
extern "C" {
  static cl_object Cblock;
}

/// Store all exposed C++ functions associated with a package
class CLCXX_API Package {
public:
  Package(cl_object cl_pack);

  /// Define a new function
  template <typename R, typename... Args>
  inline void defun(const std::string &name,
                    std::function<R(Args...)> functor) {
    try {
      cl_object index =
          ecl_make_unsigned_integer(registry().functions().size());
      auto f_ptr = std::make_shared<const void *>(
          new const std::function<R(Args...)>(functor));
      registry().functions().push_back(f_ptr);

      cl_object cfun = ecl_make_cfun(
          (cl_objectfn_fixed)detail::CallFunctor<R, Args...>::apply,
          ecl_read_from_cstring(std::string(name + "%").c_str()), Cblock,
          (1 + sizeof...(Args)));

      int i = 0;
      cl_object args = cl_list(sizeof...(Args), detail::gen_args<Args>(i)...);
      i = 0;
      cl_object fun_def = cl_list(
          4, ecl_make_symbol("DEFUN", "CL-USER"),
          ecl_read_from_cstring(name.c_str()), args,
          cl_list((3 + sizeof...(Args)), ecl_make_symbol("FUNCALL", "CL-USER"),
                  cfun, index, detail::gen_args<Args>(i)...));
      cl_safe_eval(fun_def, Cnil, OBJNULL);
    } catch (const std::runtime_error &err) {
      FEerror(err.what(), 0);
    }
  }

  /// Define a new function. Overload for pointers
  template <typename R, typename... Args>
  inline void defun(const std::string &name, R (*f)(Args...),
                    const bool force_convert = false) {
    // Conversion is automatic when using the std::function calling method, so
    // if we need conversion we use that
    bool convert =
        force_convert ||
        !std::is_same<mapped_lisp_type<R>, remove_const_ref<R>>::value ||
        detail::NeedConvertHelper<Args...>()();

    if (convert) {
      return defun(name, std::function<R(Args...)>(f));
    } else {
      // No conversion needed -> call can be through a naked function pointer
      ecl_def_c_function(ecl_read_from_cstring(name.c_str()),
                         (cl_objectfn_fixed)f, sizeof...(Args));
    }
  }

  /// Define a new function. Overload for lambda
  template <typename LambdaT>
  void defun(const std::string &name, LambdaT &&lambda) {
    add_lambda(name, std::forward<LambdaT>(lambda), &LambdaT::operator());
  }

  /// Add a composite type
  template <typename T, typename... super_classes>
  ClassWrapper<T> defclass(const std::string &name, super_classes... super);

  /// Set a global constant value at the package level
  template <typename T> void defconstant(const std::string &name, T &&value) {
    cl_object boxed_const = box(std::forward<T>(value));
    si_Xmake_constant(ecl_read_from_cstring(name.c_str()), boxed_const);
  }

  std::string name() const { return package_name(p_cl_pack); }
  cl_object lisp_package() const { return p_cl_pack; }

private:
  template <typename R, typename LambdaT, typename... ArgsT>
  void add_lambda(const std::string &name, LambdaT &&lambda,
                  R (LambdaT::*)(ArgsT...) const) {
    return defun(name,
                 std::function<R(ArgsT...)>(std::forward<LambdaT>(lambda)));
  }

  cl_object p_cl_pack;
  template <class T> friend class ClassWrapper;
};

// Helper class to wrap type methods
template <typename T> class ClassWrapper {
public:
  typedef T type;

  // ClassWrapper(Package &pack, jl_datatype_t *dt, jl_datatype_t *ref_dt,
  //             jl_datatype_t *alloc_dt)
  //     : p_package(mod), m_dt(dt), m_ref_dt(ref_dt), m_alloc_dt(alloc_dt) {}

  // /// Add a constructor with the given argument types
  // template <typename... ArgsT>
  // ClassWrapper<T> &constructor(bool finalize = true) {
  //   p_package.constructor<T, ArgsT...>(m_dt, finalize);
  //   return *this;
  // }

  /// Define a member function
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &method(const std::string &name, R (CT::*f)(ArgsT...)) {
    p_package.defun(
        name, [f](T &obj, ArgsT... args) -> R { return (obj.*f)(args...); });
    return *this;
  }

  /// Define a member function, const version
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &method(const std::string &name, R (CT::*f)(ArgsT...) const) {
    p_package.defun(name, [f](const T &obj, ArgsT... args) -> R {
      return (obj.*f)(args...);
    });
    return *this;
  }

  /// Define a "member" function using a lambda
  template <typename LambdaT>
  ClassWrapper<T> &method(const std::string &name, LambdaT &&lambda) {
    p_package.defun(name, std::forward<LambdaT>(lambda));
    return *this;
  }

  /// Call operator overload. For both reference and allocated type to work
  /// around https://github.com/JuliaLang/julia/issues/14919
  // template <typename R, typename CT, typename... ArgsT>
  // ClassWrapper<T> &method(R (CT::*f)(ArgsT...)) {
  //   p_package
  //       .defun("operator()",
  //               [f](T &obj, ArgsT... args) -> R { return (obj.*f)(args...); })
  //       .set_name(detail::make_fname("CallOpOverload", m_ref_dt));
  //   p_package
  //       .defun("operator()",
  //               [f](T &obj, ArgsT... args) -> R { return (obj.*f)(args...); })
  //       .set_name(detail::make_fname("CallOpOverload", m_alloc_dt));
  //   return *this;
  // }
  // template <typename R, typename CT, typename... ArgsT>
  // ClassWrapper<T> &method(R (CT::*f)(ArgsT...) const) {
  //   p_package
  //       .defun(
  //           "operator()",
  //           [f](const T &obj, ArgsT... args) -> R { return (obj.*f)(args...); })
  //       .set_name(detail::make_fname("CallOpOverload", m_ref_dt));
  //   p_package
  //       .defun(
  //           "operator()",
  //           [f](const T &obj, ArgsT... args) -> R { return (obj.*f)(args...); })
  //       .set_name(detail::make_fname("CallOpOverload", m_alloc_dt));
  //   return *this;
  // }

  // /// Overload operator() using a lambda
  // template <typename LambdaT> ClassWrapper<T> &method(LambdaT &&lambda) {
  //   p_package.defun("operator()", std::forward<LambdaT>(lambda))
  //       .set_name(detail::make_fname("CallOpOverload", m_ref_dt));
  //   p_package.defun("operator()", std::forward<LambdaT>(lambda))
  //       .set_name(detail::make_fname("CallOpOverload", m_alloc_dt));
  //   return *this;
  // }

  // template <typename... AppliedTypesT, typename FunctorT>
  // ClassWrapper<T> &apply(FunctorT &&apply_ftor) {
  //   static_assert(detail::IsParametric<T>::value,
  //                 "Apply can only be called on parametric types");
  //   auto dummy = {this->template apply_internal<AppliedTypesT>(
  //       std::forward<FunctorT>(apply_ftor))...};
  //   return *this;
  // }

  /// Apply all possible combinations of the given types (see example)
  template <template <typename...> class TemplateT, typename... TypeLists,
            typename FunctorT>
  void apply_combination(FunctorT &&ftor);

  template <typename ApplyT, typename... TypeLists, typename FunctorT>
  void apply_combination(FunctorT &&ftor);

  // Access to the module
  Package &packule() { return p_package; }

  // jl_datatype_t *dt() { return m_dt; }

private:
  // template <typename AppliedT, typename FunctorT>
  // int apply_internal(FunctorT &&apply_ftor) {
  //   static_assert(parameter_list<AppliedT>::nb_parameters != 0,
  //                 "No parameters found when applying type. Specialize "
  //                 "jlcxx::BuildParameterList for your combination of type and "
  //                 "non-type parameters.");
  //   static_assert(parameter_list<AppliedT>::nb_parameters >=
  //                     parameter_list<T>::nb_parameters,
  //                 "Parametric type applied to wrong number of parameters.");
  //   const bool is_abstract = jl_is_abstracttype(m_dt);

  //   jl_datatype_t *app_dt = (jl_datatype_t *)apply_type(
  //       (jl_value_t *)m_dt,
  //       parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));
  //   jl_datatype_t *app_ref_dt = (jl_datatype_t *)apply_type(
  //       (jl_value_t *)m_ref_dt,
  //       parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));
  //   jl_datatype_t *app_alloc_dt = (jl_datatype_t *)apply_type(
  //       (jl_value_t *)m_alloc_dt,
  //       parameter_list<AppliedT>()(parameter_list<T>::nb_parameters));

  //   set_julia_type<AppliedT>(app_dt);
  //   p_package.add_default_constructor<AppliedT>(DefaultConstructible<AppliedT>(),
  //                                              app_dt);
  //   p_package.add_copy_constructor<AppliedT>(CopyConstructible<AppliedT>(),
  //                                           app_dt);
  //   static_type_mapping<AppliedT>::set_reference_type(app_ref_dt);
  //   static_type_mapping<AppliedT>::set_allocated_type(app_alloc_dt);

  //   apply_ftor(
  //       ClassWrapper<AppliedT>(p_package, app_dt, app_ref_dt, app_alloc_dt));

  //   p_package.register_type_pair(app_ref_dt, app_alloc_dt);

  //   if (!std::is_same<supertype<AppliedT>, AppliedT>::value) {
  //     p_package.defun("cxxdowncast", DownCast<AppliedT>::apply);
  //   }

  //   return 0;
  // }
  Package &p_package;
  //jl_datatype_t *m_dt;
  //jl_datatype_t *m_ref_dt;
  //jl_datatype_t *m_alloc_dt;
};

} // namespace clcxx

/// Register a new package
extern "C" CLCXX_API void
register_lisp_package(cl_object pack, void (*regfunc)(clcxx::Package &));

#define CLCXX_PACKAGE extern "C" CLCXX_ONLY_EXPORTS void
