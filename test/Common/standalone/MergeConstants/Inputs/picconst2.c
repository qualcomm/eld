const float *fp2(void) {
  static const float c = 3.5f;
  return &c;
}

float scale2(void) { return *fp2(); }
