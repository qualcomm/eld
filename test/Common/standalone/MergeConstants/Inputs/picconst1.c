const float *fp1(void) {
  static const float c = 3.5f;
  return &c;
}

float scale1(void) { return *fp1(); }
