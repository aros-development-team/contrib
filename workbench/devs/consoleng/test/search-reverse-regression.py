#!/usr/bin/env python3
"""Reverse token identity and mirrored-expression POSIX oracle for the NFA."""
from pathlib import Path
import json
import random
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]


def source(name):
    return "\n".join(x for x in (root / name).read_text().splitlines()
                     if not x.startswith("#include"))


rng = random.Random(8042)


def expression(depth=0):
    if depth == 2 or rng.randrange(3) == 0:
        atom = rng.choice(["a", "b", ".", "[ab]", "[^b]", "^", "$"])
        return atom, {"^": "$", "$": "^"}.get(atom, atom)
    kind = rng.randrange(3)
    a, ar = expression(depth + 1)
    if kind == 0:
        b, br = expression(depth + 1)
        return "(" + a + b + ")", "(" + br + ar + ")"
    if kind == 1:
        b, br = expression(depth + 1)
        return "(" + a + "|" + b + ")", "(" + ar + "|" + br + ")"
    quant = rng.choice(["*", "+", "?", "{0}", "{2}", "{1,3}", "{1,}"])
    # Do not quantify anchors (POSIX portability).
    if "^" in a or "$" in a:
        return a, ar
    return "(" + a + ")" + quant, "(" + ar + ")" + quant


cases = [
    ("aba", "aba", "ababa"),
    ("(ab){1,3}", "(ba){1,3}", "ababab"),
    ("^a.*b$", "^b.*a$", "aabb"),
    ("a{2,4}b", "ba{2,4}", "aaaabaaab"),
]
for _ in range(4000):
    pattern, reverse = expression()
    text = "".join(rng.choice("ab\n") for _ in range(rng.randrange(32)))
    cases.append((pattern, reverse, text))

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;
typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
"""
layout = (root / "charmap.h").read_text()
layout = layout[layout.index("struct charmap_line\n"):
                layout.index("VOID charmap_dispose_lines")]
fixtures = ",\n".join("{" + ",".join(json.dumps(x) for x in c) + "}" for c in cases)
main = r"""
static const char *cases[][3]={FIXTURES};
static struct con_regex regex;
static uint32_t seed=71;
static unsigned rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void engine(void) {
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  const char *p=cases[i][0],*reverse=cases[i][1],*text=cases[i][2];
  ULONG q[256],cells[256];char mirrored[256];unsigned n=strlen(p),len=strlen(text);
  for(unsigned j=0;j<n;j++)q[j]=p[j];
  for(unsigned j=0;j<len;j++){cells[j]=text[j];mirrored[j]=text[len-j-1];}
  mirrored[len]=0;
  struct charmap_line row={0};row.text=cells;row.size=len;
  assert(con_regex_direction_begin(&regex,&row,q,n,FALSE,TRUE));
  struct con_regex before=regex;struct con_search_match m={{&row,999},{&row,999}},saved=m;
  assert(con_regex_step(&regex,0,&m)==CON_SEARCH_MORE);
  assert(!memcmp(&before,&regex,sizeof(regex))&&!memcmp(&m,&saved,sizeof(m)));
  regex_t oracle;assert(!regcomp(&oracle,reverse,REG_EXTENDED|REG_NEWLINE));
  unsigned offset=0,steps=0;
  for(;;){
   regmatch_t want;int found=offset<=len&&!regexec(&oracle,mirrored+offset,1,&want,
     offset&&mirrored[offset-1]!='\n'?REG_NOTBOL:0);
   enum con_search_result result;
   do{
    saved=m;result=con_regex_step(&regex,1,&m);assert(++steps<100000);
    if(result!=CON_SEARCH_FOUND)assert(!memcmp(&m,&saved,sizeof(m)));
   }while(result==CON_SEARCH_MORE);
   if((result==CON_SEARCH_FOUND)!=found || (found&&
      (m.first.column!=len-offset-want.rm_eo||m.end.column!=len-offset-want.rm_so))){
    fprintf(stderr,"reverse case %u /%s/ mirror /%s/ text [%s] offset %u found %d want %d:%d got %d %u:%u\n",
      i,p,reverse,text,offset,found,found?(int)want.rm_so:-1,found?(int)want.rm_eo:-1,result,m.first.column,m.end.column);
    abort();
   }
   if(!found)break;
   offset+=want.rm_eo;if(want.rm_so==want.rm_eo)offset++;
  }
  regfree(&oracle);con_regex_cancel(&regex);
  assert(con_regex_step(&regex,1,&m)==CON_SEARCH_DONE);
 }
}
static void streams(void){
 for(unsigned trial=0;trial<2000;trial++){
  struct charmap_line rows[16]={0},outside={0};
  ULONG cells[16][32];BYTE flags[16][32];
  struct con_link *links[16][32];
  unsigned count=1+rnd()%16;
  for(unsigned i=0;i<count;i++){
   rows[i].prev=i?rows+i-1:&outside;rows[i].next=i+1<count?rows+i+1:NULL;
   rows[i].size=rnd()%32;rows[i].text=cells[i];rows[i].flags=flags[i];rows[i].links=links[i];
   rows[i].wrapColumns=rnd()%2?1+rnd()%32:0;rows[i].wrapPadding=rnd()%2;
   for(unsigned j=0;j<rows[i].size;j++){
    cells[i][j]=(ULONG[]){0,'a',' ',0x4e2d,0xfffffffe,0x3a9}[rnd()%6];
    flags[i][j]=rnd()%2?CHARMAP_ERASED:0;
    links[i][j]=rnd()%3?NULL:(struct con_link*)&outside;
   }
  }
  struct con_search_stream stream;struct con_search_token tokens[1024],token={{0}};
  unsigned total=0,steps=0;enum con_search_stream_result result;
  con_search_stream_begin(&stream,rows,0);
  do{
   struct con_search_token saved=token;
   result=con_search_stream_step(&stream,&token);assert(++steps<3000);
   if(result==CON_STREAM_TOKEN)tokens[total++]=token;
   else assert(!memcmp(&token,&saved,sizeof(token)));
  }while(result!=CON_STREAM_DONE);
  con_search_stream_reverse_begin(&stream,rows);steps=0;
  do{
   struct con_search_token saved=token;
   result=con_search_stream_step(&stream,&token);assert(++steps<3000);
   if(result==CON_STREAM_TOKEN){
    assert(total);struct con_search_token *expected=&tokens[--total];
    assert(token.cp==expected->cp&&token.first.line==expected->first.line&&
      token.first.column==expected->first.column&&token.end.line==expected->end.line&&
      token.end.column==expected->end.column);
   }else assert(!memcmp(&token,&saved,sizeof(token)));
  }while(result!=CON_STREAM_DONE);
  assert(!total);
 }
 struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);
 for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
 ULONG cell='x',q='x';rows[9999].text=&cell;rows[9999].size=1;
 struct con_search_match match;
 assert(con_regex_direction_begin(&regex,rows,&q,1,FALSE,TRUE));
 assert(con_regex_step(&regex,9999,&match)==CON_SEARCH_MORE);
 unsigned steps=0;enum con_search_result result;
 do{result=con_regex_step(&regex,1,&match);assert(++steps<100);}while(result==CON_SEARCH_MORE);
 assert(result==CON_SEARCH_FOUND&&match.first.line==rows+9999&&match.first.column==0&&match.end.column==1);
 con_regex_cancel(&regex);free(rows);
}
static void wide_wrap(void){
 ULONG a[]={'A',0x4e2d,0xfffffffe,' '},b[]={0x3a9,'b'};
 struct charmap_line rows[2]={{.text=a,.size=4,.wrapColumns=4,.wrapPadding=1},{.text=b,.size=2}};
 rows[0].next=rows+1;rows[1].prev=rows;
 ULONG patterns[][2]={{0x3a9,0},{0x4e2d,0x3a9},{0x4e2d,0}};
 unsigned length[]={1,2,1},first_row[]={1,0,0},first_col[]={0,1,1},end_row[]={1,1,0},end_col[]={1,1,3};
 for(unsigned i=0;i<3;i++){
  assert(con_regex_direction_begin(&regex,rows,patterns[i],length[i],FALSE,TRUE));
  struct con_search_match match;enum con_search_result result;
  do{result=con_regex_step(&regex,1,&match);}while(result==CON_SEARCH_MORE);
  assert(result==CON_SEARCH_FOUND&&match.first.line==rows+first_row[i]&&
    match.first.column==first_col[i]&&match.end.line==rows+end_row[i]&&match.end.column==end_col[i]);
 }
}
int main(void){engine();streams();wide_wrap();puts("Reverse search PASS: 4004 mirrored POSIX cases, 2000 retained-token identities, wide soft-wrap endpoints, 10000-row budget and cancellation");}
""".replace("FIXTURES", fixtures).replace("token={{0}}", "token={0}")
base = prefix + layout + "\n".join(source(n) for n in (
    "search.h", "search_regex.h", "search.c", "search_unicode.h",
    "search_case.h", "search_regex.c"))
for label, body in [
    ("production", base),
    ("reverse concatenation", base.replace("if (p->r->reverse)\n", "if (FALSE)\n")),
    ("hard boundary", base.replace("if (!line->wrapColumns)\n", "if (FALSE)\n")),
]:
    if label != "production":
        assert body != base, label
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "test.c"
        binary = Path(d) / "test"
        path.write_text(body + main)
        subprocess.run(["cc", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-g", str(path),
                        "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True,
                                timeout=60)
        if label == "production":
            assert result.returncode == 0, result.stderr
            print(result.stdout, end="")
        else:
            assert result.returncode != 0, label
            print("Negative control PASS:", label)
