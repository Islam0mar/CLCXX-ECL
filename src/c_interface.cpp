#include "clcxx/array.hpp"
#include "clcxx/clcxx.hpp"
#include "clcxx/clcxx_config.hpp"

extern "C" {

CLCXX_API void register_lisp_package(cl_object cl_pack,
                                     void (*regfunc)(clcxx::Package &)) {
  try {
    clcxx::Package &pack = clcxx::registry().create_package(cl_pack);
    regfunc(pack);
    // pack.for_each_function([](FunctionWrapperBase &f) {
    //   // Make sure any pointers in the types are also resolved at module
    //   init. f.argument_types(); f.return_type();
    //   });
    clcxx::registry().reset_current_package();
  } catch (const std::runtime_error &err) {
    FEerror(err.what(), 0);
  }
}

CLCXX_API void clcxx_init(cl_object pack_name, cl_object module) {
  try {
    clcxx::Cblock = ecl_make_codeblock();
    // retreive package name function pointer
    cl_object fptr_object = si_find_foreign_symbol(
        pack_name, module, ecl_read_from_cstring(":POINTER-VOID"),
        ecl_make_fixnum(0));
    auto fptr = reinterpret_cast<void (*)(clcxx::Package &)>(
        ecl_foreign_data_pointer_safe(fptr_object));
    cl_object current_package = ecl_current_package();
    register_lisp_package(pack_name, fptr);
    si_select_package(current_package);
  } catch (const std::runtime_error &err) {
    FEerror(err.what(), 0);
  }
}

}
