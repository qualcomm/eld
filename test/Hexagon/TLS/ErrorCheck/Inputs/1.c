__thread int a = 1;
__thread int b = 2;
__thread int c = 3;
__thread int e;
__thread int d;

int main () {
e  =1;
d = 1;
return a + b + c + d + e;
}

