/* linker.ld : x86_64 higher-half 内核 */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

PHDRS {
  requests PT_LOAD;
  text     PT_LOAD;
  rodata   PT_LOAD;
  data     PT_LOAD;
}

SECTIONS
{
  /* 关键：把链接地址放到高半区（典型基址） */
  . = 0xffffffff80000000;

  /* Limine 请求段，必须保留并单独做一个 PT_LOAD */
  .limine_requests : {
    KEEP(*(.limine_requests_start))
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_end))
  } :requests

  . = ALIGN(0x1000);
  .text : { *(.text .text.*) } :text

  . = ALIGN(0x1000);
  .rodata : {
    *(.rodata .rodata.*)
    *(.note.gnu.build-id)
  } :rodata

  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } :data

  .bss : { *(.bss .bss.*) *(COMMON) } :data
}
