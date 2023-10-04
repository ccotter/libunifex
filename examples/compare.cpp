#include <stdexec/execution.hpp>
#include <exec/repeat_effect_until.hpp>

#include <unifex/just.hpp>
#include <unifex/let_value.hpp>
#include <unifex/on.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/trampoline_scheduler.hpp>

constexpr int Nops = 100*1000*1000;

struct SR_unifex {
  static constexpr decltype(unifex::just) just;
  static constexpr decltype(unifex::then) then;
  static constexpr decltype(unifex::let_value) let_value;
  static constexpr decltype(unifex::connect) connect;
  static constexpr decltype(unifex::start) start;

  template <class S, class P>
  static auto repeat_effect_until(S&& s, P&& p) {
    return unifex::let_value(unifex::just((S&&)s, (P&&)p, unifex::trampoline_scheduler{}), [](auto&& s, auto&& p, unifex::trampoline_scheduler& sched) {
      //return unifex::repeat_effect_until((decltype(s)&&)s, (decltype(p)&&)p);
      return unifex::repeat_effect_until(unifex::on(sched, ((decltype(s)&&)s)), (decltype(p)&&)p);
    });
  }
};
struct SR_stdexec {
  static constexpr decltype(stdexec::just) just;
  static constexpr decltype(stdexec::then) then;
  static constexpr decltype(stdexec::let_value) let_value;
  static constexpr decltype(stdexec::connect) connect;
  static constexpr decltype(stdexec::start) start;

  template <class S, class P>
  static auto repeat_effect_until(S&& s, P&& p) {
    return exec::repeat_effect_until((S&&)s | stdexec::then(p));
  }
};

template <class Sender, class Waiter>
void bench(Sender&& sndr, Waiter&& w, const char* prefix) {
  auto tstart = std::chrono::system_clock::now();
  std::forward<Waiter>(w)(std::forward<Sender>(sndr));
  //std::this_thread::sleep_for(std::chrono::seconds(1));
  auto tend = std::chrono::system_clock::now();
  std::chrono::duration<double> diff = tend - tstart;                                                                                                                             
  double ns = diff.count() * 1e+9;
  printf("%-25s: took %6.1f ns per op\n", prefix, ns / Nops);
}

template <class SR>
auto check_repeat_effect_until() {
  return SR::let_value(SR::just(0), [](int& c) {
      return SR::repeat_effect_until(
          SR::just()
            | SR::let_value([](auto&&...) { return SR::just(); }),
        [&c]() { ++c; return c > Nops; }
      );
  });
}

struct my_receiver {
  using is_receiver = void;

  template <class Tag, class Self, class... Ts>
  friend auto tag_invoke(Tag&&, Self&& self, Ts&&...) noexcept {
    *self.val = sizeof...(Ts);
  }

  friend auto tag_invoke(stdexec::get_env_t, const my_receiver&) noexcept {
    return stdexec::empty_env{};
  }

  int* val;
};

template <class SR>
auto check_connect() {
  int x{};
  my_receiver r{&x};
  auto op = SR::connect(SR::just(1,2,3,4), r);
  SR::start(op);
  return x;
}

void doit() {
  printf("connect: %d\n", check_connect<SR_stdexec>());
  printf("connect: %d\n", check_connect<SR_unifex>());
}

int main() {
  bench(check_repeat_effect_until<SR_stdexec>(), stdexec::sync_wait, "reu stdexec");
  bench(check_repeat_effect_until<SR_unifex>(), unifex::sync_wait, "reu unifex");

  doit();
  return 0;
}
