#include "class.hpp"


namespace clcxx {


CLCXX_API cl_object ecl_defclass(char[] name, char[] package,
                                 cl_object super, cl_object options,
                                 std::queue<cl_object> value_name_type){
  cl_object s = cl_make_array(3,ecl_make_integer(0),ecl_read_from_cstring(":adjustable"),ECL_T);
  for (auto it = value_name_type.front() ; !value_name_type.empty(); it = value_name_type.front()) {
    // TODO: chech push order
    s = cl_vector_push_extend(2,it,s);
    value_name_type.pop();
  }
  
  return clos_load_defclass(
      ecl_make_symbol(name,package), // ecl_read_from_cstring("MAP"),
      super,
      ecl_list1(s),
      options);
}
}

/* clos_load_defclass(
      ecl_make_symbol("MAP","CL-USER"), // ecl_read_from_cstring("MAP"),
      ECL_NIL,
      ecl_list1(cl_list(4, ecl_read_from_cstring(":initform"),
                        ecl_read_from_cstring("125"),
                        ecl_read_from_cstring(":name"),
                        ecl_read_from_cstring("x"))),
                        ECL_NIL);*/

