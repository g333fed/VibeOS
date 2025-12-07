/*
 * VibeOS ELF64 Loader
 */

#include "elf.h"
#include "string.h"
#include "printf.h"
#include <stddef.h>

int elf_validate(const void *data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) {
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

    // Check magic
    if (ehdr->e_ident[EI_MAG0] != 0x7F ||
        ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' ||
        ehdr->e_ident[EI_MAG3] != 'F') {
        return -2;
    }

    // Check class (64-bit)
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return -3;
    }

    // Check endianness (little endian)
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return -4;
    }

    // Check machine type (AArch64)
    if (ehdr->e_machine != EM_AARCH64) {
        return -5;
    }

    // Check type (executable)
    if (ehdr->e_type != ET_EXEC) {
        return -6;
    }

    return 0;
}

uint64_t elf_entry(const void *data) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;
    return ehdr->e_entry;
}

uint64_t elf_load(const void *data, size_t size) {
    int valid = elf_validate(data, size);
    if (valid != 0) {
        printf("[ELF] Invalid ELF: error %d\n", valid);
        return 0;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;
    const uint8_t *base = (const uint8_t *)data;

    printf("[ELF] Loading %d program headers\n", ehdr->e_phnum);

    // Process program headers
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(base + ehdr->e_phoff + i * ehdr->e_phentsize);

        // Only load PT_LOAD segments
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        printf("[ELF] LOAD: vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
               phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz);

        // Copy segment data
        void *dest = (void *)phdr->p_vaddr;
        printf("[ELF] Copying %lu bytes to %p\n", phdr->p_filesz, dest);
        const void *src = base + phdr->p_offset;

        // Copy file contents
        if (phdr->p_filesz > 0) {
            memcpy(dest, src, phdr->p_filesz);
        }

        // Zero out any remaining memory (BSS)
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((uint8_t *)dest + phdr->p_filesz, 0,
                   phdr->p_memsz - phdr->p_filesz);
        }
    }

    printf("[ELF] Entry point: 0x%lx\n", ehdr->e_entry);

    // Debug: dump first few instructions at entry
    uint32_t *code = (uint32_t *)ehdr->e_entry;
    printf("[ELF] Code at entry: %08x %08x %08x %08x\n",
           code[0], code[1], code[2], code[3]);

    return ehdr->e_entry;
}
