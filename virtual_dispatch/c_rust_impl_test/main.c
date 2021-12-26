#include <stdio.h>

struct AFunc {
        int (*a_func)(void*);
};

struct A {};

struct ImplAFuncForAVTable {
        int (*a_func)(struct A*);
};

int impl_a_func_for_a(struct A* self) {
        printf("impl_a_func_for_b\r\n");
        return 1;
}

struct B {};

struct ImplAFuncForBVTable {
        int (*a_func)(struct B*);
};

int impl_a_func_for_b(struct B* self) {
        printf("impl_a_func_for_b\r\n");
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