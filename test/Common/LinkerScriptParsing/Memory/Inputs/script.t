MEMORY {
  A (rx) : ORIGIN = 0x100, LENGTH = 0x200 + 0x20
  B (rwx) : org = 300, len = 400 + 0x15
  C (a) : o = 0x500, l = 600
  D (RX) : ORIGIN = 0x100, LENGTH = 0x200 + 0x20
  E (RWX) : org = 300, len = 400 + 0x15
  C (A) : o = 0x500, l = 600
}