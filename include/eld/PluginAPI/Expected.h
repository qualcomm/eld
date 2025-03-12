//===- Expected.h----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_PLUGINAPI_EXPECTED_H
#define ELD_PLUGINAPI_EXPECTED_H
#include "DiagnosticEntry.h"
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace eld {

/// Expected at any given time holds either an \em expected value of type
/// T, or an \em unexpected to type E. \em Expected value is commonly called
/// just value, and \em unexpected value is commonly called error.
/// Expected is never valueless.
///
/// Expected is based on std::expected (C++-23).
///
/// Templates T and U have some constraints:
/// - T and E must not be reference types.
/// - E must not be a void type.
///
// clang-format off
/// eld::Expected<T, U> special member functions behavior and availability are
/// described below:
/// - <b>Default constructor</b>: eld::Expected<T, U> is default constructible
///   only if `std::is_default_constructible<T>::value` is true. In all other cases,
///   eld::Expected<T, U> is marked as delete.
///
///   Default initialized Expected contains a value-initialized expected
///   value. Calling `has_value()` on a default initialized Expected
///   returns true.
///
/// - <b>Copy constructor</b>: eld::Expected<T, U> is copy constructible only
///   if `std::is_copy_constructible<T>::value` and
///   `std::is_copy_constructible<U>::value` are true. In all other cases,
///   eld::Expected<T, U> copy constructor is marked as delete.
///
/// - <b>Copy assignment</b>: Expected is copy assignable only if all of
///   the below constraints are true:
///   - `std::is_copy_assignable<T>::value` is true.
///   - `std::is_copy_assignable<U>::value` is true.
///   - `std::is_copy_constructible<T>::value` is true.
///   - `std::is_copy_constructible<U>::value` is true.
///
///   In all other cases, eld::Expected<T, U> copy assignment operator is
///   marked as delete.
///
/// - <b>Move Constructor</b>: Expected is move constructible only if
///   `std::is_move_constructible<T>::value` and
///   `std::is_move_constructible<U>::value` are true. In all other cases,
///   Expected move constructor is marked as delete.
///
/// - <b>Move assignment</> eld::Expected<T, U> is move assignable only if all
///   of the below constraints are true:
///   - `std::is_move_assignable<T>::value` is true.
///   - `std::is_move_assignable<U>::value` is true.
///   - `std::is_move_constructible<T>::value` is true.
///   - `std::is_move_constructible<U>::value` is true.
///
///   In all other cases, eld::Expected<T, U> move assignment operator is
///   marked as delete.
// clang-format on
template <class T, class U = std::unique_ptr<plugin::DiagnosticEntry>>
class Expected {
public:
  Expected() = default;
  Expected(const Expected &) = default;
  Expected(Expected &&) noexcept = default;
  Expected &operator=(const Expected &) = default;
  Expected &operator=(Expected &&) noexcept = default;

  template <class V, std::enable_if_t<std::is_convertible_v<V, T>> * = nullptr>
  Expected(V &&other)
      : m_Data(std::in_place_type_t<T>(), std::forward<V>(other)) {}

  template <class V, std::enable_if_t<std::is_convertible_v<V, U>> * = nullptr>
  Expected(V &&other)
      : m_Data(std::in_place_type_t<U>(), std::forward<V>(other)) {}

  /// Returns the expected value.
  constexpr T &value() & { return std::get<T>(m_Data); }
  /// Returns the expected value.
  constexpr const T &value() const & { return std::get<T>(m_Data); }
  /// Returns the expected value.
  constexpr T &&value() && { return std::move(std::get<T>(m_Data)); }
  /// Returns the expected value.
  constexpr const T &&value() const && {
    return std::move(std::get<T>(m_Data));
  }

  /// Returns the unexpected value.
  constexpr U &error() & { return std::get<U>(m_Data); }
  /// Returns the unexpected value.
  constexpr const U &error() const & { return std::get<U>(m_Data); }
  /// Returns the unexpected value.
  constexpr U &&error() && { return std::move(std::get<U>(m_Data)); }
  /// Returns the unexpected value.
  constexpr const U &&error() const && {
    return std::move(std::get<U>(m_Data));
  }

  /// Returns true if the object contains expected value.
  constexpr explicit operator bool() const noexcept { return has_value(); }

  /// Returns true if the object contains expected value.
  constexpr bool has_value() const noexcept { return m_Data.index() == 0; }

  bool operator==(const Expected &other) const {
    return m_Data == other.m_Data;
  }

  bool operator!=(const Expected &other) const { return !(*this == other); }

  /// Overload -> to allow simplified usage.
  /// Returns the expected value.
  constexpr const T *operator->() const { return &std::get<T>(m_Data); }

  constexpr T *operator->() { return &std::get<T>(m_Data); }

  /// Returns a reference to the stored value
  constexpr T &operator*() { return std::get<T>(m_Data); }

  /// Returns a const reference to the stored value
  constexpr const T &operator*() const { return std::get<T>(m_Data); }

private:
  std::variant<T, U> m_Data;
};

template <class U> class Expected<void, U> {
public:
  Expected() = default;
  Expected(const Expected &) = default;
  Expected(Expected &&) noexcept = default;
  Expected &operator=(const Expected &) = default;
  Expected &operator=(Expected &&) noexcept = default;

  template <class V, std::enable_if_t<std::is_convertible_v<V, U>> * = nullptr>
  Expected(V &&other) : m_Error(std::forward<V>(other)) {}

  constexpr void value() {};

  /// Returns the unexpected value.
  constexpr U &error() & { return m_Error.value(); }
  /// Returns the unexpected value.
  constexpr const U &error() const & { return m_Error.value(); }
  /// Returns the unexpected value.
  constexpr U &&error() && { return std::move(m_Error.value()); }
  /// Returns the unexpected value.
  constexpr const U &&error() const && { return std::move(m_Error.value()); }

  /// Returns true if the object contains expected value.
  constexpr explicit operator bool() const noexcept { return has_value(); }

  /// Returns true if the object contains expected value.
  constexpr bool has_value() const noexcept { return !m_Error.has_value(); }

  bool operator==(const Expected &other) const {
    return m_Error == other.m_Error;
  }

  bool operator!=(const Expected &other) const { return !(*this == other); }

private:
  std::optional<U> m_Error;
};

namespace plugin {
using eld::Expected;
}
} // namespace eld

#endif
