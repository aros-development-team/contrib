/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <string.h>
#include "search_regex.h"
#include "search_unicode.h"
#include "search_case.h"

enum { RX_CHAR, RX_ANY, RX_CLASS, RX_NOTCLASS, RX_SPLIT, RX_JUMP, RX_BOL, RX_EOL, RX_BOS, RX_EOS, RX_BOUNDARY, RX_NOTBOUNDARY, RX_WORDSTART, RX_WORDEND, RX_STARTHALF, RX_ENDHALF, RX_MATCH };
enum { RX_FETCH, RX_CLOSE, RX_CONSUME, RX_SKIP };
#define RX_NONE 0xffff
#define RX_FOLD 1
#define RX_DOTALL 2
#define RX_CRLF 4
#define RX_ASCII 8
struct rx_fragment { UWORD start, head, tail; };
struct rx_parser { struct con_regex *r; const ULONG *q; ULONG at, length, depth; BOOL failed; ULONG work; BOOL fold, flag_only, ungreedy, extended, dotall, multiline, crlf, unicode; UWORD name_count, names[CON_SEARCH_MAX / 4][2]; };

static VOID rx_space(struct rx_parser *p)
{
    if (!p->extended) return;
    BOOL comment = FALSE;
    while (p->at < p->length) {
        ULONG cp = p->q[p->at];
        if (!comment && cp != '#' && !rx_unicode_class(cp, 2)) break;
        if (++p->work > CON_REGEX_COMPILE_WORK) { p->failed = TRUE; return; }
        if (cp == '#') comment = TRUE;
        if (cp == '\n') comment = FALSE;
        p->at++;
    }
}

static ULONG rx_fold(ULONG cp, BOOL fold)
{ return fold ? con_case_fold(cp) : cp; }
static ULONG rx_mode_fold(ULONG cp, BOOL fold, BOOL unicode)
{
    if (!unicode) return fold && cp >= 'A' && cp <= 'Z' ? cp + ('a'-'A') : cp;
    return rx_fold(cp, fold);
}
static BOOL rx_mode_class(ULONG cp, ULONG mask, BOOL ascii)
{
    if (!ascii) return rx_unicode_class(cp, mask);
    BOOL digit = cp >= '0' && cp <= '9';
    BOOL word = digit || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || cp == '_';
    BOOL space = cp == ' ' || (cp >= 9 && cp <= 13);
    return ((mask & 1) && digit) || ((mask & 2) && space) || ((mask & 4) && word);
}

BOOL con_regex_smart_fold(const ULONG *query, ULONG length)
{
    for (ULONG i=0; i<length; i++) if (con_case_upper(query[i])) return FALSE;
    return TRUE;
}

static UWORD rx_node(struct rx_parser *p, UWORD op)
{
    /* Reserve a disposable sentinel so a rejected expansion never aliases
     * an existing fragment while recursive parsing unwinds. */
    UWORD n;
    if (++p->work > CON_REGEX_COMPILE_WORK || p->r->nodes_used >= CON_REGEX_STATES - 1) {
        p->failed = TRUE; n = CON_REGEX_STATES - 1;
    } else n = p->r->nodes_used++;
    memset(&p->r->nodes[n], 0, sizeof(p->r->nodes[n]));
    p->r->nodes[n].op = op;
    p->r->nodes[n].flags = (p->fold ? RX_FOLD : 0) | (p->dotall ? RX_DOTALL : 0) | (p->crlf ? RX_CRLF : 0) | (p->unicode ? 0 : RX_ASCII);
    p->r->nodes[n].out = p->r->nodes[n].other = RX_NONE;
    return n;
}
static UWORD *rx_edge(struct con_regex *r, UWORD edge)
{ return edge & 1 ? &r->nodes[edge / 2].other : &r->nodes[edge / 2].out; }
static struct rx_fragment rx_single(struct rx_parser *p, UWORD op)
{
    UWORD n = rx_node(p, op);
    return (struct rx_fragment){n, n * 2, n * 2};
}
static VOID rx_patch(struct con_regex *r, UWORD head, UWORD target)
{
    while (head != RX_NONE) { UWORD *edge = rx_edge(r, head), next = *edge; *edge = target; head = next; }
}
static struct rx_fragment rx_join(struct rx_parser *p,
    struct rx_fragment left, struct rx_fragment right)
{
    if (p->r->reverse)
    {
        rx_patch(p->r, right.head, left.start);
        right.head = left.head; right.tail = left.tail;
        return right;
    }
    rx_patch(p->r, left.head, right.start);
    left.head = right.head; left.tail = right.tail;
    return left;
}
static ULONG rx_shorthand(ULONG cp)
{
    switch (cp) {
        case 'd': return 1; case 's': return 2; case 'w': return 4;
        case 'D': return 8; case 'S': return 16; case 'W': return 32;
        default: return 0;
    }
}
/* Property predicates use immutable Unicode tables and one bounded range
 * slot per table. Bit 31 tags a table property; bit 30 complements after fold. */
static ULONG rx_property(struct rx_parser *p)
{
    BOOL neg=p->q[p->at++]=='P';
    char name[64];ULONG n=0;
    rx_space(p);
    if(!p->unicode || p->at>=p->length){p->failed=TRUE;return 0;}
    BOOL braced=p->q[p->at]=='{';
    if(braced)p->at++;
    while(p->at<p->length){
        if(braced){rx_space(p);if(p->at>=p->length || p->q[p->at]=='}')break;}
        ULONG cp=p->q[p->at++];
        if(++p->work>CON_REGEX_COMPILE_WORK || n+1>=sizeof(name) || cp>127){p->failed=TRUE;return 0;}
        if(braced && (cp=='_' || cp=='-' || cp==' ' || (cp>=9 && cp<=13)))continue;
        if(cp>='A'&&cp<='Z')cp+='a'-'A';
        name[n++]=cp;
        if(!braced)break;
    }
    if(!n || (braced && (p->at>=p->length || p->q[p->at]!='}'))){p->failed=TRUE;return 0;}
    if(braced)p->at++;
    name[n]=0;
    /* Colon is equality. The pinned reference ignores != during translation;
     * reject that form instead of silently giving it equality semantics. */
    char *op=strstr(name,"!=");
    if(op){p->failed=TRUE;return 0;}
    else {op=strchr(name,':');if(op)*op='=';}
    ULONG mask=0;
    if(!strcmp(name,"nd") || !strcmp(name,"decimalnumber") ||
       !strcmp(name,"gc=nd") || !strcmp(name,"gc=decimalnumber") ||
       !strcmp(name,"generalcategory=nd") || !strcmp(name,"generalcategory=decimalnumber"))mask=1;
    else if(!strcmp(name,"whitespace") || !strcmp(name,"wspace") || !strcmp(name,"space"))mask=2;
    else {
        const char *value=name;
        ULONG family=0,tag=0;
        if(!strncmp(value,"gc=",3)){value+=3;family=1;}
        else if(!strncmp(value,"generalcategory=",16)){value+=16;family=1;}
        else if(!strncmp(value,"sc=",3)){value+=3;family=2;}
        else if(!strncmp(value,"script=",7)){value+=7;family=2;}
        else if(!strncmp(value,"scx=",4)){value+=4;family=3;}
        else if(!strncmp(value,"scriptextensions=",17)){value+=17;family=3;}
        else if(!strncmp(value,"age=",4)){value+=4;family=4;}
        else if(!strncmp(value,"gcb=",4)){value+=4;family=5;}
        else if(!strncmp(value,"graphemeclusterbreak=",21)){value+=21;family=5;}
        else if(!strncmp(value,"sb=",3)){value+=3;family=6;}
        else if(!strncmp(value,"sentencebreak=",14)){value+=14;family=6;}
        else if(!strncmp(value,"wb=",3)){value+=3;family=7;}
        else if(!strncmp(value,"wordbreak=",10)){value+=10;family=7;}
        if(family==0)for(ULONG i=0;i<sizeof(rx_bp_aliases)/sizeof(*rx_bp_aliases);i++)
            if(!strcmp(value,rx_bp_aliases[i].name)){
                tag=0x30000UL | rx_bp_aliases[i].id;goto property_found;
            }
        if(family<=1){
            if(!strcmp(value,"any")){tag=0x40000UL;goto property_found;}
            if(!strcmp(value,"ascii")){tag=0x40001UL;goto property_found;}
            if(!strcmp(value,"assigned")){tag=0x40002UL;goto property_found;}
        }
        if(family<=1)for(ULONG i=0;i<sizeof(rx_gc_aliases)/sizeof(*rx_gc_aliases);i++)
            if(!strcmp(value,rx_gc_aliases[i].name)){
                tag=rx_gc_aliases[i].id;goto property_found;
            }
        if(family==0 || family==2)for(ULONG i=0;i<sizeof(rx_sc_aliases)/sizeof(*rx_sc_aliases);i++)
            if(!strcmp(value,rx_sc_aliases[i].name)){
                tag=0x10000UL | rx_sc_aliases[i].id;goto property_found;
            }
        if(family==3)for(ULONG i=0;i<sizeof(rx_scx_aliases)/sizeof(*rx_scx_aliases);i++)
            if(!strcmp(value,rx_scx_aliases[i].name)){
                tag=0x20000UL | rx_scx_aliases[i].id;goto property_found;
            }
        if(family==4)for(ULONG i=0;i<sizeof(rx_age_aliases)/sizeof(*rx_age_aliases);i++)
            if(!strcmp(value,rx_age_aliases[i].name)){
                tag=0x50000UL | rx_age_aliases[i].id;goto property_found;
            }
        if(family==5)for(ULONG i=0;i<sizeof(rx_gcb_aliases)/sizeof(*rx_gcb_aliases);i++)
            if(!strcmp(value,rx_gcb_aliases[i].name)){
                tag=0x60000UL | rx_gcb_aliases[i].id;goto property_found;
            }
        if(family==6)for(ULONG i=0;i<sizeof(rx_sb_aliases)/sizeof(*rx_sb_aliases);i++)
            if(!strcmp(value,rx_sb_aliases[i].name)){
                tag=0x70000UL | rx_sb_aliases[i].id;goto property_found;
            }
        if(family==7)for(ULONG i=0;i<sizeof(rx_wb_aliases)/sizeof(*rx_wb_aliases);i++)
            if(!strcmp(value,rx_wb_aliases[i].name)){
                tag=0x80000UL | rx_wb_aliases[i].id;goto property_found;
            }
        p->failed=TRUE;return 0;
property_found:
        return 0x80000000UL | (neg?0x40000000UL:0) | tag;
    }
    return neg?mask<<3:mask;
}
static VOID rx_property_node(struct rx_parser *p, struct con_regex_node *node)
{
    ULONG property=rx_property(p);
    if(!(property & 0x80000000UL)){node->cp |= property;return;}
    if(p->r->ranges_used>=CON_SEARCH_MAX){p->failed=TRUE;return;}
    if(!node->count)node->first=p->r->ranges_used;
    p->r->ranges[p->r->ranges_used++]=(struct con_regex_range){0xffffffffUL,property};
    node->count++;
}
/* Escapes denote Unicode scalars, including when used as class range ends.
 * Check before accumulation so long braced values cannot wrap ULONG. */
static ULONG rx_hex(struct rx_parser *p, ULONG kind)
{
    ULONG value = 0, digits = 0, limit = kind == 'x' ? 2 : kind == 'u' ? 4 : 8;
    rx_space(p);
    BOOL braced = p->at < p->length && p->q[p->at] == '{';
    if (braced) { p->at++; rx_space(p); }
    while (p->at < p->length && (braced ? p->q[p->at] != '}' : digits < limit)) {
        ULONG cp = p->q[p->at++], digit;
        if (cp >= '0' && cp <= '9') digit = cp - '0';
        else if (cp >= 'a' && cp <= 'f') digit = cp - 'a' + 10;
        else if (cp >= 'A' && cp <= 'F') digit = cp - 'A' + 10;
        else { p->failed = TRUE; return 0; }
        if (++p->work > CON_REGEX_COMPILE_WORK || value > (0x10ffff - digit) / 16) {
            p->failed = TRUE; return 0;
        }
        value = value * 16 + digit;
        digits++;
        /* Fixed forms leave the character following the final digit alone. */
        if (braced || digits < limit) rx_space(p);
    }
    if (!digits || (braced ? p->at >= p->length || p->q[p->at++] != '}' : digits != limit) ||
        (value >= 0xd800 && value <= 0xdfff)) p->failed = TRUE;
    return value;
}
static ULONG rx_escaped(struct rx_parser *p)
{
    if (p->at >= p->length) { p->failed = TRUE; return 0; }
    ULONG cp = p->q[p->at++];
    if (cp == 'x' || cp == 'u' || cp == 'U') {
        rx_space(p);
        BOOL byte = cp == 'x' && (p->at >= p->length || p->q[p->at] != '{');
        ULONG value = rx_hex(p, cp);
        if (!p->unicode && byte && value > 127) p->failed = TRUE;
        return value;
    }
    if (cp == 'a') return '\a';
    if (cp == 'f') return '\f';
    if (cp == 'v') return '\v';
    if (cp == 'n') return '\n';
    if (cp == 'r') return '\r';
    if (cp == 't') return '\t';
    /* No accidental acceptance of backreferences or unsupported properties. */
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
        (cp >= '0' && cp <= '9')) p->failed = TRUE;
    return cp;
}
static ULONG rx_class_char(struct rx_parser *p)
{
    if (p->at >= p->length) { p->failed = TRUE; return 0; }
    ULONG cp = p->q[p->at++];
    return cp == '\\' ? rx_escaped(p) : cp;
}
/* POSIX names denote ASCII sets even in Unicode mode. Positive sets use the
 * existing range pool; negative sets use bits 6..19 of a class node's mask,
 * keeping complement-after-fold distinct from the surrounding class union. */
static const struct {
    const char *name;
    UBYTE count;
    UBYTE bounds[8];
} rx_ascii_classes[] = {
    {"alnum",3,{'0','9','A','Z','a','z'}},
    {"alpha",2,{'A','Z','a','z'}},
    {"ascii",1,{0,127}},
    {"blank",2,{9,9,32,32}},
    {"cntrl",2,{0,31,127,127}},
    {"digit",1,{'0','9'}},
    {"graph",1,{33,126}},
    {"lower",1,{'a','z'}},
    {"print",1,{32,126}},
    {"punct",4,{33,47,58,64,91,96,123,126}},
    {"space",2,{9,13,32,32}},
    {"upper",1,{'A','Z'}},
    {"word",4,{'0','9','A','Z','_','_','a','z'}},
    {"xdigit",3,{'0','9','A','F','a','f'}}
};
static VOID rx_ascii_class(struct rx_parser *p, struct con_regex_node *node)
{
    p->at += 2; /* [: */
    BOOL neg = p->at < p->length && p->q[p->at] == '^';
    if (neg) { p->at++; if (!p->unicode) p->failed = TRUE; }
    ULONG start = p->at;
    while (p->at < p->length && p->q[p->at] >= 'a' && p->q[p->at] <= 'z') {
        if (++p->work > CON_REGEX_COMPILE_WORK) { p->failed = TRUE; return; }
        p->at++;
    }
    ULONG length = p->at - start;
    if (!length || p->at + 1 >= p->length || p->q[p->at] != ':' ||
        p->q[p->at+1] != ']') { p->failed = TRUE; return; }
    p->at += 2;
    for (ULONG i=0; i<sizeof(rx_ascii_classes)/sizeof(*rx_ascii_classes); i++) {
        if (strlen(rx_ascii_classes[i].name) != length) continue;
        ULONG j;
        for (j=0; j<length && p->q[start+j] == (UBYTE)rx_ascii_classes[i].name[j]; j++) {}
        if (j != length) continue;
        /* Bits 0..5 belong to Unicode shorthands. A negated ASCII member is
         * complemented after folding, before union with the rest of a class. */
        if (neg) { node->cp |= 1UL << (6+i); return; }
        if (p->r->ranges_used + rx_ascii_classes[i].count > CON_SEARCH_MAX) {
            p->failed = TRUE; return;
        }
        for (j=0; j<rx_ascii_classes[i].count; j++)
            p->r->ranges[p->r->ranges_used++] = (struct con_regex_range){
                rx_ascii_classes[i].bounds[j*2], rx_ascii_classes[i].bounds[j*2+1]};
        node->count += rx_ascii_classes[i].count;
        return;
    }
    p->failed = TRUE;
}
/* Set expressions use postfix entries in the existing bounded range pool.
 * Fold leaves before Boolean operations, including inner complements. */
#define RX_SET_FLAG 16
#define RX_SET_OP 0xfffffffeUL
#define RX_SET_MASK 0xfffffffdUL
enum { RX_SET_UNION, RX_SET_INTERSECT, RX_SET_DIFFERENCE, RX_SET_XOR, RX_SET_NOT };
static VOID rx_set_emit(struct rx_parser *p, ULONG low, ULONG high)
{
    if (++p->work > CON_REGEX_COMPILE_WORK || p->r->ranges_used >= CON_SEARCH_MAX) {
        p->failed = TRUE; return;
    }
    p->r->ranges[p->r->ranges_used++] = (struct con_regex_range){low,high};
}
static int rx_set_operator(struct rx_parser *p)
{
    if (p->at+1 >= p->length || p->q[p->at] != p->q[p->at+1]) return -1;
    return p->q[p->at]=='&' ? RX_SET_INTERSECT :
        p->q[p->at]=='-' ? RX_SET_DIFFERENCE :
        p->q[p->at]=='~' ? RX_SET_XOR : -1;
}
static VOID rx_set_bracket(struct rx_parser *p);
static VOID rx_set_item(struct rx_parser *p)
{
    struct con_regex_node leaf = {0};
    ULONG first=p->r->ranges_used;
    if (p->q[p->at]=='[') {
        if (p->at+1<p->length && p->q[p->at+1]==':') rx_ascii_class(p,&leaf);
        else {
            p->at++; rx_set_bracket(p); rx_space(p);
            if (p->at<p->length && p->q[p->at]=='-' && rx_set_operator(p)<0) {
                ULONG dash=p->at++;rx_space(p);
                if(p->at>=p->length || p->q[p->at]!=']')p->failed=TRUE;
                p->at=dash;
            }
            return;
        }
    } else if (p->q[p->at]=='\\' && p->at+1<p->length &&
        (p->q[p->at+1]=='p'||p->q[p->at+1]=='P')) {
        p->at++;rx_property_node(p,&leaf);
    } else if (p->q[p->at]=='\\' && p->at+1<p->length && rx_shorthand(p->q[p->at+1])) {
        leaf.cp=rx_shorthand(p->q[p->at+1]);p->at+=2;
        if (!p->unicode && (leaf.cp&56)) p->failed=TRUE;
    } else {
        ULONG low=rx_class_char(p),high=low;rx_space(p);
        if (p->at<p->length && p->q[p->at]=='-' && rx_set_operator(p)<0) {
            ULONG dash=p->at++;rx_space(p);
            if (p->at<p->length && p->q[p->at]==']') p->at=dash;
            else {
                if (p->at<p->length && p->q[p->at]=='[') p->failed=TRUE;
                high=rx_class_char(p);rx_space(p);
            }
        }
        if (low>high || (!p->unicode && high>127)) p->failed=TRUE;
        rx_set_emit(p,low,high);return;
    }
    ULONG leaves=p->r->ranges_used-first;
    if (leaf.cp) {rx_set_emit(p,RX_SET_MASK,leaf.cp);leaves++;}
    while (leaves>1) {rx_set_emit(p,RX_SET_OP,RX_SET_UNION);leaves--;}
    rx_space(p);
    if (p->at<p->length && p->q[p->at]=='-' && rx_set_operator(p)<0) {
        ULONG dash=p->at++;rx_space(p);
        if (p->at>=p->length || p->q[p->at]!=']') p->failed=TRUE;
        p->at=dash;
    }
}
static VOID rx_set_union(struct rx_parser *p, BOOL initial)
{
    BOOL have=FALSE;
    if(initial && p->at<p->length && p->q[p->at]=='-') {
        p->at++;rx_set_emit(p,'-','-');have=TRUE;initial=FALSE;
    }
    while (!p->failed && p->at<p->length) {
        rx_space(p);
        if (p->at>=p->length || rx_set_operator(p)>=0 ||
            (p->q[p->at]==']' && !(initial && !have))) break;
        rx_set_item(p);
        if (have) rx_set_emit(p,RX_SET_OP,RX_SET_UNION);
        have=TRUE;initial=FALSE;rx_space(p);
    }
    if (!have) rx_set_emit(p,RX_SET_MASK,0);
}
static VOID rx_set_bracket(struct rx_parser *p)
{
    if (++p->depth>32) {p->failed=TRUE;return;}
    rx_space(p);
    BOOL neg=p->at<p->length && p->q[p->at]=='^';
    if (neg) {p->at++;rx_space(p);if(!p->unicode)p->failed=TRUE;}
    rx_set_union(p,TRUE);
    while (!p->failed) {
        int op=rx_set_operator(p);if(op<0)break;
        p->at+=2;rx_space(p);rx_set_union(p,FALSE);
        rx_set_emit(p,RX_SET_OP,(ULONG)op);
    }
    if (p->at>=p->length || p->q[p->at++]!=']') p->failed=TRUE;
    if (neg) rx_set_emit(p,RX_SET_OP,RX_SET_NOT);
    p->depth--;
}
/* Keep simple classes compact. Nested brackets and binary operators select
 * the postfix compiler; escaped punctuation never changes parser selection. */
static BOOL rx_set_complex(struct rx_parser *p)
{
    if (p->at<p->length && p->q[p->at]==']') return TRUE;
    if (p->at+1<p->length && p->q[p->at]=='^' && p->q[p->at+1]==']') return TRUE;
    for (ULONG i=p->at;i<p->length;i++) {
        if (p->q[i]=='\\') {i++;continue;}
        if (p->q[i]==']') return FALSE;
        if (p->q[i]=='[') {
            if(i+1<p->length && p->q[i+1]==':') {
                i+=2;while(i<p->length && p->q[i]!=']')i++;
                continue;
            }
            return TRUE;
        }
        if (i+1<p->length && p->q[i]==p->q[i+1] &&
            (p->q[i]=='&'||p->q[i]=='-'||p->q[i]=='~')) return TRUE;
    }
    return FALSE;
}

/* Names are compile-time syntax only: search reports the complete match. */
static VOID rx_capture_name(struct rx_parser *p)
{
    ULONG start = p->at;
    while (p->at < p->length && p->q[p->at] != '>') {
        if (++p->work > CON_REGEX_COMPILE_WORK ||
            !rx_capture_char(p->q[p->at], p->at == start)) { p->failed = TRUE; return; }
        p->at++;
    }
    ULONG length = p->at - start;
    if (!length || p->at >= p->length || p->name_count >= CON_SEARCH_MAX/4) {
        p->failed = TRUE; return;
    }
    for (ULONG i=0;i<p->name_count;i++) {
        if (p->names[i][1] != length) continue;
        BOOL same = TRUE;
        for (ULONG j=0;j<length;j++) {
            if (++p->work > CON_REGEX_COMPILE_WORK) { p->failed = TRUE; return; }
            if (p->q[p->names[i][0]+j] != p->q[start+j]) { same = FALSE; break; }
        }
        if (same) { p->failed = TRUE; return; }
    }
    p->names[p->name_count][0] = start;
    p->names[p->name_count++][1] = length;
    p->at++;
}
static struct rx_fragment rx_expr(struct rx_parser *p);
static struct rx_fragment rx_atom(struct rx_parser *p)
{
    p->flag_only = FALSE;
    ULONG cp = p->q[p->at++];
    if (cp == '(') {
        BOOL saved_fold = p->fold, saved_ungreedy = p->ungreedy;
        BOOL saved_extended = p->extended, saved_dotall = p->dotall;
        BOOL saved_multiline = p->multiline, saved_crlf = p->crlf;
        BOOL saved_unicode = p->unicode;
        if (++p->depth > 32) { p->failed = TRUE; return rx_single(p, RX_JUMP); }
        rx_space(p);
        if (p->at < p->length && p->q[p->at] == '?') {
            p->at++;
            if (p->at < p->length && (p->q[p->at] == '<' ||
                (p->q[p->at] == 'P' && p->at+1 < p->length && p->q[p->at+1] == '<'))) {
                if (p->q[p->at] == 'P') p->at++;
                p->at++; rx_capture_name(p);
                if (p->failed) return rx_single(p, RX_JUMP);
            }
            else if (p->at < p->length && p->q[p->at] == ':') p->at++;
            else {
                BOOL enable = TRUE, after_minus = FALSE;
                ULONG seen = 0;
                while (p->at < p->length && p->q[p->at] != ':' && p->q[p->at] != ')') {
                    ULONG flag = p->q[p->at++];
                    if (flag == '-' && enable) { enable = FALSE; after_minus = TRUE; continue; }
                    ULONG bit = flag == 'i' ? 1 : flag == 'U' ? 2 : flag == 'x' ? 4 : flag == 's' ? 8 : flag == 'm' ? 16 : flag == 'R' ? 32 : flag == 'u' ? 64 : 0;
                    if (!bit || (seen & bit)) { p->failed = TRUE; break; }
                    seen |= bit; after_minus = FALSE;
                    if (flag == 'i') p->fold = enable;
                    else if (flag == 'U') p->ungreedy = enable;
                    else if (flag == 'x') p->extended = enable;
                    else if (flag == 'm') p->multiline = enable;
                    else if (flag == 'R') p->crlf = enable;
                    else if (flag == 'u') p->unicode = enable;
                    else p->dotall = enable;
                }
                if (p->failed || !seen || after_minus || p->at >= p->length) {
                    p->failed = TRUE; return rx_single(p, RX_JUMP);
                }
                if (p->q[p->at++] == ')') {
                    p->depth--; p->flag_only = TRUE;
                    return rx_single(p, RX_JUMP);
                }
            }
        }
        struct rx_fragment f = rx_expr(p);
        if (p->at >= p->length || p->q[p->at++] != ')') p->failed = TRUE;
        p->depth--; p->fold = saved_fold; p->ungreedy = saved_ungreedy;
        p->extended = saved_extended;
        p->dotall = saved_dotall;
        p->multiline = saved_multiline;
        p->crlf = saved_crlf;
        p->unicode = saved_unicode;
        p->flag_only = FALSE; return f;
    }
    if (cp == '[') {
        rx_space(p);
        if (rx_set_complex(p)) {
            struct rx_fragment f=rx_single(p,RX_CLASS);
            struct con_regex_node *n=&p->r->nodes[f.start];
            n->first=p->r->ranges_used;n->flags|=RX_SET_FLAG;
            rx_set_bracket(p);n->count=p->r->ranges_used-n->first;
            return f;
        }
        rx_space(p);
        BOOL neg = p->at < p->length && p->q[p->at] == '^';
        if (neg) { p->at++; rx_space(p); if (!p->unicode) p->failed = TRUE; }
        struct rx_fragment f = rx_single(p, neg ? RX_NOTCLASS : RX_CLASS);
        struct con_regex_node *node = &p->r->nodes[f.start];
        node->first = p->r->ranges_used;
        while (!p->failed && p->at < p->length && p->q[p->at] != ']') {
            if (p->q[p->at] == '[' && p->at + 1 < p->length && p->q[p->at+1] == ':') {
                rx_ascii_class(p, node);
                rx_space(p);
                if (p->at < p->length && p->q[p->at] == '-') {
                    ULONG dash = p->at++;
                    rx_space(p);
                    if (p->at >= p->length || p->q[p->at] != ']') p->failed = TRUE;
                    p->at = dash;
                }
                continue;
            }
            if (p->q[p->at] == '\\' && p->at + 1 < p->length &&
                (p->q[p->at+1]=='p'||p->q[p->at+1]=='P')) {
                p->at++;rx_property_node(p,node);rx_space(p);
                if(p->at<p->length && p->q[p->at]=='-'){
                    ULONG dash=p->at++;rx_space(p);
                    if(p->at>=p->length || p->q[p->at]!=']')p->failed=TRUE;
                    p->at=dash;
                }
                continue;
            }
            if (p->q[p->at] == '\\' && p->at + 1 < p->length && rx_shorthand(p->q[p->at+1])) {
                node->cp |= rx_shorthand(p->q[p->at+1]); p->at += 2; rx_space(p);
                if (!p->unicode && (node->cp & 56)) p->failed = TRUE;
                if (p->at < p->length && p->q[p->at] == '-') {
                    ULONG dash = p->at++;
                    rx_space(p);
                    if (p->at >= p->length || p->q[p->at] != ']') p->failed = TRUE;
                    p->at = dash;
                }
                continue;
            }
            ULONG lo = rx_class_char(p), hi = lo;
            rx_space(p);
            if (p->at < p->length && p->q[p->at] == '-') {
                ULONG dash = p->at++;
                rx_space(p);
                if (p->at < p->length && p->q[p->at] == ']') p->at = dash;
                else { hi = rx_class_char(p); rx_space(p); }
            }
            if ((!p->unicode && hi > 127) || lo > hi || p->r->ranges_used >= CON_SEARCH_MAX) { p->failed = TRUE; break; }
            p->r->ranges[p->r->ranges_used++] = (struct con_regex_range){lo, hi};
            node->count++;
        }
        if ((!node->count && !node->cp) || p->at >= p->length || p->q[p->at++] != ']') p->failed = TRUE;
        return f;
    }
    if (cp == '\\' && p->at < p->length &&
        (p->q[p->at] == 'A' || p->q[p->at] == 'z')) {
        UWORD anchor = p->q[p->at++] == 'A' ? RX_BOS : RX_EOS;
        if (p->r->reverse) anchor = anchor == RX_BOS ? RX_EOS : RX_BOS;
        return rx_single(p, anchor);
    }
    if (cp == '\\' && p->at < p->length &&
        (p->q[p->at] == 'b' || p->q[p->at] == 'B' ||
         (!p->unicode && (p->q[p->at] == '<' || p->q[p->at] == '>')))) {
        ULONG kind = p->q[p->at++];
        UWORD op = kind == 'b' ? RX_BOUNDARY : kind == 'B' ? RX_NOTBOUNDARY :
            kind == '<' ? RX_WORDSTART : RX_WORDEND;
        if (!p->unicode && kind == 'b' && p->at < p->length && p->q[p->at] == '{') {
            static const struct { const char *name; UWORD op; } names[] = {
                {"start",RX_WORDSTART},{"end",RX_WORDEND},
                {"start-half",RX_STARTHALF},{"end-half",RX_ENDHALF}};
            ULONG start = ++p->at;
            while (p->at < p->length && p->q[p->at] != '}') p->at++;
            BOOL found = FALSE;
            for (ULONG i=0;i<sizeof(names)/sizeof(*names);i++) {
                ULONG n = strlen(names[i].name), j=0;
                if (p->at-start != n) continue;
                for (;j<n && p->q[start+j] == (UBYTE)names[i].name[j];j++) {}
                if (j==n) { op=names[i].op;found=TRUE;break; }
            }
            if (!found || p->at >= p->length) p->failed = TRUE;
            else p->at++;
        }
        if (p->r->reverse) {
            if (op == RX_WORDSTART) op = RX_WORDEND;
            else if (op == RX_WORDEND) op = RX_WORDSTART;
            else if (op == RX_STARTHALF) op = RX_ENDHALF;
            else if (op == RX_ENDHALF) op = RX_STARTHALF;
        }
        return rx_single(p, op);
    }
    if(cp=='\\' && p->at<p->length && (p->q[p->at]=='p'||p->q[p->at]=='P')){
        struct rx_fragment f=rx_single(p,RX_CLASS);
        rx_property_node(p,&p->r->nodes[f.start]);return f;
    }
    if (cp == '\\' && p->at < p->length && rx_shorthand(p->q[p->at])) {
        struct rx_fragment f = rx_single(p, RX_CLASS);
        p->r->nodes[f.start].cp = rx_shorthand(p->q[p->at++]);
        if (!p->unicode && (p->r->nodes[f.start].cp & 56)) p->failed = TRUE;
        return f;
    }
    UWORD op = cp == '.' ? RX_ANY :
        cp == '^' ? (p->multiline ? RX_BOL : RX_BOS) :
        cp == '$' ? (p->multiline ? RX_EOL : RX_EOS) : RX_CHAR;
    if (p->r->reverse && (op == RX_BOL || op == RX_EOL))
        op = op == RX_BOL ? RX_EOL : RX_BOL;
    else if (p->r->reverse && (op == RX_BOS || op == RX_EOS))
        op = op == RX_BOS ? RX_EOS : RX_BOS;
    if (!p->unicode && op == RX_ANY) p->failed = TRUE;
    if (cp == '\\') cp = rx_escaped(p);
    else if (cp == '*' || cp == '+' || cp == '?' || cp == '{' || cp == '}' || cp == ']') p->failed = TRUE;
    struct rx_fragment f = rx_single(p, op);
    p->r->nodes[f.start].cp = rx_mode_fold(cp, p->fold, p->unicode);
    return f;
}
static struct rx_fragment rx_quantify(struct rx_parser *p, struct rx_fragment f,
    ULONG quant, BOOL lazy)
{
    UWORD n = rx_node(p, RX_SPLIT);
    if (p->failed) return f;
    UWORD exit = n * 2 + (lazy ? 0 : 1);
    if (lazy) p->r->nodes[n].other = f.start;
    else p->r->nodes[n].out = f.start;
    if (quant == '?') {
        *rx_edge(p->r, f.tail) = exit; f.tail = exit; f.start = n;
    } else {
        rx_patch(p->r, f.head, n); f.head = f.tail = exit;
        if (quant == '*') f.start = n;
    }
    return f;
}
static BOOL rx_lazy(struct rx_parser *p)
{
    if (p->at < p->length && p->q[p->at] == '?') { p->at++; return !p->ungreedy; }
    return p->ungreedy;
}
static ULONG rx_count(struct rx_parser *p)
{
    ULONG count = 0, digits = 0;
    rx_space(p);
    while (p->at < p->length && p->q[p->at] >= '0' && p->q[p->at] <= '9') {
        count = count * 10 + p->q[p->at++] - '0';
        digits++;
        if (count > CON_SEARCH_MAX) { p->failed = TRUE; return 0; }
        rx_space(p);
    }
    if (!digits) p->failed = TRUE;
    return count;
}
static struct rx_fragment rx_counted(struct rx_parser *p, struct rx_fragment f,
    ULONG atom_at, ULONG nodes_before, ULONG ranges_before)
{
    p->at++; /* { */
    ULONG minimum = rx_count(p), maximum = minimum;
    BOOL unlimited = FALSE;
    if (!p->failed && p->at < p->length && p->q[p->at] == ',') {
        p->at++; rx_space(p);
        unlimited = p->at < p->length && p->q[p->at] == '}';
        if (!unlimited) maximum = rx_count(p);
    }
    if (p->failed || p->at >= p->length || p->q[p->at++] != '}' || maximum < minimum) {
        p->failed = TRUE; return f;
    }
    /* The reference permits spacing before a counted repetition's lazy
     * suffix, unlike the suffix of a simple *, + or ? operator. */
    rx_space(p);
    BOOL lazy = rx_lazy(p);
    ULONG resume = p->at, copies = maximum ? maximum : 1;
    ULONG width = p->r->nodes_used - nodes_before;
    ULONG range_width = p->r->ranges_used - ranges_before;
    ULONG splits = unlimited ? 1 : maximum - minimum;
    if (p->r->nodes_used + (copies - 1) * width + splits >= CON_REGEX_STATES - 1 ||
        p->r->ranges_used + (copies - 1) * range_width > CON_SEARCH_MAX) {
        p->failed = TRUE; return f;
    }
    if (!maximum) {
        if (unlimited) return rx_quantify(p, f, '*', lazy);
        p->r->nodes_used = nodes_before; p->r->ranges_used = ranges_before;
        return rx_single(p, RX_JUMP);
    }
    struct rx_fragment result = {RX_NONE, RX_NONE, RX_NONE};
    for (ULONG i = 0; i < copies; i++) {
        if (i) { p->at = atom_at; f = rx_atom(p); }
        if (p->failed) return f;
        if (i >= minimum) f = rx_quantify(p, f, '?', lazy);
        else if (unlimited && i + 1 == copies) f = rx_quantify(p, f, '+', lazy);
        if (p->failed) return f;
        if (!i) result = f;
        else result = rx_join(p, result, f);
    }
    p->at = resume;
    return result;
}
static struct rx_fragment rx_sequence(struct rx_parser *p)
{
    struct rx_fragment result = {RX_NONE, RX_NONE, RX_NONE};
    while (!p->failed) {
        rx_space(p);
        if (p->failed || p->at >= p->length || p->q[p->at] == ')' || p->q[p->at] == '|') break;
        ULONG atom_at = p->at, nodes_before = p->r->nodes_used, ranges_before = p->r->ranges_used;
        struct rx_fragment f = rx_atom(p);
        if (p->failed) return f;
        rx_space(p);
        if (p->flag_only && p->at < p->length &&
            (p->q[p->at] == '*' || p->q[p->at] == '+' ||
             p->q[p->at] == '?' || p->q[p->at] == '{')) {
            p->failed = TRUE; return f;
        }
        if (p->at < p->length && (p->q[p->at] == '*' || p->q[p->at] == '+' || p->q[p->at] == '?')) {
            ULONG quant = p->q[p->at++];
            BOOL lazy = rx_lazy(p);
            f = rx_quantify(p, f, quant, lazy);
        }
        else if (p->at < p->length && p->q[p->at] == '{')
            f = rx_counted(p, f, atom_at, nodes_before, ranges_before);
        if (p->failed) return f;
        ULONG gap_at = p->at;
        rx_space(p);
        /* In extended mode, a spaced simple operator wraps the previous
         * repetition; it is not an adjacent lazy suffix. */
        while (!p->failed && p->extended && p->at > gap_at && p->at < p->length &&
            (p->q[p->at] == '*' || p->q[p->at] == '+' || p->q[p->at] == '?')) {
            ULONG quant = p->q[p->at++];
            f = rx_quantify(p, f, quant, rx_lazy(p));
            gap_at = p->at;
            rx_space(p);
        }
        if (p->at < p->length && (p->q[p->at] == '*' || p->q[p->at] == '+' ||
            p->q[p->at] == '?' || p->q[p->at] == '{')) { p->failed = TRUE; return f; }
        if (result.start == RX_NONE) result = f;
        else result = rx_join(p, result, f);
    }
    return result.start == RX_NONE ? rx_single(p, RX_JUMP) : result;
}
static struct rx_fragment rx_expr(struct rx_parser *p)
{
    struct rx_fragment f = rx_sequence(p);
    while (!p->failed && p->at < p->length && p->q[p->at] == '|') {
        p->at++; struct rx_fragment g = rx_sequence(p);
        if (p->failed) return g;
        UWORD n = rx_node(p, RX_SPLIT);
        if (p->failed) return g;
        p->r->nodes[n].out = f.start; p->r->nodes[n].other = g.start;
        *rx_edge(p->r, f.tail) = g.head;
        f.start = n; f.tail = g.tail;
    }
    return f;
}
VOID con_regex_cancel(struct con_regex *r) { memset(r, 0, sizeof(*r)); }
BOOL con_regex_begin(struct con_regex *r, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold)
{
    return con_regex_direction_begin(r, first, query, length, unicode_fold, FALSE);
}
BOOL con_regex_direction_begin(struct con_regex *r, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold, BOOL reverse)
{
    con_regex_cancel(r);
    if (!query || !length || length > CON_SEARCH_MAX) return FALSE;
    for (ULONG i=0; i<length; i++) if (!query[i] || query[i]>0x10ffff ||
        (query[i]>=0xd800 && query[i]<=0xdfff)) return FALSE;
    r->fold = unicode_fold;
    r->reverse = reverse;
    /* Preserve the raw scanner's logical-line anchor contract. The UI scans
     * each hard logical line independently, including with explicit -m. */
    struct rx_parser p = {r, query, 0, length, 0, FALSE, 0, unicode_fold, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, 0, {{0}}};
    struct rx_fragment f = rx_expr(&p);
    UWORD accept = rx_node(&p, RX_MATCH);
    if (p.failed || p.at != length) { con_regex_cancel(r); return FALSE; }
    rx_patch(r, f.head, accept); r->start = f.start; r->valid = TRUE;
    if (reverse) con_search_stream_reverse_begin(&r->stream, first);
    else con_search_stream_begin(&r->stream, first, 0);
    r->boundary.line = first;
    return TRUE;
}
static VOID rx_push(struct con_regex *r, UWORD pc)
{
    if (!r->seen[pc]) { r->seen[pc] = 1; r->stack[r->stack_count++] = pc; }
}
BOOL con_regex_ui_begin(struct con_regex *r, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold, BOOL reverse)
{
    if (!con_regex_direction_begin(r, first, query, length, unicode_fold, reverse))
        return FALSE;
    /* Structural nullability ignores look-around conditions. Visit each
     * compiled state at most once; no allocation or retained-text scan. */
    BOOL nullable = FALSE;
    rx_push(r, r->start);
    while (r->stack_count)
    {
        struct con_regex_node *n = &r->nodes[r->stack[--r->stack_count]];
        if (n->op == RX_MATCH) { nullable = TRUE; break; }
        if (n->op == RX_SPLIT) { rx_push(r, n->other); rx_push(r, n->out); }
        else if (n->op == RX_JUMP || n->op == RX_BOL || n->op == RX_EOL ||
            n->op == RX_BOS || n->op == RX_EOS ||
            (n->op >= RX_BOUNDARY && n->op <= RX_ENDHALF))
            rx_push(r, n->out);
    }
    r->stack_count = 0;
    memset(r->seen, 0, sizeof(r->seen));
    r->prefer_first = !nullable;
    r->line_boundaries = TRUE;
    return TRUE;
}
static BOOL rx_class_accepts(struct con_regex *r, const struct con_regex_node *n, ULONG cp)
{
    BOOL found = rx_mode_class(cp, n->cp, n->flags & RX_ASCII);
    const struct con_case_entry *equiv = (n->flags & RX_FOLD) && (!(n->flags & RX_ASCII) || cp < 128) ? con_case_lookup(cp) : NULL;
    if (n->cp & ~63UL) {
        for (ULONG i=0; i<sizeof(rx_ascii_classes)/sizeof(*rx_ascii_classes); i++) {
            if (!(n->cp & (1UL << (6+i)))) continue;
            BOOL member = FALSE;
            for (ULONG j=0; j<rx_ascii_classes[i].count; j++) {
                ULONG low = rx_ascii_classes[i].bounds[j*2];
                ULONG high = rx_ascii_classes[i].bounds[j*2+1];
                if (cp >= low && cp <= high) member = TRUE;
                if (equiv) for (ULONG k=0; k<equiv->count; k++)
                    if (equiv->other[k] >= low && equiv->other[k] <= high) member = TRUE;
            }
            if (!member) found = TRUE;
        }
    }
    for (ULONG i=0; i<n->count; i++) {
        struct con_regex_range range = r->ranges[n->first+i];
        if (range.low == 0xffffffffUL) {
            ULONG id=range.high & 0x3fffffffUL;
            BOOL member=rx_gc_member(id,cp);
            if(equiv)for(ULONG k=0;k<equiv->count;k++)
                if(rx_gc_member(id,equiv->other[k]))member=TRUE;
            if(range.high & 0x40000000UL)member=!member;
            if(member)found=TRUE;
            continue;
        }
        if (cp >= range.low && cp <= range.high) found = TRUE;
        if (equiv) for (ULONG k=0; k<equiv->count; k++)
            if (equiv->other[k] >= range.low && equiv->other[k] <= range.high) found = TRUE;
    }
    return n->op == RX_NOTCLASS ? !found && cp != '\n' : found;
}
static BOOL rx_accepts(struct con_regex *r, const struct con_regex_node *n, ULONG cp)
{
    if (n->op == RX_ANY) return (n->flags & RX_DOTALL) ||
        (cp != '\n' && (!(n->flags & RX_CRLF) || cp != '\r'));
    if (n->op == RX_CHAR) return rx_mode_fold(cp, n->flags & RX_FOLD, !(n->flags & RX_ASCII)) == n->cp;
    if (n->flags & RX_SET_FLAG) {
        UBYTE values[CON_SEARCH_MAX];ULONG used=0;
        for (ULONG i=0;i<n->count;i++) {
            struct con_regex_range entry=r->ranges[n->first+i];
            if (entry.low==RX_SET_OP) {
                if(entry.high==RX_SET_NOT) values[used-1]=!values[used-1];
                else {
                    BOOL right=values[--used],left=values[used-1];
                    values[used-1]=entry.high==RX_SET_UNION ? left||right :
                        entry.high==RX_SET_INTERSECT ? left&&right :
                        entry.high==RX_SET_DIFFERENCE ? left&&!right : left!=right;
                }
            } else {
                struct con_regex_node leaf={0};leaf.op=RX_CLASS;
                leaf.flags=n->flags&~RX_SET_FLAG;
                if(entry.low==RX_SET_MASK) leaf.cp=entry.high;
                else {leaf.first=n->first+i;leaf.count=1;}
                values[used++]=rx_class_accepts(r,&leaf,cp);
            }
        }
        return used==1 && values[0];
    }
    return rx_class_accepts(r,n,cp);
}

enum con_search_result con_regex_step(struct con_regex *r, ULONG budget,
    struct con_search_match *match)
{
    if (!r->valid || r->done) return CON_SEARCH_DONE;
    while (budget--) {
        if (r->phase == RX_SKIP) {
            enum con_search_stream_result status = con_search_stream_step(&r->stream, &r->token);
            if (status == CON_STREAM_DONE) { r->done = TRUE; return CON_SEARCH_DONE; }
            if (status == CON_STREAM_TOKEN) { r->ordinal++; r->previous = r->token.cp; r->boundary = r->reverse ? r->token.first : r->token.end; r->phase = RX_FETCH; }
            continue;
        }
        if (r->phase == RX_FETCH) {
            r->before = r->stream;
            enum con_search_stream_result status = con_search_stream_step(&r->stream, &r->token);
            if (status == CON_STREAM_MORE) continue;
            r->line_end = status == CON_STREAM_TOKEN && r->line_boundaries &&
                r->token.cp == '\n' && r->token.first.line != r->token.end.line;
            r->eof = status == CON_STREAM_DONE || r->line_end;
            /* Reverse acceptance starts at the last consumed scalar's actual
             * cell, not the previous row's equivalent soft-wrap boundary. */
            if (!r->eof && (!r->reverse || !r->ordinal))
                r->boundary = r->reverse ? r->token.end : r->token.first;
            memset(r->seen, 0, sizeof(r->seen));
            r->seed_at = r->leaf_count = r->stack_count = 0;
            r->injected = FALSE; r->phase = RX_CLOSE;
        } else if (r->phase == RX_CLOSE) {
            if (!r->stack_count) {
                if (r->seed_at < r->seed_count) r->current = r->seeds[r->seed_at++];
                else if (!r->injected && !r->have_best) {
                    struct con_search_position start =
                        r->reverse && !r->eof ? r->token.end : r->boundary;
                    r->current = (struct con_regex_thread){r->start, r->ordinal, start}; r->injected = TRUE;
                } else {
                    r->seed_count = r->leaf_at = 0; r->live = FALSE; r->phase = RX_CONSUME; continue;
                }
                rx_push(r, r->current.pc); continue;
            }
            UWORD pc = r->stack[--r->stack_count];
            struct con_regex_node *n = &r->nodes[pc];
            if (n->op == RX_SPLIT) { rx_push(r, n->other); rx_push(r, n->out); }
            else if (n->op == RX_JUMP) rx_push(r, n->out);
            else if (n->op == RX_BOL || n->op == RX_EOL) {
                BOOL boundary;
                if (n->flags & RX_CRLF) {
                    /* Reverse input sees LF then CR. Never accept the
                     * interior boundary of the pair in either direction. */
                    ULONG lead = r->reverse ? '\n' : '\r';
                    ULONG trail = r->reverse ? '\r' : '\n';
                    if (n->op == RX_BOL)
                        boundary = !r->ordinal || r->previous == trail ||
                            (r->previous == lead && (r->eof || r->token.cp != trail));
                    else
                        boundary = r->eof || r->token.cp == lead ||
                            (r->token.cp == trail && (!r->ordinal || r->previous != lead));
                } else boundary = n->op == RX_BOL ?
                    (!r->ordinal || r->previous == '\n') :
                    (r->eof || r->token.cp == '\n');
                if (boundary) rx_push(r, n->out);
            }
            else if (n->op == RX_BOS) {
                if (!r->ordinal || (r->line_boundaries && r->previous == '\n')) rx_push(r, n->out);
            }
            else if (n->op == RX_EOS) { if (r->eof) rx_push(r, n->out); }
            else if ((n->op >= RX_BOUNDARY && n->op <= RX_ENDHALF)) {
                BOOL left = r->ordinal && rx_mode_class(r->previous, 4, n->flags & RX_ASCII);
                BOOL right = !r->eof && rx_mode_class(r->token.cp, 4, n->flags & RX_ASCII);
                BOOL accept = n->op == RX_BOUNDARY ? left != right :
                    n->op == RX_NOTBOUNDARY ? left == right :
                    n->op == RX_WORDSTART ? !left && right :
                    n->op == RX_WORDEND ? left && !right :
                    n->op == RX_STARTHALF ? !left : !right;
                if (accept) rx_push(r, n->out);
            }
            else if (n->op == RX_MATCH) {
                if (!r->have_best || r->current.ordinal <= r->best_start) {
                    r->best.first = r->reverse ? r->boundary : r->current.start;
                    r->best.end = r->reverse ? r->current.start : r->boundary;
                    if (r->current.ordinal == r->ordinal)
                        r->best.first = r->best.end = r->current.start;
                    r->best_start = r->current.ordinal; r->best_end = r->ordinal;
                    r->best_resume = r->before; r->best_previous = r->previous; r->best_eof = r->eof && !r->line_end; r->have_best = TRUE;
                }
                if (r->prefer_first)
                {
                    /* Leaves visited before MATCH have higher priority and
                     * may greedily extend it. Discard every later alternative
                     * or origin, even when it could produce a longer match. */
                    r->stack_count = 0;
                    r->seed_at = r->seed_count;
                    r->injected = TRUE;
                }
            } else {
                r->leaves[r->leaf_count] = r->current;
                r->leaves[r->leaf_count++].pc = pc;
            }
        } else if (r->leaf_at < r->leaf_count) {
            struct con_regex_thread t = r->leaves[r->leaf_at++];
            if (!r->eof && rx_accepts(r, &r->nodes[t.pc], r->token.cp)) {
                t.pc = r->nodes[t.pc].out; r->seeds[r->seed_count++] = t;
                if (!r->have_best || t.ordinal <= r->best_start) r->live = TRUE;
            }
        } else {
            if (r->have_best && !r->live) {
                *match = r->best;
                BOOL empty = r->best_start == r->best_end;
                r->stream = r->best_resume; r->ordinal = r->best_end; r->previous = r->best_previous;
                r->boundary = r->reverse ? r->best.first : r->best.end; r->seed_count = 0; r->have_best = FALSE; r->phase = RX_FETCH;
                /* Empty matches must advance without rescanning the same boundary.
                 * Consume the next scalar in bounded stream steps on later calls. */
                if (empty) { r->phase = RX_SKIP; if (r->best_eof) r->done = TRUE; }
                return CON_SEARCH_FOUND;
            }
            if (r->eof && !r->line_end) { r->done = TRUE; return CON_SEARCH_DONE; }
            /* UI matching treats each hard logical line as a separate input.
             * The separator has already advanced the stream; discard its NFA
             * state but keep the global ordinal and directional row boundary. */
            r->ordinal++; r->previous = r->token.cp; r->boundary = r->reverse ? r->token.first : r->token.end; r->phase = RX_FETCH;
        }

    }
    return CON_SEARCH_MORE;
}

VOID con_regex_select_cancel(struct con_regex_selection *selection)
{
    memset(selection, 0, sizeof(*selection));
}

BOOL con_regex_select_begin(struct con_regex_selection *selection,
    const struct charmap_line *first, const ULONG *query, ULONG length,
    BOOL unicode_fold, BOOL reverse, const struct con_search_match *anchor)
{
    con_regex_select_cancel(selection);
    if (!con_regex_ui_begin(&selection->scan, first, query, length,
            unicode_fold, reverse))
        return FALSE;
    selection->reverse = reverse;
    selection->anchored = anchor != NULL;
    if (anchor) selection->anchor = *anchor;
    return TRUE;
}

static BOOL rx_same_match(const struct con_search_match *a,
    const struct con_search_match *b)
{
    return a->first.line == b->first.line && a->first.column == b->first.column &&
        a->end.line == b->end.line && a->end.column == b->end.column;
}

enum con_search_result con_regex_select_step(
    struct con_regex_selection *selection, ULONG budget,
    struct con_search_match *result)
{
    if (selection->finished || !selection->scan.valid) return CON_SEARCH_DONE;
    while (budget--)
    {
        struct con_search_match match;
        /* One engine work unit per iteration: a FOUND result never resets
         * the caller's budget. Dense/overlapping matches remain bounded. */
        enum con_search_result status = con_regex_step(&selection->scan, 1, &match);
        if (status == CON_SEARCH_DONE)
        {
            selection->finished = TRUE;
            if (!selection->have_match) return CON_SEARCH_DONE;
            *result = selection->first;
            return CON_SEARCH_FOUND;
        }
        if (status != CON_SEARCH_FOUND) continue;
        /* Empty NFA matches have no cells to highlight. Keep their bounded
         * progress in the low-level engine, but exclude them from the UI. */
        if (match.first.line == match.end.line &&
            match.first.column == match.end.column) continue;
        if (selection->anchored && rx_same_match(&selection->anchor, &match))
        {
            selection->seen_anchor = TRUE;
        }
        else if (!selection->anchored || selection->seen_anchor)
        {
            *result = match;
            selection->finished = TRUE;
            return CON_SEARCH_FOUND;
        }
        if (!selection->have_match) selection->first = match;
        selection->last = match;
        selection->have_match = TRUE;
    }
    return CON_SEARCH_MORE;
}
