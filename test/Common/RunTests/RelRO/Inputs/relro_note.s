        .section ".note.eld.run","a",%note
        .global relro_phdrs_note_marker
relro_phdrs_note_marker:
        .word 0
