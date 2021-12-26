# Anatomy of a Virtual Dispatch

## Basic Virtual Dispatch in CPP (gcc)

Minimal example of a virtual call. `bba->a_func()` will call `B::a_func()`.

```cpp
class A {
public:
    virtual int a_func() {return 1;}

    // To illustrate what happens when a function is not overriden.
    virtual int a_foo() {return 2;}
};

class B : public A {
public:
    virtual int a_func() {return 3;}
};


// These are declared globally so we can easily see
// where they are stored (otherwise they'd be on the stack)
A aa; // Intantiate this otherwise the compiler wont generate a vtable for A; even without compilation.
B bb;

int main() {
    A* bba = &bb;

    return bba->a_func();
}
```

This is the assembly produced by gcc with `-fno-rtti` targeting x86_64. Run-time information is removed because it distracts from the gist. I've annotated the assembly to show what is happening.

```assembly
B::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 3
        pop     rbp
        ret
bb:
        .quad   vtable for B+16
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        
        # Load &bb into [rbp-8]. In Intel asm, this means *addr(rbp-8) = &bb
        mov     QWORD PTR [rbp-8], OFFSET FLAT:bb

        # This is the start of the virtual dispatch.
        # Load &bb into rax
        mov     rax, QWORD PTR [rbp-8]

        # Load *(&bb) into rax (the hidden first member of class B)
        # Something like rax = (&bb)->vtable_ptr
        mov     rax, QWORD PTR [rax]

        # Load first member of vtable into rdx
        # Something like rdx = (&bb)->vtable_ptr->a_func
        mov     rdx, QWORD PTR [rax]

        # Load &bb back into rax (i.e. the class data)
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    rdx
        nop
        leave
        ret
vtable for B:
        .quad   0
        .quad   0
        .quad   B::a_func()
        .quad   A::a_foo()
vtable for A:
        .quad   0
        .quad   0
        .quad   A::a_func()
        .quad   A::a_foo()
```

This is a little bit cryptic, but we can illustrate something similar in C. (if that is any less cryptic)

```c
struct A {
       struct AVTable* vtable_ptr; 
};

struct AVTable {
        int (*a_func)(struct A*);
        int (*a_foo)(struct A*);
};

int impl_a_func_for_a(struct A* self) {
        return 1;
}

int impl_a_foo_for_a(struct A* self) {
        return 2;
}

struct B {
        struct BVTable* vtable_ptr;
};

struct BVTable {
        int (*a_func)(struct B*);
        int (*a_foo)(struct B*);
};

int impl_a_func_for_b(struct B* self) {
        return 3;
}

struct AVTable vtable_for_a;
struct BVTable vtable_for_b;

struct B bb;

int main() 
{       
        // This is a constructor for the struct, in C++, the compiler just
        // knows which function to call given its type, however, C doesn't
        // Effective, this assignment happens at compile time in C++.
        vtable_for_a.a_func = &impl_a_func_for_a;
        vtable_for_a.a_foo = &impl_a_foo_for_a;
        vtable_for_b.a_func = &impl_a_func_for_b;

        // B doesn't override a_foo, so it gets the base implementation.
        vtable_for_b.a_foo = (int (*)(struct B *))&impl_a_foo_for_a;

        // This is normally done in the construction of the class,
        // or is done at compile time if the class is constructed statically.
        bb.vtable_ptr = &vtable_for_b;

        // This is not type safe, however, it is important that both A and B
        // are structurally compatible in the location of the vtable. That is, 
        // the offset of the vtable_ptr with respect to A* must be the same as 
        // the offset of the vtable_ptr with respect to B*. 
        // 
        // So in C, our vtable must always be the first element of the struct**.
        // **or at the same offset for all structs.
        //
        struct A* pbba = (struct A*)&bb;
        
        // The structural compatibility requirement is true for AVTable and BVTable
        // as well.
        pbba->vtable_ptr->a_func((struct A*)&bb);

        return 0;
}
```

## A Slightly Different CPP Example

I'm going to rewrite the cpp example so we can more closely compare to Rust. Rust doesn't have inheritance, so the notion of a parent class doesn't exist.

```cpp
class AFunc {
public:
    virtual int a_func() = 0;
};

class B : public AFunc {
public:
    virtual int a_func() {return 3;}
};

class C : public AFunc {
public:
    virtual int a_func() {return 5;}
};


int my_virtual_dispatch(AFunc* obj) {
    return obj->a_func();
}

B bb;
C cc;
   
int main() {
    return my_virtual_dispatch(&bb);
}
```

Which produces

```assembly
B::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 3
        pop     rbp
        ret
C::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 5
        pop     rbp
        ret
my_virtual_dispatch(AFunc*):
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        mov     QWORD PTR [rbp-8], rdi
        mov     rax, QWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rax]
        mov     rdx, QWORD PTR [rax]
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    rdx
        leave
        ret
bb:
        .quad   vtable for B+16
cc:
        .quad   vtable for C+16
main:
        push    rbp
        mov     rbp, rsp
        mov     edi, OFFSET FLAT:bb
        call    my_virtual_dispatch(AFunc*)
        nop
        pop     rbp
        ret
vtable for C:
        .quad   0
        .quad   0
        .quad   C::a_func()
vtable for B:
        .quad   0
        .quad   0
        .quad   B::a_func()
```

We can see that this produces the same virtual dispatch, except now the virtual dispatch takes place in `my_virtual_function`.

Again, we can write this in C to see if it has changed at all.

```c
struct AFunc {
       struct AVTable* vtable_ptr; 
};

struct AVTable {
        int (*a_func)(struct AFunc*);
};


struct B {
        struct BVTable* vtable_ptr;
};

struct BVTable {
        int (*a_func)(struct B*);
};

int impl_a_func_for_b(struct B* self) {
        return 3;
}

struct C {
        struct BVTable* vtable_ptr;
};


struct CVTable {
        int (*a_func)(struct C*);
};

int impl_a_func_for_c(struct C* self) {
        return 5;
}

int my_virtual_dispatch(struct AFunc* obj) {
        return obj->vtable_ptr->a_func(obj);
}

// AVTable is deleted as the entire class is pure virtual
// AVTable vtable_for_a;
struct BVTable vtable_for_b;
struct CVTable vtable_for_c;

struct B bb;

int main() 
{       
        // Normally done at compile time.
        vtable_for_b.a_func = &impl_a_func_for_b;
        vtable_for_c.a_func = &impl_a_func_for_c;

        // Normally done during class construction. Writing 'constructors' omitted here.
        bb.vtable_ptr = &vtable_for_b;

        // Again, not type-safe in C, but the CPP compiler can check these
        // things at compile time.
        return my_virtual_dispatch((struct AFunc*)&bb);
}
```


## Rust

Rust doesn't have inheritance, but it still have virtual dispatch with `dyn Trait`. 

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

struct B {}

impl AFunc for B {
    fn a_func(&self) -> i32 {
        return 3;
    }
}

fn my_virtual_dispatch(obj: &dyn AFunc) -> i32 {
    obj.a_func()
}

static bb: B = B {};
pub fn main() -> i32 {
    my_virtual_dispatch(&bb)
}
```

The above rust code generates the following assembly. Again, I've left some annotations to help show what is happening.

```assembly
core::ptr::drop_in_place<example::B>:
        ret

<example::A as example::AFunc>::a_func:
        mov     eax, 1
        ret

<example::B as example::AFunc>::a_func:
        mov     eax, 3
        ret

example::my_virtual_dispatch:
        push    rax

        # Notice, rsi was loaded with the vtable in main. rsi+24 points to a_func for B.
        call    qword ptr [rsi + 24]
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::main:
        push    rax

        # Load address of bb into rdi
        lea     rdi, [rip + example::bb]

        # Load address of vtable into rsi
        lea     rsi, [rip + .L__unnamed_1]
        call    example::my_virtual_dispatch
        mov     dword ptr [rsp + 4], eax
        mov     eax, dword ptr [rsp + 4]
        pop     rcx
        ret

example::bb:

# This is the vtable for impl AFunc for B
# Perhaps unhelfully, rust codegen doesn't generate a nice label for this.
# This is generated for impl AFunc for B 
# If there were another Trait, e.g. BFunc and impl BFunc for B,
# there would be another section like this. .L__unnamed_2 with the same fields, 
# except of course the a_func.
.L__unnamed_1:
        .quad   core::ptr::drop_in_place<example::B>
        .asciz  "\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000"
        .quad   <example::B as example::AFunc>::a_func

__rustc_debug_gdb_scripts_section__:
        .asciz  "\001gdb_load_rust_pretty_printers.py"
```

Finally, lets implement this in C, trying to keep to the spirit of the assembly, and note the differences. Now is a good time to note, however, that C doesn't have references. So passing by pointer or by copy is the closest we can get. The following assembly lines load the actual address of `bb` and the vtable.

```
# Load address of bb into rdi
lea     rdi, [rip + example::bb]

# Load address of vtable into rsi
lea     rsi, [rip + .L__unnamed_1]
```

In C, we have to pass by pointer, and dereference those pointers in the function. Which, in unoptimized assembly, is two instructions.

```
# In the C ABI, rdi is the first argument, rsi is the second.
mov     QWORD PTR [rbp-8], rdi
mov     QWORD PTR [rbp-16], rsi

# Dereference second argument, this is the vtable.
mov     rax, QWORD PTR [rbp-16]
mov     rdx, QWORD PTR [rax]

# Simply copy the first argument, since we pass a pointer into the a_func anyway.
mov     rax, QWORD PTR [rbp-8]
mov     rdi, rax
call    rdx
```


Note that rust automatically includes the drop_in_place function in the vtable. This is part of Rust's Drop attribute. I'm going to ignore it in the C implementation.

```c
// Compiler only known type. The best way C can represent the Trait AFunc.
struct AFunc {
        int (*a_func)(void*);
};

struct A {};

struct ImplAFuncForAVTable {
        int (*a_func)(struct A*);
};

int impl_a_func_for_a(struct A* self) {
        return 1;
}

struct B {};

struct ImplAFuncForBVTable {
        int (*a_func)(struct B*);
};

int impl_a_func_for_b(struct B* self) {
        return 3;
}

// Notice that in a `dyn Trait` call, the first argument has to be
// type erased in C.
int my_virtual_dispatch(void* obj, struct AFunc* vtable_ptr) {
        return vtable_ptr->a_func(obj);
}

struct ImplAFuncForAVTable vtable_for_a;
struct ImplAFuncForBVTable vtable_for_b;

struct B bb;

int main() 
{       
        // Normally done at compile time.
        vtable_for_a.a_func = &impl_a_func_for_a;
        vtable_for_b.a_func = &impl_a_func_for_b;

        // The rust compiler knows which vtable to choose based on the type of bba.
        return my_virtual_dispatch(&bb, (struct AFunc*)&vtable_for_b);
}
```

An interesting point in our C code, we have declarations for `struct ImplAFuncForBVTable` and the other vtables. Those are singleton. If we were to instantiate other `struct B` objects, they would point to the same vtable.

The implementations are different obviously; the compilers implement dynamic dispatch in different ways.
While CPP doesn't specify how virtual functions must be implemented (and therefore don't mention VTables), CPP could theoretically implement virtual dispatch this way; but I'm not an expert so they may run into issues with other language requirements if they do so.

CPP allows for more than rust however. With a `dyn Trait`, you can only call functions on that trait. With an `A*` which points to a `B*`, we can access implementations on `A`, but those implementations on `A` expect the input `self` pointer to have structural compatibility with `A`. In Rust, trait don't have implementations on themselves, so this isn't a problem, but in CPP, we can inherit functionality, not just interfaces.

As stated above, `A` and `B` have to be structurally compatible (and so do their vtables). This is so that non-virtual functions can still work, as they will expect the memory layout of the `self` pointer to be the same regardless if it's pointing to `A` or `B`.


```cpp
class AFunc {
// Important! gcc will generate constructors if this is not initialized here
// Its because if my_i is undefined, then it could be anything, which means,
// GCC cant just memcpy a static instance of this object from const memory,
// to create one.
    int my_i = 0;
public:
    virtual int a_func() = 0;
};


class B : public AFunc {
public:
    virtual int a_func() {return 3;}
};

B bb;

int main() {
    AFunc* bba = &bb;

    return bba->a_func();
}
```

This produces the following assembly. You can see that the compiler embeds an `A` into `B`.

```assembly
B::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 3
        pop     rbp
        ret
bb:
        .quad   vtable for B+16
        .long   0
        .zero   4
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        mov     QWORD PTR [rbp-8], OFFSET FLAT:bb
        mov     rax, QWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rax]
        mov     rdx, QWORD PTR [rax]
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    rdx
        nop
        leave
        ret
vtable for B:
        .quad   0
        .quad   0
        .quad   B::a_func()
```

Notice that the memory location `bb` for an instance of `B` includes space for a `.long 0`. If you change the initialization of `my_i = 5`, it will change there. Despite having no data members, space is allocated for the data members implemented in `A`.

```c
struct AFunc {
       struct AVTable* vtable_ptr; 
       int my_i;
};

struct AFuncVTable {
        int (*a_func)(struct AFunc*);
};


struct B {
        struct BVTable* vtable_ptr;
        int my_i; // Necessary for structural compatibility. Compiler normally adds this like all the other stuff.
};

struct BVTable {
        int (*a_func)(struct B*);
};
struct BVTable vtable_for_b;

int impl_a_func_for_b(struct B* self) {
        return 3;
}

struct B bb;

int main() 
{       
        // Normally done at compile time.
        vtable_for_b.a_func = &impl_a_func_for_b;

        // Normally done during class construction. Writing 'constructors' omitted here.
        bb.vtable_ptr = &vtable_for_b;
        bb.my_i = 0;

        struct AFunc* bba = (struct AFunc*)&bb;

        return bba->vtable_ptr->a_func(bba);
}
```

It starts to get hairy when you start inheriting from multiple class. Since `B` inheriting from `A` must be structurally compatible with `A`, how can `B` inherit from `X` without breaking structural compatibility with `A`. It can't. This is where our C implementation of the CPP vtables falls apart. There are actually more steps.

```cpp
class AFunc {
    int my_a = 2;
public:
    virtual int a_func() {return 0;};
};

class XFunc {
    int my_x = 0;
public:
    virtual int x_func() {return 3;}
};

class YFunc {
    int my_y = 1;
public:
    virtual int y_func() {return 6;}
};

class B : public XFunc, public YFunc, public AFunc {
public:
};

B bb;

int main() {
    AFunc* bba = &bb;

    return bba->a_func();
}
```

Produces the following assembly.

```assembly
AFunc::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 0
        pop     rbp
        ret
XFunc::x_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 3
        pop     rbp
        ret
YFunc::y_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 6
        pop     rbp
        ret
bb:
        .quad   vtable for B+16
        .long   0
        .zero   4
        .quad   vtable for B+40
        .long   1
        .zero   4
        .quad   vtable for B+64
        .long   2
        .zero   4
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        # Notice this is passing a pointer to `.quad   vtable for B+64` inside `bb`
        # This is a point to the AFunc struct that the compiler implemented for B.
        # This is where the trouble is; the a_func expects a pointer to an AFunc struct,
        # but a B struct pointer will point to the first implemented XFunc struct.
        mov     QWORD PTR [rbp-8], OFFSET FLAT:bb+32
        mov     rax, QWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rax]
        mov     rdx, QWORD PTR [rax]

        # This passes a point to AFunc's implementation inside B!
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    rdx
        nop
        leave
        ret
vtable for B:
        .quad   0
        .quad   0
        .quad   XFunc::x_func()
        .quad   -16
        .quad   0
        .quad   YFunc::y_func()
        .quad   -32
        .quad   0
        .quad   AFunc::a_func()
```

This is where our C analogy breaks down a bit. We need to adjust it.

```c
struct AFunc {
       struct AFuncVTable* vtable_ptr; 
       int my_a;
};

struct AFuncVTable {
        int (*a_func)(struct AFunc*);
};


int impl_a_func_for_afunc(struct AFunc* self) {
        return 0;
}

struct XFunc {
       struct XFuncVTable* vtable_ptr; 
       int my_x;
};

struct XFuncVTable {
        int (*x_func)(struct XFunc*);
};

int impl_x_func_for_xfunc(struct XFunc* self) {
        return 3;
}

struct YFunc {
       struct YFuncVTable* vtable_ptr; 
       int my_y;
};

struct YFuncVTable {
        int (*y_func)(struct YFunc*);
};

int impl_y_func_for_yfunc(struct YFunc* self) {
        return 6;
}


struct B {
        struct XFunc xfunc;
        struct YFunc yfunc;
        struct AFunc afunc;
};

struct BImpl {};

struct BVTableImpl {
};

struct BVTable {
        struct XFuncVTable x_table;
        // gcc actually merges the vtable in the first location with the classes own vtable AFTER the first impl
        struct BVTableImpl b_table; 
        struct YFuncVTable y_table;
        struct AFuncVTable a_table;
};
struct BVTable vtable_for_b;


struct B bb;

int main() 
{       
        // Normally done at compile time.
        vtable_for_b.x_table.x_func = &impl_x_func_for_xfunc;
        vtable_for_b.y_table.y_func = &impl_y_func_for_yfunc;
        vtable_for_b.a_table.a_func = &impl_a_func_for_afunc;

        // Normally done during class construction. Writing 'constructors' omitted here.
        bb.xfunc.vtable_ptr = (struct XFuncVTable *)&vtable_for_b;
        bb.yfunc.vtable_ptr = (struct YFuncVTable *)((char*)(&vtable_for_b) + sizeof(struct XFuncVTable) + sizeof(struct BVTableImpl));
        bb.afunc.vtable_ptr = (struct AFuncVTable *)((char*)(&vtable_for_b) + sizeof(struct XFuncVTable) + sizeof(struct BVTableImpl) + sizeof(struct YFuncVTable));
        bb.xfunc.my_x = 0;
        bb.yfunc.my_y = 1;
        bb.afunc.my_a = 2;

        // When you get a pointer to a base class
        struct AFunc* bba = (struct AFunc*)&(bb.afunc);

        return bba->vtable_ptr->a_func(bba);
}
```

That certainly go complicated fast. We can see that `B` is constructed as an array `[struct X, struct Y, struct A]` (ignoring any potential members `B` might have itself). And when we perform a cast `X* x = (X*)b`, then the compiler just picks the correct position in the `B` implementation, like `bb.afunc` above.

One thing to note. GCC actually includes other information in the vtable. Our C implementation ignores that

```
vtable for B:
        .quad   0  # top_offset for XBVtable (used in RTTI, but still included even if RTTI is disabled)
        .quad   0  # ptr to RTTI which is disabled
        .quad   XFunc::x_func()  # start of vtable pointers
        .quad   -16
        .quad   0
        .quad   YFunc::y_func()
        .quad   -32
        .quad   0
        .quad   AFunc::a_func()
```

Additionally, if we added modified `B` to have it's own virtual function like so,

```cpp
class B : public XFunc, public YFunc, public AFunc {
public:
    virtual int b_func() {return 6;}
};
```

The vtable for `B` simply adds `.quad   B::b_func()` after the first inherited parent's virtual functions.

```
vtable for B:
        .quad   0  # top_offset for XBVtable (used in RTTI, but still included even if RTTI is disabled)
        .quad   0  # ptr to RTTI which is disabled
        .quad   XFunc::x_func()  # start of vtable pointers
        .quad   B::b_func()
        .quad   -16
        .quad   0
        .quad   YFunc::y_func()
        .quad   -32
        .quad   0
        .quad   AFunc::a_func()
```

This makes sense. A pointer `B*` that points to `B` shouldn't need any adjustments when calling a function that accepts a `B*`. So the functions introduced by `B` are included with the functions in the first vtable.

## Virtual function calls

Another thing to look at is what happens when `B` overrides a method. Well, if it overrides a method in the first inherited parent, then the vtable for that function is just changed to the overriding method.

If `B` overrides a method from a non-first parent, then we have a problem. We are putting all of the functions on `B` in the first vtable. But if we have a pointer, e.g. `Y*` like from above, then we need to also put that function into the `Y` vtable, like so 

```
vtable for B:
        .quad   0  # top_offset for XBVtable (used in RTTI, but still included even if RTTI is disabled)
        .quad   0  # ptr to RTTI which is disabled
        .quad   XFunc::x_func()  # start of vtable pointers
        .quad   B::y_func()
        .quad   -16
        .quad   0
        .quad   B::y_func()
        .quad   -32
        .quad   0
        .quad   AFunc::a_func()
```

But wait! What if we go even further and inherit from `B`?


What is `non-virtual thunk to B::a_func():`? When `B` overrides one of its non-intial parent classes. Gcc introduces a non-virtual thunk that simply redirects to the (`FirstParent` + `B`) vtable.

We can see for the implementation of B, that B's implementation is just an `XFunc` struct stacked on an `AFunc` struct. The problem we have is that when we have a pointer to `B` now, it is really pointing to an `XFunc` struct. So if were were to call a non-virtual function on an `A*` which is pointing to a `B`, the non-virtual function won't access the correct memory.



## Scratch - Constructors Came around when I didn't initialize my_i

```cpp
class AFunc {

    int my_i;
public:
    virtual int a_func() = 0;

    int non_virtual_a_func() {
        return my_i;
    }
};

class B : public AFunc {
public:
    virtual int a_func() {return 3;}
};


int main() {
    B bb;

    AFunc* bba = &bb;

    return bba->non_virtual_a_func();
}
```

I moved `B bb` into a stack variable because a lot more boilerplate code is created when a class is non-trivially constructed in a static context. Unfortunately, now gcc is spitting out constructors.

```assembly
AFunc::non_virtual_a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     rax, QWORD PTR [rbp-8]
        mov     eax, DWORD PTR [rax+8]
        pop     rbp
        ret
B::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 3
        pop     rbp
        ret
AFunc::AFunc() [base object constructor]:
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     edx, OFFSET FLAT:vtable for AFunc+16
        mov     rax, QWORD PTR [rbp-8]
        mov     QWORD PTR [rax], rdx
        nop
        pop     rbp
        ret
B::B() [base object constructor]:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 16
        mov     QWORD PTR [rbp-8], rdi
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    AFunc::AFunc() [base object constructor]

        # This is where the vtable is assigned to the B instance.
        # This overides the vtable that was assigned in AFunc() constructor
        mov     edx, OFFSET FLAT:vtable for B+16
        mov     rax, QWORD PTR [rbp-8]
        mov     QWORD PTR [rax], rdx
        nop
        leave
        ret
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 32
        lea     rax, [rbp-32]
        mov     rdi, rax
        call    B::B() [complete object constructor]
        lea     rax, [rbp-32]
        mov     QWORD PTR [rbp-8], rax
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    AFunc::non_virtual_a_func()
        nop
        leave
        ret
vtable for B:
        .quad   0
        .quad   0
        .quad   B::a_func()
vtable for AFunc:
        .quad   0
        .quad   0
        .quad   __cxa_pure_virtual
```

```c
struct AFunc {
       struct AVTable* vtable_ptr; 
       int my_i;
};

struct AFuncVTable {
        int (*a_func)(struct AFunc*);
};
struct AFuncVTable vtable_for_afunc;


void AFunc_constructor(struct AFunc* self) {
        self->vtable_ptr = &vtable_for_afunc;
}

int AFunc_non_virtual_a_func(struct AFunc* self) {
        // Regardless of what is passed in here
        // struct AFunc {
        //      struct AFuncVTable* vtable_ptr; 
        //      int my_i;                                  <--- self->afunc_data will always look here!
        // }
        // 
        // suppose we pass in the following struct as AFunc
        // struct DerivedFromAFunc 
        //      struct DerivedFromAFuncVTable* vtable_ptr; 
        //                                                 <--- Uhh oh! Nothing here, will return garbage.
        // }
        return self->my_i;
}

struct B {
        struct BVTable* vtable_ptr;
        int my_i; // Necessary for structural compatibility.
};

struct BVTable {
        int (*a_func)(struct B*);
};
struct BVTable vtable_for_b;

void B_constructor(struct B* self) {
        AFunc_constructor(self);
        self->vtable_ptr = &vtable_for_b;
}

int impl_a_func_for_b(struct B* self) {
        return 3;
}

int my_virtual_dispatch(struct AFunc* obj) {
        AFunc_non_virtual_a_func(obj);

        return obj->vtable_ptr->a_func(obj);
}

int main() 
{       
        // Normally done at compile time.
        vtable_for_a.a_func = (void*)0; // Pure virtual, normally compiler error, but will crash in C.
        vtable_for_b.a_func = &impl_a_func_for_b;

        struct B bb;
        B_constructor(&bb);

        return my_virtual_dispatch((struct AFunc*)&bb);
}
```

Here's where this gets hairy. What if we inherit from multiple class that aren't strictly pure virtual? Perhaps we start with inheriting from two classes with only virtual functions.


## Notes

### Compiler Layout Dumps

You can tell clang to dump the virtual table layout via `-Xclang -fdump-vtable-layouts`, which produces something like

For this input:

```
class AFunc {
    int my_a = 2;
public:
    virtual int a_func() {return 0;};
};

class XFunc {
    int my_x = 0;
public:
    virtual int x_func() {return 3;}
};


class B : public XFunc, public AFunc {
        int my_b = 3;
public:
    virtual int a_func() {return 5;}
    virtual int b_func() {return 5;}
};

B bb;

int main() {
    AFunc* bba = &bb;

    return bba->a_func();
}
```

Gives

```
Vtable for 'B' (8 entries).
   0 | offset_to_top (0)
   1 | B RTTI
       -- (B, 0) vtable address --
       -- (XFunc, 0) vtable address --
   2 | int XFunc::x_func()
   3 | int B::a_func()
   4 | int B::b_func()
   5 | offset_to_top (-16)
   6 | B RTTI
       -- (AFunc, 16) vtable address --
   7 | int B::a_func()
       [this adjustment: -16 non-virtual]

Thunks for 'int B::a_func()' (1 entry).
   0 | this adjustment: -16 non-virtual

VTable indices for 'B' (2 entries).
   1 | int B::a_func()
   2 | int B::b_func()

Vtable for 'AFunc' (3 entries).
   0 | offset_to_top (0)
   1 | AFunc RTTI
       -- (AFunc, 0) vtable address --
   2 | int AFunc::a_func()

VTable indices for 'AFunc' (1 entries).
   0 | int AFunc::a_func()

Vtable for 'XFunc' (3 entries).
   0 | offset_to_top (0)
   1 | XFunc RTTI
       -- (XFunc, 0) vtable address --
   2 | int XFunc::x_func()

VTable indices for 'XFunc' (1 entries).
   0 | int XFunc::x_func()

Compiler returned: 0

```

You can tell MSVC to do something similar. `/d1reportAllClassLayout`, however this produces a lot of output. Perhaps better to use the single class version. Note however that you can only specify one class. `/GR-` is MSVC for `-fno-rtti`.
```
/d1reportSingleClassLayoutXFunc
/d1reportSingleClassLayoutAFunc
/d1reportSingleClassLayoutXB
```

The nice thing about MSVC is that it even includes the class layout.

```
/GR- /d1reportSingleClassLayoutB

example.cpp

class _s__RTTIBaseClassDescriptor	size(28):
	+---
 0	| pTypeDescriptor
 4	| numContainedBases
 8	| _PMD where
20	| attributes
24	| pClassDescriptor
	+---

class _s__RTTIBaseClassArray	size(1):
	+---
 0	| arrayOfBaseClassDescriptors
	+---

class B	size(20):
	+---
 0	| +--- (base class XFunc)
 0	| | {vfptr}
 4	| | my_x
	| +---
 8	| +--- (base class AFunc)
 8	| | {vfptr}
12	| | my_a
	| +---
16	| my_b
	+---

B::$vftable@XFunc@:
	| &B_meta
	|  0
 0	| &XFunc::x_func 
 1	| &B::b_func 

B::$vftable@AFunc@:
	| -8
 0	| &B::a_func 

B::a_func this adjustor: 8
B::b_func this adjustor: 0
Compiler returned: 0
```

## Misc

The `top_pointer` in the vtables produced by GCC seem to only be needed for dynamic_cast?? I can't think of a reason why that would be necessary otherwise. What's weird is that it's present even when `-fno-rtti` is specified, which means dynamic cast won't work anyway??

```
class A {
    int a_1 = 1;
public:
    virtual int a_func() {return a_1;}
};

class X {
public:
    virtual int x_func() {return 4;}
    virtual int x_foo() {return 3;}
};

class Y : public X {
public:
    virtual int x_foo() {return 9;} 
    virtual int y_func() {return 5;}
};

class B : public A, public Y {
    int b_2 = 2;
public:
    virtual int b_func() {return 2;} 
    virtual int a_func() {return b_2;}
};

A aa;
B bb;

int main() {
    A* paa = &aa;
    int paa_val = paa->a_func();

    A* pba = &bb;
    int pba_val = pba->a_func();

    return paa_val + pba_val;
}
```

```
A::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     rax, QWORD PTR [rbp-8]
        mov     eax, DWORD PTR [rax+8]
        pop     rbp
        ret
X::x_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 4
        pop     rbp
        ret
Y::x_foo():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 9
        pop     rbp
        ret
Y::y_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 5
        pop     rbp
        ret
B::b_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     eax, 2
        pop     rbp
        ret
B::a_func():
        push    rbp
        mov     rbp, rsp
        mov     QWORD PTR [rbp-8], rdi
        mov     rax, QWORD PTR [rbp-8]
        mov     eax, DWORD PTR [rax+24]
        pop     rbp
        ret
aa:
        .quad   vtable for A+16
        .long   1
        .zero   4

# struct B {
#    BVTable* vtable;
#    u32 b_2;
#    u32 _padding = 0;
# }
#
bb:
        .quad   vtable for B+16
        .long   1
        .zero   4
        .quad   vtable for B+48
        .long   2
        .zero   4
main:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 32
        mov     QWORD PTR [rbp-8], OFFSET FLAT:aa
        mov     rax, QWORD PTR [rbp-8]
        mov     rax, QWORD PTR [rax]
        mov     rdx, QWORD PTR [rax]
        mov     rax, QWORD PTR [rbp-8]
        mov     rdi, rax
        call    rdx
        mov     DWORD PTR [rbp-12], eax

        # Load bb* into address at [rbp-24]; 
        mov     QWORD PTR [rbp-24], OFFSET FLAT:bb

        # Load (bb*)->vtable* into rax (BVTable* into rax)
        mov     rax, QWORD PTR [rbp-24]

        # Load ((bb*)->vtable*)->a_func (which is type a_func*) in rax
        mov     rax, QWORD PTR [rax]
        mov     rdx, QWORD PTR [rax]
        mov     rax, QWORD PTR [rbp-24]
        mov     rdi, rax
        call    rdx
        mov     DWORD PTR [rbp-28], eax
        mov     edx, DWORD PTR [rbp-12]
        mov     eax, DWORD PTR [rbp-28]
        add     eax, edx
        leave
        ret

# struct BVTable {
#         // vtable for class A impls (and B impls)
#         u32 top_offset;
#         u32 empty_type_information; // empty because -fno-rtti
#         a_func* a_func;
#         b_func* b_func;
# 
#         // vtable for Y
#         ui32 top_offset;
#         u32 empty_type_information; // empty because -fno-rtti
#         x_func* x_func;
#         x_foo*  x_foo;
#         y_func* y_func;
# }
vtable for B:
        .quad   0
        .quad   0
        .quad   B::a_func()
        .quad   B::b_func()
        .quad   -16
        .quad   0
        .quad   X::x_func()
        .quad   Y::x_foo()
        .quad   Y::y_func()
vtable for A:
        .quad   0
        .quad   0
        .quad   A::a_func()
```