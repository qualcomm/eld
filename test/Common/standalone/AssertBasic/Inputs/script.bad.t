u = 1;
v = 3;
w = 5;
ASSERT(u * v * w == 14, "Incorrect basic multiplication!")
SECTIONS {
  ASSERT(u + v + w == 9, "Incorrect basic addition!")
}