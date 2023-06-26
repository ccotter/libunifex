#include <unifex/sender_concepts.hpp>

#include <gtest/gtest.h>

using namespace unifex;

struct my_receiver {
  template <class... Ts>
  void set_value(Ts&&...) noexcept {}
  void set_error(std::exception_ptr) noexcept {}
  void set_done() noexcept {}
};

struct bad_sender {

  template <
    template <typename...> class Variant,
    template <typename...> class Tuple>
  using value_types = Variant<Tuple<>>;

  template <template <typename...> class Variant>
  using error_types = Variant<>;

  static constexpr bool sends_done = false;

  template <class Receiver>
  struct operation {
    friend auto tag_invoke(tag_t<start>, operation& self) noexcept {
      set_value(std::move(self.receiver), 0);
    }

    Receiver receiver;
  };

  template <class Receiver>
  friend auto tag_invoke(tag_t<connect>, bad_sender, Receiver&& receiver) {
    return operation<Receiver>{std::forward<Receiver>(receiver)};
  }
};

TEST(bad_sender, sends_wrong_value) {
  auto op = connect(bad_sender{}, my_receiver{});
  start(op);
}
