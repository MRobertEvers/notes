## Building

`clang main.cpp --std=c++20 -lstdc++  `

## Assembly analysis

The following program compiled with gcc and `-fcoroutines`,

```cpp
#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>

// for gcc, enable -fcoroutines and change to std::
namespace async = std;
 
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
```

Produces the following assembly with `-Oz`

```
        ldr     r3, [r0, #4]
        ldr     r0, [r3]
        ldr     r3, [r0]
        bx      r3  @ indirect register sibling call
switch_to_new_thread(std::thread&):
        bx      lr
.LC0:
        .ascii  "thread::id of a non-executing thread\000"
.LC1:
        .ascii  "Coroutine started on thread: \000"
.LC2:
        .ascii  "Output jthread parameter not empty\000"
.LC3:
        .ascii  "New thread ID: \000"
.LC4:
        .ascii  "Coroutine resumed on thread: \000"
resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .actor]:
        push    {r0, r1, r2, r3, r4, r5, r6, lr}
        mov     r4, r0
        ldrh    r3, [r0, #20]
        lsls    r2, r3, #31
        bpl     .L17
        cmp     r3, #7
        bhi     .L18
        movs    r2, #170
        lsr     r3, r2, r3
        lsls    r3, r3, #31
        bmi     .L19
.L18:
        .inst   0xdeff
.L17:
        cmp     r3, #6
        bhi     .L18
        adr     r2, .L20

# This poerforms a jump to the correct location in the function.
        ldr     pc, [r2, r3, lsl #2]

# This is the jump table.
.L20:
        .word   .L23+1
        .word   .L18+1
        .word   .L22+1
        .word   .L18+1
        .word   .L21+1
        .word   .L18+1
        .word   .L19+1
.L23:
        str     r0, [r4, #12]
.L22:
        movs    r3, #1
        ldr     r1, .L46
        ldr     r0, .L46+4
        strb    r3, [r4, #23]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*)
        mov     r5, r0
        bl      pthread_self
        mov     r1, r0
        mov     r0, r5
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, std::thread::id)
        movs    r2, #1
        add     r1, sp, #2
        movs    r3, #10
        strb    r3, [sp, #2]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::__ostream_insert<char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*, int)
        ldr     r5, [r4, #16]
        movs    r3, #4
        strh    r3, [r4, #20]   @ movhi
        ldr     r3, [r4, #12]
        str     r3, [sp, #4]
        ldr     r3, [r5]
        str     r5, [r4, #28]
        cbz     r3, .L24
        movs    r0, #8
        bl      __cxa_allocate_exception
        ldr     r1, .L46+8
        mov     r6, r0
        bl      std::runtime_error::runtime_error(char const*) [complete object constructor]
        ldr     r2, .L46+12
        mov     r0, r6
        ldr     r1, .L46+16
        bl      __cxa_throw
        b       .L28
.L24:
        movs    r0, #8
        str     r3, [sp, #8]
        bl      operator new(unsigned int)
        ldr     r3, .L46+20
        movs    r2, #0
        str     r3, [r0]
        add     r1, sp, #12
        add     r3, sp, #4
        str     r0, [sp, #12]
        str     r3, [r0, #4]
        add     r0, sp, #8
        bl      std::thread::_M_start_thread(std::unique_ptr<std::thread::_State, std::default_delete<std::thread::_State> >, void (*)())
        add     r0, sp, #12
        bl      std::unique_ptr<std::thread::_State, std::default_delete<std::thread::_State> >::~unique_ptr() [complete object destructor]
        ldr     r3, [r5]
        cbz     r3, .L41
        bl      std::terminate()
        mov     r5, r0
        add     r0, sp, #12
        bl      std::unique_ptr<std::thread::_State, std::default_delete<std::thread::_State> >::~unique_ptr() [complete object destructor]
.L45:
        mov     r0, r5
.L28:
        bl      __cxa_begin_catch
        ldrb    r3, [r4, #23]   @ zero_extendqisi2
        cbnz    r3, .L31
        bl      __cxa_rethrow
        bl      __cxa_end_catch
        bl      __cxa_end_cleanup
.L41:
        ldr     r3, [sp, #8]
        ldr     r1, .L46+24
        ldr     r0, .L46+4
        str     r3, [r5]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*)
        ldr     r1, [r5]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, std::thread::id)
        movs    r2, #1
        add     r1, sp, #3
        movs    r3, #10
        strb    r3, [sp, #3]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::__ostream_insert<char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*, int)
        b       .L16
        mov     r5, r0
        mov     r0, r6
        bl      __cxa_free_exception
        b       .L45
.L21:
        ldr     r1, .L46+28
        ldr     r0, .L46+4
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*)
        mov     r5, r0
        bl      pthread_self
        mov     r1, r0
        mov     r0, r5
        bl      std::basic_ostream<char, std::char_traits<char> >& std::operator<< <char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, std::thread::id)
        movs    r2, #1
        add     r1, sp, #12
        movs    r3, #10
        strb    r3, [sp, #12]
        bl      std::basic_ostream<char, std::char_traits<char> >& std::__ostream_insert<char, std::char_traits<char> >(std::basic_ostream<char, std::char_traits<char> >&, char const*, int)
        b       .L32
.L31:
        movs    r3, #0
        str     r3, [r4]
        strh    r3, [r4, #20]   @ movhi
        bl      __cxa_end_catch
.L32:
        movs    r3, #0
        str     r3, [r4]
.L19:
        ldrb    r3, [r4, #22]   @ zero_extendqisi2
        cbz     r3, .L16
        mov     r0, r4
        bl      operator delete(void*)
.L16:
        add     sp, sp, #16
        pop     {r4, r5, r6, pc}
.L46:
        .word   .LC1
        .word   _ZSt4cout
        .word   .LC2
        .word   _ZNSt13runtime_errorD1Ev
        .word   _ZTISt13runtime_error
        .word   .LANCHOR0+8
        .word   .LC3
        .word   .LC4
resuming_on_new_thread(std::thread&):
        push    {r3, r4, r5, lr}
        mov     r5, r0
        movs    r0, #36
        bl      operator new(unsigned int)
        movs    r3, #1
        strb    r3, [r0, #22]
        mov     r4, r0
        ldr     r3, .L55
        str     r3, [r0]
        ldr     r3, .L55+4
        str     r3, [r0, #4]
        movs    r3, #0
        str     r5, [r0, #16]
        strh    r3, [r0, #20]   @ movhi
        bl      resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .actor]
        b       .L54
        bl      __cxa_begin_catch
        mov     r0, r4
        bl      operator delete(void*)
        bl      __cxa_rethrow
        bl      __cxa_end_catch
        bl      __cxa_end_cleanup
.L54:
        mov     r0, #0
        pop     {r3, r4, r5, pc}
.L55:
        .word   resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .actor]
        .word   resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .destroy]
main:
        push    {r0, r1, r2, lr}
        movs    r3, #0
        add     r0, sp, #4
        str     r3, [sp, #4]
        bl      resuming_on_new_thread(std::thread&)
        ldr     r0, [sp, #4]
        cbz     r0, .L62
.L60:
        bl      std::terminate()
        ldr     r3, [sp, #4]
        cmp     r3, #0
        bne     .L60
        bl      __cxa_end_cleanup
.L62:
        add     sp, sp, #12
        ldr     pc, [sp], #4
resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .destroy]:
        ldrh    r2, [r0, #20]
        orr     r2, r2, #1
        strh    r2, [r0, #20]   @ movhi
        b       resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .actor]
_GLOBAL__sub_I_switch_to_new_thread(std::thread&):
        push    {r4, lr}
        ldr     r4, .L65
        mov     r0, r4
        bl      std::ios_base::Init::Init() [complete object constructor]
        mov     r0, r4
        ldr     r2, .L65+4
        pop     {r4, lr}
        ldr     r1, .L65+8
        b       __aeabi_atexit
.L65:
        .word   .LANCHOR1
        .word   __dso_handle
        .word   _ZNSt8ios_base4InitD1Ev
```

I won't go into much detail, but just like zig and rust, the entry point of the coroutine function contains a jump table. Near `.L20`, by the standard, the frame for the coroutine is heap allocated near 



```
resuming_on_new_thread(std::thread&):
        push    {r3, r4, r5, lr}
        mov     r5, r0

        # 36 is the size of the frame
        movs    r0, #36 
        bl      operator new(unsigned int)
```

Actually, cpp makes a hidden special resuming function that contains the jump table.

```
resuming_on_new_thread(resuming_on_new_thread(std::thread&)::_Z22resuming_on_new_threadRSt6thread.Frame*) [clone .actor]:
```

It's interesting to note that both Cpp and Rust give the author about the same flexibility for interacting with the compilers code gen. E.g. A Future in rust implements the future trait, and in cpp, implements various member functions.