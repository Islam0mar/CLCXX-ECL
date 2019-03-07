#pragma once

#include <ecl/ecl.h>

#include <complex>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

#include "clcxx_config.hpp"

namespace clcxx {

// Get the symbol name
inline std::string symbol_name(cl_object sym) {
  return (std::string)(ecl_base_string_pointer_safe(cl_symbol_name(sym)));
}

inline std::string package_name(cl_object p) {
  return (std::string)(symbol_name(p));
}

/// type composite list eg: (complex long-float) (array character 3)
template <typename... Args>
CLCXX_API cl_object apply_type_list(cl_object tc, Args... args);

// inline cl_object apply_array_type(cl_object type, cl_object dim) {
//   return apply_type_list(ecl_read_from_cstring("ARRAY"), dim,
//                          ecl_read_from_cstring(":ELEMENT-TYPE"),
//                          (cl_object)type);
// }

/// Check if we have a string
inline bool is_lisp_string(cl_object v) { return ecl_stringp(v); }

inline const char *lisp_string(cl_object v) {
  return ecl_base_string_pointer_safe(si_copy_to_simple_base_string(v));
}

// FIXME: unsafe!!
inline std::string lisp_type_name(cl_object dt) {
  if (!ecl_to_bool(cl_subtypep(2, dt, ECL_T))) {
    return NULL;
  }
  if (ECL_SYMBOLP(dt)) {
    return ecl_base_string_pointer_safe(cl_symbol_name(dt));
  } else {
    return ecl_base_string_pointer_safe(cl_write_to_string(1, dt));
  }
}

// Unbox boxed type
template <typename CppT> inline CppT unbox(cl_object v) {
  return *reinterpret_cast<CppT *>(&v);
}

/// Equivalent of the basic C++ type layout in Lisp
struct WrappedCppPtr {
  void *voidptr;
};

template <typename T> T *unbox_wrapped_ptr(cl_object v) {
  return reinterpret_cast<T *>(unbox<WrappedCppPtr>(v).voidptr);
}

namespace detail {
/// Finalizer function for type T
template <typename T> void finalizer(cl_object to_delete) {
  T *stored_obj = unbox_wrapped_ptr<T>(to_delete);
  if (stored_obj != nullptr) {
    delete stored_obj;
  }

  reinterpret_cast<WrappedCppPtr *>(to_delete)->voidptr = nullptr;
}
template <typename T> struct unused_type {};

template <typename T1, typename T2> struct DefineIfDifferent {
  typedef T1 type;
};

template <typename T> struct DefineIfDifferent<T, T> {
  typedef unused_type<T> type;
};

template <typename T1, typename T2>
using define_if_different = typename DefineIfDifferent<T1, T2>::type;

} // namespace detail

template <typename SourceT, bool compound = false> struct static_type_mapping {
  typedef void *type;
  static cl_object lisp_type() {
    // assert(type_pointer() != nullptr);
    if (type_pointer() == nullptr) {
      throw std::runtime_error("Type " + std::string(typeid(SourceT).name()) +
                               " has no lisp wrapper");
    }
    return type_pointer();
  }

private:
  static cl_object &type_pointer() {
    static cl_object m_type_pointer = nullptr;
    return m_type_pointer;
  }
};

namespace detail {
// Helper to deal with references
template <typename T> struct LispReferenceMapping { typedef T type; };

template <typename T> struct LispReferenceMapping<T &> { typedef T type; };

template <typename T> struct LispReferenceMapping<const T &> {
  typedef T type;
};

template <typename T> struct LispReferenceMapping<const T> { typedef T type; };
} // namespace detail

template <typename SourceT>
using dereference_for_mapping =
    typename detail::LispReferenceMapping<SourceT>::type;
template <typename SourceT>
using dereferenced_type_mapping =
    static_type_mapping<dereference_for_mapping<SourceT>>;
// TODO:
template <typename SourceT>
using mapped_lisp_type = cl_object;

namespace detail {
template <typename T> struct MappedReferenceType {
  typedef typename std::remove_const<T>::type type;
};

template <typename T> struct MappedReferenceType<T &> { typedef T &type; };

template <typename T> struct MappedReferenceType<const T &> {
  typedef const T &type;
};
} // namespace detail

/// Remove reference and const from value types only, pass-through otherwise
template <typename T>
using mapped_reference_type = typename detail::MappedReferenceType<T>::type;

// Needed for Visual C++, static members are different in each DLL
// Implemented in c_interface.cpp
extern "C" CLCXX_API cl_object get_cxxwrap_module();

CLCXX_API cl_object lisp_type(const std::string &name,
                              const std::string &module_name = "");
// TODO: return void type
template <> struct static_type_mapping<void> {
  typedef void type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("SI:FOREIGN-DATA");
  }
};

template <typename NumberT> struct static_type_mapping<std::complex<NumberT>> {
  typedef std::complex<NumberT> type;
  static cl_object lisp_type() {
    static cl_object dt = nullptr;
    if (dt == nullptr) {
      dt =
          (cl_object)apply_type_list(ecl_read_from_cstring("COMPLEX"),
                                     static_type_mapping<NumberT>::lisp_type());
    }
    return dt;
  }
};

template <> struct static_type_mapping<bool> {
  typedef bool type;
  static cl_object lisp_type() { return ecl_read_from_cstring("BOOLEAN"); }
};

template <> struct static_type_mapping<double> {
  typedef double type;
  static cl_object lisp_type() { return ecl_read_from_cstring("DOUBLE-FLOAT"); }
};

template <> struct static_type_mapping<float> {
  typedef float type;
  static cl_object lisp_type() { return ecl_read_from_cstring("SINGLE-FLOAT"); }
};

template <> struct static_type_mapping<short> {
  static_assert(sizeof(short) == 2, "short is expected to be 16 bits");
  typedef short type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(SIGNED-BYTE 16)");
  }
};

template <> struct static_type_mapping<int> {
  static_assert(sizeof(int) == 4, "int is expected to be 32 bits");
  typedef int type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(SIGNED-BYTE 32)");
  }
};

template <> struct static_type_mapping<unsigned int> {
  static_assert(sizeof(unsigned int) == 4,
                "unsigned int is expected to be 32 bits");
  typedef unsigned int type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(UNSIGNED-BYTE 32)");
  }
};

template <> struct static_type_mapping<unsigned char> {
  typedef unsigned char type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(UNSIGNED-BYTE 8)");
  }
};

template <> struct static_type_mapping<int64_t> {
  typedef int64_t type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(SIGNED-BYTE 64)");
  }
};

template <> struct static_type_mapping<uint64_t> {
  typedef uint64_t type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(SIGNED-BYTE 64)");
  }
};

template <>
struct static_type_mapping<detail::define_if_different<long, int64_t>> {
  static_assert(sizeof(long) == 8 || sizeof(long) == 4,
                "long is expected to be 64 bits or 32 bits");
  typedef long type;
  static cl_object lisp_type() {
    return sizeof(long) == 8 ? ecl_read_from_cstring("(SIGNED-BYTE 64)")
                             : ecl_read_from_cstring("(SIGNED-BYTE 32)");
  }
};

template <>
struct static_type_mapping<detail::define_if_different<long long, int64_t>> {
  static_assert(sizeof(long long) == 8,
                " long long is expected to be 64 bits"); // TYPO
  typedef long long type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("(SIGNED-BYTE 64)");
  }
};

template <>
struct static_type_mapping<
    detail::define_if_different<unsigned long, uint64_t>> {
  static_assert(sizeof(unsigned long) == 8 || sizeof(unsigned long) == 4,
                "unsigned long is expected to be 64 bits or 32 bits");
  typedef unsigned long type;
  static cl_object lisp_type() {
    return sizeof(unsigned long) == 8
               ? ecl_read_from_cstring("(UNSIGNED-BYTE 64)")
               : ecl_read_from_cstring("(UNSIGNED-BYTE 32)");
  }
};

template <> struct static_type_mapping<std::string> {
  typedef cl_object type;
  static cl_object lisp_type() { return ecl_read_from_cstring("STRING"); }
};

// TODO: correct type
template <typename T> struct static_type_mapping<T *> {
  typedef T *type;
  static cl_object lisp_type() {
    return (cl_object)ecl_read_from_cstring("SI:FOREIGN-DATA");
  }
};

// TODO: correct type
template <typename T> struct static_type_mapping<const T *> {
  typedef T *type;
  static cl_object lisp_type() {
    return (cl_object)ecl_read_from_cstring("SI:FOREIGN-DATA");
  }
};

template <> struct static_type_mapping<const char *> {
  typedef cl_object type;
  static cl_object lisp_type() { return ecl_read_from_cstring("STRING"); }
};

template <> struct static_type_mapping<void *> {
  typedef void *type;
  static cl_object lisp_type() {
    return ecl_read_from_cstring("SI:FOREIGN-DATA");
  }
};

template <> struct static_type_mapping<cl_object> {
  typedef cl_object type;
  static cl_object lisp_type() { return ECL_T; }
};

/// Convenience function to get the lisp data type associated with T
template <typename T> inline cl_object lisp_type() {
  return static_type_mapping<T>::lisp_type();
}

/// Wrap a C++ pointer in a lisp type that contains a single void pointer field,
/// returning the result as an any
template <typename T>
cl_object boxed_cpp_pointer(T *cpp_ptr, cl_object dt, bool add_finalizer) {
  // assert(ecl_to_bool(
  //     cl_subtypep(2, dt, ecl_read_from_cstring("SI:FOREIGN-DATA"))));
  // assert((dt)->foreign.tag == ECL_FFI_VOID);
  cl_object void_ptr = nullptr;
  cl_object result = nullptr;
  cl_object finalizer = nullptr;
  void_ptr = ecl_make_pointer((void *)cpp_ptr);
  result = void_ptr;
  if (add_finalizer) {
    finalizer = ecl_make_pointer((void *)detail::finalizer<T>);
    si_set_finalizer(result, finalizer);
  }
  return result;
};

// Box an automatically converted value
template <typename CppT> cl_object box(const CppT &cpp_val) {
  return boxed_cpp_pointer(&cpp_val, static_type_mapping<CppT>::lisp_type(),
                           false);
}

template <> inline cl_object box(const bool &b) { return ecl_make_bool(b); }

template <> inline cl_object box(const int32_t &i) {
  return ecl_make_int32_t(i);
}

template <> inline cl_object box(const int64_t &i) {
  return ecl_make_int64_t(i);
}

template <> inline cl_object box(const uint32_t &i) {
  return ecl_make_uint32_t(i);
}

template <> inline cl_object box(const uint64_t &i) {
  return ecl_make_uint64_t(i);
}

template <> inline cl_object box(const float &x) {
  return ecl_make_single_float(x);
}

template <> inline cl_object box(const double &x) {
  return ecl_make_double_float(x);
}

template <> inline cl_object box(const long double &x) {
  return ecl_make_long_float(x);
}

template <> inline cl_object box(const std::complex<float> &x) {
  return ecl_make_complex(box(std::real(x)), box(std::imag(x)));
}

template <> inline cl_object box(const std::complex<double> &x) {
  return ecl_make_complex(box(std::real(x)), box(std::imag(x)));
}

template <> inline cl_object box(const std::complex<long double> &x) {
  return ecl_make_complex(box(std::real(x)), box(std::imag(x)));
}

template <> inline cl_object box(cl_object const &x) { return (cl_object)x; }

template <> inline cl_object box(void *const &x) { return ecl_make_pointer(x); }

namespace detail {
inline cl_object box_long(long x) { return ecl_make_long(x); }

inline cl_object box_long(unused_type<long>) {
  // never called
  return nullptr;
}

inline cl_object box_us_long(unsigned long x) {
  if (sizeof(unsigned long) == 8) {
    return ecl_make_uint64_t(x);
  }
  return ecl_make_uint32_t(x);
}

inline cl_object box_us_long(unused_type<unsigned long>) {
  // never called
  return nullptr;
}
} // namespace detail

template <>
inline cl_object box(const detail::define_if_different<long, int64_t> &x) {
  return detail::box_long(x);
}

template <>
inline cl_object
box(const detail::define_if_different<unsigned long, uint64_t> &x) {
  return detail::box_us_long(x);
}

// unbox
template <> inline bool unbox(cl_object v) { return ecl_to_bool(v); }

template <> inline float unbox(cl_object v) { return ecl_to_float(v); }

template <> inline double unbox(cl_object v) { return ecl_to_double(v); }

template <> inline long double unbox(cl_object v) {
  return ecl_to_long_double(v);
}

template <> inline std::complex<float> unbox(cl_object v) {
  return std::complex<float>(unbox<float>(cl_realpart(v)),
                             unbox<float>(cl_imagpart(v)));
}

template <> inline std::complex<double> unbox(cl_object v) {
  return std::complex<double>(unbox<double>(cl_realpart(v)),
                              unbox<double>(cl_imagpart(v)));
}

template <> inline std::complex<long double> unbox(cl_object v) {
  return std::complex<long double>(unbox<long double>(cl_realpart(v)),
                                   unbox<long double>(cl_imagpart(v)));
}

template <> inline int32_t unbox(cl_object v) { return ecl_to_int32_t(v); }

template <> inline int64_t unbox(cl_object v) { return ecl_to_int64_t(v); }

template <> inline uint32_t unbox(cl_object v) { return ecl_to_uint32_t(v); }

template <> inline uint64_t unbox(cl_object v) { return ecl_to_uint64_t(v); }

template <> inline void *unbox(cl_object v) { return ecl_to_pointer(v); }

template <typename T> struct IsFundamental {
  static constexpr bool value =
      std::is_fundamental<T>::value || std::is_void<T>::value ||
      std::is_same<T, std::complex<float>>::value ||
      std::is_same<T, std::complex<double>>::value ||
      std::is_same<T, std::complex<long double>>::value ||
      std::is_same<T, void *>::value;
};

// to CPP
template <typename T, bool Fundamental = false, typename Enable = void>
struct ConvertToCpp {
  template <typename lispT> T *operator()(lispT &&) const {
    static_assert(sizeof(T) == 0,
                  "No appropriate specialization for ConvertToCpp");
    return nullptr; // not reached
  }
};

// Fundamental type conversion
template <typename T> struct ConvertToCpp<T, true> {
  T operator()(cl_object lisp_val) const { return unbox<cl_object>(lisp_val); }
};

// pass-through for cl_object
template <> struct ConvertToCpp<cl_object, false> {
  cl_object operator()(cl_object lisp_value) const { return lisp_value; }
};

// strings
template <> struct ConvertToCpp<const char *, false> {
  const char *operator()(cl_object jstr) const {
    if (jstr == nullptr || !is_lisp_string(jstr)) {
      throw std::runtime_error(
          "Any type to convert to string is not a string but a " +
          lisp_type_name((cl_object)cl_type_of(jstr)));
    }
    return lisp_string(jstr);
  }
};

template <> struct ConvertToCpp<std::string, false> {
  std::string operator()(cl_object jstr) const {
    return std::string(ConvertToCpp<const char *, false>()(jstr));
  }
};

template <>
struct ConvertToCpp<void *, false> {
  cl_object operator()(void *p) const { return ecl_make_pointer(p); }
};

// To lisp
template <typename T, bool Fundamental = false, typename Enable = void>
struct ConvertToLisp;

template <typename T> struct ConvertToLisp<T, true> {
  cl_object operator()(T cpp_val) const { return box<T>(cpp_val); }
};

template <> struct ConvertToLisp<std::string, false> {
  cl_object operator()(const std::string &str) const {
    return ecl_make_simple_base_string(str.c_str(), str.length());
  }
};

template <> struct ConvertToLisp<std::string *, false> {
  cl_object operator()(const std::string *str) const {
    return ConvertToLisp<std::string, false>()(*str);
  }
};

template <> struct ConvertToLisp<const std::string *, false> {
  cl_object operator()(const std::string *str) const {
    return ConvertToLisp<std::string, false>()(*str);
  }
};

template <> struct ConvertToLisp<const char *, false> {
  cl_object operator()(const char *str) const {
    return ecl_make_simple_base_string(str, strlen(str));
  }
};

template <>
struct ConvertToLisp<void *, false> {
  cl_object operator()(void *p) const { return ecl_make_pointer(p); }
};

template <> struct ConvertToLisp<cl_object, false> {
  cl_object operator()(cl_object p) const { return p; }
};

namespace detail {
template <typename T> struct StrippedConversionType {
  typedef mapped_reference_type<T> type;
};

template <typename T> struct StrippedConversionType<T *&> { typedef T *type; };

template <typename T> struct StrippedConversionType<T *const &> {
  typedef T *type;
};
} // namespace detail

/// Remove reference and const from a type
template <typename T>
using remove_const_ref =
    typename std::remove_const<typename std::remove_reference<T>::type>::type;

template <typename T>
using lisp_converter_type =
    ConvertToLisp<typename detail::StrippedConversionType<T>::type,
                  IsFundamental<remove_const_ref<T>>::value>;

/// Conversion to the statically mapped target type.
template <typename T>
inline auto convert_to_lisp(T &&cpp_val)
    -> decltype(lisp_converter_type<T>()(std::forward<T>(cpp_val))) {
  return lisp_converter_type<T>()(std::forward<T>(cpp_val));
}

template <typename T>
using cpp_converter_type =
    ConvertToCpp<T, IsFundamental<remove_const_ref<T>>::value>;

/// Conversion to C++
template <typename CppT, typename LispT>
inline CppT convert_to_cpp(const LispT &lisp_val) {
  return cpp_converter_type<CppT>()(lisp_val);
}

// TODO: Generic conversion for C++ classes wrapped in a composite type

// template <typename T>
// struct CLCXX_API static_type_mapping<T, true> {
//   typedef class type;
//   static cl_object lisp_type() {
//     return ecl_read_from_cstring("STANDARD-OBJECT");
//   }
// };

// template <>
// struct ConvertToCpp<std::string, false> {
//   std::string operator()(cl_object jstr) const {
//     return std::string(ConvertToCpp<const char *, false>()(jstr));
//   }
// };

// template <typename T>
// struct ConvertToLisp<T, false, > {
//   cl_object operator()(T cpp_val) const { return box<T>(cpp_val); }
// };

// CLCXX_API cl_object ecl_defclass(cl_object name, cl_object package,
//                                  cl_object super, cl_object options,
//                                  std::queue<cl_object> slots,
//                                  std::queue<cl_object> types);

} // namespace clcxx
