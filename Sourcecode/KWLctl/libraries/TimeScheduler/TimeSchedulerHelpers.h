/*
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

/*!
 * @file
 * @brief Internal stuff for scheduler for cooperative multitasking.
 */
#pragma once

// Helpers for C++11 - here to make it self-contained.
namespace scheduler_cpp11_support
{
  template<typename T> struct remove_reference { using type = T; };
  template<typename T> struct remove_reference<T&> { using type = T; };
  template<typename T> struct remove_reference<T&&> { using type = T; };

  template<typename T> constexpr typename remove_reference<T>::type&& move(T&& arg) noexcept { return static_cast<typename remove_reference<T>::type&&>(arg); }
  template<typename T> constexpr T&& forward(typename remove_reference<T>::type& t) noexcept { return static_cast<T&&>(t); }
  template<typename T> constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept { return static_cast<T&&>(t); }

  template<bool B, typename T = void> struct enable_if {};
  template<typename T> struct enable_if<true, T> { using type = T; };
}

// Helpers for scheduler implementation - not on API.
namespace SchedulerImpl
{
  template<typename... Args>
  class callable_tuple;

  template<>
  class callable_tuple<>
  {
  public:
    inline callable_tuple() noexcept {}

    template<typename Func>
    inline void call(Func&& f) noexcept { f(); }
  };

  template<typename T, typename... Args>
  class callable_tuple<T, Args...>
  {
  public:
    inline callable_tuple(T&& value, Args&&... args) noexcept :
      value_(scheduler_cpp11_support::forward<T>(value)),
      nested_(scheduler_cpp11_support::forward<Args>(args)...)
    {}

    template<typename Func>
    inline void call(Func&& f) noexcept
    {
      auto& v = value_;
      nested_.call([f, &v](Args&&... args) noexcept {
        f(scheduler_cpp11_support::forward<T>(v), scheduler_cpp11_support::forward<Args>(args)...);
      });
    }

  private:
    T value_;
    callable_tuple<Args...> nested_;
  };

  template<typename T>
  struct has_method
  {
    template<typename, typename> struct checker;
    template<typename C> static bool test(checker<C, void (C::*)()>*);
    template<typename C> static int test(...);
    static constexpr bool value = sizeof(decltype(test<T>(nullptr))) == 1;
  };

  template<typename Enable, typename... Args>
  class call_invoker_impl;

  template<typename T, typename... Args>
  class call_invoker_impl<typename scheduler_cpp11_support::enable_if<has_method<T>::value>::type, T, Args...>
  {
  public:
    using invoker_type = void (T::*)(Args...);

    inline call_invoker_impl(invoker_type method, T& instance, Args&&... args) noexcept :
      invoker_(method), instance_(instance), call_(scheduler_cpp11_support::forward<Args>(args)...)
    {}

    inline void invoke() noexcept {
      call_.call([this](Args&&... args) noexcept {
        (instance_.*invoker_)(scheduler_cpp11_support::forward<Args>(args)...);
      });
    }

  private:
    invoker_type invoker_;
    T& instance_;
    callable_tuple<Args...> call_;
  };

  template<typename T, typename... Args>
  class call_invoker_impl<typename scheduler_cpp11_support::enable_if<!has_method<T>::value>::type, T, Args...>
  {
  public:
    using invoker_type = void (*)(T, Args...);

    inline call_invoker_impl(invoker_type invoker, T&& arg0, Args&&... args) noexcept :
      invoker_(invoker),
      call_(scheduler_cpp11_support::forward<T>(arg0), scheduler_cpp11_support::forward<Args>(args)...)
    {}

  protected:
    inline void invoke() noexcept {
      call_.call([this](T&& arg0, Args&&... args) noexcept {
        invoker_(scheduler_cpp11_support::forward<T>(arg0), scheduler_cpp11_support::forward<Args>(args)...);
      });
    }

  private:
    invoker_type invoker_;
    callable_tuple<T, Args...> call_;
  };

  template<>
  class call_invoker_impl<void>
  {
  public:
    using invoker_type = void (*)();

    inline call_invoker_impl(invoker_type invoker) noexcept : invoker_(invoker) {}

  protected:
    inline void invoke() noexcept { invoker_(); }

  private:
    invoker_type invoker_;
  };

  template<typename... Args>
  class call_invoker : private call_invoker_impl<void, Args...>
  {
  public:
    using invoker_type = typename call_invoker_impl<void, Args...>::invoker_type;

    template<typename... CArgs>
    inline call_invoker(CArgs&&... args) noexcept :
      call_invoker_impl<void, Args...>(scheduler_cpp11_support::forward<CArgs>(args)...)
    {}

    void invoke() { this->call_invoker_impl<void, Args...>::invoke(); }
  };
}
