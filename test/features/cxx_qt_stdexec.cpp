#include <qthread/qloop.h>
#include <qthread/qthread.h>
#include <stdexec/execution.hpp>
#include <stdio.h>

#include "argparsing.h"

namespace qthreads {
struct qthreads_scheduler {
  constexpr qthreads_scheduler() = default;

  bool operator==(qthreads_scheduler const &rhs) const noexcept { return true; }
  bool operator!=(qthreads_scheduler const &rhs) const noexcept {
    return !(*this == rhs);
  }

  template <typename Receiver>
  struct operation_state {
    [[no_unique_address]] std::decay_t<Receiver> receiver;

    template <typename Receiver_>
    operation_state(Receiver_ &&receiver):
      receiver(std::forward<Receiver_>(receiver)) {}

    operation_state(operation_state &&) = delete;
    operation_state(operation_state const &) = delete;
    operation_state &operator=(operation_state &&) = delete;
    operation_state &operator=(operation_state const &) = delete;

    static aligned_t task(void *arg) noexcept {
      auto *os = static_cast<operation_state *>(arg);
      printf("Hello from qthreads in task! id = %i\n", qthread_id());
      stdexec::set_value(std::move(os->receiver));
      return 0;
    }

    friend void tag_invoke(stdexec::start_t, operation_state &os) noexcept {
      aligned_t ret = 0;
      int r = qthread_fork(&task, &os, &ret);
      qthread_readFF(NULL, &ret);

      if (r != QTHREAD_SUCCESS) {
        stdexec::set_error(std::move(os.receiver), r);
      }
    }
  };

  struct sender {
    using is_sender = void;

    using completion_signatures =
      stdexec::completion_signatures<stdexec::set_value_t(),
                                     stdexec::set_error_t(int)>;

    template <typename Receiver>
    friend operation_state<Receiver>
    tag_invoke(stdexec::connect_t, sender &&s, Receiver &&receiver) {
      return {std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    friend operation_state<Receiver>
    tag_invoke(stdexec::connect_t, sender const &s, Receiver &&receiver) {
      return {std::forward<Receiver>(receiver)};
    }

    struct env {
      friend qthreads_scheduler
      tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                 env const &e) noexcept {
        return {};
      }
    };

    friend env tag_invoke(stdexec::get_env_t, sender const &s) noexcept {
      return {};
    }
  };

  friend sender tag_invoke(stdexec::schedule_t, qthreads_scheduler &&) {
    return {};
  }

  friend sender tag_invoke(stdexec::schedule_t, qthreads_scheduler const &) {
    return {};
  }

  template <typename Sender, typename Shape, typename F>
  struct qthreads_bulk_sender {
    [[no_unique_address]] std::decay_t<Sender> sender;
    [[no_unique_address]] std::decay_t<Shape> shape;
    [[no_unique_address]] std::decay_t<F> f;

    template <typename Sender_, typename Shape_, typename F_>
    qthreads_bulk_sender(Sender_ &&sender, Shape_ &&shape, F_ &&f):
      sender(std::forward<Sender_>(sender)), shape(std::forward<Shape_>(shape)),
      f(std::forward<F_>(f)) {}

    qthreads_bulk_sender(qthreads_bulk_sender &) = default;
    qthreads_bulk_sender(qthreads_bulk_sender const &) = default;
    qthreads_bulk_sender &operator=(qthreads_bulk_sender &) = default;
    qthreads_bulk_sender &operator=(qthreads_bulk_sender const &) = default;

    using is_sender = void;

    using completion_signatures = stdexec::make_completion_signatures<
      Sender,
      stdexec::empty_env,
      stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>>;

    template <typename Receiver>
    struct operation_state {
      struct bulk_receiver {
        using is_receiver = void;

        operation_state *op_state;

        // TODO: tag_invoke is replaced with member functions in P2300R9.
        // tag_invoke is still supported by stdexec.
        template <typename E>
        friend void
        tag_invoke(stdexec::set_error_t, bulk_receiver &&r, E &&e) noexcept {
          stdexec::set_error(std::move(r.op_state->receiver),
                             std::forward<E>(e));
        }

        friend void tag_invoke(stdexec::set_stopped_t,
                               bulk_receiver &&r) noexcept {
          stdexec::set_stopped(std::move(r.op_state->receiver));
        }

        static void
        task(std::size_t startat, std::size_t stopat, void *arg) noexcept {
          auto &f = static_cast<operation_state *>(arg)->f;
          // TODO: Handle exceptions, if f is noexcept(false).
          for (std::size_t i = startat; i < stopat; ++i) {
            // TODO: Pass values sent by predecessor as references.
            f(i);
          }
        }

        template <typename... Ts>
        friend void tag_invoke(stdexec::set_value_t,
                               bulk_receiver &&r,
                               Ts &&...ts) noexcept {
          // TODO: Don't spawn tasks if there is no work to be done? Maybe
          // qt_loop_balance already does that?
          // TODO: Are there other qt_loop_* functions that are better?
          // TODO: Is there a non-blocking version of this where one can attach
          // a continuation? One can also emulate this in task.
          qt_loop_balance(static_cast<std::size_t>(0),
                          static_cast<std::size_t>(r.op_state->shape),
                          &task,
                          r.op_state);
          stdexec::set_value(std::move(r.op_state->receiver),
                             std::forward<Ts>(ts)...);
        }

        friend constexpr stdexec::empty_env
        tag_invoke(stdexec::get_env_t, bulk_receiver const &) noexcept {
          return {};
        }
      };

      using operation_state_type =
        stdexec::connect_result_t<Sender, bulk_receiver>;

      operation_state_type op_state;
      [[no_unique_address]] std::decay_t<Shape> shape;
      [[no_unique_address]] std::decay_t<F> f;
      [[no_unique_address]] std::decay_t<Receiver> receiver;
      // TODO: Store values sent by predecessor.

      template <typename Sender_,
                typename Shape_,
                typename F_,
                typename Receiver_>
      operation_state(Sender_ &&sender,
                      Shape_ &&shape,
                      F_ &&f,
                      Receiver_ &&receiver):
        op_state(
          stdexec::connect(std::forward<Sender_>(sender), bulk_receiver{this})),
        shape(std::forward<Shape_>(shape)), f(std::forward<F_>(f)),
        receiver(std::forward<Receiver_>(receiver)) {}

      friend void tag_invoke(stdexec::start_t, operation_state &os) noexcept {
        stdexec::start(os.op_state);
      }
    };

    template <typename Receiver>
    friend auto tag_invoke(stdexec::connect_t,
                           qthreads_bulk_sender &&s,
                           Receiver &&receiver) {
      return operation_state<std::decay_t<Receiver>>{
        std::move(s.sender),
        std::move(s.shape),
        std::move(s.f),
        std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    friend auto tag_invoke(stdexec::connect_t,
                           qthreads_bulk_sender const &s,
                           Receiver &&receiver) {
      return operation_state<std::decay_t<Receiver>>{
        s.sender, s.shape, s.f, std::forward<Receiver>(receiver)};
    }

    friend constexpr auto tag_invoke(stdexec::get_env_t,
                                     qthreads_bulk_sender const &s) noexcept {
      return stdexec::get_env(s.sender);
    }
  };

  // TODO: This uses the new eager and lazy transform_sender customization
  // mechanism from P2300. It's mostly copy-pasted from stdexec and uses stdexec
  // internals. Remove the internals.
  struct transform_bulk {
    template <class Data, class Sender>
    auto operator()(stdexec::bulk_t, Data &&data, Sender &&sndr) {
      auto [shape, fun] = std::forward<Data>(data);
      return qthreads_bulk_sender<Sender, decltype(shape), decltype(fun)>{
        std::forward<Sender>(sndr), shape, std::move(fun)};
    }
  };

  struct domain {
    template <stdexec::sender_expr_for<stdexec::bulk_t> Sender>
    auto transform_sender(Sender &&sndr) const noexcept {
      return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                    transform_bulk{});
    }

    template <stdexec::sender_expr_for<stdexec::bulk_t> Sender, class Env>
    auto transform_sender(Sender &&sndr, Env const &env) const noexcept {
      if constexpr (stdexec::__completes_on<Sender, qthreads_scheduler>) {
        return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      } else if constexpr (stdexec::
                             __starts_on<Sender, qthreads_scheduler, Env>) {
        return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      }
    }
  };

  friend domain tag_invoke(stdexec::get_domain_t,
                           qthreads_scheduler const &) noexcept {
    return {};
  }
};
} // namespace qthreads

int main(int argc, char **argv) {
  qthread_initialize();

  CHECK_VERBOSE();

  stdexec::sender auto s =
    stdexec::schedule(qthreads::qthreads_scheduler{}) | stdexec::then([] {
      printf("Hello from qthreads in then-functor! id = %i\n", qthread_id());
    }) |
    stdexec::bulk(20, [](int i) {
      printf("Hello from qthreads bulk! i = %i, id = %i\n", i, qthread_id());
    });
  stdexec::sync_wait(std::move(s));

  return 0;
}

/* vim:set expandtab: */
