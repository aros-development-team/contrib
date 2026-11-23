#!/usr/bin/env python3
"""Viewport row seek and first occupied cell with strict work budgets."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
body=r"""
int main(void){
 struct charmap_line rows[10000]={0};ULONG cells[10000]={0};struct con_nav n;
 cells[9999]='x';
 for(unsigned i=0;i<10000;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<9999?&rows[i+1]:NULL;rows[i].text=cells;rows[i].size=10000;}
 con_nav_begin(&n,rows,0,0,10000);n.target_row=9999;con_nav_move(&n,CON_NAV_VIEW_ROW);
 assert(!con_nav_step(&n,0)&&n.cursor.row==0);
 unsigned steps=0;
 while(n.pending){unsigned row=n.cursor.row,scan=n.scan_column;con_nav_step(&n,1);
  assert(n.cursor.row-row<=1);assert(n.scan_column-scan<=1);assert(++steps<21000);}
 assert(n.cursor.row==9999&&n.cursor.column==9999);
 n.target_row=0;con_nav_move(&n,CON_NAV_VIEW_ROW);steps=0;
 while(n.pending){unsigned row=n.cursor.row;con_nav_step(&n,1);assert(row-n.cursor.row<=1);assert(++steps<21000);}
 assert(n.cursor.row==0&&n.cursor.column==9999);
 for(unsigned i=0;i<10000;i++)cells[i]=' ';
 n.target_row=20000;con_nav_move(&n,CON_NAV_VIEW_ROW);while(!con_nav_step(&n,1024));
 assert(n.cursor.row==9999&&n.cursor.column==0);
 struct charmap_line shortrows[40]={0};ULONG shortcells[40][16]={0};
 for(unsigned i=0;i<40;i++){shortrows[i].prev=i?&shortrows[i-1]:NULL;shortrows[i].next=i<39?&shortrows[i+1]:NULL;
  shortrows[i].text=shortcells[i];shortrows[i].size=16;for(unsigned j=0;j<16;j++)shortcells[i][j]=' ';shortcells[i][i%16]='x';}
 for(unsigned start=0;start<40;start++)for(unsigned target=0;target<50;target++){
  con_nav_begin(&n,&shortrows[start],start,0,16);n.target_row=target;con_nav_move(&n,CON_NAV_VIEW_ROW);
  unsigned count=0;while(!con_nav_step(&n,1))assert(++count<100);
  unsigned expected=target<40?target:39;assert(n.cursor.row==expected&&n.cursor.column==expected%16);
 }
 puts("Viewport navigation PASS: 2000 targets, 10000 rows/cells, bidirectional unit budgets, blank fallback and history-end clamp");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+(r/'navigation.h').read_text()+body)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
for label,old,new in [
 ('first occupied cell', "cp!=' ' && cp!='\\t'", "cp!='\\t'"),
 ('target row', 'n->cursor.row<n->target_row', 'n->cursor.row<=n->target_row'),
]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';header=(r/'navigation.h').read_text();assert old in header
  src.write_text(prefix+header.replace(old,new)+body)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  assert subprocess.run([str(exe)],capture_output=True).returncode,label
  print('Negative control PASS:',label)
