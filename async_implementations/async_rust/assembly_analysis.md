# Assembly Analysis

I've constructed what I think is the simplest non-trivial async code (i.e. it doesn't just run to completion.)

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

The above rust produces the following assembly.

Note: Call codegen in `do_call` method in rust compiler code https://github.com/rust-lang/rust/blob/51e8031e14a899477a5e2d78ce461cab31123354/compiler/rustc_codegen_ssa/src/mir/block.rs#L125; which is basically a wrapper around llvm backend, https://github.com/rust-lang/rust/blob/51e8031e14a899477a5e2d78ce461cab31123354/compiler/rustc_codegen_llvm/src/builder.rs#L217.

The poll method is similar to zig's `resume` builtin, but the closure is provided explicitly by the code writer. The context object is solely to gain access to the waker object in the `poll` call. It seems to me that access to the waker inside the `poll` function is usually unecessary. The waker has a is called by some other part of the code with the intention that it will schedule an executor (or something), to call `poll` again. (TODO: Why does `poll` need access to waker?) My guess is that the poll function can gain some information about the state of the awaiting thing, and then give the waker a hint about when to call again?

Additionally, if poll returns `Poll::Ready`, then execution continues directly after the await, after poll returns. Much like zig, when an async function returns, it immediately jumps to the awaiting function and continues.



```assembly
core::pin::Pin<&mut T>::map_unchecked_mut:
        mov     rax, rdi
        ret

core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>:
        mov     rdi, qword ptr [rdi]
        push    2
        pop     rsi
        push    1
        pop     rdx
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

<&mut T as core::ops::deref::Deref>::deref:
        mov     rax, qword ptr [rdi]
        ret

alloc::boxed::<impl core::convert::From<alloc::boxed::Box<T,A>> for core::pin::Pin<alloc::boxed::Box<T,A>>>::from:
        mov     rax, rdi
        ret

alloc::boxed::Box<T,A>::into_pin:
        mov     rax, rdi
        ret

<core::pin::Pin<P> as core::ops::deref::Deref>::deref:
        mov     rax, qword ptr [rdi]
        ret

<alloc::boxed::Box<T,A> as core::ops::deref::DerefMut>::deref_mut:
        mov     rax, qword ptr [rdi]
        ret

<core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll:
        push    r14
        push    rbx
        sub     rsp, 56
        mov     r14, rdi
        movzx   eax, byte ptr [rdi]

        # Look up in the jump table where the function should return.
        lea     rcx, [rip + .LJTI11_0]
        movsxd  rax, dword ptr [rcx + 4*rax]
        add     rax, rcx
        jmp     rax
.LBB11_1:
        mov     byte ptr [r14 + 1], 0

# This corresponds to fut.await in the source code. You can see that a poll call is inserted here.
.LBB11_2:
        lea     rdi, [r14 + 1]
        call    qword ptr [rip + <example::DummyFuture as core::future::future::Future>::poll@GOTPCREL]
        mov     ebx, eax
        mov     al, 3
        test    bl, 1

        # If the poll function returns `Poll::Ready`, then continue execution following the await.
        jne     .LBB11_5
        lea     rax, [rip + .L__unnamed_1]
        lea     rdi, [rsp + 8]
        mov     qword ptr [rdi], rax
        mov     qword ptr [rdi + 8], 1
        and     qword ptr [rdi + 16], 0
        lea     rax, [rip + .L__unnamed_2]
        mov     qword ptr [rdi + 32], rax
        and     qword ptr [rdi + 40], 0
        call    qword ptr [rip + std::io::stdio::_print@GOTPCREL]
        mov     al, 1
.LBB11_5:
        and     bl, 1
        mov     byte ptr [r14], al
        mov     eax, ebx
        add     rsp, 56
        pop     rbx
        pop     r14
        ret
.LBB11_6:
        lea     rdi, [rip + str.1]
        lea     rdx, [rip + .L__unnamed_3]
        push    35
        jmp     .LBB11_8
.LBB11_7:
        lea     rdi, [rip + str.0]
        lea     rdx, [rip + .L__unnamed_3]
        push    34
.LBB11_8:
        pop     rsi
        call    qword ptr [rip + core::panicking::panic@GOTPCREL]
        ud2
        mov     byte ptr [r14], 2
        mov     rdi, rax
        call    _Unwind_Resume@PLT
        ud2
.LJTI11_0:
        .long   .LBB11_1-.LJTI11_0
        .long   .LBB11_6-.LJTI11_0
        .long   .LBB11_7-.LJTI11_0
        .long   .LBB11_2-.LJTI11_0

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

<example::DummyFuture as core::future::future::Future>::poll:
        mov     al, byte ptr [rdi]
        cmp     al, 1
        je      .LBB15_2
        lea     ecx, [rax + 1]
        mov     byte ptr [rdi], cl
.LBB15_2:
        cmp     al, 1
        setne   al
        mov     dl, 11
        ret

example::main:
        push    rbx
        sub     rsp, 80
        lea     rax, [rsp + 16]
        and     qword ptr [rax], 0
        lea     rcx, [rip + .L__unnamed_4]
        mov     qword ptr [rax + 8], rcx
        mov     qword ptr [rsp], rax
        push    2
        pop     rdi
        push    1
        pop     rsi
        call    qword ptr [rip + __rust_alloc@GOTPCREL]
        test    rax, rax
        je      .LBB16_9
        mov     rbx, rax
        and     word ptr [rax], 0
        mov     qword ptr [rsp + 8], rax
        mov     rsi, rsp
        mov     rdi, rax
        call    qword ptr [rip + <core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll@GOTPCREL]
        test    al, al
        jne     .LBB16_4
        lea     rax, [rip + .L__unnamed_5]
        lea     rdi, [rsp + 32]
        mov     qword ptr [rdi], rax
        mov     qword ptr [rdi + 8], 1
        and     qword ptr [rdi + 16], 0
        lea     rax, [rip + .L__unnamed_2]
        mov     qword ptr [rdi + 32], rax
        and     qword ptr [rdi + 40], 0
        call    qword ptr [rip + std::io::stdio::_print@GOTPCREL]
.LBB16_4:
        mov     rsi, rsp
        mov     rdi, rbx
        call    qword ptr [rip + <core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll@GOTPCREL]
        test    al, al
        jne     .LBB16_7
        lea     rax, [rip + .L__unnamed_6]
        lea     rdi, [rsp + 32]
        mov     qword ptr [rdi], rax
        mov     qword ptr [rdi + 8], 1
        and     qword ptr [rdi + 16], 0
        lea     rax, [rip + .L__unnamed_2]
        mov     qword ptr [rdi + 32], rax
        and     qword ptr [rdi + 40], 0
        call    qword ptr [rip + std::io::stdio::_print@GOTPCREL]
.LBB16_7:
        lea     rdi, [rsp + 8]
        call    core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>
        mov     rdi, qword ptr [rsp + 16]
        mov     rax, qword ptr [rsp + 24]
        call    qword ptr [rax + 24]
        add     rsp, 80
        pop     rbx
        ret
.LBB16_9:
        push    2
        pop     rdi
        push    1
        pop     rsi
        call    qword ptr [rip + alloc::alloc::handle_alloc_error@GOTPCREL]
        ud2
        mov     rbx, rax
        lea     rdi, [rsp + 8]
        call    core::ptr::drop_in_place<core::pin::Pin<alloc::boxed::Box<core::future::from_generator::GenFuture<example::hello_world::{{closure}}>>>>
        lea     rdi, [rsp + 16]
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

.L__unnamed_7:
        .ascii  "/app/example.rs"

.L__unnamed_3:
        .quad   .L__unnamed_7
        .asciz  "\017\000\000\000\000\000\000\000*\000\000\000\030\000\000"

str.0:
        .ascii  "`async fn` resumed after panicking"

str.1:
        .ascii  "`async fn` resumed after completion"

.L__unnamed_8:
        .ascii  "hello, world!\n"

.L__unnamed_1:
        .quad   .L__unnamed_8
        .asciz  "\016\000\000\000\000\000\000"

.L__unnamed_9:
        .ascii  "Completed on first poll!\n"

.L__unnamed_5:
        .quad   .L__unnamed_9
        .asciz  "\031\000\000\000\000\000\000"

.L__unnamed_10:
        .ascii  "Completed on second poll!\n"

.L__unnamed_6:
        .quad   .L__unnamed_10
        .asciz  "\032\000\000\000\000\000\000"

DW.ref.rust_eh_personality:
        .quad   rust_eh_personality
```

From the rust source code, async is implemented in terms of generators (internally), so the code that produces an async call is the code that produces generators.

```rust
// expr.rs

    /// Lower an `async` construct to a generator that is then wrapped so it implements `Future`.
    ///
    /// This results in:
    ///
    /// ```text
    /// std::future::from_generator(static move? |_task_context| -> <ret_ty> {
    ///     <body>
    /// })
    /// ```
    pub(super) fn make_async_expr(
        ...
```

and, later, we can see

```rust
// compiler/rustc_ast_lowering/src/expr.rs
    /// Desugar `<expr>.await` into:
    /// ```rust
    /// match ::std::future::IntoFuture::into_future(<expr>) {
    ///     mut pinned => loop {
    ///         match unsafe { ::std::future::Future::poll(
    ///             <::std::pin::Pin>::new_unchecked(&mut pinned),
    ///             ::std::future::get_context(task_context),
    ///         ) } {
    ///             ::std::task::Poll::Ready(result) => break result,
    ///             ::std::task::Poll::Pending => {}
    ///         }
    ///         task_context = yield ();
    ///     }
    /// }
    /// ```
    fn lower_expr_await(&mut self, await_span: Span, expr: &Expr) -> hir::ExprKind<'hir> {
```

__Lowering__ is a term that appears in the rust compiler and generally it means taking higher-level constructs and translating them to lower level constructs. In the rust source code, `hir` appears to stand for all the language's possible features and tokens, and `mir` appears to stand for 'middle' representation, i.e. right before low level IR or LLVM IR.