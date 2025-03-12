//.tbss & .tdata
__thread int x = 1;
__thread int y;

int main () {
  y= 0;
    return x+y;
}
