#include <cstdlib>

class MyClass {
  public:
    void doSomething() {}
};

int check(int *param) {
    int x = 10;
    int *p1, *p2 = &x;

    if (p1 == nullptr) {
        p1 = &x;
    } else if (p2) {
        p2 = (int *)malloc(4);
        free(p2);
    } else
        *p2 = 10;

    if (p2 != nullptr) {
        p2 = new int;
        delete p2;
    }

    for (int i = 0; i < 10; i++) {
        x = i + 5;
        break;
    }

    while (p1);

    MyClass obj;
    MyClass *pObj = &obj;
    pObj->doSomething();

    return *p2;
}