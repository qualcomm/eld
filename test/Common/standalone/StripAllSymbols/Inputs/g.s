.global foo
foo:
.word local
.global bar
bar:
.word local
.local local
local:
.word 100
