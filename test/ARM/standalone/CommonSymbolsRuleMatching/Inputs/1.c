char a_common;
char a1_common;
short b_common;
int c_common;
int c1_common;
long long d_common;
double e_common[100];
int f_common[100];

int bar(char a, int f) {
    return a + f;
}

int main() {
    return bar(a_common, f_common[0]);
}