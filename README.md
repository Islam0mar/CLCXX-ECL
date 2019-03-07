# CLCXX

It is forked from julia language [libcxxwrap](https://github.com/JuliaInterop/libcxxwrap-julia)
This is the C++ library to be used with ecl such as boost.python

It uses dev-branch of [ECL](https://gitlab.com/embeddable-common-lisp/ecl)

# done
- C++ function, lambda and c functions auto type conversion.

# TODO:
- support classes
- replace ECL with SBCL

# compilation

```shell
    mkdir build
    cd build
    cmake ..
    make
```

then open ecl 

```lisp
(defmacro load-foriegn-package (f_name lib_path)
           "Load foreign package"
           (let ((c-name (coerce "clcxx_init" 'base-string))
                 (f_name (coerce f_name 'base-string)))
             `(progn
               (ffi:def-function (,c-name clcxx) ((name :object) (path :object))
                  :module ,lib_path)
               (clcxx ,f_name ,lib_path))))
```
the try functions defined in src/test.cpp

```lisp
    (load-foriegn-package "SHIT" #p"path/to/build/lib/libclcxx.so")
    (shit::hi "CLCXX")
    (shit::greet)
```


