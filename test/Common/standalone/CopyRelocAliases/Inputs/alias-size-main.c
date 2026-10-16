extern unsigned char small_alias;
extern unsigned int full_alias;

int main(void) {
  void *volatile small = &small_alias;
  void *volatile full = &full_alias;

  return small != full || full_alias != 0x12345678u;
}
