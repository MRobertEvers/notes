# Async Rust

Build with `cargo build`.

Run with `./target/debug/async_rust`. Compiler explorer requires `--edition=2021`.

## Discussion

Async rust takes a different route than, say, zig. Async functions in rust return an `impl Future`, which has one function

```rust
fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>
```

On the generated functions `poll` causes the future to execute as much as possible until it reaches a `.await` that returns `Poll::Pending`. It runs through all `.await`s calling `poll` and continuing if they return `Poll::Ready`. That is, `.await` produces a `poll` call.

See [assembly analysis](./assembly_analysis.md).

## Example

With `-C opt-level=z`, the following produces smallish assembly.

```
hello, world!
Completed on second poll!
```

```rust
use core::ptr::null;
use std::task::{Context, Poll};
use std::future::{Future};
use std::pin::Pin;
use core::task::{RawWaker, RawWakerVTable, Waker};

unsafe fn noop_clone(_data: *const ()) -> RawWaker {
    noop_raw_waker()
}

unsafe fn noop(_data: *const ()) {}

const NOOP_WAKER_VTABLE: RawWakerVTable = RawWakerVTable::new(noop_clone, noop, noop, noop);

const fn noop_raw_waker() -> RawWaker {
    RawWaker::new(null(), &NOOP_WAKER_VTABLE)
}

#[inline]
pub fn noop_waker() -> Waker {
    unsafe { Waker::from_raw(noop_raw_waker()) }
}

pub struct DummyFuture {
    count: u8
}

impl Future for DummyFuture {
    type Output = u8;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if self.count == 1 {
            Poll::Ready(11)
        } else {
            Pin::into_inner(self).count += 1;
            Poll::Pending
        }
    }
}


async fn hello_world() {
    let fut: DummyFuture = DummyFuture {
        count: 0
    };
    fut.await;
    println!("hello, world!");
}

pub fn main() {
    let waker = noop_waker();
    let mut c: Context =  Context::from_waker(&waker);
    let mut future = Box::pin(hello_world()); 

    if let Poll::Ready(res) = future.as_mut().poll(&mut c) {
        println!("Completed on first poll!");
    }

    if let Poll::Ready(res2) = future.as_mut().poll(&mut c) {
        println!("Completed on second poll!");
    }
}
```

Assembly

```assembly
core::pin::Pin<&mut T>::map_unchecked_mut:
        mov     rax, rdi
        ret

core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>:
        mov     rdi, qword ptr [rdi]
        push    1
        pop     rdx
        mov     rsi, rdx
        jmp     qword ptr [rip + __rust_dealloc@GOTPCREL]

core::ptr::drop_in_place<core::task::wake::Waker>:
        mov     rax, qword ptr [rdi]
        mov     rcx, qword ptr [rdi + 8]
        mov     rdi, rax
        jmp     qword ptr [rcx + 24]

<T as core::convert::From<T>>::from:
        ret

<T as core::convert::Into<U>>::into:
        mov     rax, rdi
        ret

<T as core::convert::Into<U>>::into:
        mov     rax, rdi
        ret

alloc::boxed::<impl core::convert::From<alloc::boxed::Box<T,A>> for core::pin::Pin<alloc::boxed::Box<T,A>>>::from:
        mov     rax, rdi
        ret

alloc::boxed::Box<T,A>::into_pin:
        mov     rax, rdi
        ret

<alloc::boxed::Box<T,A> as core::ops::deref::DerefMut>::deref_mut:
        mov     rax, qword ptr [rdi]
        ret

<core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll:
        push    rbx
        sub     rsp, 48
        movzx   eax, byte ptr [rdi]
        test    eax, eax
        jne     .LBB9_3
        mov     rbx, rdi
        lea     rax, [rip + .L__unnamed_1]
        mov     rdi, rsp
        mov     qword ptr [rdi], rax
        mov     qword ptr [rdi + 8], 1
        and     qword ptr [rdi + 16], 0
        lea     rax, [rip + .L__unnamed_2]
        mov     qword ptr [rdi + 32], rax
        and     qword ptr [rdi + 40], 0
        call    qword ptr [rip + std::io::stdio::_print@GOTPCREL]
        mov     byte ptr [rbx], 1
        xor     eax, eax
        add     rsp, 48
        pop     rbx
        ret
.LBB9_3:
        cmp     eax, 1
        jne     .LBB9_6
        lea     rdi, [rip + str.1]
        lea     rdx, [rip + .L__unnamed_3]
        push    35
        jmp     .LBB9_5
.LBB9_6:
        lea     rdi, [rip + str.0]
        lea     rdx, [rip + .L__unnamed_3]
        push    34
.LBB9_5:
        pop     rsi
        call    qword ptr [rip + core::panicking::panic@GOTPCREL]
        ud2
        mov     byte ptr [rbx], 2
        mov     rdi, rax
        call    _Unwind_Resume@PLT
        ud2

example::noop_clone:
        lea     rdx, [rip + .L__unnamed_4]
        xor     eax, eax
        ret

example::noop:
        ret

example::noop_raw_waker:
        lea     rdx, [rip + .L__unnamed_4]
        xor     eax, eax
        ret

example::main:
        push    rbx
        sub     rsp, 32
        and     qword ptr [rsp + 8], 0
        lea     rax, [rip + .L__unnamed_4]
        mov     qword ptr [rsp + 16], rax
        push    1
        pop     rdi
        mov     rsi, rdi
        call    qword ptr [rip + __rust_alloc@GOTPCREL]
        test    rax, rax
        je      .LBB13_4
        mov     byte ptr [rax], 0
        mov     qword ptr [rsp], rax
        lea     rsi, [rsp + 24]
        mov     rdi, rax
        call    qword ptr [rip + <core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll@GOTPCREL]
        mov     rdi, rsp
        call    core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>
        mov     rdi, qword ptr [rsp + 8]
        mov     rax, qword ptr [rsp + 16]
        call    qword ptr [rax + 24]
        add     rsp, 32
        pop     rbx
        ret
.LBB13_4:
        push    1
        pop     rdi
        mov     rsi, rdi
        call    qword ptr [rip + alloc::alloc::handle_alloc_error@GOTPCREL]
        ud2
        mov     rbx, rax
        mov     rdi, rsp
        call    core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>
        lea     rdi, [rsp + 8]
        call    core::ptr::drop_in_place<core::task::wake::Waker>
        mov     rdi, rbx
        call    _Unwind_Resume@PLT
        ud2

.L__unnamed_2:

.L__unnamed_4:
        .quad   example::noop_clone
        .quad   example::noop
        .quad   example::noop
        .quad   example::noop

.L__unnamed_5:
        .ascii  "/app/example.rs"

.L__unnamed_3:
        .quad   .L__unnamed_5
        .asciz  "\017\000\000\000\000\000\000\000\033\000\000\000\030\000\000"

str.0:
        .ascii  "`async fn` resumed after panicking"

str.1:
        .ascii  "`async fn` resumed after completion"

.L__unnamed_6:
        .ascii  "hello, world!\n"

.L__unnamed_1:
        .quad   .L__unnamed_6
        .asciz  "\016\000\000\000\000\000\000"

DW.ref.rust_eh_personality:
        .quad   rust_eh_personality
```