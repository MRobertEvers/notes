#include <experimental/coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>

// for gcc, enable -fcoroutines and change to std::
namespace async = std::experimental;
 
auto switch_to_new_thread(std::thread& out) {
  struct awaitable {
    std::thread* p_out;
    bool await_ready() { return false; }
    void await_suspend(async::coroutine_handle<> h) {
      std::thread& out = *p_out;
      if (out.joinable())
        throw std::runtime_error("Output jthread parameter not empty");
      out = std::thread([ &h] () { h.resume(); });
      // Potential undefined behavior: accessing potentially destroyed *this
      // std::cout << "New thread ID: " << p_out->get_id() << '\n';
      std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
    }
    void await_resume() {}
  };
  return awaitable{&out};
}
 
struct task{
  struct promise_type {
    task get_return_object() { return {}; }
    async::suspend_never initial_suspend() { return {}; }
    async::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};
 
task resuming_on_new_thread(std::thread& out) {
  std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
  co_await switch_to_new_thread(out);
  // awaiter destroyed here
  std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}
 
int main() {
  std::thread out;
  resuming_on_new_thread(out);
}