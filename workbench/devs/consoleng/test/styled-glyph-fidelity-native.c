/* Compare console output with complete native graphics.library glyphs. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/bullet.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/glyph.h>
#include <diskfont/oterrors.h>
#include <stdlib.h>
#include <exec/errors.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
struct Library *BulletBase;
static int metric = 2;
static int descenders(struct TextFont *tf, int bold)
{
    struct GlyphEngine *ge;
    BulletBase = OpenLibrary((CONST_STRPTR)"freetype2.library", 0);
    if (!BulletBase) return 0;
    ge = OpenEngine();
    if (!ge) { CloseLibrary(BulletBase); return 0; }
    struct TagItem config[] = {
        {OT_Spec1 | OT_Indirect, (IPTR)(bold ? "FONTS:TrueType/DejaVuSansMono-Bold.ttf" : "FONTS:TrueType/DejaVuSansMono.ttf")},
        {OT_Level1 | 0x104, metric},
        {OT_DeviceDPI, (72 << 16) | 72},
        {OT_PointHeight, (IPTR)tf->tf_YSize << 16}, {TAG_DONE, 0}};
    int ok = SetInfoA(ge, config) == OTERR_Success;
    const char *letters = "gjpqy_";
    for (; ok && *letters; letters++) {
        struct GlyphMap *gm = NULL;
        struct TagItem set[] = {{OT_GlyphCode, *letters}, {TAG_DONE, 0}};
        struct TagItem get[] = {{OT_GlyphMap8Bits, (IPTR)&gm}, {TAG_DONE, 0}};
        ok = SetInfoA(ge, set) == OTERR_Success &&
            ObtainInfoA(ge, get) == OTERR_Success && gm;
        if (gm) {
            LONG top = tf->tf_Baseline + 1 - gm->glm_Y0;
            LONG bottom = top + gm->glm_BlackHeight;
            printf("DESCENDER metric=%d char=%c height=%d baseline=%d glyphY0=%ld glyphHeight=%ld top=%ld bottom=%ld\n", metric, *letters, tf->tf_YSize, tf->tf_Baseline, (long)gm->glm_Y0, (long)gm->glm_BlackHeight, (long)top, (long)bottom);
            ok = ok && top >= 0 && bottom <= tf->tf_YSize;
            if (ok) {
                struct ColorTextFont *ctf = (struct ColorTextFont *)tf;
                ULONG location = ((ULONG *)tf->tf_CharLoc)[(UBYTE)*letters - tf->tf_LoChar];
                ok = (tf->tf_Style & FSF_COLORFONT) && (ctf->ctf_Flags & CT_ANTIALIAS) &&
                    (location & 0xffff) == gm->glm_BlackWidth;
                for (ULONG y = 0; ok && y < gm->glm_BlackHeight; y++)
                    for (ULONG x = 0; x < gm->glm_BlackWidth; x++) {
                        UBYTE actual = ((UBYTE *)ctf->ctf_CharData[0])[
                            (top + y) * tf->tf_Modulo * 8 + (location >> 16) + x];
                        UBYTE expected = gm->glm_BitMap[
                            (gm->glm_BlackTop + y) * gm->glm_BMModulo + gm->glm_BlackLeft + x];
                        if (actual != expected) {
                            printf("DESCENDER_PIXEL char=%c bold=%d x=%lu y=%lu expected=%u actual=%u\n",
                                *letters, bold, (unsigned long)x, (unsigned long)y, expected, actual);
                            ok = 0; break;
                        }
                    }
                if (ok) printf("DESCENDER_PIXELS_PASS char=%c bold=%d size=%d\n", *letters, bold, tf->tf_YSize);
            }
            struct TagItem release[] = {{OT_GlyphMap8Bits, (IPTR)gm}, {TAG_DONE, 0}};
            ReleaseInfoA(ge, release);
        }
    }
    CloseEngine(ge); CloseLibrary(BulletBase); BulletBase = NULL;
    return ok;
}
static struct IOStdReq *io;
static struct Window *window;
static struct ConUnit *unit;
static ULONG original[32768];
static int send(const char *s, ULONG length, int error)
{
    io->io_Command = CMD_WRITE;
    io->io_Data = (APTR)s;
    io->io_Length = length;
    io->io_Actual = 0xdeadbeef;
    LONG result = DoIO((struct IORequest *)io);
    return result == error && io->io_Error == error &&
        io->io_Actual == (error ? 0 : length);
}
static int put(const char *s) { return send(s, strlen(s), 0); }
static int pixels(int save)
{
    ULONG at = 0;
    for (int y = 0; y < 3 * unit->cu_YRSize; y++)
        for (int x = 0; x < 20 * unit->cu_XRSize; x++) {
            ULONG p = ReadRGBPixel(window->RPort, unit->cu_XROrigin + x,
                unit->cu_YROrigin + y);
            if (at >= 32768) return 0;
            if (save) original[at] = p;
            else if (original[at] != p) { printf("FIDELITY_PIXEL x=%d y=%d expected=%08lx actual=%08lx\n", x, y, (unsigned long)original[at], (unsigned long)p); return 0; }
            at++;
        }
    return 1;
}
#define CHECK(c) do { if (!(c)) { printf("FIDELITY_FAIL line=%d font=%d sequence=%d split=%d\n", __LINE__, font, sequence, split); fflush(stdout); Delay(500); goto out; } } while (0)
int main(int argc, char **argv)
{
    int kind = argc > 2 ? atoi(argv[2]) : CONU_CHARMAP;
    if (argc > 1) metric = atoi(argv[1]);
    int rc = 20, opened = 0, font = 0, sequence = 0, split = 0, cases = 0;
    struct MsgPort *port = CreateMsgPort();
    io = port ? (void *)CreateIORequest(port, sizeof(*io)) : NULL;
    CHECK(io);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    CHECK(CyberGfxBase);
    window = OpenWindowTags(NULL, WA_Left, 30, WA_Top, 30, WA_Width, 730,
        WA_Height, 470, WA_Title, (IPTR)"ConsoleNG complete styled glyphs",
        WA_Activate, TRUE, WA_DragBar, TRUE, WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE, WA_SizeBRight, TRUE, TAG_DONE);
    CHECK(window);
    io->io_Data = window; io->io_Length = sizeof(*window);
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device", kind,
        (struct IORequest *)io, 0));
    opened = 1; unit = (void *)io->io_Unit;
    const char *fonts[] = {"\x1b]50;topaz,8\7", "\x1b]50;DejaVuSansMono.font,16\7"};
    const char *styles[] = {"\x1b[1m", "\x1b[3m", "\x1b[1;3m"};
    const char *glyphs[] = {"W", "M", "f", "y", "\xa3", "Style", "_"};
    const char *reset = "\x1b[?25l\x1b%@\x1b[0m\x1b[2J\x1b[H";
    for (font = 0; font < 2; font++) {
        CHECK(put(fonts[font]));
        if (font == 1) {
            static const int sizes[] = {8, 12, 13, 16, 24, 32};
            for (int bold = 0; bold < 2; bold++)
            for (unsigned size = 0; size < sizeof(sizes)/sizeof(sizes[0]); size++) {
                char command[80];
                snprintf(command, sizeof(command), "\x1b]50;%s.font,%d\7", bold ? "DejaVuSansMono-Bold" : "DejaVuSansMono", sizes[size]);
                CHECK(put(command));
                CHECK(unit->cu_YRSize == sizes[size]);
                CHECK(descenders(window->RPort->Font, bold));
            }
            CHECK(put(fonts[font]));
        }
        for (sequence = 0; sequence < 3; sequence++) {
            for (split = 0; split < 7; split++) {
                /* The oracle bypasses the console renderer: preserve the
                 * complete native styled glyph, including its overhang. */
                CHECK(put(reset)); CHECK(put(styles[sequence]));
                SetABPenDrMd(window->RPort, unit->cu_FgPen, unit->cu_BgPen, JAM2);
                SetSoftStyle(window->RPort, unit->cu_TxFlags,
                    FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
                Move(window->RPort, unit->cu_XROrigin,
                    unit->cu_YROrigin + window->RPort->Font->tf_Baseline);
                Text(window->RPort, (CONST_STRPTR)glyphs[split], strlen(glyphs[split]));
                CHECK(pixels(1));
                CHECK(put(reset)); CHECK(put(styles[sequence]));
                CHECK(put(glyphs[split])); CHECK(pixels(0));
                if (kind != CONU_STANDARD) { CHECK(put("\x1b[>9h\x1b[>9l")); CHECK(pixels(0)); }
                cases++;
            }
        }
    }
    CHECK(put("\x1b%G\x1b[0m\x1b[2J\x1b[HNative styled glyph fidelity: PASS\r\n\r\n42 comparisons against graphics.library Text().\r\nBold, italic, bold italic; Topaz and DejaVu.\r\n72 FreeType descender/underscore maps match font pixels.\r\n\r\n\x1b[1mBold: W M f gjpqy_ Style\x1b[0m\r\n\r\n\x1b[3mItalic: W M f gjpqy_ Style\x1b[0m\r\n\r\n\x1b[1;3mBold italic: W M f gjpqy_ Style\x1b[0m"));
    printf("FIDELITY_PASS unit=%d cases=%d\n", kind, cases); fflush(stdout); Delay(600); rc = 0;
out:
    if (opened) CloseDevice((struct IORequest *)io);
    if (window) CloseWindow(window);
    if (io) DeleteIORequest((struct IORequest *)io);
    if (port) DeleteMsgPort(port);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    printf("FIDELITY_SCRIPT rc=%d\n", rc);
    return rc;
}
