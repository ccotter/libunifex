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

    auto makeTasks = []() {
        std::vector<task<int>> tasks;
        tasks.push_back([]() -> task<int> { co_return 2; }());
        tasks.push_back([]() -> task<int> { co_return 4; }());
        tasks.push_back([]() -> task<int> { co_return 6; }());
        return tasks;
    };
    auto makeTasksWithDone = []() {
        std::vector<task<int>> tasks;
        tasks.push_back([]() -> task<int> { co_return 2; }());
        tasks.push_back([]() -> task<int> { co_await just_done(); co_return 4; }());
        tasks.push_back([]() -> task<int> { co_return 6; }());
        return tasks;
    };
    auto makeTasksWithException = []() {
        std::vector<task<int>> tasks;
        tasks.push_back([]() -> task<int> { co_return 2; }());
        tasks.push_back([]() -> task<int> { throw std::runtime_error("Oops"); co_return 4; }());
        tasks.push_back([]() -> task<int> { co_return 6; }());
        return tasks;
    };

    auto when_all_iterator = [](auto first, auto last) {
        using Iterator = decltype(first);
        struct Data {
            Data(Iterator first, Iterator last)
                : first(first)
                , last(last)
                , n(std::distance(first, last))
                , counter{n}
                , resultStorage(n)
            { }

            Iterator first, last;
            size_t n;
            std::atomic<size_t> counter;
            std::vector<std::optional<int>> resultStorage;
            async_manual_reset_event evt;
            std::optional<std::exception_ptr> error;
            std::atomic<bool> doneOrError{false};
            async_scope scope;
        };

        return let_value_with([first, last]() {
                return Data(first, last);
            }, [](Data& data) {

            auto signal = [&data]() {
                if (data.counter.fetch_sub(1) == 1) {
                    data.evt.set();
                }
                assert(data.counter >= 0);
            };

            size_t i = 0;
            for (auto itr = data.first; itr != data.last; ++i, ++itr) {
                data.scope.spawn(
                    let_done(
                        let_error(
                            let_value(
                                std::move(*itr),
                                [signal, &data, i](auto&& result) {
                                    try {
                                        data.resultStorage[i].emplace(std::move(result));
                                        signal();
                                        return just();
                                    } catch (...) {
                                        //return just_error(std::current_exception());
                                        return just();
                                    }
                                }
                            ),
                            [signal, &data](auto&& e) noexcept {
                                if (!data.doneOrError.exchange(true, std::memory_order_relaxed)) {
                                    data.error.emplace(e);
                                    // TODO - request stop
                                }
                                signal();
                                return just();
                            }
                        ) ,[signal, &data]() noexcept {
                            if (!data.doneOrError.exchange(true, std::memory_order_relaxed)) {
                                // TODO - request stop
                            }
                            signal();
                            return just();
                        }
                    )
                );
            }

            return sequence(
                data.evt.async_wait(),
                let_value(
                    let_value(data.scope.cleanup(), [&data]() {
                        bool should_send_done =
                            data.doneOrError.load(std::memory_order_relaxed) && !data.error.has_value();
                        return just_void_or_done(!should_send_done);
                    }),
                    [&data]() {
                        if (data.doneOrError.load(std::memory_order_relaxed)) {
                            if (data.error.has_value()) {
                                std::rethrow_exception(data.error.value());
                            } else {
                                assert(!"Done result not expected here");
                            }
                        } else {
                            std::vector<int> results;
                            results.reserve(data.n);
                            std::transform(
                                data.resultStorage.begin(),
                                data.resultStorage.end(),
                                std::back_inserter(results),
                                [](auto&& e) { return std::move(e.value()); });
                            return just(std::move(results));
                        }
                })
            );
        });
    };

    {
        auto tasks = makeTasks();
        auto results = sync_wait(when_all_iterator(tasks.begin(), tasks.end()));
        assert(results.has_value());
        assert((*results)[0] == 2);
        assert((*results)[1] == 4);
        assert((*results)[2] == 6);
        std::cout << "All values returned as expected\n";
    }
    {
        auto tasks = makeTasksWithDone();
        auto results = sync_wait(when_all_iterator(tasks.begin(), tasks.end()));
        assert(!results.has_value());
        std::cout << "None result returned as expected\n";
    }
    {
        auto tasks = makeTasksWithException();
        try {
            sync_wait(when_all_iterator(tasks.begin(), tasks.end()));
            assert(!"Exception should have been caught");
        } catch (const std::runtime_error& e) {
            std::cout << "Caught exception as expected\n";
        }
    }

    return 0;
}
