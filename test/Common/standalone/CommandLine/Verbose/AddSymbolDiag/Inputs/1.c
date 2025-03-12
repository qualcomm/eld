int from_static(int);
int from_shared(double);

int foo(int i) {
    return from_static(i);
}

int bar(double d) {
    return from_shared(d);
}

int main() {
    return 0;
}

double double_variable = 17;