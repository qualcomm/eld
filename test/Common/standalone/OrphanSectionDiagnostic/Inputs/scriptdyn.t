SECTIONS {
    .varfoo : {*(.text.foo*)}
    .hash : {*(.hash)}
    .text : {*(.text)}
    .hash : {*(.hash)}
    .interp : {*(.interp)}
    .dynamic : {*(.dynamic)}
}