from pathlib import Path
import subprocess,json
root=Path.cwd()
base=root/'build/experiments/p1/gpt2'
gem=root/'chipyard/generators/gemmini/software/gemmini-rocc-tests'
body=r'''
#undef main
int main(void) {
  p1_build();
  printf("{\"lanes\":%u,\"native_total\":%u,\"profiles_total\":%u,\"segments\":[", LANES, p1.natives,p1.runs);
  for(unsigned si=0;si<p1.operators;si++) {
    const p1_segment_t *s=&p1.segments[si];
    unsigned natives[64]={0},cg[LANES][32]={0};
    for(unsigned pi=s->first;pi<s->first+s->count;pi++) {
      const p1_profile_t *p=&p1.profiles[pi];
      natives[p->jobs[0]]=1;
      if(p->stages>1) {
        natives[p->jobs[p->stages-1]]=1;
        for(unsigned lane=0;lane<p->stages-2;lane++) {
          unsigned j=p->jobs[lane+1];
          while(1) { cg[lane][j]=1; unsigned next=PROGRAM(lane,j).next;if(!next)break;j=next-1; }
        }
      }
    }
    unsigned nj=0,nc=0,np=0;
    for(unsigned j=0;j<p1.natives;j++)if(natives[j]){nj++;p1_capture(j,&p1.native[j]);nc+=commands;np+=patches;}
    printf("%s{\"name\":\"%s\",\"first\":%u,\"profiles\":%u,\"native_jobs\":%u,\"native_commands\":%u,\"native_patches\":%u,\"cgra\":[",si?",":"",s->name,s->first,s->count,nj,nc,np);
    for(unsigned lane=0;lane<LANES;lane++) {
      unsigned count=0,packets=0;
      for(unsigned j=0;j<32;j++)if(cg[lane][j]) {count++;const cgra_kernel_t *k=PROGRAM(lane,j).kernel;packets+=k->static_count+k->config_count+k->launch_count;}
      printf("%s{\"jobs\":%u,\"packets\":%u,\"ids\":[",lane?",":"",count,packets);
      unsigned wrote=0;for(unsigned j=0;j<32;j++)if(cg[lane][j])printf("%s%u",wrote++?",":"",j);
      printf("]}");
    }
    printf("]}");
  }
  printf("]}\n");return 0;
}
'''
for n in (1,2,4):
    run=base/('cgra1/v002' if n==1 else f'cgra{n}/v001')
    # cgra4 uses the same independently archived host checking harness as cgra2.
    harness=run/'checks/plan.c'
    if not harness.exists():harness=base/'cgra2/v001/checks/plan.c'
    source=Path(f'/tmp/p1_segments_{n}.c')
    source.write_text(f'#define LANES {n}\n#define PROGRAM(l,j) '+('p1.cgra[j]' if n==1 else 'p1.cgra[l][j]')+'\n#define main archived_main\n#include "'+str(harness)+'"\n'+body)
    inc=[run/'source',run,run/'generated',base/'cgra1/v001/rows',base/'cgra1/v002',root/'tests/include',root/'tests',root/'runtime/include',root/'tests/cgra-gemmini/common',root/'chipyard/tests',gem,gem/'include',gem/'rocc-software/src',gem/'riscv-tests/env',gem/'riscv-tests/benchmarks/common']
    cmd=['gcc','-std=gnu11','-O2','-D__riscv=1','-include',str(base/'cgra1/v002/generated/cgra_layout.h')]
    if n>1:
        cmd+=['-DP1_CGRAS='+str(n)]
        for h in ('accel_generated.h','cgra_link_control_generated.h','auto_link_generated.h'):cmd+=['-include',str(run/'generated'/h)]
    cmd+=['-I'+str(x) for x in inc]+[str(source),'-o',f'/tmp/p1_segments_{n}']
    subprocess.run(cmd,check=True)
    output=subprocess.check_output([f'/tmp/p1_segments_{n}'],text=True)
    Path(f'/tmp/p1_segments_{n}.json').write_text(output)
    d=json.loads(output)
    print(n,[(s['name'],s['profiles'],s['native_jobs'],s['native_commands'],s['cgra'][0]['jobs'],s['cgra'][0]['packets']) for s in d['segments']])
