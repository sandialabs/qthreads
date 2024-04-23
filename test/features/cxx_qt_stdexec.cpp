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
      qthreads_scheduler
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
};
} // namespace qthreads

int main(int argc, char **argv) {
  qthread_initialize();

  CHECK_VERBOSE();

  stdexec::sender auto s =
    stdexec::schedule(qthreads::qthreads_scheduler{}) | stdexec::then([] {
      printf("Hello from qthreads in then-functor! id = %i\n", qthread_id());
    });
  stdexec::sync_wait(std::move(s));

  return 0;
}

/* vim:set expandtab: */
