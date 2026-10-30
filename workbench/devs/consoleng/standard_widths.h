/* Private plain-console width metadata, never glyph storage or scrollback. */
#ifndef CONSOLENG_STANDARD_WIDTHS_H
#define CONSOLENG_STANDARD_WIDTHS_H
struct con_width_grid {
    UBYTE *cells;
    ULONG columns, rows;
    WORD cellWidth, cellHeight;
};
static void con_width_clear(struct con_width_grid *g)
{
    if (g->cells) memset(g->cells, 0, g->columns * g->rows);
}
static UBYTE *con_width_row(struct con_width_grid *g, LONG y)
{
    return g->cells && y >= 0 && (ULONG)y < g->rows ? g->cells + y*g->columns : NULL;
}
/* Only allocate on the first wide glyph. Preserve the visible intersection
 * on a geometric resize; a font-cell change invalidates all old coordinates. */
static BOOL con_width_resize(struct con_width_grid *g, ULONG columns, ULONG rows,
    WORD cw, WORD ch, BOOL create)
{
    if (!columns || !rows || columns > 32768 || rows > 32768) {
        FreeVec(g->cells); memset(g, 0, sizeof(*g)); return FALSE;
    }
    if (g->cells && g->columns == columns && g->rows == rows &&
        g->cellWidth == cw && g->cellHeight == ch) return TRUE;
    if (!g->cells && !create) return FALSE;
    UBYTE *next = AllocVec(columns * rows, MEMF_CLEAR);
    if (!next) { FreeVec(g->cells); memset(g, 0, sizeof(*g)); return FALSE; }
    if (g->cells && g->cellWidth == cw && g->cellHeight == ch) {
        ULONG w = columns < g->columns ? columns : g->columns;
        ULONG h = rows < g->rows ? rows : g->rows;
        for (ULONG y=0; y<h; y++) memcpy(next+y*columns, g->cells+y*g->columns, w);
    }
    FreeVec(g->cells); g->cells=next; g->columns=columns; g->rows=rows;
    g->cellWidth=cw; g->cellHeight=ch;
    return TRUE;
}
/* 1=head, 2=tail. A cut between halves clears BOTH before shifting, so
 * fragments from different glyphs cannot accidentally become a valid pair. */
static LONG con_width_cut(UBYTE *row, ULONG columns, LONG x)
{
    if (!row || x <= 0 || (ULONG)x >= columns || row[x] != 2) return -1;
    row[x-1]=row[x]=0;
    return x-1;
}
static void con_width_edit(UBYTE *row, ULONG columns, ULONG x, ULONG n, int op)
{
    if (!row || x >= columns || !n) return;
    if (n > columns-x) n=columns-x;
    if (op == 1) { memmove(row+x+n,row+x,columns-x-n); memset(row+x,0,n); }
    else if (op == -1) { memmove(row+x,row+x+n,columns-x-n); memset(row+columns-n,0,n); }
    else memset(row+x,0,n);
}
static void con_width_scroll(struct con_width_grid *g, LONG top, LONG bottom, LONG count)
{
    if (!g->cells || top < 0 || bottom < top || (ULONG)bottom >= g->rows || !count) return;
    ULONG n=count < 0 ? -count : count, rows=bottom-top+1;
    if(n>rows)n=rows;
    UBYTE *p=g->cells+top*g->columns;
    if(count>0) { memmove(p,p+n*g->columns,(rows-n)*g->columns); memset(p+(rows-n)*g->columns,0,n*g->columns); }
    else { memmove(p+n*g->columns,p,(rows-n)*g->columns); memset(p,0,n*g->columns); }
}
#endif
