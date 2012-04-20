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
static void elf_iterate_fdes(const char *filename, GList *entries)
{
    int fd;
    Elf *e;
    const unsigned char *e_ident;
    const char *scnname;
    Elf_Scn *scn;
    Elf_Data *scn_data;
    GElf_Shdr shdr;
    GElf_Phdr phdr;
    size_t shstrndx, phnum;

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
        goto ret_close;
    }

    e_ident = (unsigned char *)elf_getident(e, NULL);
    if (e_ident == NULL)
    {
        VERB1 log_elf_error("elf_getident", filename);
        goto ret_elf;
    }

    /* Look up the .eh_frame section */
    if (elf_getshdrstrndx(e, &shstrndx) != 0)
    {
        VERB1 log_elf_error("elf_getshdrstrndx", filename);
        goto ret_elf;
    }

    scn = NULL;
    while ((scn = elf_nextscn(e, scn)) != NULL)
    {
        if (gelf_getshdr(scn, &shdr) != &shdr)
        {
            VERB1 log_elf_error("gelf_getshdr", filename);
            continue;
        }

        scnname = elf_strptr(e, shstrndx, shdr.sh_name);
        if (scnname == NULL)
        {
            VERB1 log_elf_error("elf_strptr", filename);
            continue;
        }

        if (strcmp(scnname, ".eh_frame") == 0)
        {
            break; /* Found. */
        }
    }

    if (scn == NULL)
    {
        VERB1 log("Section .eh_frame not found in %s", filename);
        goto ret_elf;
    }

    scn_data = elf_getdata(scn, NULL);
    if (scn_data == NULL)
    {
        VERB1 log_elf_error("elf_getdata", filename);
        goto ret_elf;
    }

    /* Get the address at which the executable segment is loaded. If the
     * .eh_frame addresses are absolute, this is used to convert them to
     * relative to the beginning of executable segment. We are looking for the
     * first LOAD segment that is executable, I hope this is sufficient.
     */
    if (elf_getphdrnum(e, &phnum) != 0)
    {
        VERB1 log_elf_error("elf_getphdrnum", filename);
        goto ret_elf;
    }

    uintptr_t exec_base;
    int i;
    for (i = 0; i < phnum; i++)
    {
        if (gelf_getphdr(e, i, &phdr) != &phdr)
        {
            VERB1 log_elf_error("gelf_getphdr", filename);
            goto ret_elf;
        }

        if (phdr.p_type == PT_LOAD && phdr.p_flags & PF_X)
        {
            exec_base = (uintptr_t)phdr.p_vaddr;
            goto base_found;
        }
    }

    VERB1 log("Can't determine executable base for '%s'", filename);
    goto ret_elf;

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
             * errorneous cfi. */
            if (cfi_offset_next > cfi_offset)
            {
                continue;
            }
            VERB1 log("dwarf_next_cfi failed for %s: %s", filename, dwarf_errmsg(-1));
            goto ret_list;
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

ret_list:
    list_free_with_free(cie_list);
ret_elf:
    elf_end(e);
ret_close:
    close(fd);
}

static char* fingerprint_insns(GList *insns)
{
    return xasprintf("number_of_instructions:%d", g_list_length(insns));
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

/* Open filename, initialize binutils/libopcodes disassembler, disassemble each
 * function in 'entries' given by the ranges computed earlier and compute
 * fingerprint based on the disassembly.
 */
static void disassemble_file(const char *filename, GList *entries)
{
    bfd *bfdFile;
    asection *section;
    disassembler_ftype disassemble;
    struct disassemble_info info;
    uintptr_t count, pc;
    GList *it, *insns;
    struct backtrace_entry *entry;

    static bool initialized = false;
    if (!initialized)
    {
        bfd_init();
        initialized = true;
    }

    bfdFile = bfd_openr(filename, NULL);
    if (bfdFile == NULL)
    {
        VERB1 log_bfd_error("bfd_openr", filename);
        return;
    }

    if (!bfd_check_format(bfdFile, bfd_object))
    {
        VERB1 log_bfd_error("bfd_check_format", filename);
        goto ret_close;
    }

    section = bfd_get_section_by_name(bfdFile, ".text");
    if (section == NULL)
    {
        VERB1 log_bfd_error("bfd_get_section_by_name", filename);
        goto ret_close;
    }

    disassemble = disassembler(bfdFile);
    if (disassemble == NULL)
    {
        VERB1 log("Unable to find disassembler");
        goto ret_close;
    }

    init_disassemble_info(&info, NULL, buffer_printf);
    info.arch = bfd_get_arch(bfdFile);
    info.mach = bfd_get_mach(bfdFile);
    info.buffer_vma = section->vma;
    info.buffer_length = section->size;
    info.section = section;
    /*TODO: memory error func*/
    bfd_malloc_and_get_section(bfdFile, section, &info.buffer);
    disassemble_init_for_target(&info);

    /* Iterate over backtrace entries, disassembly and fingerprint each one. */
    for (it = entries; it != NULL; it = g_list_next(it))
    {
        entry = it->data;
        uintptr_t function_begin = entry->function_initial_loc;
        uintptr_t function_end = function_begin + entry->function_length;

        /* Check whether the address range is sane. */
        if (!(section->vma <= function_begin
             && function_end <= section->vma + section->size
             && function_begin < function_end))
        {
            VERB2 log("Function range 0x%jx-0x%jx probably wrong", (uintmax_t)function_begin,
                    (uintmax_t)function_end);
            continue;
        }

        /* Iterate over each instruction and add its string representation to a list. */
        insns = NULL;
        pc = count = function_begin;
        while (count > 0 && pc < function_end)
        {
            info.stream = strbuf_new();
            count = disassemble(pc, &info);
            pc += count;
            insns = g_list_append(insns, strbuf_free_nobuf(info.stream));
        }

        /* Compute the actual fingerprint from the list. */
        /* TODO: Check for failures. */
        entry->fingerprint = fingerprint_insns(insns);

        list_free_with_free(insns);
    }

ret_close:
    bfd_close(bfdFile);
}

static gint filename_cmp(const struct backtrace_entry *entry, const char *filename)
{
    return (entry->filename ? strcmp(filename, entry->filename) : 1);
}

static void disassemble_and_fingerprint(GList *backtrace)
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
        VERB2 log("Extracting function ranges from %s", filename);
        elf_iterate_fdes(filename, worklist);

        VERB2 log("Disassembling functions from %s", filename);
        disassemble_file(filename, worklist);
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
    disassemble_and_fingerprint(backtrace);
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
