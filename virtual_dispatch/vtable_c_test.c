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

#include <stdio.h>
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

        int res = bba->vtable_ptr->a_func(bba);

        printf("res: %d\r\n", res);
        return 0;
}