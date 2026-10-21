"""Read-only host contract addendum from frozen inputs and native process logs."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import finite_followup as f, diagnostic as d


def hashed(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()


def process(record,plan):
    parsed = f.host_record(record,plan)
    side = record['side']
    source = plan['builds'][side]
    content = {p:h for p,h in source['source_identity']['files'].items()
               if p.startswith('assets/projects/stage3_project/')}
    if not content:
        raise ValueError('Missing frozen host content inventory')
    chart_path = 'assets/projects/stage3_project/assets/charts/stage3_example.cuexis.chart.json'
    path = Path(source['source'])/chart_path
    if f.sha(path) != content[chart_path]:
        raise ValueError('Host chart changed since freeze')
    chart = json.loads(path.read_text(encoding='utf-8-sig'))
    objects, cameras = len(chart['objects']), int(bool(chart.get('camera')))
    if objects != parsed['identity']['objects'] or cameras != 1:
        raise ValueError('Host source count differs from the fixed harness')
    started = f.m.records(Path(record['log']).read_text(encoding='utf-8-sig'), 'PD5_PROCESS_START ')[-1]
    count = parsed['identity']['warmup'] + parsed['identity']['measured']
    sequence = dict(warmup=parsed['identity']['warmup'],measured=parsed['identity']['measured'],
                    tick='(ordinal+1)*10',delta_ms=10,mode='closed-loop update/extract/present',
                    viewport=parsed['identity']['viewport'])
    return dict(schema='playback.diagnostic-process.v1',method=d.METHOD,protocol=d.PROTOCOL,
                source_sha256=source['source_identity']['sha256'],binary_sha256=record['binary_sha256'],
                run_id=record['name'],pid=started['pid'],native_start_qpc=started['qpc'],
                log_sha256=record['log_sha256'],output_validated=True,
                workload=dict(objects=objects,cameras=cameras,registry_entities=objects+cameras,
                              U=count,F=count,R=0,content_sha256=hashed(content),
                              sequence_sha256=hashed(sequence),viewport=parsed['identity']['viewport'],
                              output_digest=str(parsed['identity']['digest'])),
                metric=dict(name='request_to_ready',start='host-sample-request',end='owning-frame-ready',
                            unit='us',sample_kind='operation'),
                samples=[dict(operations=1,elapsed_us=row['request_to_ready_us']) for row in parsed['samples']])


def analyze(folder):
    plan = json.loads((folder/'plan.json').read_text(encoding='utf-8-sig'))
    ids = {s:dict(source_sha256=plan['builds'][s]['source_identity']['sha256'],
                  binary_sha256=plan['builds'][s]['bindings']['host']['sha256']) for s in 'AB'}
    pairs = [[process(json.loads((folder/f'host-{i}-{s}.process.json').read_text(encoding='utf-8-sig')),plan)
              for s in 'AB'] for i in range(3)]
    return dict(identities=ids,pairs=pairs)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    data = analyze(args.folder)
    result = d.compare(data['pairs'],data['identities'])
    result.update(tool_sha256=f.sha(Path(__file__)),input=data,
                  count_provenance='Camera/registry counts derived from frozen chart and instantiator; U/F/R from fixed harness loop, not independent runtime counters',
                  completion_endpoints_comparable=False,
                  unavailable=['receipt-enqueue','receipt-drain','GPU-complete'])
    f.save(args.output,result)
    print(result['effect'])
