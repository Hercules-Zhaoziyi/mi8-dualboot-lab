SECTIONS { . = 0; .text : { *(.text*) *(.rodata*) } /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) } }
