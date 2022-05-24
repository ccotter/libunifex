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
#include <unifex/async_scope.hpp>
#include <unifex/async_manual_reset_event.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/just_done.hpp>
#include <unifex/just_error.hpp>
#include <unifex/just.hpp>
#include <unifex/sequence.hpp>
#include <unifex/just_void_or_done.hpp>
#include <unifex/let_done.hpp>
#include <unifex/let_error.hpp>
#include <unifex/let_value.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

#include <iostream>
#include <thread>

using namespace unifex;

int main() {

    let_done(
        let_error(
                []() -> task<int> { co_return 5; }(),
            [](auto&&) noexcept {
                return just();
            }
        ) ,[]() noexcept {
            return just();
        }
    );

    return 0;
}
