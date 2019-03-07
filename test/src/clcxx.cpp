#include <ecl/ecl.h>

#include "clcxx/clcxx.hpp"
// #include "clcxx/array.hpp"
#include "clcxx/functions.hpp"
#include "clcxx/clcxx_config.hpp"

namespace clcxx {

cl_object g_cxxwrap_module;
cl_object g_cppfunctioninfo_type;

CLCXX_API cl_object gc_protected() {
  static cl_object m_arr = nullptr;
  if (m_arr == nullptr) {
    m_arr = si_make_vector(ECL_T, ecl_make_fixnum(0),
                           ECL_T, ecl_make_fixnum(0), ECL_NIL, ECL_NIL);
    int intern_flag;
    cl_object sym = ecl_intern(ecl_read_from_cstring("GC-PROTECTED"),
                               g_cxxwrap_module,
                               &intern_flag);
    si_Xmake_constant(sym, m_arr);
  }
  return m_arr;
}

CLCXX_API std::stack<std::size_t> &gc_free_stack() {
  static std::stack<std::size_t> m_stack;
  return m_stack;
}

CLCXX_API std::map<cl_object, std::pair<std::size_t, std::size_t>> &
gc_index_map() {
  static std::map<cl_object, std::pair<std::size_t, std::size_t>> m_map;
  return m_map;
}

Module::Module(cl_object clmod)
    : m_cl_mod(clmod), m_pointer_array(cl_find_symbol(2,
                                                      ecl_read_from_cstring("CXXWRAP-POINTERS"), clmod)) {}

int_t Module::store_pointer(void *ptr) {
  assert(ptr != nullptr);
  m_pointer_array.push_back(ptr);
  return m_pointer_array.size();
}

void Module::bind_constants(cl_object mod) {
  for (auto &dt_pair : m_cl_constants) {
    cl_object sym = cl_intern(2, ecl_read_from_cstring(dt_pair.first.c_str()), mod)
    si_Xmake_constant(sym, dt_pair.second);
  }
}

Module &ModuleRegistry::create_module(cl_object package) {
  if (package == nullptr)
    throw std::runtime_error("Can't create package from null lisp name");
  if (m_modules.count(package))
    throw std::runtime_error("Error registering package: " +
                             package_name(package) + " was already registered");

  package = ecl_make_package(package, ECL_NIL, ECL_NIL);
  m_current_module = new Module(package); //TODO
  m_modules[package].reset(m_current_module);
  return *m_current_module;
}

Module &ModuleRegistry::current_module() {
  assert(m_current_module != nullptr);
  return *m_current_module;
}

void FunctionWrapperBase::set_pointer_indices() {
  m_pointer_index = m_module->store_pointer(pointer());
  void *thk = thunk();
  if (thk != nullptr) {
    m_thunk_index = m_module->store_pointer(thunk());
  }
}

CLCXX_API ModuleRegistry &registry() {
  static ModuleRegistry m_registry;
  return m_registry;
}

// done
CLCXX_API cl_object lisp_type(const std::string &name,
                              const std::string &module_name) {
  std::vector<cl_object> mods;
  mods.reserve(6);
  cl_object current_mod = registry().has_current_module()
                              ? registry().current_module().lisp_module()
                              : ECL_NIL;
  if (!module_name.empty()) {
    cl_object modsym = ecl_read_from_cstring(module_name.c_str());
    cl_object found_mod = ecl_find_package(modsym);
    if (ecl_to_bool(found_mod)) {
      mods.push_back(found_mod);
    } else {
      throw std::runtime_error("Failed to find module " + module_name);
    }
  } else {
    if (ecl_to_bool(current_mod)) {
      mods.push_back(current_mod);
    }
    mods.push_back(ecl_read_from_cstring("CL"));
    mods.push_back(g_cxxwrap_module);
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
      "Symbol for type " + name + " was not found. Searched modules:";
  for (cl_object mod : mods) {
    if (mod != nullptr) {
      errmsg += " " + symbol_name(mod->name);
    }
  }
  throw std::runtime_error(errmsg);
}

InitHooks &InitHooks::instance() {
  static InitHooks hooks;
  return hooks;
}

InitHooks::InitHooks() {}

void InitHooks::add_hook(const hook_t hook) { m_hooks.push_back(hook); }

void InitHooks::run_hooks() {
  for (const hook_t &h : m_hooks) {
    h();
  }
}


template <typename... Args>
CLCXX_API cl_object apply_type_list(cl_object tc, Args... args) {
  return cl_list(++(sizeof...(args)), tc, args...);
}

static constexpr const char *dt_prefix = "CXXWRAP-DT-";

cl_object existing_datatype(cl_object mod, cl_object name) {
  const std::string prefixed_name = dt_prefix + ecl_base_string_pointer_safe(ecl_symbol_name(name));
  cl_object found_dt = cl_find_symbol(2, ecl_read_from_cstring(prefixed_name.c_str()), mod);
  if (!elc_to_bool(found_dt)) {
    return nullptr;
  }
  return found_dt;
}

void set_internal_constant(cl_object mod, cl_object dt,
                           const std::string &prefixed_name) {
  cl_object sym = cl_intern(2, ecl_read_from_cstring(prefixed_name.c_str()), mod)  
  si_Xmake_constant(sym, dt)
}

// TODO
inline cl_object expand_into_list(cl_object name,cl_object type) {
  return ecl_list1(cl_list(4,
                           ecl_read_from_cstring(":type"),
                           type,
                           ecl_read_from_cstring(":name"),
                           name));
}

// // NOTE: name should be in capital letters
template <typename... Slots>
CLCXX_API cl_objetc new_datatype(cl_object name, cl_object package,
                                 cl_object super, cl_object options,
                                 cl_object slots[], cl_object types[]) {
  // FIXME: not needed!
  if (package == ECL_NIL) {
    throw std::runtime_error("null module when creating type");
  }

  // ecl_fixnum()
  int intern_flag;
  cl_object dt = ecl_find_symbol(name, package, int *intern_flag);

  if (ecl_to_bool(dt)) {
    return dt;
  }

  dt = ecl_make_symbol(ecl_base_string_pointer_safe(name), ecl_base_string_pointer_safe(cl_symbol_name(package)));

  // make class fields
  cl_object body = ECL_NIL;
  for (unsigned long i=0; i<(sizeof(slots)/sizeof(slots[0])); ++i) {
    body = cl_append(body, expand_into_list(slots[i],types[i]));
  }

  dt = clos_load_defclass(dt,
                          super,
                          body,
                          options);
  cl_object sym  = cl_intern(2, dt_prefix + symbol_name(name) , module);    
  si_Xmake_constant(cl_object sym, cl_object dt);
  return dt;
}

} // namespace clcxx

