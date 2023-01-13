#include <unifex/split.hpp>

#include <unifex/just.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

#include <gtest/gtest.h>

using namespace unifex;

TEST(Split, convert_single_shot_to_multi_shot) {
    struct move_only_type {
        move_only_type(int v) : val(v) { }
        move_only_type(move_only_type&&) = default;
        int val;
    };

    int called = 0;

    auto sender = unifex::just(move_only_type(10)) |
        unifex::then([&](move_only_type mot) { ++called; return mot; }) |
        split();
    auto then1 = sender | unifex::then([](const move_only_type& mot) { return mot.val*2; });
    auto then2 = sender | unifex::then([](const move_only_type& mot) { return mot.val*3; });

    EXPECT_EQ(20, unifex::sync_wait(then1));
    EXPECT_EQ(30, unifex::sync_wait(then2));
    EXPECT_EQ(1, called);
}

TEST(Split, when_all_split) {
    auto sndr = unifex::just(0) | split();
    auto result = unifex::sync_wait(unifex::when_all(
        sndr,
        sndr | unifex::then([](int) { return 100; }))
    );
    EXPECT_TRUE(result.has_value());
    auto [v1, v2] = result.value();
    EXPECT_EQ(0, std::get<0>(std::get<0>(v1)));
    EXPECT_EQ(100, std::get<0>(std::get<0>(v2)));
}
