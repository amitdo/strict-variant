//  (C) Copyright 2016 Christopher Beck

//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

/***
 * Some metafunctions that support safely_constructible type_trait
 *
 * Permits to classify all fundamental arithmetic types
     integral, character, wide_char, bool, floating point
 *
 * and assign numeric ranks to them which are *portable*.
 *
 * These ranks should be the same on any standards-conforming implementation of
 * C++, if not, it is a bug in this header.
 */

#include <type_traits>

//[ strict_variant_arithmetic_category
namespace strict_variant {
namespace mpl {

enum class arithmetic_category : char { integer, character, wide_char, boolean, floating };

template <typename T, typename ENABLE = void>
struct classify_arithmetic;

} // end namespace mpl
} // end namespace strict_variant
//]

namespace strict_variant {
namespace mpl {

template <typename T>
struct classify_arithmetic<T, typename std::enable_if<std::is_integral<T>::value>::type> {
  static constexpr arithmetic_category value = arithmetic_category::integer;
};

#define CLASSIFY(T, C)                                                                             \
  template <>                                                                                      \
  struct classify_arithmetic<T, void> {                                                               \
    static constexpr arithmetic_category value = arithmetic_category::C;                                       \
  }

CLASSIFY(char, character);
CLASSIFY(signed char, character);
CLASSIFY(unsigned char, character);
CLASSIFY(char16_t, character);
CLASSIFY(char32_t, character);
CLASSIFY(wchar_t, wide_char);
CLASSIFY(bool, boolean);
CLASSIFY(float, floating);
CLASSIFY(double, floating);
CLASSIFY(long double, floating);

#undef CLASSIFY

template <typename T>
struct arithmetic_rank;

#define RANK(T, V)                                                                                 \
  template <>                                                                                      \
  struct arithmetic_rank<T> {                                                                      \
    static constexpr int value = V;                                                                \
  }

#define URANK(T, V)                                                                                \
  RANK(T, V);                                                                                      \
  RANK(unsigned T, V)

RANK(bool, 0);

RANK(signed char, 0);
URANK(char, 1);
RANK(char16_t, 2);
RANK(char32_t, 3);

URANK(short, 0);
URANK(int, 1);
URANK(long, 2);
URANK(long long, 3);

RANK(float, 0);
RANK(double, 1);
RANK(long double, 2);

#undef RANK

/***
 * As a hack, we consider `char` always to be signed, regardless of implementation.
 * This is why `signed char` is simply one lower rank than `char` and `unsigned char`.
 */
template <typename T>
struct is_unsigned : std::is_unsigned<T> {};

template <>
struct is_unsigned<char> : std::false_type {};

/***
 * Compare two types to see if they are same class, sign and rank A >= rank B,
 * or it is a signed -> unsigned conversion of same rank
 *
 * Rationale:
 * For two integral types, signed -> unsigned is allowed if they have the same
 * rank.
 *
 * This conversion is well-defined by the standard to be a no-op for most
 * implementations.
 * (Since it means there is no truncation.)
 * [conv.integral][2]
 *
 * For things like signed char -> unsigned int, we don't allow it, since it's
 * potentially confusing that this won't be the same as
 * signed char -> unsigned char -> unsigned int.
 */
template <typename A, typename B>
struct safe_by_rank {
  static constexpr bool same_class =
    (mpl::classify_arithmetic<A>::value == mpl::classify_arithmetic<B>::value);
  static constexpr bool unsign_to_sign = !mpl::is_unsigned<A>::value && mpl::is_unsigned<B>::value;

  static constexpr int ra = mpl::arithmetic_rank<A>::value;
  static constexpr int rb = mpl::arithmetic_rank<B>::value;

  static constexpr bool value = same_class && !unsign_to_sign && (ra >= rb);
};

} // end namespace mpl
} // end namespace strict_variant