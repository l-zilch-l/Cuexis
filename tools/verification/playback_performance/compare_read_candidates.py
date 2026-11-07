"""One bounded read-path candidate, paired with archived A and the preserved F1 build."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import finite_followup as f, diagnostic as d
from tools.verification.playback_performance.historical_integrity import sha


def freeze(args):
    original = json.loads(args.f1_plan.read_text(encoding='utf-8-sig'))
    plan = copy.deepcopy(original)
    plan['builds']['B']['source'] = str(args.f1_source.resolve())
    snapshot_path = args.f1_plan.parent.parent/'historical-tool-snapshot.json'
    snapshot = json.loads(snapshot_path.read_text(encoding='utf-8-sig'))
    # Audit the historical method's archived bytes, separately from today's tool eligibility.
    archived = {}
    for path, expected in original['tools'].items():
        saved = snapshot[path]
        if saved['sha256'] != expected:
            raise ValueError('Historical tool snapshot does not match original plan')
        archived[saved['path']] = expected
    plan['tools'] = archived
    f.verify(plan)
    build = args.candidate_build.resolve()
    exe = build/'bin/cuexis_pd_baseline_probe.exe'
    metadata = {str(p):sha(p) for p in (build/'CMakeCache.txt',build/'compile_commands.json',build/'build.ninja')}
    metadata.update({str(p):sha(p) for p in sorted((build/'lib').glob('*.lib'))})
    plan['builds']['C'] = dict(source=str(f.ROOT), source_identity=f.source_identity(f.ROOT),
                             bindings={'baseline':dict(path=str(exe),sha256=sha(exe))},build_files=metadata)
    plan.update(schema='pd5.read-candidate-plan.v1', formal=False, method=d.METHOD,
                scope='Scale read/update only; host and 40k results remain F1 evidence',
                head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=f.ROOT,text=True).strip(),
                previous_plan_sha256=sha(args.f1_plan),
                historical_tools_snapshot_sha256=sha(snapshot_path),
                order=[['A','B','C'],['C','B','A'],['A','B','C']],
                candidate_limit=1,
                retention='C/F1: read64 CI upper < 1; all read noise <=15%; read1 CI upper <=1.10; no updated definite >1.10; exact output and applicable semantic checks required')
    for key in ('production','host'):
        plan.pop(key,None)
    plan['tools'] = {str(p):sha(p) for p in Path(__file__).parent.glob('*.py')}
    args.output.mkdir(parents=True,exist_ok=True)
    f.save(args.output/'plan.json',plan)
    print(sha(args.output/'plan.json'))


def run(args):
    folder = args.output.resolve()
    plan = json.loads((folder/'plan.json').read_text(encoding='utf-8-sig'))
    f.verify(plan)
    f.save(folder/'run.claim.json',{'plan_sha256':sha(folder/'plan.json')})
    collected = {side:[] for side in 'ABC'}
    result = {'acceptance':False,'plan_sha256':sha(folder/'plan.json'),'comparisons':{},'errors':[]}
    try:
        for index, order in enumerate(plan['order']):
            for side in order:
                collected[side].append(f.scale_records(f.launch(folder,plan,side,'scale',index),plan))
            if any(collected['A'][index][key]['workload'] != collected[s][index][key]['workload']
                   for s in 'BC' for key in collected['A'][index]):
                raise ValueError('Candidate changed logical work or output')
        for before,after in (('A','B'),('B','C'),('A','C')):
            name = after+'-over-'+before
            result['comparisons'][name] = {}
            ids = {alias:dict(source_sha256=plan['builds'][s]['source_identity']['sha256'],
                             binary_sha256=plan['builds'][s]['bindings']['baseline']['sha256'])
                   for alias,s in (('A',before),('B',after))}
            for key in collected['A'][0]:
                data = dict(identities=ids,pairs=[[collected[before][i][key],collected[after][i][key]] for i in range(3)])
                f.save(folder/(name+'-'+key+'.input.json'),data)
                result['comparisons'][name][key] = d.compare(data['pairs'],ids)
    except Exception as error:
        result['errors'].append(str(error))
        raise
    finally:
        f.save(folder/'analysis.json',result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['freeze','run'])
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--f1-plan',type=Path)
    parser.add_argument('--f1-source',type=Path)
    parser.add_argument('--candidate-build',type=Path)
    args = parser.parse_args()
    {'freeze':freeze,'run':run}[args.command](args)
