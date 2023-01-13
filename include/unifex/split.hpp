/*
 * Copyright (c) NVIDIA
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

#pragma once

//#include <unifex/bind_back.hpp>
#include <unifex/blocking.hpp>
#include <unifex/config.hpp>
#include <unifex/inplace_stop_token.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/get_stop_token.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/std_concepts.hpp>
#include <unifex/bind_back.hpp>

#include <atomic>
#include <exception>
#include <memory>
#include <tuple>
#include <type_traits>
#include <variant>
#include <utility>

#include <unifex/detail/prologue.hpp>

namespace unifex {

/////////////////////////////////////////////////////////////////////////////
// [execution.senders.adaptors.split]
namespace __split {

  using set_value_t = std::remove_cv_t<decltype(set_value)>;
  using set_error_t = std::remove_cv_t<decltype(set_error)>;
  using set_stopped_t = std::remove_cv_t<decltype(set_done)>;
  using get_stop_token_t = decltype(get_stop_token);
  using in_place_stop_token = inplace_stop_token;
  using in_place_stop_source = inplace_stop_source;

  template <class _T, class... _As>
    UNIFEX_CONCEPT one_of =
      (same_as<_T, _As> ||...);

  template <class T>
  using __t = typename T::type;

  template <class _T>
    struct __x_ {
      struct __t {
        using type = _T;
      };
    };
  template <class _T>
    using __x = typename __x_<_T>::__t;

  template<class _T, class _U>
    UNIFEX_CONCEPT _decays_to =
    same_as<std::decay_t<_T>, _U>;


  template <class _SharedState>
    class __receiver {
      _SharedState &__sh_state_;

    template <class... Values>
    using decayed_tuple_t = std::tuple<std::decay_t<Values>...>;

    public:
      template (class _Tag, class... _As)
        (requires one_of<_Tag, set_value_t, set_error_t, set_stopped_t>)
      friend void tag_invoke(_Tag __tag, __receiver&& __self, _As&&... __as) noexcept {
        _SharedState &__state = __self.__sh_state_;

        try {
          using __tuple_t = decayed_tuple_t<_Tag, _As...>;
          __state.__data_.template emplace<__tuple_t>(__tag, (_As &&) __as...);
        } catch (...) {
          using __tuple_t = decayed_tuple_t<set_error_t, std::exception_ptr>;
          __state.__data_.template emplace<__tuple_t>(set_error, std::current_exception());
        }

        __state.__notify();
      }

      explicit __receiver(_SharedState &__sh_state) noexcept
        : __sh_state_(__sh_state) {
      }
    };

  struct __operation_base {
    using __notify_fn = void(__operation_base*) noexcept;

    __operation_base * __next_{};
    __notify_fn* __notify_{};
  };

  template <class _SenderId>
    struct __sh_state {
      using _Sender = __t<_SenderId>;

      template <typename... Values>
        using decayed_value_tuple = type_list<std::tuple<set_value_t, std::decay_t<Values>...>>;

      template <typename... Errors>
        using unique_decayed_error_types = concat_type_lists_unique_t<
          type_list<std::tuple<set_error_t, std::decay_t<Errors>>...>>;


      using datas_t = typename concat_type_lists_unique_t<
        type_list<std::tuple<set_stopped_t>>,
        sender_value_types_t<_Sender, concat_type_lists_unique_t, decayed_value_tuple>,
        sender_error_types_t<_Sender, unique_decayed_error_types>
        >::template apply<std::variant>;

      using __receiver_ = __receiver<__sh_state>;

      _Sender __sndr_;
      in_place_stop_source __stop_source_{};
      connect_result_t<_Sender, __receiver_> __op_state2_;
      datas_t __data_;
      std::atomic<void*> __head_;

      explicit __sh_state(_Sender&& __sndr)
        : __sndr_((_Sender&&)__sndr)
        , __op_state2_(connect((_Sender&&) __sndr_, __receiver_{*this}))
        , __head_{nullptr}
      {}

      void __notify() noexcept {
        void* const __completion_state = static_cast<void*>(this);
        void *__old = __head_.exchange(__completion_state, std::memory_order_acq_rel);
        __operation_base *_op_state = static_cast<__operation_base*>(__old);

        while(_op_state != nullptr) {
          __operation_base *__next = _op_state->__next_;
          _op_state->__notify_(_op_state);
          _op_state = __next;
        }
      }
    };

  template <class _SenderId, class _ReceiverId>
    class __operation : public __operation_base {
      using _Sender = __t<_SenderId>;
      using _Receiver = __t<_ReceiverId>;

      struct __on_stop_requested {
        in_place_stop_source& __stop_source_;
        void operator()() noexcept {
          __stop_source_.request_stop();
        }
      };
      //using __on_stop = optional<typename stop_token_of_t<
      //    env_of_t<_Receiver> &>::template callback_type<__on_stop_requested>>;

      _Receiver __recvr_;
      //__on_stop __on_stop_{};
      std::shared_ptr<__sh_state<_SenderId>> __shared_state_;

    public:
      __operation(_Receiver&& __rcvr,
                  std::shared_ptr<__sh_state<_SenderId>> __shared_state)
          noexcept(std::is_nothrow_move_constructible_v<_Receiver>)
        : __operation_base{nullptr, __notify}
        , __recvr_((_Receiver&&)__rcvr)
        , __shared_state_(move(__shared_state)) {
      }
      __operation(__operation&&) = delete;

      static void __notify(__operation_base* __self) noexcept {
        __operation *__op = static_cast<__operation*>(__self);
        //__op->__on_stop_.reset();

        std::visit([&](const auto& __tupl) noexcept -> void {
          std::apply([&](auto __tag, const auto&... __args) noexcept -> void {
            __tag((_Receiver&&) __op->__recvr_, __args...);
          }, __tupl);
        }, __op->__shared_state_->__data_);
      }

      friend void tag_invoke(tag_t<start>, __operation& __self) noexcept {
        __sh_state<_SenderId>* __shared_state = __self.__shared_state_.get();
        std::atomic<void*>& __head = __shared_state->__head_;
        void* const __completion_state = static_cast<void*>(__shared_state);
        void* __old = __head.load(std::memory_order_acquire);

        if (__old != __completion_state) {
          //__self.__on_stop_.emplace(
          //    get_stop_token(get_env(__self.__recvr_)),
          //    __on_stop_requested{__shared_state->__stop_source_});
        }

        do {
          if (__old == __completion_state) {
            __self.__notify(&__self);
            return;
          }
          __self.__next_ = static_cast<__operation_base*>(__old);
        } while (!__head.compare_exchange_weak(
            __old, static_cast<void *>(&__self),
            std::memory_order_release,
            std::memory_order_acquire));

        if (__old == nullptr) {
            start(__shared_state->__op_state2_);
        }
      }
    };

  template <class _SenderId>
    class __sender {
      using _Sender = __t<_SenderId>;
      using __sh_state_ = __sh_state<_SenderId>;
      template <class _Receiver>
        using __operation = __operation<_SenderId, __x<remove_cvref_t<_Receiver>>>;

      std::shared_ptr<__sh_state_> __shared_state_;

    public:
      template (class _Self, class _Receiver)
        (requires _decays_to<_Self, __sender> AND receiver<_Receiver>)
          //requires receiver_of<_Receiver, completion_signatures_of_t<_Self, __empty_env>>
        friend auto tag_invoke(tag_t<connect>, _Self&& __self, _Receiver&& __recvr)
          noexcept(std::is_nothrow_constructible_v<std::decay_t<_Receiver>, _Receiver>)
          -> __operation<_Receiver> {
          return __operation<_Receiver>{(_Receiver &&) __recvr,
                                        __self.__shared_state_};
        }

      template <
          template <typename...> class Variant,
          template <typename...> class Tuple>
        using value_types = sender_value_types_t<_Sender, Variant, Tuple>;
      template <template <typename...> class Variant>
        using error_types = sender_error_types_t<_Sender, Variant>;

      static constexpr bool sends_done = _Sender::sends_done;

#if 0
      template <__sender_queries::__sender_query _Tag, class... _As>
          requires (!__is_instance_of<_Tag, get_completion_scheduler_t>) &&
            __callable<_Tag, const _Sender&, _As...>
        friend auto tag_invoke(_Tag __tag, const __sender& __self, _As&&... __as)
          noexcept(__nothrow_callable<_Tag, const _Sender&, _As...>)
          -> __call_result_if_t<__sender_queries::__sender_query<_Tag>, _Tag, const _Sender&, _As...> {
          return ((_Tag&&) __tag)(__self.__sndr_, (_As&&) __as...);
        }
#endif

      explicit __sender(_Sender __sndr)
          : __shared_state_{std::make_shared<__sh_state_>((_Sender&&)__sndr)}
      {}
    };

  struct split_t {

    template <class _Sender>
      using __sender = __sender<__x<remove_cvref_t<_Sender>>>;

    template <typename Sender>
    using _result_t =
      typename std::conditional_t<
        tag_invocable<split_t, Sender>,
        meta_tag_invoke_result<split_t>,
        meta_quote1<__sender>>::template apply<Sender>;

    template(typename Sender)
      (requires (tag_invocable<split_t, Sender>))
    auto operator()(Sender&& sender) const
        noexcept(is_nothrow_tag_invocable_v<split_t, Sender>)
        -> _result_t<Sender> {
      return tag_invoke(split_t{}, (Sender&&)sender);
    }

    template(typename Sender)
      (requires (!tag_invocable<split_t, Sender>))
    auto operator()(Sender&& sender) const
        noexcept(std::is_nothrow_constructible_v<
          __sender<Sender>, Sender>)
        -> _result_t<Sender> {
      return __sender<Sender>{(Sender&&) sender};
    }

    constexpr auto operator()() const {
      return bind_back(*this);
    }
  };
}
using __split::split_t;
inline constexpr split_t split{};

} // namespace unifex

#include <unifex/detail/epilogue.hpp>
