#pragma once

#include "type_conversion.hpp"


namespace clcxx
{

template<typename PointedT, typename CppT>
struct ValueExtractor
{
  inline CppT operator()(PointedT* p)
  {
    return ConvertToCpp<CppT>(*p);
  }
};

inline cl_object apply_array_type(cl_object type, cl_object dim) {
  return cl_make_array(3, dim,ecl_read_from_cstring(":element-type"), (cl_object)type);
}

template<typename PointedT>
struct ValueExtractor<PointedT, PointedT>
{
  inline PointedT& operator()(PointedT* p)
  {
    return *p;
  }
};

template<typename PointedT, typename CppT>
class array_iterator_base : public std::iterator<std::random_access_iterator_tag, PointedT>
{
private:
  PointedT* m_ptr;
public:
  array_iterator_base() : m_ptr(nullptr)
  {
  }

  explicit array_iterator_base(PointedT* p) : m_ptr(p)
  {
  }

  template <class OtherPointedT, class OtherCppT>
  array_iterator_base(array_iterator_base<OtherPointedT, OtherCppT> const& other) : m_ptr(other.m_ptr) {}

  auto operator*() -> decltype(ValueExtractor<PointedT,CppT>()(m_ptr))
  {
    return ValueExtractor<PointedT,CppT>()(m_ptr);
  }

  array_iterator_base<PointedT, CppT>& operator++()
  {
    ++m_ptr;
    return *this;
  }

  array_iterator_base<PointedT, CppT>& operator--()
  {
    --m_ptr;
    return *this;
  }

  array_iterator_base<PointedT, CppT>& operator+=(std::ptrdiff_t n)
  {
    m_ptr += n;
    return *this;
  }

  array_iterator_base<PointedT, CppT>& operator-=(std::ptrdiff_t n)
  {
    m_ptr -= n;
    return *this;
  }

  PointedT* ptr() const
  {
    return m_ptr;
  }
};

/// Wrap a Julia 1D array in a C++ class. Array is allocated on the C++ side
template<typename ValueT>
class Array
{
public:
  Array(const size_t n = 0)
  {
    cl_object array_type = apply_array_type(static_type_mapping<ValueT>::lisp_type(), 1);
    m_array = si_make_vector(array_type, ecl_make_fixnum(n),
                           ECL_T, ecl_make_fixnum(0), ECL_NIL, ECL_NIL);
  }

  Array(cl_object applied_type, const size_t n = 0)
  {
    cl_object array_type = apply_array_type(applied_type, 1);
    m_array = si_make_vector(array_type, ecl_make_fixnum(n),
                           ECL_T, ECL_NIL, ECL_NIL, ECL_NIL);
  }

  /// Append an element to the end of the list
  void push_back(const ValueT& val)
  {
    cl_vector_push_extend(2, m_array, box(val));
  }

  /// Access to the wrapped array
  cl_object wrapped()
  {
    return m_array;
  }

  // access to the pointer for GC macros
  cl_object* gc_pointer()
  {
    return &m_array;
  }

private:
  cl_object m_array;
};

/// Only provide read/write operator[] if the array contains non-boxed values
template<typename PointedT, typename CppT>
struct IndexedArrayRef
{
  IndexedArrayRef(cl_object arr) : m_array(arr)
  {
  }

  CppT operator[](const std::size_t i) const
  {
    return ConvertToCpp<CppT>(ecl_aref1(m_array, i));
  }

  cl_object m_array;
};

template<typename ValueT>
struct IndexedArrayRef<ValueT,ValueT>
{
  IndexedArrayRef(cl_object arr) : m_array(arr)
  {
  }

  ValueT& operator[](const std::size_t i)
  {
    return ((cl_object)m_array->array.self)[i];
  }

  ValueT operator[](const std::size_t i) const
  {
    return ((cl_object)m_array->array.self)[i];
  }

  cl_object m_array;
};

/// Reference a Julia array in an STL-compatible wrapper
template<typename ValueT, int Dim = 1>
class ArrayRef : public IndexedArrayRef<mapped_lisp_type<ValueT>, ValueT>
{
public:
  ArrayRef(cl_object arr) : IndexedArrayRef<mapped_lisp_type<ValueT>, ValueT>(arr)
  {
    assert(wrapped() != nullptr);
  }

  /// Convert from existing C-array (memory owned by C++)
  template<typename... SizesT>
  ArrayRef(cl_object ptr, const SizesT... sizes);

  /// Convert from existing C-array, explicitly setting Julia ownership
  template<typename... SizesT>
  ArrayRef(const bool lisp_owned, cl_object ptr, const SizesT... sizes);

  typedef mapped_lisp_type<ValueT> lisp_t;

  typedef array_iterator_base<lisp_t, ValueT> iterator;
  typedef array_iterator_base<lisp_t const, ValueT const> const_iterator;

  inline cl_object wrapped() const
  {
    return IndexedArrayRef<lisp_t, ValueT>::m_array;
  }

  iterator begin()
  {
    return iterator(static_cast<lisp_t*>(wrapped()->array.self));
  }

  const_iterator begin() const
  {
    return const_iterator(static_cast<lisp_t*>(wrapped()->array.self));
  }

  iterator end()
  {
    return iterator(static_cast<lisp_t*>(wrapped()->array.self) + (wrapped()->array.dim));
  }

  const_iterator end() const
  {
    return const_iterator(static_cast<lisp_t*>(wrapped()->array.self) + (wrapped()->array.dim));
  }

  void push_back(const ValueT& val)
  {
    static_assert(Dim == 1, "push_back is only for 1D ArrayRef");
    cl_object arr_ptr = wrapped();
    cl_vector_push_extend(2, arr_ptr, box(val));
  }

  /*const*/ cl_object data() const
  {
    return (cl_object)wrapped()->array.self;
  }

  cl_object data()
  {
    return (cl_object)wrapped()->array.self;
  }

  std::size_t size() const
  {
    return (wrapped()->array.dim);
  }
};

// template<typename T, int Dim> struct IsValueType<ArrayRef<T,Dim>> : std::true_type {};

// Conversions
template<typename T, int Dim> struct static_type_mapping<ArrayRef<T, Dim>>
{
  typedef cl_object type;
  static cl_object lisp_type() { return (cl_object)apply_array_type(static_type_mapping<T>::lisp_type(), Dim); }
};


// // TODO:
// template<typename ValueT, typename... SizesT>
// cl_object wrap_array(const bool lisp_owned, cl_object c_ptr, const SizesT... sizes)
// {
//   cl_object dt = static_type_mapping<ArrayRef<ValueT, sizeof...(SizesT)>>::lisp_type();
//   cl_object dims = nullptr;
//   dims = convert_to_lisp(std::make_tuple(static_cast<int_t>(sizes)...));
//   cl_object result = jl_ptr_to_array((cl_object)dt, c_ptr, dims, lisp_owned);
//   return result;
// }

// template<typename ValueT, int Dim>
// template<typename... SizesT>
// ArrayRef<ValueT, Dim>::ArrayRef(cl_object c_ptr, const SizesT... sizes) : IndexedArrayRef<lisp_t, ValueT>(nullptr)
// {
//   IndexedArrayRef<lisp_t, ValueT>::m_array = wrap_array(false, c_ptr, sizes...);
// }

// template<typename ValueT, int Dim>
// template<typename... SizesT>
// ArrayRef<ValueT, Dim>::ArrayRef(const bool lisp_owned, cl_object c_ptr, const SizesT... sizes) : IndexedArrayRef<lisp_t, ValueT>(nullptr)
// {
//   IndexedArrayRef<lisp_t, ValueT>::m_array = wrap_array(lisp_owned, c_ptr, sizes...);
// }

// template<typename ValueT, typename... SizesT>
// auto make_lisp_array(cl_object c_ptr, const SizesT... sizes) -> ArrayRef<ValueT, sizeof...(SizesT)>
// {
//   return ArrayRef<ValueT, sizeof...(SizesT)>(true, c_ptr, sizes...);
// }

template<typename T, int Dim>
struct ConvertToLisp<ArrayRef<T,Dim>, false>
{
  template<typename ArrayRefT>
  cl_object operator()(ArrayRefT&& arr) const
  {
    return arr.wrapped();
  }
};

template<typename T, int Dim>
struct ConvertToCpp<ArrayRef<T,Dim>, false>
{
  ArrayRef<T,Dim> operator()(cl_object arr) const
  {
    return ArrayRef<T,Dim>(arr);
  }
};

// Iterator operator implementation
template<typename L, typename R>
bool operator!=(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return r.ptr() != l.ptr();
}

template<typename L, typename R>
bool operator==(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return r.ptr() == l.ptr();
}

template<typename L, typename R>
bool operator<=(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return l.ptr() <= r.ptr();
}

template<typename L, typename R>
bool operator>=(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return l.ptr() >= r.ptr();
}

template<typename L, typename R>
bool operator>(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return l.ptr() > r.ptr();
}

template<typename L, typename R>
bool operator<(const array_iterator_base<L,L>& l, const array_iterator_base<R,R>& r)
{
  return l.ptr() < r.ptr();
}

template<typename T>
array_iterator_base<T, T> operator+(const array_iterator_base<T,T>& l, const std::ptrdiff_t n)
{
  return array_iterator_base<T, T>(l.ptr() + n);
}

template<typename T>
array_iterator_base<T, T> operator+(const std::ptrdiff_t n, const array_iterator_base<T,T>& r)
{
  return array_iterator_base<T, T>(r.ptr() + n);
}

template<typename T>
array_iterator_base<T, T> operator-(const array_iterator_base<T,T>& l, const std::ptrdiff_t n)
{
  return array_iterator_base<T, T>(l.ptr() - n);
}

template<typename T>
std::ptrdiff_t operator-(const array_iterator_base<T,T>& l, const array_iterator_base<T,T>& r)
{
  return l.ptr() - r.ptr();
}

}

