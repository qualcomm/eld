SECTIONS {
.text : { *(.text.foo)
          *(.text.bar)
          *(.text.baz)
        }
/DISCARD/ : {
              KEEP (*(.text.discard))
              *(.ARM.exidx*)
            }
}
