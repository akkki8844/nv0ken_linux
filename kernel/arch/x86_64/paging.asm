bits 64

global paging_invalidate_page

section .text
paging_invalidate_page:
    invlpg [rdi]
    ret
