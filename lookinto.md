# Things to look into
- How exactly do lambdas work?
It doesn't seem like you can use a member function as a callback directly. E.g. a function with the signature:
void callMe(std::function<void(int)>);
Cannot be called like:
class C {
    void f() {}
    C() {
        callMe(&f); // compiler error.
    }
}

But it can be done with lambdas:
class C {
    void f() {}
    C() {
        callMe([this](){this->f();}); // ok.
    }
}

- Memory allocation is a bit of a pain point for me. As well as exact rules for copyability etc. What are some general rules-of-thumb when doing some common tasks like:
    - allocating/deallocating objects on the fly
