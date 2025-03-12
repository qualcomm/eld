char a_common;
short b_common;
int c_common;
long long d_common;
double e_common[100];
int f_common[100];

char increment_a_common() {
    return ++a_common;
}

int foo() { return 0; }
int asdf() { return 0; }

int bar(char a, int f) {
    return a + f + c_common + foo() + asdf();
}

int main() {
    f_common[0] = 11;
    c_common = 13;
    return bar(increment_a_common(), f_common[0]);
}