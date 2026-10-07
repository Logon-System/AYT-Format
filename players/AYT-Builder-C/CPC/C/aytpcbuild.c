/*
 * aytpcbuild.c - PC-side AYT player builder for Amstrad CPC.
 *
 * This reproduces the runtime player generation performed by
 * AytPlayerBuilder-CPC.asm, but on a modern host. It reads an AYT file,
 * relocates its sequence pattern pointers for a target CPC address, and emits
 * the specialized Z80 player bytes that the original builder would create.
 *
 * Build:
 *   gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild aytpcbuild.c
 *
 * Example matching AytPlayerBuilder-CPC-demo.asm:
 *   aytpcbuild --input stillscrolling.ayt --player-addr 0x0100 \
 *     --ayt-addr 0x1000 --loops 2 --mode jp --return-addr 0x0532 \
 *     --out-player stillscrolling.player.bin \
 *     --out-ayt-runtime stillscrolling.runtime.ayt \
 *     --out-bundle stillscrolling.bundle.bin \
 *     --report stillscrolling.report.txt
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} ByteBuf;

typedef struct {
    const char *input_path;
    const char *out_player_path;
    const char *out_ayt_path;
    const char *out_bundle_path;
    const char *out_asm_two_path;
    const char *out_asm_bundle_path;
    const char *report_path;
    uint16_t player_addr;
    uint16_t ayt_addr;
    uint16_t bundle_base;
    uint16_t program_addr;
    uint16_t return_addr;
    int loops;
    int mode_jp;
    int have_player_addr;
    int have_ayt_addr;
    int have_bundle_base;
    int have_program_addr;
    int have_return_addr;
    int have_loops;
    int have_mode;
} Options;

typedef struct {
    uint8_t version;
    uint16_t active_mask;
    uint8_t pattern_size;
    uint16_t first_seq;
    uint16_t loop_seq;
    uint16_t list_init;
    uint16_t nb_pattern_ptr;
    uint8_t platform_freq;
    uint8_t reserved;
    int active[14];
    int active_count;
} AytHeader;

typedef struct {
    uint8_t *data;
    size_t len;
} FileData;

typedef struct {
    uint16_t cpu_nops;
    size_t player_size;
    size_t b4_offset;
    int has_init;
    int init_count;
    int first_reg;
    int b2_blocks;
    int patched_ld_c;
    int removed_inc_c;
} BuildInfo;

enum {
    DEFAULT_PROGRAM_ADDR = 0x0500,
    JP_RETURN_OFFSET = 36,
    CALL_LAUNCHER_SIZE = 63,
    JP_LAUNCHER_SIZE = 70
};

enum {
    B1_JP_FIRST_SEQ = 4,
    B1_JP_FIRST_REG = 8,
    B1_JP_PATTERN_IDX = 11,

    B1_CALL_SAVE_SP = 5,
    B1_CALL_FIRST_SEQ = 8,
    B1_CALL_FIRST_REG = 12,
    B1_CALL_PATTERN_IDX = 15,

    B3_SEQ_PTR_UPD = 30,
    B3_PAT_COUNT_1 = 21,
    B3_PATTERN_SIZE = 25,
    B3_PAT_COUNT_2 = 34,
    B3_JP_EXIT_PTR = 37,
    B3_JP_MUSIC_CNT = 44,
    B3_JP_MUSIC_CNT_PTR = 49,
    B3_JP_LOOP_SEQ = 52,

    B3_CALL_RELOAD_SP = 37,
    B3_CALL_MUSIC_CNT = 45,
    B3_CALL_MUSIC_CNT_PTR = 50,
    B3_CALL_LOOP_SEQ = 53,

    B4_PTR_INIT = 11,
    B4_PLAYER_ENTRY = 37,
    B4_FINE_WAIT = 46,
    B4_CPT_WAIT = 51
};

static const uint8_t B1_JP[] = {
    0x01, 0x80, 0xf6, 0x31, 0x00, 0x00, 0xd9, 0x01,
    0x00, 0xf4, 0x11, 0x00, 0x00, 0x3e, 0xe6, 0xd3,
    0xff
};

static const uint8_t B1_CALL[] = {
    0x01, 0x80, 0xf6, 0xed, 0x73, 0x00, 0x00, 0x31,
    0x00, 0x00, 0xd9, 0x01, 0x00, 0xf4, 0x11, 0x00,
    0x00, 0x3e, 0xe6, 0xd3, 0xff
};

static const uint8_t B2[] = {
    0xed, 0x49, 0x47, 0xed, 0x71, 0x05, 0xe1, 0x19,
    0xed, 0xa3, 0xd9, 0xed, 0x49, 0xd3, 0xff, 0xd9,
    0x0c
};

static const uint8_t B3_JP[] = {
    0xe1, 0x19, 0xcb, 0x7e, 0x20, 0x21, 0x0c, 0xed,
    0x49, 0x47, 0xed, 0x71, 0x05, 0xed, 0xa3, 0xd9,
    0xed, 0x49, 0xd3, 0xff, 0x3a, 0x00, 0x00, 0x3c,
    0xfe, 0x00, 0x20, 0x2c, 0xed, 0x73, 0x00, 0x00,
    0xaf, 0x32, 0x00, 0x00, 0xc3, 0x00, 0x00, 0xcb,
    0x76, 0x20, 0x18, 0x3e, 0x00, 0x3d, 0x28, 0x0a,
    0x32, 0x00, 0x00, 0x31, 0x00, 0x00, 0xe3, 0xe3,
    0x18, 0xe2, 0xe3, 0xe3, 0xe3, 0xe3, 0xbe, 0x18,
    0x00, 0x18, 0xe1, 0xbe, 0xe3, 0xe3, 0x18, 0xcc,
    0xbe, 0x00, 0x18, 0xd5
};

static const uint8_t B3_CALL[] = {
    0xe1, 0x19, 0xcb, 0x7e, 0x20, 0x22, 0x0c, 0xed,
    0x49, 0x47, 0xed, 0x71, 0x05, 0xed, 0xa3, 0xd9,
    0xed, 0x49, 0xd3, 0xff, 0x3a, 0x00, 0x00, 0x3c,
    0xfe, 0x00, 0x20, 0x2d, 0xed, 0x73, 0x00, 0x00,
    0xaf, 0x32, 0x00, 0x00, 0x31, 0x00, 0x00, 0xc9,
    0xcb, 0x76, 0x20, 0x18, 0x3e, 0x00, 0x3d, 0x28,
    0x0a, 0x32, 0x00, 0x00, 0x31, 0x00, 0x00, 0xe3,
    0xe3, 0x18, 0xe1, 0xe3, 0xe3, 0xe3, 0xe3, 0xbe,
    0x18, 0x00, 0x18, 0xe0, 0xbe, 0xe3, 0xe3, 0x18,
    0xcb, 0xbe, 0x00, 0x18, 0xd4
};

static const uint8_t B4_JP[] = {
    0x01, 0x80, 0xf6, 0xd9, 0x06, 0xf4, 0x3e, 0xe6,
    0xd3, 0xff, 0x21, 0x00, 0x00, 0x4e, 0xcb, 0x79,
    0x20, 0x11, 0x23, 0xed, 0x49, 0x47, 0xed, 0x71,
    0x05, 0xed, 0xa3, 0xd9, 0xed, 0x49, 0xd3, 0xff,
    0xd9, 0x18, 0xea, 0xd9, 0x21, 0x00, 0x00, 0x36,
    0x01, 0x23, 0x71, 0x23, 0x70, 0x18, 0xfe, 0x00,
    0x00, 0x00, 0x3e, 0x00, 0x3d, 0x20, 0xfd, 0x18,
    0x9f
};

static const uint8_t B4_CALL[] = {
    0x01, 0x80, 0xf6, 0xd9, 0x06, 0xf4, 0x3e, 0xe6,
    0xd3, 0xff, 0x21, 0x00, 0x00, 0x4e, 0xcb, 0x79,
    0x20, 0x11, 0x23, 0xed, 0x49, 0x47, 0xed, 0x71,
    0x05, 0xed, 0xa3, 0xd9, 0xed, 0x49, 0xd3, 0xff,
    0xd9, 0x18, 0xea, 0xd9, 0x21, 0x00, 0x00, 0x36,
    0x01, 0x23, 0x71, 0x23, 0x70, 0x18, 0xfe, 0x00,
    0x00, 0x00, 0x3e, 0x00, 0x3d, 0x20, 0xfd, 0xc9
};

static void die(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static void die_errno(const char *path)
{
    fprintf(stderr, "error: %s: %s\n", path, strerror(errno));
    exit(1);
}

static uint16_t rd16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void wr16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t add16(uint16_t a, uint16_t b)
{
    return (uint16_t)(a + b);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        die("out of memory");
    }
    return p;
}

static void bb_init(ByteBuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void bb_free(ByteBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void bb_reserve(ByteBuf *b, size_t need)
{
    uint8_t *p;
    size_t ncap;

    if (need <= b->cap) {
        return;
    }
    ncap = b->cap ? b->cap : 256;
    while (ncap < need) {
        if (ncap > ((size_t)-1) / 2) {
            die("allocation overflow");
        }
        ncap *= 2;
    }
    p = (uint8_t *)realloc(b->data, ncap);
    if (!p) {
        die("out of memory");
    }
    b->data = p;
    b->cap = ncap;
}

static size_t bb_append(ByteBuf *b, const uint8_t *src, size_t n)
{
    size_t off = b->len;
    bb_reserve(b, b->len + n);
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return off;
}

static void bb_patch16(ByteBuf *b, size_t off, uint16_t v)
{
    if (off + 2 > b->len) {
        die("internal patch out of range");
    }
    wr16le(b->data + off, v);
}

static void bb_patch8(ByteBuf *b, size_t off, uint8_t v)
{
    if (off >= b->len) {
        die("internal patch out of range");
    }
    b->data[off] = v;
}

static FileData read_file(const char *path)
{
    FILE *f;
    long end;
    FileData fd;

    fd.data = NULL;
    fd.len = 0;
    f = fopen(path, "rb");
    if (!f) {
        die_errno(path);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        die_errno(path);
    }
    end = ftell(f);
    if (end < 0) {
        die_errno(path);
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        die_errno(path);
    }
    fd.len = (size_t)end;
    fd.data = (uint8_t *)malloc(fd.len ? fd.len : 1);
    if (!fd.data) {
        die("out of memory");
    }
    if (fd.len && fread(fd.data, 1, fd.len, f) != fd.len) {
        die_errno(path);
    }
    if (fclose(f) != 0) {
        die_errno(path);
    }
    return fd;
}

static void write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        die_errno(path);
    }
    if (len && fwrite(data, 1, len, f) != len) {
        die_errno(path);
    }
    if (fclose(f) != 0) {
        die_errno(path);
    }
}

static uint8_t *make_bundle(const Options *o, const uint8_t *player,
                            size_t player_len, const uint8_t *ayt,
                            size_t ayt_len, size_t *bundle_len)
{
    uint32_t p0 = o->player_addr;
    uint32_t p1 = p0 + (uint32_t)player_len;
    uint32_t a0 = o->ayt_addr;
    uint32_t a1 = a0 + (uint32_t)ayt_len;
    uint32_t base = o->have_bundle_base ? o->bundle_base : p0;
    uint8_t *out;

    if (base != p0) {
        die("--bundle-base must equal --player-addr");
    }
    if (a0 != p1) {
        die("bundle internal error: AYT is not immediately after player");
    }
    if (a1 > 0x10000u) {
        die("bundle exceeds CPC 64K address space");
    }
    *bundle_len = player_len + ayt_len;
    out = (uint8_t *)xmalloc(*bundle_len);
    memcpy(out, player, player_len);
    memcpy(out + player_len, ayt, ayt_len);
    return out;
}

static int ranges_overlap(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1)
{
    return a0 < b1 && b0 < a1;
}

static void check_program_range(uint16_t program_addr, size_t launcher_size,
                                uint32_t d0a, uint32_t d0b,
                                uint32_t d1a, uint32_t d1b)
{
    uint32_t p0 = program_addr;
    uint32_t p1 = p0 + (uint32_t)launcher_size;

    if (p1 > 0x10000u) {
        die("launcher program range exceeds CPC 64K address space");
    }
    if (ranges_overlap(p0, p1, d0a, d0b) ||
        (d1a != d1b && ranges_overlap(p0, p1, d1a, d1b))) {
        die("launcher program overlaps generated player/AYT data");
    }
}

static uint16_t pick_call_program_addr(const Options *o, size_t launcher_size,
                                       uint32_t d0a, uint32_t d0b,
                                       uint32_t d1a, uint32_t d1b)
{
    uint32_t p = o->have_program_addr ? o->program_addr : DEFAULT_PROGRAM_ADDR;
    uint32_t max_end = d0b > d1b ? d0b : d1b;

    if (!o->have_program_addr) {
        uint32_t p1 = p + (uint32_t)launcher_size;
        if (p < max_end ||
            ranges_overlap(p, p1, d0a, d0b) ||
            (d1a != d1b && ranges_overlap(p, p1, d1a, d1b))) {
            p = max_end;
        }
    } else if (p < max_end) {
        die("--program-addr must be after generated data for generated ASM");
    }
    if (p > 0xffffu) {
        die("cannot place launcher program");
    }
    if (p < (d0b > d1b ? d0b : d1b)) {
        die("--return-addr places generated launcher before generated data");
    }
    check_program_range((uint16_t)p, launcher_size, d0a, d0b, d1a, d1b);
    return (uint16_t)p;
}

static uint16_t pick_jp_program_addr(const Options *o, size_t launcher_size,
                                     uint32_t d0a, uint32_t d0b,
                                     uint32_t d1a, uint32_t d1b)
{
    uint32_t p;
    uint32_t expected_return;

    if (o->return_addr < JP_RETURN_OFFSET) {
        die("--return-addr is too low for generated jp launcher");
    }
    p = (uint32_t)o->return_addr - JP_RETURN_OFFSET;
    if (o->have_program_addr) {
        expected_return = (uint32_t)o->program_addr + JP_RETURN_OFFSET;
        if (expected_return != o->return_addr) {
            die("--program-addr + jp return offset does not match --return-addr");
        }
        p = o->program_addr;
    }
    if (p > 0xffffu) {
        die("cannot place launcher program");
    }
    check_program_range((uint16_t)p, launcher_size, d0a, d0b, d1a, d1b);
    return (uint16_t)p;
}

static void fprint_hex16(FILE *f, uint16_t v)
{
    fprintf(f, "#%04X", (unsigned)v);
}

static const char *path_basename(const char *path)
{
    const char *base = path;

    if (!path) {
        return "";
    }
    while (*path) {
        if (*path == '/' || *path == '\\') {
            base = path + 1;
        }
        path++;
    }
    return base;
}

static uint16_t asm_program_addr_two(const Options *o, size_t player_len,
                                     size_t ayt_len)
{
    uint32_t p0 = o->player_addr;
    uint32_t p1 = p0 + (uint32_t)player_len;
    uint32_t a0 = o->ayt_addr;
    uint32_t a1 = a0 + (uint32_t)ayt_len;
    size_t launcher_size = o->mode_jp ? JP_LAUNCHER_SIZE : CALL_LAUNCHER_SIZE;

    if (o->mode_jp) {
        return pick_jp_program_addr(o, launcher_size, p0, p1, a0, a1);
    }
    return pick_call_program_addr(o, launcher_size, p0, p1, a0, a1);
}

static uint16_t asm_program_addr_bundle(const Options *o, size_t bundle_len)
{
    uint32_t b0 = o->have_bundle_base ? o->bundle_base : o->player_addr;
    uint32_t b1 = b0 + (uint32_t)bundle_len;
    size_t launcher_size = o->mode_jp ? JP_LAUNCHER_SIZE : CALL_LAUNCHER_SIZE;

    if (o->mode_jp) {
        return pick_jp_program_addr(o, launcher_size, b0, b1, 0, 0);
    }
    return pick_call_program_addr(o, launcher_size, b0, b1, 0, 0);
}

static void write_asm_launcher(FILE *f, const Options *o, int bundle,
                               const char *binary0, const char *binary1,
                               uint16_t program_addr)
{
    fprintf(f, ";; AYT prebuilt %s test / %s mode\n",
            bundle ? "bundle" : "two-file", o->mode_jp ? "SP-JP" : "CALL");
    fprintf(f, ";; Generated by aytpcbuild.\n\n");

    if (bundle) {
        fprintf(f, "AYT_Bundle equ ");
        fprint_hex16(f, o->have_bundle_base ? o->bundle_base : o->player_addr);
        fprintf(f, "\n");
        fprintf(f, "AYT_Player equ AYT_Bundle\n");
        fprintf(f, "MyProgram  equ ");
        fprint_hex16(f, program_addr);
        fprintf(f, "\n\n");
        fprintf(f, "        org AYT_Bundle\n");
        fprintf(f, "        incbin \"%s\"\n\n", binary0);
    } else {
        fprintf(f, "AYT_Player equ ");
        fprint_hex16(f, o->player_addr);
        fprintf(f, "\n");
        fprintf(f, "AYT_File   equ ");
        fprint_hex16(f, o->ayt_addr);
        fprintf(f, "\n");
        fprintf(f, "MyProgram  equ ");
        fprint_hex16(f, program_addr);
        fprintf(f, "\n\n");
        fprintf(f, "        org AYT_Player\n");
        fprintf(f, "        incbin \"%s\"\n\n", binary0);
        fprintf(f, "        org AYT_File\n");
        fprintf(f, "        incbin \"%s\"\n\n", binary1);
    }

    fprintf(f, "        org MyProgram\n");
    fprintf(f, "        run $\n\n");
    fprintf(f, "StartExample\n");
    fprintf(f, "        di\n");
    fprintf(f, "        ld sp,MyStack\n");
    if (o->mode_jp) {
        fprintf(f, "        ld (AYT_Player_ReloadSP),sp\n");
    }
    fprintf(f, "        ld hl,#c9fb\n");
    fprintf(f, "        ld (#38),hl\n");
    fprintf(f, "        ei\n\n");
    fprintf(f, "MainLoop\n");
    fprintf(f, "        ld b,#f5\n\n");
    fprintf(f, "WaitVsync\n");
    fprintf(f, "        in a,(c)\n");
    fprintf(f, "        rra\n");
    fprintf(f, "        jr nc,WaitVsync\n\n");
    fprintf(f, "        halt\n");
    fprintf(f, "        halt\n\n");
    fprintf(f, "        ld bc,#7f10\n");
    fprintf(f, "        ld a,#4c\n");
    fprintf(f, "        out (c),c\n");
    fprintf(f, "        out (c),a\n\n");
    if (o->mode_jp) {
        fprintf(f, "        jp AYT_Player\n");
        fprintf(f, "AYT_Player_Ret\n");
        fprintf(f, "AYT_Player_ReloadSP equ $+1\n");
        fprintf(f, "        ld sp,0\n\n");
    } else {
        fprintf(f, "        call AYT_Player\n\n");
    }
    fprintf(f, "        ld bc,#7f10\n");
    fprintf(f, "        ld a,#54\n");
    fprintf(f, "        out (c),c\n");
    fprintf(f, "        out (c),a\n\n");
    fprintf(f, "        jr MainLoop\n\n");
    fprintf(f, "        ds 20\n");
    fprintf(f, "MyStack\n");
}

static void write_asm_file(const char *path, const Options *o, int bundle,
                           const char *binary0, const char *binary1,
                           uint16_t program_addr)
{
    FILE *f = fopen(path, "wb");
    const char *binary0_name = path_basename(binary0);
    const char *binary1_name = path_basename(binary1);

    if (!f) {
        die_errno(path);
    }
    write_asm_launcher(f, o, bundle, binary0_name, binary1_name, program_addr);
    if (fclose(f) != 0) {
        die_errno(path);
    }
}

static int parse_u16_arg(const char *s, uint16_t *out)
{
    char *end = NULL;
    unsigned long v;
    int base = 10;

    if (!s || !*s) {
        return 0;
    }
    if (s[0] == '#') {
        s++;
        base = 16;
    } else if (s[0] == '$') {
        s++;
        base = 16;
    } else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
    }
    errno = 0;
    v = strtoul(s, &end, base);
    if (errno || !end || *end || v > 0xffffUL) {
        return 0;
    }
    *out = (uint16_t)v;
    return 1;
}

static int parse_int_arg(const char *s, int minv, int maxv, int *out)
{
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, 0);
    if (errno || !end || *end || v < minv || v > maxv) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static void usage(FILE *f)
{
    fprintf(f,
        "usage: aytpcbuild --input music.ayt --player-addr 0x0100 \\\n"
        "       --ayt-addr 0x1000 --loops N --mode jp|call [--return-addr ADDR] \\\n"
        "       [--out-player player.bin] [--out-ayt-runtime music.runtime.ayt] \\\n"
        "       [--out-bundle runtime.bin] [--bundle-base ADDR] \\\n"
        "       [--out-asm-two test-two.asm] [--out-asm-bundle test-bundle.asm] \\\n"
        "       [--program-addr ADDR] \\\n"
        "       [--report report.txt]\n");
}

static Options parse_options(int argc, char **argv)
{
    Options o;
    int i;

    memset(&o, 0, sizeof(o));
    o.loops = -1;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = NULL;

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(stdout);
            exit(0);
        }

        if (i + 1 < argc) {
            v = argv[i + 1];
        }

        if (strcmp(a, "--input") == 0 && v) {
            o.input_path = v;
            i++;
        } else if (strcmp(a, "--out-player") == 0 && v) {
            o.out_player_path = v;
            i++;
        } else if (strcmp(a, "--out-ayt-runtime") == 0 && v) {
            o.out_ayt_path = v;
            i++;
        } else if (strcmp(a, "--out-bundle") == 0 && v) {
            o.out_bundle_path = v;
            i++;
        } else if (strcmp(a, "--out-asm-two") == 0 && v) {
            o.out_asm_two_path = v;
            i++;
        } else if (strcmp(a, "--out-asm-bundle") == 0 && v) {
            o.out_asm_bundle_path = v;
            i++;
        } else if (strcmp(a, "--report") == 0 && v) {
            o.report_path = v;
            i++;
        } else if (strcmp(a, "--player-addr") == 0 && v) {
            if (!parse_u16_arg(v, &o.player_addr)) {
                die("invalid --player-addr");
            }
            o.have_player_addr = 1;
            i++;
        } else if (strcmp(a, "--ayt-addr") == 0 && v) {
            if (!parse_u16_arg(v, &o.ayt_addr)) {
                die("invalid --ayt-addr");
            }
            o.have_ayt_addr = 1;
            i++;
        } else if (strcmp(a, "--bundle-base") == 0 && v) {
            if (!parse_u16_arg(v, &o.bundle_base)) {
                die("invalid --bundle-base");
            }
            o.have_bundle_base = 1;
            i++;
        } else if (strcmp(a, "--program-addr") == 0 && v) {
            if (!parse_u16_arg(v, &o.program_addr)) {
                die("invalid --program-addr");
            }
            o.have_program_addr = 1;
            i++;
        } else if (strcmp(a, "--return-addr") == 0 && v) {
            if (!parse_u16_arg(v, &o.return_addr)) {
                die("invalid --return-addr");
            }
            o.have_return_addr = 1;
            i++;
        } else if (strcmp(a, "--loops") == 0 && v) {
            if (!parse_int_arg(v, 0, 255, &o.loops)) {
                die("invalid --loops");
            }
            o.have_loops = 1;
            i++;
        } else if (strcmp(a, "--mode") == 0 && v) {
            if (strcmp(v, "jp") == 0) {
                o.mode_jp = 1;
            } else if (strcmp(v, "call") == 0) {
                o.mode_jp = 0;
            } else {
                die("invalid --mode, expected jp or call");
            }
            o.have_mode = 1;
            i++;
        } else {
            usage(stderr);
            die("unknown or incomplete argument");
        }
    }

    if (!o.input_path) {
        die("--input is required");
    }
    if (!o.have_player_addr) {
        die("--player-addr is required");
    }
    if (!o.have_ayt_addr) {
        die("--ayt-addr is required");
    }
    if (!o.have_loops) {
        die("--loops is required");
    }
    if (!o.have_mode) {
        die("--mode is required");
    }
    if (o.mode_jp && !o.have_return_addr) {
        die("--return-addr is required in jp mode");
    }
    if (o.out_asm_two_path && (!o.out_player_path || !o.out_ayt_path)) {
        die("--out-asm-two requires --out-player and --out-ayt-runtime");
    }
    if (o.out_asm_bundle_path && !o.out_bundle_path) {
        die("--out-asm-bundle requires --out-bundle");
    }
    if (!o.out_player_path && !o.out_ayt_path && !o.out_bundle_path &&
        !o.out_asm_two_path && !o.out_asm_bundle_path && !o.report_path) {
        die("nothing to write; provide at least one output option");
    }
    return o;
}

static AytHeader parse_header(const uint8_t *ayt, size_t len)
{
    AytHeader h;
    int r;

    if (len < 14) {
        die("AYT file is smaller than 14-byte header");
    }
    memset(&h, 0, sizeof(h));
    h.version = ayt[0];
    h.active_mask = rd16le(ayt + 1);
    h.pattern_size = ayt[3];
    h.first_seq = rd16le(ayt + 4);
    h.loop_seq = rd16le(ayt + 6);
    h.list_init = rd16le(ayt + 8);
    h.nb_pattern_ptr = rd16le(ayt + 10);
    h.platform_freq = ayt[12];
    h.reserved = ayt[13];

    if (h.pattern_size == 0) {
        die("AYT pattern size is zero");
    }
    if (h.first_seq < 14 || h.first_seq > len) {
        die("AYT first sequence offset is invalid");
    }
    if (h.loop_seq < h.first_seq || h.loop_seq > len) {
        die("AYT loop sequence offset is invalid");
    }
    if (h.list_init >= len) {
        die("AYT init list offset is invalid");
    }
    if ((size_t)h.first_seq + (size_t)h.nb_pattern_ptr * 2 > len) {
        die("AYT sequence pointer table exceeds file size");
    }

    for (r = 0; r <= 13; r++) {
        if (h.active_mask & (uint16_t)(0x8000u >> r)) {
            h.active[r] = 1;
            h.active_count++;
        }
    }
    if (!h.active[13]) {
        die("AYT R13 must be active for this player format");
    }
    return h;
}

static int count_init_pairs(const uint8_t *ayt, size_t len, uint16_t list_init)
{
    size_t p = list_init;
    int n = 0;

    while (p < len) {
        uint8_t r = ayt[p++];
        if (r == 0xff) {
            return n;
        }
        if (p >= len) {
            die("AYT init list is truncated");
        }
        p++;
        n++;
    }
    die("AYT init list is missing 0xff terminator");
    return 0;
}

static uint8_t *make_runtime_ayt(const uint8_t *src, size_t len,
                                 const AytHeader *h, uint16_t ayt_addr)
{
    uint8_t *out = (uint8_t *)xmalloc(len);
    size_t i;
    uint16_t pattern_base = add16(ayt_addr, 14);

    memcpy(out, src, len);
    for (i = 0; i < h->nb_pattern_ptr; i++) {
        size_t p = (size_t)h->first_seq + i * 2;
        uint16_t off = rd16le(out + p);
        wr16le(out + p, add16(pattern_base, off));
    }
    return out;
}

static int find_first_active_before_r13(const AytHeader *h)
{
    int r;
    for (r = 0; r <= 12; r++) {
        if (h->active[r]) {
            return r;
        }
    }
    return -1;
}

static int find_next_active_before_r13(const AytHeader *h, int from)
{
    int r;
    for (r = from + 1; r <= 12; r++) {
        if (h->active[r]) {
            return r;
        }
    }
    return -1;
}

static uint16_t abs_addr(uint16_t base, size_t off)
{
    return (uint16_t)(base + (uint16_t)off);
}

static ByteBuf build_player(const Options *o, const uint8_t *runtime_ayt,
                            size_t ayt_len, const AytHeader *h,
                            BuildInfo *info)
{
    ByteBuf out;
    int first_reg;
    int current;
    int b1_first_seq;
    int b1_first_reg;
    int b1_pattern_idx;
    int music_cnt_off;
    int music_cnt_ptr_off;
    int loop_seq_off;
    const uint8_t *b3;
    size_t b3_len;
    size_t b3_off;
    uint16_t cpu;
    uint16_t ptr_first_seq;
    uint16_t ptr_loop_seq;
    uint16_t ptr_init;
    int init_count;

    memset(info, 0, sizeof(*info));
    bb_init(&out);

    first_reg = find_first_active_before_r13(h);
    if (first_reg < 0) {
        die("AYT has no active register in R0..R12; original builder would hang");
    }
    info->first_reg = first_reg;

    ptr_first_seq = add16(o->ayt_addr, h->first_seq);
    ptr_loop_seq = add16(o->ayt_addr, h->loop_seq);
    ptr_init = add16(o->ayt_addr, h->list_init);

    if (o->mode_jp) {
        bb_append(&out, B1_JP, sizeof(B1_JP));
        b1_first_seq = B1_JP_FIRST_SEQ;
        b1_first_reg = B1_JP_FIRST_REG;
        b1_pattern_idx = B1_JP_PATTERN_IDX;
        cpu = 21 + 58;
    } else {
        bb_append(&out, B1_CALL, sizeof(B1_CALL));
        b1_first_seq = B1_CALL_FIRST_SEQ;
        b1_first_reg = B1_CALL_FIRST_REG;
        b1_pattern_idx = B1_CALL_PATTERN_IDX;
        cpu = 29 + 61;
    }

    bb_patch16(&out, (size_t)b1_first_seq, ptr_first_seq);
    bb_patch8(&out, (size_t)b1_first_reg, (uint8_t)first_reg);
    bb_patch16(&out, (size_t)b1_pattern_idx, 0);

    current = first_reg;
    while (current >= 0 && current <= 12) {
        int next;
        size_t b2_off = bb_append(&out, B2, sizeof(B2));
        info->b2_blocks++;
        cpu = (uint16_t)(cpu + 31);

        next = find_next_active_before_r13(h, current);
        if (next >= 0) {
            if (next == current + 1) {
                current = next;
                continue;
            }
            bb_patch8(&out, b2_off + sizeof(B2) - 1, 0x0e);
            bb_reserve(&out, out.len + 1);
            memmove(out.data + b2_off + sizeof(B2) + 1,
                    out.data + b2_off + sizeof(B2),
                    out.len - (b2_off + sizeof(B2)));
            out.data[b2_off + sizeof(B2)] = (uint8_t)next;
            out.len++;
            info->patched_ld_c++;
            cpu = (uint16_t)(cpu + 1);
            current = next;
        } else {
            if (current == 12) {
                out.len--;
                info->removed_inc_c++;
                cpu = (uint16_t)(cpu - 1);
            } else {
                bb_patch8(&out, b2_off + sizeof(B2) - 1, 0x0e);
                bb_reserve(&out, out.len + 1);
                memmove(out.data + b2_off + sizeof(B2) + 1,
                        out.data + b2_off + sizeof(B2),
                        out.len - (b2_off + sizeof(B2)));
                out.data[b2_off + sizeof(B2)] = 12;
                out.len++;
                info->patched_ld_c++;
                cpu = (uint16_t)(cpu + 1);
            }
            break;
        }
    }

    b3 = o->mode_jp ? B3_JP : B3_CALL;
    b3_len = o->mode_jp ? sizeof(B3_JP) : sizeof(B3_CALL);
    b3_off = bb_append(&out, b3, b3_len);

    bb_patch16(&out, b3_off + B3_SEQ_PTR_UPD,
               abs_addr(o->player_addr, (size_t)b1_first_seq));
    bb_patch16(&out, b3_off + B3_PAT_COUNT_1,
               abs_addr(o->player_addr, (size_t)b1_pattern_idx));
    bb_patch8(&out, b3_off + B3_PATTERN_SIZE, h->pattern_size);
    bb_patch16(&out, b3_off + B3_PAT_COUNT_2,
               abs_addr(o->player_addr, (size_t)b1_pattern_idx));

    if (o->mode_jp) {
        bb_patch16(&out, b3_off + B3_JP_EXIT_PTR, o->return_addr);
        music_cnt_off = B3_JP_MUSIC_CNT;
        music_cnt_ptr_off = B3_JP_MUSIC_CNT_PTR;
        loop_seq_off = B3_JP_LOOP_SEQ;
    } else {
        uint16_t reload_sp_abs = abs_addr(o->player_addr, b3_off + B3_CALL_RELOAD_SP);
        bb_patch16(&out, (size_t)B1_CALL_SAVE_SP, reload_sp_abs);
        music_cnt_off = B3_CALL_MUSIC_CNT;
        music_cnt_ptr_off = B3_CALL_MUSIC_CNT_PTR;
        loop_seq_off = B3_CALL_LOOP_SEQ;
    }

    bb_patch8(&out, b3_off + (size_t)music_cnt_off, (uint8_t)o->loops);
    bb_patch16(&out, b3_off + (size_t)music_cnt_ptr_off,
               abs_addr(o->player_addr, b3_off + (size_t)music_cnt_off));
    bb_patch16(&out, b3_off + (size_t)loop_seq_off, ptr_loop_seq);

    init_count = count_init_pairs(runtime_ayt, ayt_len, h->list_init);
    info->init_count = init_count;
    info->has_init = init_count > 0;
    info->cpu_nops = cpu;

    if (init_count > 0) {
        size_t b4_off;
        uint16_t b4_abs;
        uint16_t init_cpu;
        uint16_t diff;
        uint8_t fine_wait;
        uint8_t cpt_wait;
        const uint8_t *b4 = o->mode_jp ? B4_JP : B4_CALL;
        size_t b4_len = o->mode_jp ? sizeof(B4_JP) : sizeof(B4_CALL);

        b4_abs = abs_addr(o->player_addr, out.len);
        bb_patch8(&out, 0, 0xc3);
        bb_patch16(&out, 1, b4_abs);

        init_cpu = (uint16_t)((o->mode_jp ? (83 - 35) : (82 - 35)) +
                              init_count * 35);
        diff = (uint16_t)(cpu - init_cpu);
        fine_wait = (uint8_t)((diff & 3u) ^ 3u);
        cpt_wait = (uint8_t)(((diff >> 2) - 1u) & 0xffu);

        b4_off = bb_append(&out, b4, b4_len);
        bb_patch16(&out, b4_off + B4_PTR_INIT, ptr_init);
        bb_patch16(&out, b4_off + B4_PLAYER_ENTRY, o->player_addr);
        bb_patch8(&out, b4_off + B4_FINE_WAIT, fine_wait);
        bb_patch8(&out, b4_off + B4_CPT_WAIT, cpt_wait);
        info->b4_offset = b4_off;
    }

    info->player_size = out.len;
    return out;
}

static void write_report(const char *path, const Options *o, const AytHeader *h,
                         const BuildInfo *bi, size_t ayt_size,
                         size_t bundle_size,
                         int have_asm_two_program,
                         uint16_t asm_two_program,
                         int have_asm_bundle_program,
                         uint16_t asm_bundle_program)
{
    FILE *f;
    int r;

    f = fopen(path, "wb");
    if (!f) {
        die_errno(path);
    }

    fprintf(f, "AYT PC BUILD REPORT\n");
    fprintf(f, "===================\n\n");

    fprintf(f, "Input file        : %s\n", o->input_path);
    fprintf(f, "Mode              : %s\n", o->mode_jp ? "SP/JP" : "CALL");
    fprintf(f, "Player address    : #%04X\n", (unsigned)o->player_addr);
    fprintf(f, "Runtime AYT addr  : #%04X\n", (unsigned)o->ayt_addr);
    if (o->out_bundle_path) {
        unsigned bundle_base = o->have_bundle_base
            ? (unsigned)o->bundle_base
            : (unsigned)o->player_addr;
        fprintf(f, "Bundle layout     : compact\n");
        fprintf(f, "Bundle base       : #%04X\n", bundle_base);
        fprintf(f, "Bundle size       : %lu bytes\n", (unsigned long)bundle_size);
    }
    if (o->mode_jp) {
        fprintf(f, "Return address    : #%04X\n", (unsigned)o->return_addr);
    }
    fprintf(f, "Loops             : %d\n\n", o->loops);

    fprintf(f, "Outputs\n");
    fprintf(f, "-------\n");
    if (o->out_player_path) {
        fprintf(f, "Player binary     : %s\n", o->out_player_path);
    }
    if (o->out_ayt_path) {
        fprintf(f, "Runtime AYT       : %s\n", o->out_ayt_path);
    }
    if (o->out_bundle_path) {
        fprintf(f, "Bundle binary     : %s\n", o->out_bundle_path);
    }
    if (o->out_asm_two_path) {
        fprintf(f, "Two-file ASM      : %s", o->out_asm_two_path);
        if (have_asm_two_program) {
            fprintf(f, " (RUN #%04X)", (unsigned)asm_two_program);
        }
        fprintf(f, "\n");
    }
    if (o->out_asm_bundle_path) {
        fprintf(f, "Bundle ASM        : %s", o->out_asm_bundle_path);
        if (have_asm_bundle_program) {
            fprintf(f, " (RUN #%04X)", (unsigned)asm_bundle_program);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");

    fprintf(f, "AYT\n");
    fprintf(f, "---\n");
    fprintf(f, "Size              : %lu bytes\n", (unsigned long)ayt_size);
    fprintf(f, "Version           : #%02X\n", (unsigned)h->version);
    fprintf(f, "Pattern size      : %u\n", (unsigned)h->pattern_size);
    fprintf(f, "First seq offset  : #%04X\n", (unsigned)h->first_seq);
    fprintf(f, "Loop seq offset   : #%04X\n", (unsigned)h->loop_seq);
    fprintf(f, "Init list offset  : #%04X\n", (unsigned)h->list_init);
    fprintf(f, "Pattern pointers  : %u\n", (unsigned)h->nb_pattern_ptr);
    fprintf(f, "Platform id       : %u\n", (unsigned)(h->platform_freq & 31));
    fprintf(f, "Frequency id      : %u\n", (unsigned)(h->platform_freq >> 5));
    fprintf(f, "Active registers  : ");
    for (r = 0; r <= 13; r++) {
        if (h->active[r]) {
            fprintf(f, "%sR%02d", r == bi->first_reg ? "" : ", ", r);
        }
    }
    fprintf(f, "\n");
    fprintf(f, "Active count      : %d\n\n", h->active_count);

    fprintf(f, "Player\n");
    fprintf(f, "------\n");
    fprintf(f, "Size              : %lu bytes\n", (unsigned long)bi->player_size);
    fprintf(f, "CPU               : %u nops\n", (unsigned)bi->cpu_nops);
    fprintf(f, "First AY reg      : R%02d\n", bi->first_reg);
    fprintf(f, "B2 blocks         : %d\n", bi->b2_blocks);
    fprintf(f, "Patched ld c,n    : %d\n", bi->patched_ld_c);
    fprintf(f, "Removed inc c     : %d\n", bi->removed_inc_c);
    fprintf(f, "Init block        : %s\n", bi->has_init ? "yes" : "no");
    fprintf(f, "Init pairs        : %d\n", bi->init_count);
    if (bi->has_init) {
        fprintf(f, "B4 offset         : %lu\n", (unsigned long)bi->b4_offset);
    }

    if (fclose(f) != 0) {
        die_errno(path);
    }
}

int main(int argc, char **argv)
{
    Options opt = parse_options(argc, argv);
    FileData input = read_file(opt.input_path);
    AytHeader h = parse_header(input.data, input.len);
    uint8_t *runtime_ayt;
    ByteBuf player;
    BuildInfo info;
    uint8_t *bundle = NULL;
    size_t bundle_len = 0;
    uint16_t asm_two_program = 0;
    uint16_t asm_bundle_program = 0;
    int have_asm_two_program = 0;
    int have_asm_bundle_program = 0;

    runtime_ayt = make_runtime_ayt(input.data, input.len, &h, opt.ayt_addr);
    player = build_player(&opt, runtime_ayt, input.len, &h, &info);

    if (opt.out_bundle_path) {
        uint32_t compact_ayt_addr = (uint32_t)opt.player_addr + (uint32_t)player.len;
        if (compact_ayt_addr + (uint32_t)input.len > 0x10000u) {
            die("compact player+AYT range exceeds CPC 64K address space");
        }
        if (compact_ayt_addr != opt.ayt_addr) {
            opt.ayt_addr = (uint16_t)compact_ayt_addr;
            free(runtime_ayt);
            bb_free(&player);
            runtime_ayt = make_runtime_ayt(input.data, input.len, &h, opt.ayt_addr);
            player = build_player(&opt, runtime_ayt, input.len, &h, &info);
        }
    }

    if (opt.out_bundle_path) {
        bundle = make_bundle(&opt, player.data, player.len,
                             runtime_ayt, input.len, &bundle_len);
    }
    if (opt.out_asm_two_path) {
        asm_two_program = asm_program_addr_two(&opt, player.len, input.len);
        have_asm_two_program = 1;
    }
    if (opt.out_asm_bundle_path) {
        asm_bundle_program = asm_program_addr_bundle(&opt, bundle_len);
        have_asm_bundle_program = 1;
    }

    if (opt.out_player_path) {
        write_file(opt.out_player_path, player.data, player.len);
    }
    if (opt.out_ayt_path) {
        write_file(opt.out_ayt_path, runtime_ayt, input.len);
    }
    if (opt.out_bundle_path) {
        write_file(opt.out_bundle_path, bundle, bundle_len);
    }
    if (opt.out_asm_two_path) {
        write_asm_file(opt.out_asm_two_path, &opt, 0,
                       opt.out_player_path, opt.out_ayt_path,
                       asm_two_program);
    }
    if (opt.out_asm_bundle_path) {
        write_asm_file(opt.out_asm_bundle_path, &opt, 1,
                       opt.out_bundle_path, NULL,
                       asm_bundle_program);
    }
    if (opt.report_path) {
        write_report(opt.report_path, &opt, &h, &info, input.len,
                     bundle_len,
                     have_asm_two_program, asm_two_program,
                     have_asm_bundle_program, asm_bundle_program);
    }

    printf("AYT PC build ok: player=%lu bytes, ayt=%lu bytes at #%04X, cpu=%u nops%s\n",
           (unsigned long)player.len,
           (unsigned long)input.len,
           (unsigned)opt.ayt_addr,
           (unsigned)info.cpu_nops,
           info.has_init ? ", init block present" : "");

    free(input.data);
    free(runtime_ayt);
    free(bundle);
    bb_free(&player);
    return 0;
}
