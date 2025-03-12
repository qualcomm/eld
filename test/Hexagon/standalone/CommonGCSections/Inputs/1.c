char a_common;
short b_common;
int c_common;
long long d_common;
double e_common[100];
int f_common[100];

char increment_a_common() {
    return ++a_common;
}

int bar(char a, int f) {
    return a + f;
}

int main() {
    return bar(increment_a_common(), f_common[0]);
}