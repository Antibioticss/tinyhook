
.section __TEXT,__text,regular,pure_instructions

.globl _decode

_decode:
    .incbin "src/fde64/decode.bin"
    // This binary is from https://github.com/Antibioticss/fde64
