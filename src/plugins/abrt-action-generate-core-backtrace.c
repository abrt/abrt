/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "libabrt.h"
#include <btparser/core-backtrace.h>

/* NOTE: ENABLE_DISASSEMBLY should be only enabled on x86_64 as the code won't
 * work anywhere else. The configure script should take care of this. */
#ifdef ENABLE_DISASSEMBLY
#include <libelf.h>
#include <gelf.h>
#include <elfutils/libdw.h>
#include <dwarf.h>

#include <bfd.h>
#include <dis-asm.h>
#endif /* ENABLE_DISASSEMBLY */

/* mostly copypasted from abrt-action-generate-backtrace */
static char *get_gdb_output(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    char *uid_str = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
    uid_t uid = -1L;
    if (uid_str)
    {
        uid = xatoi_positive(uid_str);
        free(uid_str);
        if (uid == geteuid())
        {
            uid = -1L; /* no need to setuid/gid if we are already under right uid */
        }
    }
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    char *args[11];
    args[0] = (char*)"gdb";
    args[1] = (char*)"-batch";

    /* NOTE: We used to use additional dirs here, but we don't need them. Maybe
     * we don't need 'set debug-file-directory ...' at all?
     */
    args[2] = (char*)"-ex";
    args[3] = (char*)"set debug-file-directory /usr/lib/debug";

    /* "file BINARY_FILE" is needed, without it gdb cannot properly
     * unwind the stack. Currently the unwind information is located
     * in .eh_frame which is stored only in binary, not in coredump
     * or debuginfo.
     *
     * Fedora GDB does not strictly need it, it will find the binary
     * by its build-id.  But for binaries either without build-id
     * (= built on non-Fedora GCC) or which do not have
     * their debuginfo rpm installed gdb would not find BINARY_FILE
     * so it is still makes sense to supply "file BINARY_FILE".
     *
     * Unfortunately, "file BINARY_FILE" doesn't work well if BINARY_FILE
     * was deleted (as often happens during system updates):
     * gdb uses specified BINARY_FILE
     * even if it is completely unrelated to the coredump.
     * See https://bugzilla.redhat.com/show_bug.cgi?id=525721
     *
     * TODO: check mtimes on COREFILE and BINARY_FILE and not supply
     * BINARY_FILE if it is newer (to at least avoid gdb complaining).
     */
    args[4] = (char*)"-ex";
    args[5] = xasprintf("file %s", executable);
    free(executable);

    args[6] = (char*)"-ex";
    args[7] = xasprintf("core-file %s/"FILENAME_COREDUMP, dump_dir_name);

    args[8] = (char*)"-ex";
    /*args[9] = ... see below */
    args[10] = NULL;

    /* Get the backtrace, but try to cap its size */
    /* Limit bt depth. With no limit, gdb sometimes OOMs the machine */
    unsigned bt_depth = 2048;
    char *bt = NULL;
    while (1)
    {
        args[9] = xasprintf("backtrace %u", bt_depth);
        bt = exec_vp(args, uid, /*redirect_stderr:*/ 1, /*exec_timeout_sec:*/ 240, NULL);
        free(args[9]);
        if ((bt && strnlen(bt, 256*1024) < 256*1024) || bt_depth <= 32)
        {
            break;
        }

        free(bt);
        bt_depth /= 2;
    }

    free(args[5]);
    free(args[7]);
    return bt;
}

#ifdef ENABLE_DISASSEMBLY

/* Global iteration limit for algorithms walking call graphs. */
static const unsigned call_graph_iteration_limit = 512;

/* When using hash tables, we need to have addresses of the disassembled
 * program as keys. We could either embed the addresses directly into the
 * pointer (glib supports that), but that wouldn't allow us to disassemble
 * 64-bit programs on 32-bit hosts. So we're using this function to allocate
 * space for the pointer and store it there.
 *
 * Note that currently, the code won't work for any other situation than x86-64
 * host disassembling x86-64 file anyway. This issue should be resolved when
 * migrating the code to btparser -- either make it fully portable or take the
 * restriction into account and don't do any of this pointer nonsense.
 */
static uintptr_t* addr_alloc(uintptr_t addr)
{
    uintptr_t *a = xmalloc(sizeof(*a));
    *a = addr;
    return a;
}

/* Copies the mnemonic of `insn` to the (preallocated) buffer `buf` */
static void insn_mnemonic(const char *insn, char *buf)
{
    while (*insn && !isspace(*insn))
    {
        *buf++ = *insn++;
    }
    *buf = '\0';
}

static bool insn_is_one_of(const char *insn, const char **mnem_list)
{
    char mnembuf[256];
    insn_mnemonic(insn, mnembuf);

    const char **p;
    for (p = mnem_list; *p != NULL; p++)
    {
        if (strcmp(*p, mnembuf) == 0)
        {
            return true;
        }
    }

    return false;
}

static bool insn_has_single_addr_operand(const char *insn, uintptr_t *dest)
{
    int ret, chars_read;
    const char *p = insn;
    uintptr_t addr;

    /* mnemonic */
    p = skip_non_whitespace(p);

    /* space */
    p = skip_whitespace(p);

    /* address */
    ret = sscanf(p, "%jx %n", &addr, &chars_read);
    if (ret < 1)
    {
        return false;
    }

    /* check that there is nothing else after the address */
    p += chars_read;
    if(*p != '\0')
    {
        return false;
    }

    if (dest)
    {
        *dest = addr;
    }
    return true;
}

static void fingerprint_add_bool(struct strbuf *buffer, const char *name, bool value)
{
    strbuf_append_strf(buffer, "%s:%d", name, value ? 1 : 0);
}

static void fingerprint_add_list(struct strbuf *buffer, const char *name, GList *strings)
{
    bool first = true;
    GList *it;

    strbuf_append_strf(buffer, "%s:", name);

    for (it = strings; it != NULL; it = g_list_next(it))
    {
        strbuf_append_strf(buffer, "%s%s", (first ? "" : ","), (char*)it->data);
        first = false;
    }

    if (first)
    {
        strbuf_append_strf(buffer, "-");
    }
}

static void instruction_present(struct strbuf *buffer, GList *insns, const char **mnem_list, const char *fp_name)
{
    bool found = false;

    GList *it;
    for (it = insns; it != NULL; it = g_list_next(it))
    {
        if (insn_is_one_of(it->data, mnem_list))
        {
            found = true;
            break;
        }
    }

    fingerprint_add_bool(buffer, fp_name, found);
}

static void fp_jump_equality(struct strbuf *buffer, GList *insns,
        uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"je", "jne", "jz", "jnz", NULL};
    instruction_present(buffer, insns, mnems, "j_eql");
}

static void fp_jump_signed(struct strbuf *buffer, GList *insns,
        uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"jg", "jl", "jnle", "jnge", "jng", "jnl", "jle", "jge", NULL};
    instruction_present(buffer, insns, mnems, "j_sgn");
}

static void fp_jump_unsigned(struct strbuf *buffer, GList *insns,
        uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"ja", "jb", "jnae", "jnbe", "jna", "jnb", "jbe", "jae", NULL};
    instruction_present(buffer, insns, mnems, "j_usn");
}

static void fp_and_or(struct strbuf *buffer, GList *insns,
        uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"and", "or", NULL};
    instruction_present(buffer, insns, mnems, "and_or");
}

static void fp_shift(struct strbuf *buffer, GList *insns,
        uintptr_t b, uintptr_t e, GHashTable *c, GHashTable *p)
{
    static const char *mnems[] = {"shl", "shr", NULL};
    instruction_present(buffer, insns, mnems, "shift");
}

static void fp_has_cycle(struct strbuf *buffer, GList *insns,
        uintptr_t begin, uintptr_t end, GHashTable *call_graph, GHashTable *plt)
{
    GList *it;
    static const char *jmp_mnems[] = {"jmp", "jmpb", "jmpw", "jmpl", "jmpq", NULL};
    bool found = false;

    for (it = insns; it != NULL; it = g_list_next(it))
    {
        char *insn = it->data;
        uintptr_t target;

        if (!insn_is_one_of(insn, jmp_mnems))
        {
            continue;
        }

        if (!insn_has_single_addr_operand(insn, &target))
        {
            continue;
        }

        if (begin <= target && target < end)
        {
            found = true;
            break;
        }
    }

    fingerprint_add_bool(buffer, "has_cycle", found);
}

static void fp_libcalls(struct strbuf *buffer, GList *insns,
        uintptr_t begin, uintptr_t end, GHashTable *call_graph, GHashTable *plt)
{
    GList *it, *sym = NULL;

    /* Look up begin in call graph */
    GList *callees = g_hash_table_lookup(call_graph, &begin);

    /* Resolve addresses to names if they are in PLT */
    for (it = callees; it != NULL; it = g_list_next(it))
    {
        char *s = g_hash_table_lookup(plt, it->data);
        if (s && !g_list_find_custom(sym, s, (GCompareFunc)strcmp))
        {
            sym = g_list_insert_sorted(sym, s, (GCompareFunc)strcmp);
        }
    }

    /* Format the result */
    fingerprint_add_list(buffer, "libcalls", sym);
}

static void fp_calltree_leaves(struct strbuf *buffer, GList *insns,
        uintptr_t begin, uintptr_t end, GHashTable *call_graph, GHashTable *plt)
{
    unsigned iterations_allowed = call_graph_iteration_limit;
    GHashTable *visited = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);
    GList *queue = g_list_append(NULL, addr_alloc(begin));
    GList *it, *sym = NULL;

    while (queue != NULL && iterations_allowed)
    {
        /* Pop one element */
        it = g_list_first(queue);
        queue = g_list_remove_link(queue, it);
        uintptr_t *key = (uintptr_t*)(it->data);
        /* uintptr_t addr = *key; */
        g_list_free(it);
        iterations_allowed--;

        /* Check if it is not already visited */
        if (g_hash_table_lookup_extended(visited, key, NULL, NULL))
        {
            free(key);
            continue;
        }
        g_hash_table_insert(visited, key, key);

        /* Lookup callees */
        GList *callees = g_hash_table_lookup(call_graph, key);

        /* If callee is PLT, add the corresponding symbols, otherwise
         * extend the worklist */
        for (it = callees; it != NULL; it = g_list_next(it))
        {
            char *s = g_hash_table_lookup(plt, it->data);
            if (s && !g_list_find_custom(sym, s, (GCompareFunc)strcmp))
            {
                sym = g_list_insert_sorted(sym, s, (GCompareFunc)strcmp);
            }
            else if (s == NULL)
            {
                queue = g_list_append(queue, addr_alloc(*(uintptr_t*)(it->data)));
            }
        }
    }
    g_hash_table_destroy(visited);
    list_free_with_free(queue);

    fingerprint_add_list(buffer, "calltree_leaves", sym);
}

/* Type of function that implements one fingerprint component.
 *
 * @param buf        string buffer the fingerprint will be appended to
 *                   format should be "fingerprint_name:fingerprint_value"
 * @param insns      list of machine instructions
 * @param begin      start address of the disassembled function
 * @param end        address after the last instruction
 * @param call_graph call graph in the form of hashtable (address -> list(address))
 * @param plt_names  mapping from PLT addresses to symbols of library functions
 */
typedef void (*fp_function_type)(struct strbuf* buf, GList* insns,
    uintptr_t begin, uintptr_t end, GHashTable* call_graph, GHashTable* plt_names);

static fp_function_type fp_components[] = {
    fp_jump_equality,
    fp_jump_signed,
    fp_jump_unsigned,
    fp_and_or,
    fp_shift,
    fp_has_cycle,
    fp_libcalls,
    fp_calltree_leaves,
    NULL
};

/* Read len bytes and interpret them as a number. Pointer p does not have to be
 * aligned.
 * XXX Assumption: we'll always run on architecture the ELF is run on,
 * therefore we don't consider byte order.
 */
static uintptr_t fde_read_address(const uint8_t *p, unsigned len)
{
    int i;
    union {
        uint8_t b[8];
        /* uint16_t n2; */
        uint32_t n4;
        uint64_t n8;
    } u;

    for (i = 0; i < len; i++)
    {
        u.b[i] = *p++;
    }

    return (len == 4 ? (uintptr_t)u.n4 : (uintptr_t)u.n8);
}

/* Given DWARF pointer encoding, return the length of the pointer in bytes.
 */
static unsigned encoded_size(const uint8_t encoding, const unsigned char *e_ident)
{
    switch (encoding & 0x07)
    {
        case DW_EH_PE_udata2:
            return 2;
        case DW_EH_PE_udata4:
            return 4;
        case DW_EH_PE_udata8:
            return 8;
        case DW_EH_PE_absptr:
            return (e_ident[EI_CLASS] == ELFCLASS32 ? 4 : 8);
        default:
            return 0; /* Don't know/care. */
    }
}

static void log_elf_error(const char *function, const char *filename)
{
    log("%s failed for %s: %s", function, filename, elf_errmsg(-1));
}

/*
 * @param e         elf handle
 * @param name      name of section to be found
 * @param filename  filename for logging messages
 * @param dest      save the resulting elf data pointer here (can be NULL)
 * @param shdr_dest save the section header here (can be NULL)
 * @returns zero on error, index of the section on success
 */
static unsigned xelf_section_by_name(Elf *e, const char *name, const char *filename, Elf_Data **dest, GElf_Shdr *shdr_dest)
{
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    unsigned section_index = 0;
    size_t shstrndx;

    /* Find the string table index */
    if (elf_getshdrstrndx(e, &shstrndx) != 0)
    {
        VERB1 log_elf_error("elf_getshdrstrndx", filename);
        return 0;
    }

    while ((scn = elf_nextscn(e, scn)) != NULL)
    {
        section_index++; /* starting index is 1 */

        if (gelf_getshdr(scn, &shdr) != &shdr)
        {
            VERB1 log_elf_error("gelf_getshdr", filename);
            continue;
        }

        const char *scnname = elf_strptr(e, shstrndx, shdr.sh_name);
        if (scnname == NULL)
        {
            VERB1 log_elf_error("elf_strptr", filename);
            continue;
        }

        if (strcmp(scnname, name) == 0)
        {
            /* Found, save data */
            if (dest)
            {
                *dest = elf_getdata(scn, NULL);
                if (*dest == NULL)
                {
                    VERB1 log_elf_error("elf_getdata", filename);
                    break;
                }
            }

            /* save shdr */
            if (shdr_dest)
            {
                *shdr_dest = shdr;
            }

            return section_index;
        }
    }

    VERB1 log("Section %s not found in %s\n", name, filename);
    return 0;
}

static GHashTable* parse_plt(Elf *e, const char *filename)
{
    GElf_Shdr shdr;

    Elf_Data *plt_data;
    uintptr_t plt_base;
    size_t plt_section_index = xelf_section_by_name(e, ".plt", filename, &plt_data, &shdr);
    if (plt_section_index == 0)
    {
        VERB1 log("No .plt section found for %s", filename);
        return NULL;
    }
    plt_base = shdr.sh_addr;

    /* Find the relocation section for .plt (typically .rela.plt), together
     * with its symbol and string table
     */
    Elf_Data *rela_plt_data = NULL;
    Elf_Data *plt_symbols = NULL;
    size_t stringtable = 0;
    Elf_Scn *scn = NULL;
    while ((scn = elf_nextscn(e, scn)) != NULL)
    {
        if (gelf_getshdr(scn, &shdr) != &shdr)
        {
            VERB1 log_elf_error("gelf_getshdr", filename);
            continue;
        }

        if (shdr.sh_type == SHT_RELA && shdr.sh_info == plt_section_index)
        {
            rela_plt_data = elf_getdata(scn, NULL);
            if (rela_plt_data == NULL)
            {
                VERB1 log_elf_error("elf_getdata", filename);
                break;
            }

            /* Get symbol section for .rela.plt */
            Elf_Scn *symscn = elf_getscn(e, shdr.sh_link);
            if (symscn == NULL)
            {
                VERB1 log_elf_error("elf_getscn", filename);
                break;
            }

            plt_symbols = elf_getdata(symscn, NULL);
            if (plt_symbols == NULL)
            {
                VERB1 log_elf_error("elf_getdata", filename);
                break;
            }

            /* Get string table for the symbol table. */
            if (gelf_getshdr(symscn, &shdr) != &shdr)
            {
                VERB1 log_elf_error("gelf_getshdr", filename);
                break;
            }

            stringtable = shdr.sh_link;
            break;
        }
    }

    if (stringtable == 0)
    {
        VERB1 log("Unable to read symbol table for .plt for file %s", filename);
        return NULL;
    }

    /* Init hash table
     * keys are pointers to integers which we allocate with malloc
     * values are owned by libelf, so we don't need to free them */
    GHashTable *hash = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);

    /* PLT looks like this (see also AMD64 ABI, page 78):
     *
     * Disassembly of section .plt:
     *
     * 0000003463e01010 <attr_removef@plt-0x10>:
     *   3463e01010:   ff 35 2a 2c 20 00       pushq  0x202c2a(%rip)         <-- here is plt_base
     *   3463e01016:   ff 25 2c 2c 20 00       jmpq   *0x202c2c(%rip)            each "slot" is 16B wide
     *   3463e0101c:   0f 1f 40 00             nopl   0x0(%rax)                  0-th slot is skipped
     *
     * 0000003463e01020 <attr_removef@plt>:
     *   3463e01020:   ff 25 2a 2c 20 00       jmpq   *0x202c2a(%rip)
     *   3463e01026:   68 00 00 00 00          pushq  $0x0                   <-- this is the number we want
     *   3463e0102b:   e9 e0 ff ff ff          jmpq   3463e01010 <_init+0x18>
     *
     * 0000003463e01030 <fgetxattr@plt>:
     *   3463e01030:   ff 25 22 2c 20 00       jmpq   *0x202c22(%rip)
     *   3463e01036:   68 01 00 00 00          pushq  $0x1
     *   3463e0103b:   e9 d0 ff ff ff          jmpq   3463e01010 <_init+0x18>
     */

    unsigned plt_offset;
    uint32_t *plt_index;
    GElf_Rela rela;
    GElf_Sym symb;
    for (plt_offset = 16; plt_offset < plt_data->d_size; plt_offset += 16)
    {
        plt_index = (uint32_t*)(plt_data->d_buf + plt_offset + 7);

        if(gelf_getrela(rela_plt_data, *plt_index, &rela) != &rela)
        {
            VERB1 log_elf_error("gelf_getrela", filename);
            continue;
        }

        if(gelf_getsym(plt_symbols, GELF_R_SYM(rela.r_info), &symb) != &symb)
        {
            VERB1 log_elf_error("gelf_getsym", filename);
            continue;
        }

        char *symbol = elf_strptr(e, stringtable, symb.st_name);
        uintptr_t *addr = addr_alloc((uintptr_t)(plt_base + plt_offset));

        VERB3 log("[%02x] %jx: %s", *plt_index, (uintptr_t)(*addr), symbol);
        g_hash_table_insert(hash, addr, symbol);
    }

    return hash;
}

/* TODO: sensible types */
/* Load ELF 'filename', parse the .eh_frame contents, and for each entry in the
 * second argument check whether its address is contained in the range of some
 * Frame Description Entry. If it does, fill in the function range of the
 * entry. In other words, try to assign start address and length of function
 * corresponding to each backtrace entry. We'll need that for the disassembly.
 *
 * Fails quietly - we should still be able to use the build ids.
 *
 * I wonder if this is really better than parsing eu-readelf text output.
 */
static GHashTable *elf_iterate_fdes(const char *filename, GList *entries, Elf *e)
{
    const unsigned char *e_ident;
    Elf_Data *scn_data;
    GElf_Shdr shdr;
    GElf_Phdr phdr;
    size_t phnum;
    GHashTable *retval = NULL; /* NULL = error */

    e_ident = (unsigned char *)elf_getident(e, NULL);
    if (e_ident == NULL)
    {
        VERB1 log_elf_error("elf_getident", filename);
        return NULL;
    }

    /* Look up the .eh_frame section */
    if (!xelf_section_by_name(e, ".eh_frame", filename, &scn_data, &shdr))
    {
        VERB1 log("Section .eh_frame not found in %s", filename);
        return NULL;
    }

    /* Get the address at which the executable segment is loaded. If the
     * .eh_frame addresses are absolute, this is used to convert them to
     * relative to the beginning of executable segment. We are looking for the
     * first LOAD segment that is executable, I hope this is sufficient.
     */
    if (elf_getphdrnum(e, &phnum) != 0)
    {
        VERB1 log_elf_error("elf_getphdrnum", filename);
        return NULL;
    }

    uintptr_t exec_base;
    int i;
    for (i = 0; i < phnum; i++)
    {
        if (gelf_getphdr(e, i, &phdr) != &phdr)
        {
            VERB1 log_elf_error("gelf_getphdr", filename);
            return NULL;
        }

        if (phdr.p_type == PT_LOAD && phdr.p_flags & PF_X)
        {
            exec_base = (uintptr_t)phdr.p_vaddr;
            goto base_found;
        }
    }

    VERB1 log("Can't determine executable base for '%s'", filename);
    return NULL;

base_found:
    VERB2 log("Executable base: %jx", (uintmax_t)exec_base);

    /* We now have a handle to .eh_frame data. We'll use dwarf_next_cfi to
     * iterate through all FDEs looking for those matching the addresses we
     * have.
     * Some info on .eh_frame can be found at http://www.airs.com/blog/archives/460
     * and in DWARF documentation for .debug_frame. The initial_location and
     * address_range decoding is 'inspired' by elfutils source.
     * XXX: If this linear scan is too slow, we can do binary search on
     * .eh_frame_hdr -- see http://www.airs.com/blog/archives/462
     */
    int ret;
    Dwarf_Off cfi_offset;
    Dwarf_Off cfi_offset_next = 0;
    Dwarf_CFI_Entry cfi;

    struct cie_encoding {
        Dwarf_Off cie_offset;
        int ptr_len;
        bool pcrel;
    } *cie;
    GList *cie_list = NULL;

    /* Init hash table
     * keys are pointers to integers which we allocate with malloc
     * values stored directly */
    GHashTable *hash = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, NULL);

    while(1)
    {
        cfi_offset = cfi_offset_next;
        ret = dwarf_next_cfi(e_ident, scn_data, 1, cfi_offset, &cfi_offset_next, &cfi);

        if (ret > 0)
        {
            /* We're at the end. */
            break;
        }

        if (ret < 0)
        {
            /* Error. If cfi_offset_next was updated, we may skip the
             * erroneous cfi. */
            if (cfi_offset_next > cfi_offset)
            {
                continue;
            }
            VERB1 log("dwarf_next_cfi failed for %s: %s", filename, dwarf_errmsg(-1));
            goto ret_free;
        }

        if (dwarf_cfi_cie_p(&cfi))
        {
            /* Current CFI is a CIE. We store its offset and FDE encoding
             * attributes to be used when reading FDEs.
             */

            /* Default FDE encoding (i.e. no R in augmentation string) is
             * DW_EH_PE_absptr.
             */
            cie = xzalloc(sizeof(*cie));
            cie->cie_offset = cfi_offset;
            cie->ptr_len = encoded_size(DW_EH_PE_absptr, e_ident);

            /* Search the augmentation data for FDE pointer encoding.
             * Unfortunately, 'P' can come before 'R' (which we are looking
             * for), so we may have to parse the whole thing. See the
             * abovementioned blog post for details.
             */
            const char *aug = cfi.cie.augmentation;
            const uint8_t *augdata = cfi.cie.augmentation_data;
            bool skip_cie = 0;
            if (*aug == 'z')
            {
                aug++;
            }
            while (*aug != '\0')
            {
                if(*aug == 'R')
                {
                    cie->ptr_len = encoded_size(*augdata, e_ident);

                    if (cie->ptr_len != 4 && cie->ptr_len != 8)
                    {
                        VERB1 log("Unknown FDE encoding (CIE %jx) in %s",
                                (uintmax_t)cfi_offset, filename);
                        skip_cie = 1;
                    }
                    if ((*augdata & 0x70) == DW_EH_PE_pcrel)
                    {
                        cie->pcrel = 1;
                    }
                    break;
                }
                else if (*aug == 'L')
                {
                    augdata++;
                }
                else if (*aug == 'P')
                {
                    unsigned size = encoded_size(*augdata, e_ident);
                    if (size == 0)
                    {
                        VERB1 log("Unknown size for personality encoding in %s",
                                filename);
                        skip_cie = 1;
                        break;
                    }
                    augdata += (size + 1);
                }
                else
                {
                    VERB1 log("Unknown augmentation char in %s", filename);
                    skip_cie = 1;
                    break;
                }
                aug++;
            }
            if (skip_cie)
            {
                free(cie);
                continue;
            }

            cie_list = g_list_append(cie_list, cie);
        }
        else
        {
            /* Current CFI is an FDE.
             */
            GList *it = cie_list;
            cie = NULL;

            /* Find the CIE data that we should have saved earlier. XXX: We can
             * use hash table/tree to speed up the search, the number of CIEs
             * should usally be very low though. */
            while (it != NULL)
            {
                cie = it->data;

                /* In .eh_frame, CIE_pointer is relative, but libdw converts it
                 * to absolute offset. */
                if(cfi.fde.CIE_pointer == cie->cie_offset)
                {
                    break; /* Found. */
                }

                it = g_list_next(it);
            }

            if (it == NULL)
            {
                VERB1 log("CIE not found for FDE %jx in %s",
                        (uintmax_t)cfi_offset, filename);
                continue;
            }

            /* Read the two numbers we need and if they are PC-relative,
             * compute the offset from VMA base
             */

            uintptr_t initial_location = fde_read_address(cfi.fde.start, cie->ptr_len);
            uintptr_t address_range = fde_read_address(cfi.fde.start+cie->ptr_len, cie->ptr_len);

            if (cie->pcrel)
            {
                /* We need to determine how long is the 'length' (and
                 * consequently CIE id) field of this FDE -- it can be either 4
                 * or 12 bytes long. */
                uintptr_t length = fde_read_address(scn_data->d_buf + cfi_offset, 4);
                uintptr_t skip = (length == 0xffffffffUL ? 12 : 4);

                uintptr_t mask = (cie->ptr_len == 4 ? 0xffffffffUL : 0xffffffffffffffffUL);
                initial_location += (uintptr_t)shdr.sh_offset + (uintptr_t)cfi_offset + 2*skip;
                initial_location &= mask;
            }
            else
            {
                /* Assuming that not pcrel means absolute address (what if the file is a library?).
                 * Convert to text-section-start-relative.
                 */
                initial_location -= exec_base;
            }

            /* Insert the pair into hash */
            uintptr_t *key = addr_alloc(initial_location + exec_base);
            g_hash_table_insert(hash, key, (gpointer)address_range);
            VERB3 log("FDE start: 0x%jx length: %u", (uintmax_t)*key, (unsigned)address_range);

            /* Iterate through the backtrace entries and check each address
             * member whether it belongs into the range given by current FDE.
             */
            for (it = entries; it != NULL; it = g_list_next(it))
            {
                struct backtrace_entry *entry = it->data;
                if (initial_location <= entry->build_id_offset
                        && entry->build_id_offset < initial_location + address_range)
                {
                    /* Convert to before-relocation absolute addresses, disassembler uses those. */
                    entry->function_initial_loc = exec_base + initial_location;
                    entry->function_length = address_range;
                    /*TODO: remove the entry from the list to save a bit of time in next iteration?*/
                }
            }
        }
    }

    retval = hash; /* success */

ret_free:
    list_free_with_free(cie_list);
    if (retval == NULL)
        g_hash_table_destroy(hash);
    return retval;
}

/* Capture disassembler output into a strbuf.
 * XXX: This may be slow due to lots of reallocations, so if it's a problem,
 * we can replace strbuf with a fixed-size buffer.
 */
static int buffer_printf(void *buffer, const char *fmt, ...)
{
    struct strbuf *strbuf = buffer;
    va_list p;
    int orig_len = strbuf->len;

    va_start(p, fmt);
    buffer = strbuf_append_strfv(buffer, fmt, p);
    va_end(p);

    return (strbuf->len - orig_len);
}

static void log_bfd_error(const char *function, const char *filename)
{
    log("%s failed for %s: %s", function, filename, bfd_errmsg(bfd_get_error()));
}

struct disasm_data
{
    bfd *bfd_file; /* NULL indicates error */
    disassembler_ftype disassemble;
    struct disassemble_info info;
};

/* Initialize disassembler (libopcodes/libbfd) for given file.
 *
 * If the the function returns data with data.bfd_file = NULL,
 * then the function failed.
 */
static struct disasm_data disasm_init(const char *filename)
{
    asection *section;
    struct disasm_data data;

    static bool initialized = false;
    if (!initialized)
    {
        bfd_init();
        initialized = true;
    }

    data.bfd_file = bfd_openr(filename, NULL);
    if (data.bfd_file == NULL)
    {
        VERB1 log_bfd_error("bfd_openr", filename);
        return data;
    }

    if (!bfd_check_format(data.bfd_file, bfd_object))
    {
        VERB1 log_bfd_error("bfd_check_format", filename);
        goto ret_fail;
    }

    section = bfd_get_section_by_name(data.bfd_file, ".text");
    if (section == NULL)
    {
        VERB1 log_bfd_error("bfd_get_section_by_name", filename);
        goto ret_fail;
    }

    data.disassemble = disassembler(data.bfd_file);
    if (data.disassemble == NULL)
    {
        VERB1 log("Unable to find disassembler");
        goto ret_fail;
    }

    init_disassemble_info(&data.info, NULL, buffer_printf);
    data.info.arch = bfd_get_arch(data.bfd_file);
    data.info.mach = bfd_get_mach(data.bfd_file);
    data.info.buffer_vma = section->vma;
    data.info.buffer_length = section->size;
    data.info.section = section;
    /*TODO: memory error func*/
    bfd_malloc_and_get_section(data.bfd_file, section, &data.info.buffer);
    disassemble_init_for_target(&data.info);

    return data;

ret_fail:
    bfd_close(data.bfd_file);
    data.bfd_file = NULL;
    return data;
}

static void disasm_close(struct disasm_data data)
{
    if (data.bfd_file)
    {
        bfd_close(data.bfd_file);
    }
}

/* Disassemble the function starting at function_begin and ending before
 * function_end, returning a list of (char*) instructions.
 */
static GList* disasm_function(struct disasm_data data, uintptr_t function_begin,
    uintptr_t function_end)
{
    uintptr_t count, pc;
    GList *insns = NULL;

    /* Check whether the address range is sane. */
    if (!(data.info.section->vma <= function_begin
         && function_end <= data.info.section->vma + data.info.section->size
         && function_begin < function_end))
    {
        VERB2 log("Function range 0x%jx-0x%jx probably wrong", (uintmax_t)function_begin,
                (uintmax_t)function_end);
        return NULL;
    }

    /* Iterate over each instruction and add its string representation to a list. */
    pc = count = function_begin;
    while (count > 0 && pc < function_end)
    {
        data.info.stream = strbuf_new();
        /* log("0x%jx: ", (uintmax_t)pc); */
        count = data.disassemble(pc, &data.info);
        pc += count;
        insns = g_list_append(insns, strbuf_free_nobuf(data.info.stream));
        /* log("%s\n", (char*)g_list_last(insns)->data); */
    }

    return insns;
}

/* Given list of instructions, returns lists of addresses that are (directly)
 * called by them.
 */
static GList* callees(GList *insns)
{
    GList *it, *res = NULL;
    static const char *call_mnems[] = {"call", "callb", "callw", "calll", "callq", NULL};

    for (it = insns; it != NULL; it = g_list_next(it))
    {
        char *insn = it->data;
        uintmax_t addr;

        if (!insn_is_one_of(insn, call_mnems))
        {
            continue;
        }

        if (!insn_has_single_addr_operand(insn, &addr))
        {
            continue;
        }

        uintptr_t *a = addr_alloc((uintptr_t)addr);
        res = g_list_append(res, a);
    }

    return res;
}

/* Compute intra-module call graph (that is, only calls within the binary).
 */
static GHashTable* compute_call_graph(struct disasm_data data, GHashTable *fdes, GList *entries)
{
    unsigned iterations_allowed = call_graph_iteration_limit;
    GList *it, *insns;
    struct backtrace_entry *entry;
    GList *queue = NULL;
    /* Keys are pointers to addresses, values are GLists of pointers to addresses. */
    GHashTable *succ = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, (GDestroyNotify)list_free_with_free);

    /* Seed the queue with functions from entries */
    for (it = entries; it != NULL; it = g_list_next(it))
    {
        entry = it->data;
        uintptr_t *k = addr_alloc(entry->function_initial_loc);
        queue = g_list_append(queue, k);
    }

    /* Note: allocated addresses that belong to the queue must either be
     * 'reassigned' to succ or freed. */
    while (queue != NULL && iterations_allowed)
    {
        /* Pop one item from the queue */
        it = g_list_first(queue);
        queue = g_list_remove_link(queue, it);
        uintptr_t *key = (uintptr_t*)(it->data);
        uintptr_t function_begin = *key;
        g_list_free(it);
        iterations_allowed--;

        /* Check if it is not already processed */
        if (g_hash_table_lookup_extended(succ, key, NULL, NULL))
        {
            free(key);
            continue;
        }

        /* Look up function length in fdes
         * note: length is stored casted to pointer */
        uintptr_t p = (uintptr_t)g_hash_table_lookup(fdes, key);
        if (p == 0)
        {
            VERB3 log("Range not present for 0x%jx, skipping disassembly", (uintmax_t)function_begin);
            /* Insert empty list of callees so that we avoid looping infinitely */
            g_hash_table_insert(succ, key, NULL);
            continue;
        }
        uintptr_t function_end = function_begin + p;

        /* Disassemble function */
        insns = disasm_function(data, function_begin, function_end);
        if (insns == NULL)
        {
            VERB2 log("Disassembly of 0x%jx-0x%jx failed", (uintmax_t)function_begin, (uintmax_t)function_end);
            /* Insert empty list of callees so that we avoid looping infinitely */
            g_hash_table_insert(succ, key, NULL);
            continue;
        }

        /* Scan for callees */
        GList *fn_callees = callees(insns);
        list_free_with_free(insns);

        VERB3 log("Callees of 0x%jx:", (uintmax_t)function_begin);

        /* Insert callees to the workqueue */
        for (it = fn_callees; it != NULL; it = g_list_next(it))
        {
            uintptr_t c = *(uintptr_t*)it->data;
            queue = g_list_append(queue, addr_alloc(c));

            VERB3 log("\t0x%jx", (uintmax_t)c);
        }

        /* Insert it to the hash so we don't have to recompute it */
        g_hash_table_insert(succ, key, fn_callees);
    }

    return succ;
}

static void generate_fingerprints(struct disasm_data ddata, GHashTable *plt_names, GHashTable *call_graph, GList *entries)
{
    GList *it;
    GList *insns;
    struct backtrace_entry *entry;
    fp_function_type *fp;

    for (it = entries; it != NULL; it = g_list_next(it))
    {
        entry = it->data;
        uintptr_t function_begin = entry->function_initial_loc;
        uintptr_t function_end = function_begin + entry->function_length;

        insns = disasm_function(ddata, function_begin, function_end);
        if (insns == NULL)
        {
            VERB1 log("Cannot disassemble function at 0x%jx, not computing fingerprint", (uintmax_t)function_begin);
            continue;
        }

        struct strbuf *strbuf = strbuf_new();
        strbuf_append_strf(strbuf, "v1"); /* Fingerprint version */

        for (fp = fp_components; *fp != NULL; fp++)
        {
            strbuf_append_strf(strbuf, ";");
            (*fp)(strbuf, insns, function_begin, function_end, call_graph, plt_names);
        }

        list_free_with_free(insns);
        entry->fingerprint = strbuf_free_nobuf(strbuf);
    }
}

static void process_executable(const char *filename, GList *entries)
{
    int fd;
    Elf *e = NULL;
    GHashTable *fdes = NULL;
    GHashTable *plt_names = NULL;
    GHashTable *call_graph = NULL;
    struct disasm_data ddata = { 0 };

    /* Initialize libelf, open the file and get its Elf handle. */
    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        VERB1 log_elf_error("elf_version", filename);
        return;
    }

    fd = xopen(filename, O_RDONLY);

    e = elf_begin(fd, ELF_C_READ, NULL);
    if (e == NULL)
    {
        VERB1 log_elf_error("elf_begin", filename);
        goto ret_fail;
    }

    /* Read FDEs into hash, fill startPC and endPC for entries */
    fdes = elf_iterate_fdes(filename, entries, e);
    if (fdes == NULL)
    {
        VERB1 log("Failed to read .eh_frame function ranges from %s", filename);
        goto ret_fail;
    }

    /* Read PLT into hash */
    plt_names = parse_plt(e, filename);
    if (plt_names == NULL)
    {
        VERB1 log("Failed to parse .plt from %s", filename);
        goto ret_fail;
    }

    /* Initialize disassembler */
    ddata = disasm_init(filename);
    if (ddata.bfd_file == NULL)
    {
        VERB1 log("Failed to initialize disassembler for file %s", filename);
        goto ret_fail;
    }

    /* Compute call graph for functions in entries list */
    call_graph = compute_call_graph(ddata, fdes, entries);
    if (call_graph == NULL)
    {
        VERB1 log("Failed to compute call graph for file %s", filename);
        goto ret_fail;
    }

    /* Fill in fingerprints for entries (requires disasm, plt, callgraph) */
    generate_fingerprints(ddata, plt_names, call_graph, entries);

ret_fail:
    if (call_graph)
        g_hash_table_destroy(call_graph);
    disasm_close(ddata);
    if (plt_names)
        g_hash_table_destroy(plt_names);
    if (fdes)
        g_hash_table_destroy(fdes);
    if (e)
        elf_end(e);
    close(fd);
}

static gint filename_cmp(const struct backtrace_entry *entry, const char *filename)
{
    return (entry->filename ? strcmp(filename, entry->filename) : 1);
}

static void fingerprint_executables(GList *backtrace)
{
    GList *to_be_done = g_list_copy(backtrace);
    GList *worklist, *it;
    const char *filename;
    struct backtrace_entry *entry;

    /* Process each element
     * We need to divide the backtrace to smaller lists such that each list has
     * entries associated to the same file name. */
    while (to_be_done != NULL)
    {
        /* Take first entry */
        entry = to_be_done->data;
        filename = entry->filename;
        worklist = NULL;

        /* Skip entries without file names */
        if (filename == NULL || strcmp(filename, "-") == 0)
        {
            to_be_done = g_list_remove(to_be_done, entry);
            continue;
        }

        /* Find entries with the same filename */
        while ((it = g_list_find_custom(to_be_done, filename, (GCompareFunc)filename_cmp)) != NULL)
        {
            entry = it->data;

            /* Add it to the worklist */
            worklist = g_list_append(worklist, entry);

            /* Remove it from the list of remaining elements */
            to_be_done = g_list_remove(to_be_done, entry);
        }

        /* This should never happen, but if it does, we end up in an infinite loop. */
        if (worklist == NULL)
        {
            VERB1 log("%s internal error", __func__);
            return;
        }

        /* Process the worklist */
        VERB2 log("Analyzing executable %s", filename);
        process_executable(filename, worklist);
    }
}
#endif /* ENABLE_DISASSEMBLY */

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] -d DIR\n"
        "\n"
        "Creates coredump-level backtrace from core dump and corresponding binary"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);


    VERB1 log("Querying gdb for backtrace");
    char *gdb_out = get_gdb_output(dump_dir_name);
    if (gdb_out == NULL)
        return 1;

    /* parse addresses and eventual symbols from the output*/
    GList *backtrace = btp_backtrace_extract_addresses(gdb_out);
    VERB1 log("Extracted %d frames from the backtrace", g_list_length(backtrace));
    free(gdb_out);

    VERB1 log("Running eu-unstrip -n to obatin build ids");
    /* Run eu-unstrip -n to obtain the ids. This should be rewritten to read
     * them directly from the core. */
    char *unstrip_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
    if (unstrip_output == NULL)
        error_msg_and_die("Running eu-unstrip failed");

    /* Get the executable name -- unstrip doesn't know it. */
    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        xfunc_die(); /* dd_opendir already printed error msg */
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    dd_close(dd);

    btp_core_assign_build_ids(backtrace, unstrip_output, executable);

    free(executable);
    free(unstrip_output);


#ifdef ENABLE_DISASSEMBLY
    /* Extract address ranges from all the executables in the backtrace*/
    VERB1 log("Computing function fingerprints");
    fingerprint_executables(backtrace);
#endif /* ENABLE_DISASSEMBLY */

    char *formated_backtrace = btp_core_backtrace_fmt(backtrace);

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    dd_save_text(dd, FILENAME_CORE_BACKTRACE, formated_backtrace);
    dd_close(dd);

    free(formated_backtrace);
    return 0;
}
