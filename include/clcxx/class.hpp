#pragma once

#include "type_conversion.hpp"
#include <vector>
#include <queue>


namespace clcxx
{

// template <typename T>
// cl_object add_class_member(T value, char name[]){
//   return cl_list(6,ecl_read_from_cstring(":initform"),
//           ConvertToLisp<T, IsFundamental(T)>(value)
//           ecl_read_from_cstring(":name"),
//           ecl_read_from_cstring(name),
//           ecl_read_from_cstring(":type"),
//           static_type_mapping<T>.lisp_type());
// }


CLCXX_API cl_object ecl_defclass(char name[], char package[],
                                 cl_object super, cl_object options,
                                 std::queue<cl_object> value_name_type);

} // namespace clcxx

