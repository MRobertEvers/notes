# More than one way to skin a cat I guess

RFC https://github.com/rust-lang/rfcs/blob/master/text/1951-expand-impl-trait.md#the-proposal-in-a-nutshell

From Stack Overflow
```
impl Traits as function arguments are desugared to an anonymous generic parameter. See the RFC, which says:

Expand impl Trait to allow use in arguments, where it behaves like an anonymous generic parameter.

There's also an example in the RFC:

// These two are equivalent
fn map<U>(self, f: impl FnOnce(T) -> U) -> Option<U>
fn map<U, F>(self, f: F) -> Option<U> where F: FnOnce(T) -> U
However, one difference is that impl Trait arguments cannot have their types explicitly specified:

fn foo<T: Trait>(t: T)
fn bar(t: impl Trait)

foo::<u32>(0) // this is allowed
bar::<u32>(0) // this is not
The Motivation for expanding to argument position section explains why additional syntax was added for an existing feature. In short, it's for having similar syntax as impl traits in function return position, which improves learnability, and to improve ergonomics.
```


```rust
trait AFunc {
    fn a_func(&self) -> i32;
}

struct A {}

impl AFunc for A {
    fn a_func(&self) -> i32 {
        return 1;
    }
}

struct B {
    x: u32
}

impl AFunc for B {
    fn a_func(&self) -> i32 {
        return 3;
    }
    
}

fn impl_template(obj: &impl AFunc) -> i32 {
    obj.a_func()
}

fn gen_template<T: AFunc>(obj: &T) -> i32 {
    obj.a_func()
}

static bb: B = B {
    x: 4
};
static aa: A = A {};
pub fn main() -> i32 {
    let aaa = A {};
    let bbb = B {x: 4};
    gen_template(&aaa);
    gen_template(&bbb);
    impl_template(&aa);
    impl_template(&bb)
}

```

We can see that identical code is generated.

```
<example::A as example::AFunc>::a_func:
        mov     eax, 1
        ret

<example::B as example::AFunc>::a_func:
        mov     eax, 3
        ret

example::impl_template:
        push    rax
        call    qword ptr [rip + <example::B as example::AFunc>::a_func@GOTPCREL]
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::impl_template:
        push    rax
        call    qword ptr [rip + <example::A as example::AFunc>::a_func@GOTPCREL]
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::gen_template:
        push    rax
        call    qword ptr [rip + <example::B as example::AFunc>::a_func@GOTPCREL]
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::gen_template:
        push    rax
        call    qword ptr [rip + <example::A as example::AFunc>::a_func@GOTPCREL]
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::main:
        sub     rsp, 24
        mov     dword ptr [rsp + 20], 4
        lea     rdi, [rsp + 16]
        call    example::gen_template
        lea     rdi, [rsp + 20]
        call    example::gen_template
        lea     rdi, [rip + example::aa]
        call    example::impl_template
        lea     rdi, [rip + example::bb]
        call    example::impl_template
        mov     dword ptr [rsp + 12], eax
        mov     eax, dword ptr [rsp + 12]
        add     rsp, 24
        ret

example::bb:
        .asciz  "\004\000\000"

example::aa:

__rustc_debug_gdb_scripts_section__:
        .asciz  "\001gdb_load_rust_pretty_printers.py"
```