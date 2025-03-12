SECTIONS {
	.text : { . = . + 10;
                  *(.text.foo)
                  . = . + 50;
                  *(.text.bar)
                  . = . + 80;
                  *(.text.baz)
                  . = . + 100;
                } =0x00c0007f
        . = . + 200;
}
