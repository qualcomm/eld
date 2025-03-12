__thread int a = 0;
__thread int b = 1;
__thread int c = 2;
extern int _TLS_START_, _TLS_DATA_END_, _TLS_END_;

int d = 4;

int main () {
  a = 10;
  return a+b+c+d + _TLS_START_ + _TLS_DATA_END_ + _TLS_END_;
}
