#pragma once

#include "clcxx.hpp"
#include "tuple.hpp"

namespace jlcxx
{

typedef int_t index_t;

namespace detail
{
  // Helper to make a C++ tuple of longs based on the number of elements
  template<index_t N, typename... TypesT>
  struct LongNTuple
  {
    typedef typename LongNTuple<N-1, index_t, TypesT...>::type type;
  };

  template<typename... TypesT>
  struct LongNTuple<0, TypesT...>
  {
    typedef std::tuple<TypesT...> type;
  };
}

/// Wrap a const pointer
template<typename T>
struct ConstPtr
{
  const T* ptr;
};

template<typename T> struct IsBits<ConstPtr<T>> : std::true_type {};

template<typename T>
struct InstantiateParametricType<ConstPtr<T>>
{
  int operator()(Module&) const
  {
    // Register the Julia type if not already instantiated
    if(!static_type_mapping<ConstPtr<T>>::has_lisp_type())
    { // FIXME:
      jl_datatype_t* dt = (jl_datatype_t*)apply_type((cl_object)lisp_type("ConstPtr"), jl_svec1(static_type_mapping<T>::lisp_type()));
      set_lisp_type<ConstPtr<T>>(dt);
      protect_from_gc(dt);
    }
    return 0;
  }
};

/// Wrap a pointer, providing the Julia array interface for it
/// The parameter N represents the number of dimensions
template<typename T, index_t N>
class ConstArray
{
public:
  typedef typename detail::LongNTuple<N>::type size_t;

  template<typename... SizesT>
  ConstArray(const T* ptr, const SizesT... sizes) :
    m_arr(ptr),
    m_sizes(sizes...)
  {
  }

  T getindex(const int_t i) const
  {
    return m_arr[i-1];
  }

  size_t size() const
  {
    return m_sizes;
  }

  const T* ptr() const
  {
    return m_arr;
  }

private:
  const T* m_arr;
  const size_t m_sizes;
};

template<typename T, index_t N>
struct InstantiateParametricType<ConstArray<T,N>> : InstantiateParametricType<ConstPtr<T>>
{
};

template<typename T, typename... SizesT>
ConstArray<T, sizeof...(SizesT)> make_const_array(const T* p, const SizesT... sizes)
{
  return ConstArray<T, sizeof...(SizesT)>(p, sizes...);
}

template<typename T, index_t N> struct IsImmutable<ConstArray<T,N>> : std::true_type {};

template<typename T, index_t N>
struct ConvertToLisp<ConstArray<T,N>, false, true, false>
{
  cl_object operator()(const ConstArray<T,N>& arr)
  {
    cl_object result = nullptr;
    cl_object ptr = nullptr;
    cl_object size = nullptr;
    ptr = box(ConstPtr<T>({arr.ptr()}));
    size = convert_to_lisp(arr.size());
    result = jl_new_struct(lisp_type<ConstArray<T,N>>(), ptr, size); // FIXME:
    return result;
  }
};

template<typename T, index_t N>
struct static_type_mapping<ConstArray<T,N>>
{
  typedef cl_object type;
  static cl_object lisp_type()
  {
    static cl_object app_dt = nullptr;
    if(app_dt == nullptr)
    {
      cl_object pdt = (cl_object)::jlcxx::lisp_type("ConstArray");
      cl_object boxed_n = box(N);
      // FIXME: ??
      app_dt = (cl_object)apply_type((cl_object)pdt, jl_svec2(::jlcxx::lisp_type<T>(), boxed_n));
      protect_from_gc(app_dt);
    }
    return app_dt;
  }
};

} // namespace jlcxx

