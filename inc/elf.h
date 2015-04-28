#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H

#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */

/*
 * RELATION:
 * ELF hdr tells about proghdrs and secthdrs
 * each proghdr consists of one or more sects
 */

struct Elf {
	uint32_t e_magic;	// must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	// va to which address first transfers control, thus
	// starting the process
	uint32_t e_entry;
	// program header table's file offset in bytes
	uint32_t e_phoff;
	// section header table's file offset in bytes
	uint32_t e_shoff;
	uint32_t e_flags;
	// elf header section in bytes
	uint16_t e_ehsize;
	// one entry in the file's program header table; all entries are the same
	uint16_t e_phentsize;
	// number of entries in the program header table
	uint16_t e_phnum;
	// one entry in the file's section header; all entries are the same
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

// program header table is an array of structures, each describing a
// segment or other info that system needs to prepare the program for exec
// an object file segment contains one or more sections
struct Proghdr {
	uint32_t p_type;
	uint32_t p_offset;
	//va at which the first byte of the segment resides in memory
	uint32_t p_va;
	uint32_t p_pa;
	// num of bytes in the file image of the segment
	uint32_t p_filesz;
	// num of bytes in the memory image of the segment
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
};

// section header table is an array of Secthdr structs;
// e_shoff gives the byte offset from the beginning of the file to
// the section hdr table;
// e_shnum tells how many entries the section header table contains;
// e_shentsize gives the size in bytes of each entry;
struct Secthdr {
	// name of section; value is an index into section header
	// string table section
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	// gives the address at which the section's first byte should reside
	uint32_t sh_addr;
	// offset from beginning of file to the first byte in sect
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
};

// Values for Proghdr::p_type
#define ELF_PROG_LOAD		1

// Flag bits for Proghdr::p_flags
#define ELF_PROG_FLAG_EXEC	1
#define ELF_PROG_FLAG_WRITE	2
#define ELF_PROG_FLAG_READ	4

// Values for Secthdr::sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// Values for Secthdr::sh_name
#define ELF_SHN_UNDEF		0

#endif /* !JOS_INC_ELF_H */
