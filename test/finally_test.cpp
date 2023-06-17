/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <unifex/finally.hpp>
#include <unifex/inplace_stop_token.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/then.hpp>
#include <unifex/just.hpp>
#include <unifex/just_done.hpp>
#include <unifex/just_error.hpp>
#include <unifex/let_done.hpp>
#include <unifex/let_error.hpp>

#include <cstdio>
#include <thread>

#include <gtest/gtest.h>

using namespace unifex;

#if 0

TEST(Finally, Value) {
  timed_single_thread_context context;

  auto res = just(42)
    | finally(schedule(context.get_scheduler()))
    | then([](int i){ return std::make_pair(i, std::this_thread::get_id() ); })
    | sync_wait();

  ASSERT_FALSE(!res);
  EXPECT_EQ(res->first, 42);
  EXPECT_EQ(res->second, context.get_thread_id());
}

TEST(Finally, Done) {
  timed_single_thread_context context;

  auto res = just_done()
    | finally(schedule(context.get_scheduler()))
    | let_done([](){ return just(std::this_thread::get_id()); })
    | sync_wait();

  ASSERT_FALSE(!res);
  EXPECT_EQ(*res, context.get_thread_id());
}

TEST(Finally, Error) {
  timed_single_thread_context context;

  auto res = just_error(-1)
    | finally(schedule(context.get_scheduler()))
    | let_error([](auto&&){ return just(std::this_thread::get_id()); })
    | sync_wait();

  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(*res, context.get_thread_id());
}

TEST(Finally, BlockingKind) {
  auto snd1 = finally(just(), just());
  using Snd1 = decltype(snd1);
  static_assert(blocking_kind::always_inline == sender_traits<Snd1>::blocking);

  timed_single_thread_context context;

  auto snd2 = finally(just(), schedule(context.get_scheduler()));
  using Snd2 = decltype(snd2);
  static_assert(blocking_kind::never == sender_traits<Snd2>::blocking);
}
#endif

int global;

template <class T> constexpr auto ref_to_pointer(T&& t) -> T { return std::forward<T>(t); }
template <class T, std::enable_if_t<std::is_lvalue_reference_v<T>>> constexpr auto ref_to_pointer(T& t) -> T { return &t; }

template <class T> constexpr auto pointer_to_ref(T&& t) -> T { return std::forward<T>(t); }
template <class T, std::enable_if_t<std::is_lvalue_reference_v<T>>> constexpr auto pointer_to_ref(std::decay_t<T>* t) -> T { return &t; }

//template <class Sender>
//auto preserve_ref_finally(Sender&& sender) {
//    return then(
//}

TEST(Finally, Ref) {
  auto res = then(just(), []() -> int& { return global; })
    | then([](auto&& v) { return &v; })
    | finally(just())
    | then([](int* v) -> int& { return *v; })
    | sync_wait();

  ASSERT_FALSE(!res);
  EXPECT_EQ(*res, 0);
  global = 10;
  EXPECT_EQ(*res, 10);
}
