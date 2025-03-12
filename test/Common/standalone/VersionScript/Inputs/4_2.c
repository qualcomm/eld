int data = 1;
int common;
extern int func_dso();
int func_exe() {
return data;
}
int main() {
common = ++data;
return common + func_dso();
}
