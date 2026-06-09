ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    . = 0x00000000;

    .text :
    {
        *(.text.startup)
        *(.text)
    } > RAM

    .bss (NOLOAD) :
    {
        *(.bss)
        . = ORIGIN(RAM) + LENGTH(RAM);
    } > RAM
}