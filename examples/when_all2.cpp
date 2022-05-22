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
#include <iostream>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/just.hpp>
#include <unifex/let_value.hpp>
#include <unifex/task.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

#include <chrono>
#include <iostream>

using namespace unifex;
using namespace std::chrono;
using namespace std::chrono_literals;

int main() {

    {
        std::cout << "when_all2 on just()\n";
        std::vector<decltype(just(5.))> vs;
        vs.push_back(just(5.));
        vs.push_back(just(10.));
        vs.push_back(just(15.1));
        vs.push_back(just(20.1));
        auto wa = when_all2(vs.begin(), vs.end());
        auto res = sync_wait(std::move(wa));

        auto x = res.value();

        for (auto&& e: res.value()) {
            std::cout << "e=" << std::get<0>(std::get<0>(e)) << "\n";
        }
    }
    {
        std::cout << "\nwhen_all2 on task\n";
        std::vector<task<int>> vs;
        vs.push_back([]() -> task<int> { co_return 1; }());
        vs.push_back([]() -> task<int> { co_return 2; }());
        vs.push_back([]() -> task<int> { co_return 4; }());
        vs.push_back([]() -> task<int> { co_return 8; }());
        auto wa = when_all2(vs.begin(), vs.end());
        auto res = sync_wait(std::move(wa));

        auto x = res.value();

        for (auto&& e: res.value()) {
            std::cout << "e=" << std::get<0>(std::get<0>(e)) << "\n";
        }
    }

    std::cout << "all done\n";

  return 0;
}
