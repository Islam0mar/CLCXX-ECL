// #include "clcxx/array.hpp"
#include "clcxx/clcxx.hpp"
#include "clcxx/clcxx_config.hpp"

extern "C" {

using namespace clcxx;

cl_object g_any_type = ECL_T;

/// Initialize the module
CLCXX_API void initialize(cl_object lisp_module, cl_object cpp_any_type,
                          cl_object cppfunctioninfo_type) {
  g_cxxwrap_module = (cl_object)julia_module;
  g_any_type = (cl_object)cpp_any_type;
  g_cppfunctioninfo_type = (cl_object)cppfunctioninfo_type;

  InitHooks::instance().run_hooks();
}

CLCXX_API void register_lisp_module(cl_object clmod,
                                    void (*regfunc)(clcxx::Module &)) {
  try {
    clcxx::Module &mod = clcxx::registry().create_module(clmod);
    regfunc(mod);
    mod.for_each_function([](FunctionWrapperBase &f) {
      // Make sure any pointers in the types are also resolved at module init.
      f.argument_types();
      f.return_type();
    });
    clcxx::registry().reset_current_module();
  } catch (const std::runtime_error &e) {
    cl_error(1, ecl_make_simple_base_string(e.what()));
  }
}

CLCXX_API bool has_cxx_module(cl_object clmod) {
  return clcxx::registry().has_module(clmod);
}

CLCXX_API cl_object get_any_type() { return g_any_type; }

CLCXX_API cl_object get_cxxwrap_module() { return g_cxxwrap_module; }

/// Bind cl_object structures to corresponding Lisp symbols in the given module
CLCXX_API void bind_module_constants(cl_object module_any) {
  cl_object mod = (cl_object)module_any;
  registry().get_module(mod).bind_constants(mod);
}

/// TODO: match vector used in other functions
void fill_types_vec(Array<cl_object> &types_array,
                    const std::vector<cl_object> &types_vec) {
  for (const auto &t : types_vec) {
    types_array.push_back(t);
  }
}

/// Get the functions defined in the modules. Any classes used by these
/// functions must be defined on the Lisp side first
CLCXX_API cl_object get_module_functions(cl_object clmod) { // TODO:
  Array<cl_object> function_array(g_cppfunctioninfo_type);

  const clcxx::Module &module = registry().get_module(clmod);
  module.for_each_function([&](FunctionWrapperBase &f) {
    Array<cl_object> arg_types_array, ref_arg_types_array;
    cl_object boxed_f = nullptr;
    cl_object boxed_thunk = nullptr;

    fill_types_vec(arg_types_array, f.argument_types());
    fill_types_vec(ref_arg_types_array, f.reference_argument_types());

    boxed_f = clcxx::box(f.pointer_index());
    boxed_thunk = clcxx::box(f.thunk_index());

    function_array.push_back(jl_new_struct(
        g_cppfunctioninfo_type, f.name(), arg_types_array.wrapped(),
        ref_arg_types_array.wrapped(), f.return_type(), boxed_f, boxed_thunk));

  });
  return function_array.wrapped();
}

cl_object convert_type_vector(const std::vector<cl_object> types_vec) {
  Array<cl_object> datatypes;
  for (cl_object dt : types_vec) {
    datatypes.push_back(dt);
  }
  return datatypes.wrapped();
}

CLCXX_API cl_object get_reference_types(cl_object clmod) {
  return convert_type_vector(registry().get_module(clmod).reference_types());
}

CLCXX_API cl_object get_allocated_types(cl_object clmod) {
  return convert_type_vector(registry().get_module(clmod).allocated_types());
}

CLCXX_API void gcprotect(cl_object val) { clcxx::protect_from_gc(val); }

CLCXX_API void gcunprotect(cl_object val) { clcxx::unprotect_from_gc(val); }

CLCXX_API const char *version_string() { return CLCXX_VERSION_STRING; }
}


