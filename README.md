# safe variant

[![Build Status](https://travis-ci.org/cbeck88/safe-variant.svg?branch=master)](http://travis-ci.org/cbeck88/safe-variant)
[![Coverage Status](https://coveralls.io/repos/cbeck88/safe-variant/badge.svg?branch=master&service=github)](https://coveralls.io/github/cbeck88/safe-variant?branch=master)
[![Boost licensed](https://img.shields.io/badge/license-Boost-blue.svg)](./LICENSE)

Do you use `boost::variant` or one of the many open-source C++11 implementations of a "tagged union" or variant type
in your C++ projects?

Do you get annoyed that code like this will compile, without any warning or error message?

```c++
  boost::variant<std::string, int> v;  

  v = true;  
```

Do you get annoyed that code like this will compile on some machines, but not others?

```c++
  boost::variant<long, unsigned int> v;  

  v = 10;  
```


If so, then this may be the variant type for you.

**safe variant** is yet another C++11 variant type, with the twist that it prevents "unsafe" implicit conversions
such as narrowing conversions, conversions from bool to other integral types, pointer conversions, etc., and handles
overload ambiguity differently from other C++ variant types.

It may be well-suited for use in scenarios where you need to have a variant holding multiple different integral types,
and really don't want to have any loss of precision or any "gotcha" conversions happening.

Overview
========

The reason that `boost::variant` and most other variants, including the `std::variant` which was accepted to C++17,
will allow implicit conversions, is that fundamentally they work through C++ overload resolution.

Generally, when you assign a value of some type `T` to such a variant, these variants are going to construct a temporary function object
which is overloaded once for each type in the variant's list. Then they apply the function object to the value, and overload
resolution selects which conversion will actually happen.

Overload resolution is a core C++ language feature, and in 90% of cases it works very well and does the right thing.

However, in the 10% of cases where overload resolution does the wrong thing, it can be quite difficult to work around it.
This includes the scenarios in which overload resolution is ambiguous, as well as the cases in which, due to some implicit conversion,
an overload is selected which the user did not intend.

Because integral types have so many permitted conversions, these problems are particularly obvious when you have a variant with
several integral types.

This happens commonly when using variant types to interface with some scripting language for instance. The typical dynamically-typed
scripting language will permit a variety of primitive values, so when binding to it, you may naturally end up with something like

```c++
    boost::variant<bool, int, float, std::string, ...>
```

**safe variant** therefore does not use overload resolution in its implementation.  

Instead, it uses a very simple iterative strategy.

- When the variant is constructed from a value, each type is checked one by one, to see if a *safe* conversion to that type is possible.
  If so, it is selected. If not, we check the next type. If no safe conversion is possible, then a compile-time error results.  
  This means that usually, when you declare your variants you simply list your integral types in "increasing" order, and it does the right thing.

- What conversions are safe?  
  I wrote a type trait that implements a strict notion of safety which was appropriate for the project in which
  I developed this. (See [1](include/safe_variant/conversion_rank.hpp), [2](include/safe_variant/safely_constructible.hpp)).
  - Conversions are not permitted between any two of the following classes:  
  Integral types, Floating point types, Character types, Boolean, Pointer types, and `wchar_t`.
  - If an integral or floating point conversion *could* be narrowing on some conforming implementation of C++, then it is not safe.  
  (So, `long` cannot be converted to `int`
  even if you are on a 32-bit machine and they have the same size for you, because it could be narrowing on a 64-bit machine.)
  - Signed can be promoted to unsigned, but the reverse is not allowed (since it is implementation-defined).
  - Conversions like `char *` to `const char *` are permitted, and standard conversions like array-to-pointer are permitted, but otherwise no pointer conversions are permitted.

- You can force the variant to a particular type using the `emplace` template function. Rarely necessary but sometimes useful, and saves a `move`.
  (There is also an emplace-ctor, where you select the type using tag dispatch.)


So, keep in mind, this is not a drop-in replacement for `boost::variant` or one of the other versions, its semantics are fundamentally different.
But in scenarios like those it was designed for, it may be easier to reason about and less error-prone.

Never Empty Guarantee
=====================

We deal with the "never empty" issue as follows:

**Any type used with the variant must be no-throw move constructible.**

This is enforced using static asserts, but sometimes that can be a pain if you are forced to use e.g. GCC 4-series versions of the C++ standard library which
are not C++11 conforming. So there is also a flag to turn the static asserts off, see `static constexpr bool assume_move_nothrow` in the header. (Note that if a move
does throw, you will get UB.)

This allows the implementation to be very simple and efficient compared with some other variant types, which may have to make extra copies to facilitate
exception-safety, or make only a "rarely empty" guarantee.

If you have a type with a throwing move, you are encouraged to use `safe_variant::recursive_wrapper<T>` instead of `T` in the variant.

`recursive_wrapper<T>` is behaviorally equivalent to `std::unique_ptr<T>`, making a copy of `T` on the heap. But it is implicitly convertible to `T&`, and so
for most purposes is syntactically the same as `T`. There is special support within `safe_variant::variant` so that you can call `get<T>(&v)` and get a pointer
to a `T` rather than the wrapper, for instance.

`recursive_wrapper<T>` always has a `noexcept` move ctor even if `T` does not.

This decision allows the guts of the variant to be very clean and simple -- there are no dynamic allocations taking place behind your back.  

If you want dynamic allocations to support throwing-moves, you opt-in to that using `recursive_wrapper`.

Note that the *stated* purpose of recursive wrapper, in `boost::variant` docs, is to allow you to declare variants which contain an incomplete type.
They also work great for that in `safe_variant::variant`.

Synopsis
========

The actual interface is in most ways the same as `boost::variant`, which strongly inspired this.  

(However, my interface is exception-free, if you want to have
analogues of the throwing functions in `boost::variant` you'll have to write them, which is pretty easy to do on top of the exception-free interface.)

```c++
namespace safe_variant {

  template <typename First, typename... Types>
  class variant {

    // Attempts to default construct the First type.
    // If First is not default-constructible then this is not available.
    variant();

    // Special member functions: Nothing special here
    variant(const variant &);
    variant & operator=(const variant &);

    variant(variant &&) noexcept;
    variant & operator=(variant &&) noexcept;
    ~variant() noexcept;

    // Constructs the variant from a type outside the variant,
    // using iterative strategy described in docs.
    // (SFINAE expression omitted here)
    template <typename T>
    variant(T &&);

    // Constructs the variant from a "subvariant", that is, another variant
    // type which has strictly fewer types, modulo recursive_wrapper.
    // (SFINAE expression omitted here)
    template <typename... OTypes>
    variant(const variant<Otypes...> &);

    template <typename... OTypes>
    variant(variant<Otypes...> &&);

    // Emplace ctor. Used to explicitly specify the type of the variant, and
    // invoke an arbitrary ctor of that type.
    template <typename T>
    struct emplace_tag {};

    template <typename T, typename... Args>
    explicit variant(emplace_tag<T>, Args && ... args);

    // Emplace operation
    // Force the variant to a particular value.
    // The user explicitly specifies the desired type as template parameter,
    // which must be one of the variant types, modulo const, recursive wrapper.
    template <typename T, typename... Args>
    void emplace(Args &&... args) {
      *this = variant(emplace_tag<T>, std::forward<Args>(args)...);
    }

    // Reports the runtime type. The returned value is an index into the list
    // <First, Types...>.
    int which() const;

    // Test for equality. The which values must match, and operator == for the
    // underlying values must return true.
    bool operator == (const variant &) const;
    bool operator != (const variant &) const;
  };

  // Apply a static_visitor to the variant. It is called using the current value
  // of the variant with its current type as the argument.
  // Any additional arguments to `apply_visitor` are forwarded to the visitor. 
  template <typename V, typename... Types, typename... Args>
  void apply_visitor(V && visitor, variant<Types...> && var, Args && ... args);

  // Access the stored value. Returns `nullptr` if `T` is not the currently
  // engaged type.
  template <typename T, typename ... Types>
  T * get(variant<Types...> * v);

  template <typename T, typename ... Types>
  const T * get(const variant<Types...> * v);

  // Returns a reference to the stored value. If it does not currently have the
  // indicated type, then it is move-assigned with the value "default", and
  // a reference to that value, within the variant, is returned.
  template <typename T, typename ... Types>
  T & get_or_default(variant<Types...> & v, T def = {});

} // end namespace safe_variant
```


Compiler Compatibility
======================

`safe_variant` is coded to the C++11 standard.

It is known to work with `gcc >= 4.9` and `clang >= 3.5`.  

(It used to work with `gcc-4.8`, but at some point that was lost, I'm not
 sure exactly why. `gcc-4.8` seems to have some `constexpr` troubles now.)

Usage
=====

This is a header-only C++11 template library. To use it, all you need to do is
add the `include` folder to your include path. Then use the following includes in your code.

Forward-facing includes:

- `#include <safe_variant/variant_fwd.hpp>`  
  Forward declares the variant type, recursive_wrapper type.  
- `#include <safe_variant/variant.hpp>`  
  Defines the variant type, as well as `apply_visitor`, `get`, `get_or_default` functions.  
- `#include <safe_variant/recursive_wrapper.hpp>`  
  Similar to `boost::recursive_wrapper`, but for this variant type.  
- `#include <safe_variant/static_visitor.hpp>`  
  Similar to `boost::static_visitor`, but for this variant type.
- `#include <safe_variant/variant_compare.hpp>`  
  Gets a template type `variant_comparator`, which is appropriate to use with `std::map` or `std::set`.  
  By default `safe_variant::variant` is not comparable.  
- `#include <safe_variant/variant_hash.hpp>`  
  Makes variant hashable. By default this is not brought in.
- `#include <safe_variant/variant_stream_ops.hpp>`  
  Gets ostream operations for the variant template type.  
  By default `safe_variant::variant` is not streamable.  
- `#include <safe_variant/variant_spirit.hpp>`  
  Defines customization points within `boost::spirit` so that `safe_variant::variant` can be used just like `boost::variant` in your `qi` grammars.

All the library definitions are made within the namespace `safe_variant`.

I guess I recommend you to use a namespace alias for that, e.g. `namespace util = safe_variant;`, or
use a series of using declarations. In another project that uses this library, I did this:


```c++
    // util/variant_fwd.hpp
    
    #include <safe_variant/variant_fwd.hpp>

    namespace util {
      using variant = safe_variant::variant;
    }
```

```c++
    // util/variant.hpp
    #include <safe_variant/variant.hpp>
    #include <safe_variant/static_visitor.hpp>
    #include <safe_variant/recursive_wrapper.hpp>
    #include <safe_variant/variant_hash.hpp>

    namespace util {
      using safe_variant::variant;
      using safe_variant::get;
      using safe_variant::apply_visitor;
      using safe_variant::get_or_default;
    }
```

...

but you should be able to do it however you like of course.


Licensing and Distribution
==========================

**safe variant** is available under the boost software license.

Known issues
============

- There are some issues with `noexcept` correctness which I would like to fix.  
- Currently, you cannot use `apply_visitor` with an rvalue-reference to the variant.  
  It must be an lvalue-reference or a const reference.  
  There is no reason for this restriction, but some of the dispatch code needs to be fixed
  to support this. I didn't need it in my original application.  
  It's okay for the visitor to be an rvalue-reference.  
- You can't use a lamba directly as a visitor. It needs to derive from `safe_variant::static_visitor`.
  This is similar to `boost::variant`. It could be fixed using `std::result_of`, that is a TODO item.
  Since generic lambdas are a C++14 feature anyways, this isn't that big a deal.
- No `constexpr` support. This is really extremely difficult to do in a variant at
  C++11 standard, it's only really feasible in C++14. If you want `constexpr` support
  then I suggest having a look at [`eggs::variant`](https://github.com/eggs-cpp/variant).
