#include <ecl/ecl.h>

#include <stack>
#include <string>

#include "clcxx/clcxx.hpp"
#include "clcxx/array.hpp"
#include "clcxx/clcxx_config.hpp"
#include "clcxx/functions.hpp"

namespace clcxx {

cl_object g_cxxwrap_module;
cl_object g_cppfunctioninfo_type;

CLCXX_API cl_object gc_protected() {
  static cl_object p_arr = nullptr;
  if (p_arr == nullptr) {
    p_arr = si_make_vector(ECL_T, ecl_make_fixnum(0), ECL_T, ecl_make_fixnum(0),
                           ECL_NIL, ECL_NIL);
    int intern_flag;
    cl_object sym = ecl_intern(ecl_read_from_cstring("GC-PROTECTED"),
                               g_cxxwrap_module, &intern_flag);
    si_Xmake_constant(sym, p_arr);
  }
  return p_arr;
}

CLCXX_API std::stack<std::size_t> &gc_free_stack() {
  static std::stack<std::size_t> p_stack;
  return p_stack;
}

CLCXX_API std::map<cl_object, std::pair<std::size_t, std::size_t>> &
gc_index_map() {
  static std::map<cl_object, std::pair<std::size_t, std::size_t>> p_map;
  return p_map;
}

Package::Package(cl_object cl_pack) : p_cl_pack(cl_pack) {}

Package &PackageRegistry::create_package(cl_object pack_name) {
  pack_name = cl_string_upcase(1, pack_name);
  cl_object package = ecl_make_package(pack_name, ECL_NIL, ECL_NIL, ECL_NIL);
  si_select_package(package);
  p_current_package = new Package(package); // TODO
  p_packages[package].reset(p_current_package);
  return *p_current_package;
}

Package &PackageRegistry::current_package() {
  assert(p_current_package != nullptr);
  return *p_current_package;
}

CLCXX_API PackageRegistry &registry() {
  static PackageRegistry p_registry;
  return p_registry;
}

// done
CLCXX_API cl_object lisp_type(const std::string &name,
                              const std::string &package_name) {
  std::vector<cl_object> mods;
  mods.reserve(6);
  cl_object current_mod = // registry().has_current_package()
                          //     ? registry().current_package().lisp_package() :
      ECL_NIL;
  if (!package_name.empty()) {
    // cl_object modsym = ecl_read_frop_cstring(package_name.c_str());
    cl_object found_mod = ecl_find_package(package_name.c_str());
    if (ecl_to_bool(found_mod)) {
      mods.push_back(found_mod);
    } else {
      throw std::runtime_error("Failed to find package " + package_name);
    }
  } else {
    if (ecl_to_bool(current_mod)) {
      mods.push_back(current_mod);
    }
    mods.push_back(ecl_read_from_cstring("CL"));
    mods.push_back(ecl_read_from_cstring("CL-USER"));
  }

  std::string found_type = "null";
  for (cl_object mod : mods) {
    if (ecl_to_bool(mod)) {
      continue;
    }

    cl_object gval = cl_subtypep(2, ecl_read_from_cstring(name.c_str()), ECL_T);
    if (ecl_to_bool(gval)) {
      return gval;
    }
  }
  std::string errmsg =
      "Symbol for type " + name + " was not found. Searched packages:";
  for (cl_object mod : mods) {
    if (mod != nullptr) {
      errmsg += " " + symbol_name(mod);
    }
  }
  throw std::runtime_error(errmsg);
}

// InitHooks &InitHooks::instance() {
//   static InitHooks hooks;
//   return hooks;
// }

// InitHooks::InitHooks() {}

// void InitHooks::add_hook(const hook_t hook) { p_hooks.push_back(hook); }

// void InitHooks::run_hooks() {
//   for (const hook_t &h : p_hooks) {
//     h();
//   }
// }

template <typename... Args>
CLCXX_API cl_object apply_type_list(cl_object tc, Args... args) {
  return cl_list((sizeof...(args) + 1), tc, args...);
}

static constexpr const char *dt_prefix = "CXXWRAP-DT-";

cl_object existing_datatype(cl_object mod, cl_object name) {
  const std::string prefixed_name =
      dt_prefix +
      std::string(ecl_base_string_pointer_safe(ecl_symbol_name(name)));
  cl_object found_dt =
      cl_find_symbol(2, ecl_read_from_cstring(prefixed_name.c_str()), mod);
  if (!ecl_to_bool(found_dt)) {
    return nullptr;
  }
  return found_dt;
}

void set_internal_constant(cl_object mod, cl_object dt,
                           const std::string &prefixed_name) {
  cl_object sym =
      cl_intern(2, ecl_read_from_cstring(prefixed_name.c_str()), mod);
  si_Xmake_constant(sym, dt);
}

// TODO
inline cl_object expand_into_list(cl_object name, cl_object type) {
  return ecl_list1(cl_list(4, ecl_read_from_cstring(":type"), type,
                           ecl_read_from_cstring(":name"), name));
}

// // NOTE: name should be in capital letters
template <typename... Slots>
CLCXX_API cl_object new_datatype(cl_object name, cl_object package,
                                 cl_object super, cl_object options,
                                 cl_object slots[], cl_object types[]) {
  // FIXME: not needed!
  if (package == ECL_NIL) {
    throw std::runtime_error("null package when creating type");
  }

  // ecl_fixnum()
  int *intern_flag;
  cl_object dt = ecl_find_symbol(name, package, intern_flag);

  if (ecl_to_bool(dt)) {
    return dt;
  }

  dt = ecl_make_symbol(ecl_base_string_pointer_safe(name),
                       ecl_base_string_pointer_safe(cl_symbol_name(package)));

  // make class fields
  cl_object body = ECL_NIL;
  for (unsigned long i = 0; i < (sizeof(slots) / sizeof(slots[0])); ++i) {
    body = cl_append(2, body, expand_into_list(slots[i], types[i]));
  }

  dt = clos_load_defclass(dt, super, body, options);

  std::string temp_str = dt_prefix + symbol_name(name);
  cl_object sym =
      ecl_make_symbol(temp_str.c_str(), symbol_name(package).c_str());
  si_Xmake_constant(sym, dt);
  return dt;
}

} // namespace clcxx
