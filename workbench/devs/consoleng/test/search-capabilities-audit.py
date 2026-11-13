#!/usr/bin/env python3
"""Probe syntax through pinned Alacritty RegexSearch, not its library in isolation.

Usage: search-capabilities-audit.py --reference PATH --work NEW_DIRECTORY
Requires Cargo and cached dependencies; builds offline. Does not change reference.
"""
import argparse, json, shutil, subprocess, tomllib
from pathlib import Path
PIN='542147067dd016fc5f49120ef0ba605a98a4e3a7'
CASES=[
 ('unicode boundary',r'\bword\b',False),('unicode nonboundary',r'\Bword',False),
 ('unicode start angle',r'\<word',False),('unicode end angle',r'word\>',False),
 ('unicode start',r'\b{start}word',False),('unicode end',r'word\b{end}',False),
 ('unicode start half',r'\b{start-half}word',False),('unicode end half',r'word\b{end-half}',False),
 ('ASCII boundary',r'(?-u:\b)word',True),('ASCII start',r'(?-u:\<)word',True),
 ('ASCII half',r'(?-u:\b{start-half})word',True),('Unicode letter',r'\p{L}+',True),
 ('Unicode Greek',r'\p{Greek}+',True),('set intersection',r'[a-z&&[^aeiou]]+',True),
 ('multiline',r'(?m:^word$)',True),('CRLF anchors',r'(?mR:^word$)',True),
 ('named capture',r'(?P<name>word)',True),('angle named capture',r'(?<name>word)',True),
 ('lookahead',r'(?=word)',False),('backreference',r'(word)\1',False),
 ('quoted literal',r'\Qword\E',False),('ASCII word class',r'(?-u:\w)+',True)]
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--reference',type=Path,required=True);parser.add_argument('--work',type=Path,required=True)
 parser.add_argument('--cases',type=Path,help='Optional captured constructor expectations JSON')
 args=parser.parse_args();root=args.reference
 cases=CASES
 if args.cases:
  data=json.loads(args.cases.read_text());assert data['reference']==PIN
  cases=data['cases'];assert cases and all(len(c)==3 and isinstance(c[2],bool) for c in cases)
 assert subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==PIN
 assert not subprocess.check_output(['git','-C',str(root),'status','--porcelain','--','alacritty_terminal','Cargo.lock'],text=True)
 args.work.mkdir(parents=True,exist_ok=False);dest=args.work/'reference'
 shutil.copytree(root/'alacritty_terminal',dest)
 manifest=dest/'Cargo.toml';manifest.write_text(manifest.read_text().replace('edition.workspace = true','edition = "2024"').replace('rust-version.workspace = true','rust-version = "1.85.0"'))
 shutil.copy2(root/'Cargo.lock',dest/'Cargo.lock');(dest/'examples').mkdir(exist_ok=True)
 pairs=','.join('('+json.dumps(a,ensure_ascii=False)+','+json.dumps(b,ensure_ascii=False)+')' for a,b,_ in cases)
 (dest/'examples/capabilities.rs').write_text('use alacritty_terminal::term::search::RegexSearch;\nfn main(){for (label,pattern) in ['+pairs+'] {match RegexSearch::new(pattern){Ok(_)=>println!("{}\\tACCEPT\\t{}",label,pattern),Err(e)=>println!("{}\\tREJECT\\t{}\\t{}",label,pattern,e),}}}\n')
 with (args.work/'build.log').open('w') as log:
  output=subprocess.check_output(['cargo','run','--offline','--manifest-path',str(manifest),'--example','capabilities'],stderr=log,text=True)
 (args.work/'capabilities.tsv').write_text(output)
 original=tomllib.loads((root/'Cargo.lock').read_text());built=tomllib.loads((dest/'Cargo.lock').read_text())
 keys={(p['name'],p['version'],p.get('checksum')) for p in original['package']}
 assert all((p['name'],p['version'],p.get('checksum')) in keys for p in built['package']), 'Dependency drift'
 rows=output.splitlines();assert len(rows)==len(cases)
 for row,(label,pattern,accepted) in zip(rows,cases):
  assert row.split('\t')[:3]==[label,'ACCEPT' if accepted else 'REJECT',pattern],row
 print(f'Pinned terminal capability audit: {len(rows)} expected accept/reject results; dependency versions and checksums match')
if __name__=='__main__':main()
