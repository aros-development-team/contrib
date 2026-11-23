#!/usr/bin/env python3
"""Hard/soft row-boundary spans from the actual pinned terminal."""
from pathlib import Path
import argparse
import importlib.util
import json
import subprocess
import tempfile

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('audit', here / 'search-reference-audit.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

def reference(root, work, data, capture=False):
    # Reuse the source/lock verification and isolated reference construction.
    bootstrap = work / 'bootstrap'
    bootstrap.mkdir()
    audit.reference(root, bootstrap, [('a', 'a', (0, 1), (0, 1))], 80)
    ref = bootstrap / 'reference'
    pairs = []
    width = data['columns']
    for pattern, rows, wraps, _, _ in data['cases']:
        content = '\n'.join(row.ljust(width) + ('' if i == len(rows)-1 or wraps[i] else '\r')
                            for i, row in enumerate(rows))
        pairs.append('('+json.dumps(pattern)+','+json.dumps(content)+','+str(len(rows))+')')
    source = '''use alacritty_terminal::{term::{search::RegexSearch,test::mock_term},index::{Point,Line,Column,Direction,Side}};
fn main(){for(p,t,rows)in[PAIRS]{let term=mock_term(t);for d in[Direction::Right,Direction::Left]{
let mut regex=RegexSearch::new(p).unwrap();let origin=if d==Direction::Right{Point::new(Line(0),Column(0))}else{Point::new(Line(rows-1),Column(WIDTH-1))};
match term.search_next(&mut regex,origin,d,Side::Left,None){Some(m)=>println!("{} {}",m.start().line.0 as usize*WIDTH+m.start().column.0,m.end().line.0 as usize*WIDTH+m.end().column.0+1),None=>println!("NONE")}}}}
'''.replace('PAIRS', ','.join(pairs)).replace('WIDTH', str(width))
    (ref / 'examples/lines.rs').write_text(source)
    with (work / 'reference-build.log').open('w') as log:
        output = subprocess.check_output(['cargo', 'run', '--locked', '--manifest-path', str(ref/'Cargo.toml'), '--example', 'lines'], text=True, stderr=log)
    (work/'reference-results.txt').write_text(output)
    values=output.splitlines()
    assert len(values)==len(data['cases'])*2
    for i,case in enumerate(data['cases']):
        spans=[None if v=='NONE' else list(map(int,v.split())) for v in values[2*i:2*i+2]]
        if capture: case[3:]=spans
        else: assert case[3:]==spans, (case,spans)
    print('Pinned multirow reference:',len(values),'spans verified')

def production(work, data, mutation=None):
    width=data['columns']
    fixtures=[]
    fake=[]
    for pattern, rows, wraps, forward, backward in data['cases']:
        qs=json.dumps(pattern).replace('?',r'\?')
        rs=','.join(json.dumps(r.ljust(width)) for r in rows)
        ws=','.join(str(int(w)) for w in wraps) or '0'
        fixtures.append('{'+qs+',{'+rs+'},{'+ws+'},'+str(len(rows))+'}')
        fake.append((pattern,' | '.join(rows),forward,backward))
    main='''
static struct con_regex_selection selection;
int main(void){
 const struct {const char *q,*rows[4];unsigned wraps[4],count;} cases[]={FIXTURES};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
  ULONG q[256],cells[4][WIDTH];struct charmap_line rows[4]={0};
  unsigned n=strlen(cases[i].q);for(unsigned j=0;j<n;j++)q[j]=(unsigned char)cases[i].q[j];
  for(unsigned row=0;row<cases[i].count;row++){
   for(unsigned col=0;col<WIDTH;col++)cells[row][col]=(unsigned char)cases[i].rows[row][col];
   rows[row].text=cells[row];rows[row].size=WIDTH;rows[row].wrapColumns=cases[i].wraps[row]?WIDTH:0;
   rows[row].next=row+1<cases[i].count?&rows[row+1]:NULL;rows[row].prev=row?&rows[row-1]:NULL;
  }
  for(int reverse=0;reverse<2;reverse++){
   assert(con_regex_select_begin(&selection,rows,q,n,con_regex_smart_fold(q,n),reverse,NULL));
   struct con_search_match m;enum con_search_result result;unsigned steps=0;
   do{result=con_regex_select_step(&selection,1,&m);assert(++steps<200000);}while(result==CON_SEARCH_MORE);
   if(result==CON_SEARCH_FOUND)printf("%u %u\\n",(unsigned)(m.first.line-rows)*WIDTH+m.first.column,(unsigned)(m.end.line-rows)*WIDTH+m.end.column);else puts("NONE");
  }
 }
 /* Empty hard rows must advance without prematurely ending a nullable scan. */
 static struct charmap_line empty[10000];
 for(unsigned i=0;i<10000;i++){
  empty[i].next=i+1<10000?&empty[i+1]:NULL;
  empty[i].prev=i?&empty[i-1]:NULL;
 }
 ULONG nullable[]={'a','*'};
 for(int reverse=0;reverse<2;reverse++){
  assert(con_regex_select_begin(&selection,empty,nullable,2,FALSE,reverse,NULL));
  struct con_search_match m;enum con_search_result result;unsigned steps=0;
  do{result=con_regex_select_step(&selection,1,&m);assert(++steps<500000);}while(result==CON_SEARCH_MORE);
  assert(result==CON_SEARCH_DONE);
 }
 /* The low-level API still exposes hard newlines in both directions. */
 struct charmap_line raw[2]={0};ULONG a='a',b='b',q[]={'a','\\n','b'};
 raw[0].text=&a;raw[0].size=1;raw[0].next=&raw[1];
 raw[1].text=&b;raw[1].size=1;raw[1].prev=&raw[0];
 for(int reverse=0;reverse<2;reverse++){
  assert(con_regex_direction_begin(&selection.scan,raw,q,3,FALSE,reverse));
  struct con_search_match m;enum con_search_result result;unsigned steps=0;
  do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<1000);}while(result==CON_SEARCH_MORE);
  assert(result==CON_SEARCH_FOUND&&m.first.line==raw&&!m.first.column&&m.end.line==raw+1&&m.end.column==1);
 }
}
'''.replace('FIXTURES',','.join(fixtures)).replace('WIDTH',str(width))
    def transform(body):
        body=body[:body.index('static struct con_regex_selection selection;')]+main
        return mutation(body) if mutation else body
    audit.production(work,fake,width,transform)
    report=json.loads((work/'report.json').read_text())
    allowed={(e['case'],e['direction']):e['expected'] for e in data.get('exceptions',[])}
    unexpected=[]
    for i,item in enumerate(report):
        override=allowed.get((i//2,i%2))
        expected=' '.join(map(str,override)) if override is not None else item['expected']
        if item['actual']!=expected:unexpected.append(item)
    print(len(allowed),'documented anchored reference omissions;',len(unexpected),'unexpected differences')
    return bool(unexpected)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cases',type=Path,default=here/'search-lines-cases.json')
    parser.add_argument('--work',type=Path)
    parser.add_argument('--reference',type=Path)
    parser.add_argument('--capture',action='store_true')
    args=parser.parse_args();data=json.loads(args.cases.read_text())
    assert data['reference']==audit.PIN and 2<=data['columns']<=80
    assert all(1<=len(c[1])<=4 and len(c[2])==len(c[1])-1 and all(len(r)<=data['columns'] for r in c[1]) for c in data['cases'])
    if args.capture and not args.reference:parser.error('--capture needs --reference')
    with tempfile.TemporaryDirectory() as temp:
        work=args.work or Path(temp);work.mkdir(exist_ok=True,parents=True)
        if args.reference:
            reference(args.reference.resolve(),work,data,args.capture)
            if args.capture:args.cases.write_text(json.dumps(data,indent=2)+'\n')
        failed=production(work,data)
        if not failed and not args.capture:
            negative=work/'negative';negative.mkdir()
            def disable(body):
                assert 'r->line_boundaries = TRUE;' in body
                return body.replace('r->line_boundaries = TRUE;','r->line_boundaries = FALSE;')
            assert production(negative,data,disable), 'Hard-boundary negative control escaped'
            print('Negative control PASS: hard-line input reset')
        raise SystemExit(failed)
