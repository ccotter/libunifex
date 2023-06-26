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
#include <unifex/stop_when.hpp>

#include <unifex/just_from.hpp>
#include <unifex/let_value_with_stop_source.hpp>
#include <unifex/never.hpp>
#include <unifex/on.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/when_all.hpp>

#if !UNIFEX_NO_COROUTINES
#include <unifex/task.hpp>
#endif

#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#if !UNIFEX_NO_COROUTINES
TEST(StopWhen, SynchronousCompletionFromReceiverStopRequestIsASANSafe) {
  unifex::sync_wait(
      unifex::let_value_with_stop_source([](auto& stopSource) noexcept {
        return unifex::when_all(
            []() -> unifex::task<void> {
              co_await unifex::stop_when(
                  unifex::never_sender{}, unifex::never_sender{});
            }(),
            unifex::just_from([&]() noexcept { stopSource.request_stop(); }));
      }));
}
#endif
