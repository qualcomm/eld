" ftdetect for ELD linker map files and linker scripts

" Map files: *.map whose first line starts with "# Linker"
au BufRead,BufNewFile *.map call s:DetectEldMap()

" Linker scripts: common extensions
au BufRead,BufNewFile *.lcs,*.lcs.template,*.ld,*.lds,*.t,*.x  setfiletype eld

function! s:DetectEldMap()
  if getline(1) =~# '^# Linker\b'
    set filetype=eld
  endif
endfunction
