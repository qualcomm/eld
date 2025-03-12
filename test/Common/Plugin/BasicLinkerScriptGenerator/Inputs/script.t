SECTIONS {
  .foo : { *(*foo*) BYTE(15) }
  .bar : { *(*bar*) SHORT(2) }
  .baz : { *(*baz*) LONG(3) }
  .text : {
    *(*text*)
    QUAD(0xabab)
    SQUAD(0xefef)
  }
}

LINKER_PLUGIN("BasicLinkerScriptGenerator", "BasicLinkerScriptGenerator")