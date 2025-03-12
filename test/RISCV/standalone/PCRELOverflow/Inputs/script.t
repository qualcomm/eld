MEMORY {
  RAM (rwx) : ORIGIN = 0x180000000, LENGTH = ((262144) << 10)
}

SECTIONS {
  text : { *(.text*) } > RAM
  data : { *(.data); *(.sdata) } > RAM

  /* out of range for auipc + addi, in range for lui + addi */
  PROVIDE(__var1 = 0x1);
  /* out of range for auipc + addi and lui + addi */
  PROVIDE(__var2 = 0x88000000);
  /* should be in range for auipc + addi but out of range for lui + addi */
  PROVIDE(__var3 = 0x110000000);
  /* should be out of range for everything */
  PROVIDE(__var4 = 0x210000000);
}
