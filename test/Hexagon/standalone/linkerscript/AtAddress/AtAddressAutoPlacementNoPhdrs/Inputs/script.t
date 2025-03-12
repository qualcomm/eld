SECTIONS {
.bar (0x300) : {
                  *(.bar*)
                  *(.foo1)
                  *(.foo2)
                  *(.foo3)
                  *(.foo)
               }
}
