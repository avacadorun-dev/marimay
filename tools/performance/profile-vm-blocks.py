#!/usr/bin/env python3
"""Build bounded diagnostic instrumentation against a matching Release archive."""
from pathlib import Path
import subprocess
import json
import hashlib

import argparse

if not __debug__:
    raise RuntimeError("Profiling integrity checks require Python without -O")

parser=argparse.ArgumentParser(description="Count unchanged VM instructions and candidate region coverage; not a speed benchmark.")
parser.add_argument('--benchmarks',type=Path,required=True)
parser.add_argument('--reference',type=Path,required=True)
parser.add_argument('--build',type=Path,default=Path('build/release'))
parser.add_argument('--revision',default='HEAD')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--workload',action='append')
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
build=args.build.resolve()
out=root/'build/block-profile'
out.mkdir(parents=True,exist_ok=True)
args.output.parent.mkdir(parents=True,exist_ok=True)
reference=json.loads(args.reference.read_text())
assert reference['status']=='passed' and len(reference['workloads'])==32
bench=args.benchmarks.resolve()
revision=subprocess.check_output(['git','rev-parse',args.revision],cwd=root,text=True).strip()
workloads=args.workload or list(reference['workloads'])
assert len(set(workloads))==len(workloads) and all(n in reference['workloads'] for n in workloads)
for path, digest in reference['metadata']['source_sha256'].items():
    assert hashlib.sha256((bench/path).read_bytes()).hexdigest()==digest,path

header='''#include "mmay/bytecode/vm.h"
extern uint64_t profile_op_counts[MMAY_OPCODE_COUNT];
void profile_block(const MarimayBytecodeFunction *, size_t);
void profile_dump(void);
'''
(out/'profile.h').write_text(header)
recorder=(Path(__file__).with_name('vm_block_profile.c')).read_text()
(out/'profile.c').write_text(recorder)
def original(path):
    return subprocess.check_output(['git','show',revision+':'+path],cwd=root,text=True)
hot=original('bootstrap/src/bytecode/vm_hot.c')
mark='        const MarimayBytecodeBlock *block = &blocks[*block_id];'
assert hot.count(mark)==1
hot=hot.replace(mark,'        if (*instruction_id == 0) profile_block(function, *block_id);\n'+mark)
mark='        if (options.max_steps != SIZE_MAX) *steps += 1;'
assert hot.count(mark)==1
hot=hot.replace(mark,'        profile_op_counts[(size_t)instruction->opcode] += 1;\n'+mark)
(out/'vm_hot.c').write_text(hot)
vm=original('bootstrap/src/bytecode/vm.c')
mark='if (image.functions[function_id].has_specialized_loop)'
assert vm.count(mark)==1
vm=vm.replace(mark,'if (false && image.functions[function_id].has_specialized_loop)')
mark='        if (instruction->opcode == MMAY_OPCODE_RETURN\n'
assert vm.count(mark)==1
vm=vm.replace(mark,'        profile_op_counts[(size_t)instruction->opcode] += 1;\n'+mark)
(out/'vm.c').write_text(vm)
(out/'vm_array.c').write_text(original('bootstrap/src/bytecode/vm_array.c'))
main=original('bootstrap/main.c')
mark='    return exit_code;'
assert main.count(mark)==1
(out/'main.c').write_text(main.replace(mark,'    profile_dump();\n'+mark))
flags=['-O3','-DNDEBUG','-std=c17','-fno-fast-math','-frounding-math','-ffp-contract=off',
       '-fexcess-precision=standard','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror']
for folder in ['bootstrap/include','runtime/include','bootstrap/src/bytecode','build/block-profile']:
    flags+=['-I',str(root/folder)]
flags+=['-I',str(build/'generated')]
objects=[]
for name in ['profile','main','vm','vm_hot','vm_array']:
    obj=out/(name+'.o')
    subprocess.run(['cc',*flags,'-include',str(out/'profile.h'),'-c',str(out/(name+'.c')),'-o',str(obj)],check=True)
    objects.append(str(obj))
subprocess.run(['cc','-O3',*objects,str(build/'libmmay_bootstrap.a'),'-lm','-o',str(out/'mmay')],check=True)
report={'schema':'marimay.block-profile/1','status':'running','revision':revision,'archive_sha256':hashlib.sha256((build/'libmmay_bootstrap.a').read_bytes()).hexdigest(),
        'note':'Diagnostic opcode/block counts only; image specializations disabled. Not timing evidence.',
        'source_sha256':reference['metadata']['source_sha256'],
        'profiler_sha256':hashlib.sha256(Path(__file__).with_name('vm_block_profile.c').read_bytes()).hexdigest(),
        'compiler_sha256':hashlib.sha256((out/'mmay').read_bytes()).hexdigest(),
        'block_fields':['visits','instructions','core_operations','core_segments','extended_operations','extended_segments'],
        'workloads':{}}
for name in workloads:
    row=reference['workloads'][name]
    command=list(row['commands']['marimay-vm']);command[0]=str(out/'mmay')
    source=Path('marimay/benchmarks')/name.replace('-','_') if row.get('anchor') else Path('marimay/catalog')
    assert Path(command[2]).as_posix().endswith('/'+source.as_posix()),name
    command[2]=str(bench/source)
    p=subprocess.run(command,cwd=bench,capture_output=True,text=True,timeout=180)
    assert p.returncode==0 and p.stdout==row['expected_stdout'],(name,p.returncode,p.stdout,p.stderr[:500])
    assert p.stderr.startswith('PROFILE ') and p.stderr.count('\n')==1,(name,p.stderr[:500])
    data=json.loads(p.stderr[8:]);assert not data['overflow'],name
    data['blocks'].sort()
    actual=sum(data['opcodes'].values())
    reconstructed=sum(v[0]*v[1] for v in data['blocks'])
    assert actual==reconstructed,(name,actual,reconstructed)
    data['total_instructions']=actual
    for kind,op_index,segment_index in [('core',2,3),('extended',4,5)]:
        eligible=sum(v[0]*v[op_index] for v in data['blocks'])
        groups=sum(v[0]*(v[1]-v[op_index]+v[segment_index]) for v in data['blocks'])
        data[kind]={'instructions':eligible,'fraction':eligible/actual,'groups':groups,'dispatch_reduction':actual/groups}
    report['workloads'][name]=data
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(name,actual,round(data['core']['fraction']*100,1),round(data['extended']['fraction']*100,1),flush=True)
for path,digest in report['source_sha256'].items():
    assert hashlib.sha256((bench/path).read_bytes()).hexdigest()==digest,path
report['status']='passed'
args.output.write_text(json.dumps(report,indent=2)+'\n')
