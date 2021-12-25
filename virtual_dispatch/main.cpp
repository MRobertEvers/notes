#include <iostream>
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
    A* pba = &bb;

    int paa_val = paa->a_func();
    int pba_val = pba->a_func();

    return paa_val + pba_val;
}