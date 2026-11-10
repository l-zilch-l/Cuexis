"""Count the real Gameplay-host dictionaries in an isolated instrumented source copy.

This is a correctness/admission investigation, never a performance or formal registration.
Production sources, frozen builds, limits and previous evidence are not edited.
"""
import argparse
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import finite_followup as files, measurement as m
from tools.verification.playback_performance import process_control as control

ROOT = Path(__file__).resolve().parents[3]
UNIT = 'PD5 full content Snapshot Seek Replay boundary'
CONFIGS = [(n, reuse) for n in (1, 64, 40000) for reuse in ('low', 'high')]

HEADER = r'''#pragma once
#include <cuexis_internal/sha256.hpp>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace dictionary_audit {
template <typename Strings, typename References>
void stage(const char* label, const Strings& strings, const References& references) {
    std::cout << "AUDIT_STAGE {\"stage\":\"" << label << "\",\"strings\":" << strings.size()
              << ",\"references\":" << references.size() << "}\n";
}
template <typename Dictionaries, typename Limits>
void merged(const Dictionaries& dict, const Limits& limits, std::size_t entities) {
    cuexis::core::detail::Sha256 hash;
    std::uint64_t bytes{}, local{}, projected{};
    for (const auto& text : dict.strings) {
        std::array<std::byte, 8> length{};
        for (unsigned i = 0; i < 8; ++i)
            length[i] = static_cast<std::byte>((text.size() >> (8U * i)) & 255U);
        hash.update(length);
        hash.update(std::as_bytes(std::span{text.data(), text.size()}));
        bytes += text.size();
        local += text.starts_with("capacity.");
        projected += text.starts_with("cxgp2:");
    }
    std::map<unsigned, std::size_t> kinds;
    std::set<std::string> refStrings;
    for (const auto& [key, index] : dict.refIndex) {
        (void)index;
        ++kinds[key.first];
        refStrings.insert(key.second);
    }
    std::cout << "AUDIT_DICT {\"entities\":" << entities
              << ",\"strings\":" << dict.strings.size()
              << ",\"references\":" << dict.refIndex.size()
              << ",\"utf8_bytes\":" << bytes
              << ",\"str0_bytes\":" << 12 + 4 * dict.strings.size() + bytes
              << ",\"capacity_ids\":" << local << ",\"projection_strings\":" << projected
              << ",\"reference_strings\":" << refStrings.size()
              << ",\"strings_sha256\":\"" << cuexis::core::detail::sha256Hex(hash.finish())
              << "\",\"strings_limit\":" << limits.maxPackedStrings
              << ",\"references_limit\":" << limits.maxPackedReferences
              << ",\"section_limit\":" << limits.maxPackedSectionBytes << ",\"reference_kinds\":{";
    bool comma = false;
    for (const auto& [kind, count] : kinds) {
        if (comma) std::cout << ',';
        comma = true;
        std::cout << '"' << kind << "\":" << count;
    }
    std::cout << "}}\n";
}
} // namespace dictionary_audit
'''

FIXTURE_AUDIT = r'''
    if (test_support::environmentVariable("CUEXIS_DICTIONARY_AUDIT") == "1") {
        auto prepared = judgement::prepareResolvedGameplay(fixture->request);
        REQUIRE(prepared);
        std::cout << "AUDIT_FIXTURE {\"entities\":" << fixture->chart.entities.size()
                  << ",\"requirements\":" << document.requirements.size()
                  << ",\"reuse\":\"" << reuse << "\"}\n";
        auto encoded = gameplay_packed::encode(
            {fixture->chart, *prepared, fixture->owners,
             {"normalization.one", "coordinator.policy.greedy_v1"}, {}, {}, 3});
        std::cout << "AUDIT_RESULT {\"wire_accepted\":" << (encoded ? "true" : "false")
                  << ",\"code\":\"" << (encoded ? "" : std::string{encoded.error().code()})
                  << "\",\"packed_bytes\":" << (encoded ? encoded->size() : 0)
                  << ",\"performance_acceptance\":false}\n";
        if (scale == 40000) {
            REQUIRE_FALSE(encoded);
            REQUIRE(encoded.error().code() == "packed.budget.strings");
        } else {
            REQUIRE(encoded);
        }
        return;
    }
'''


def replace_once(path, old, new):
    text = path.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise ValueError('Instrumentation anchor changed: '+str(path))
    path.write_text(text.replace(old, new), encoding='utf-8')


def prepare(folder):
    folder.mkdir(parents=True, exist_ok=False)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    archive = subprocess.check_output(['git', 'archive', '--format=zip', head], cwd=ROOT)
    (folder/'source-input.zip').write_bytes(archive)
    source = folder/'source'
    with zipfile.ZipFile(io.BytesIO(archive)) as stream:
        stream.extractall(source)
    original = files.source_identity(source)
    (source/'_audit').mkdir()
    (source/'_audit/packed_dictionary_audit.hpp').write_text(HEADER, encoding='utf-8')
    paths = ['engine/gameplay_packed/src/gameplay_capsule.cpp',
             'engine/chart/src/packed_chart_tables.cpp', 'tests/pd/pd5_capacity_probe.hpp']
    originals = {name: (source/name).read_text(encoding='utf-8') for name in paths}
    for name in paths[:2]:
        path = source/name
        text = path.read_text(encoding='utf-8')
        path.write_text('#include "../../../_audit/packed_dictionary_audit.hpp"\n'+text,
                        encoding='utf-8')
    replace_once(source/paths[0], '    allRows(collect, m);',
        '''    globalRow(collect, m);
    dictionary_audit::stage("global", strings, references);
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i)
        requirementRow(collect, m.graph.requirements[i], m.indices[i], m);
    dictionary_audit::stage("requirements", strings, references);
    definitions(collect, m);
    dictionary_audit::stage("definitions", strings, references);
    coordination(collect, m);
    dictionary_audit::stage("coordination", strings, references);''')
    replace_once(source/paths[1],
        '    if (dict.strings.size() > budget.maxPackedStrings) {',
        '    dictionary_audit::merged(dict, budget, chart.entities.size());\n'
        '    if (dict.strings.size() > budget.maxPackedStrings) {')
    replace_once(source/paths[2], '    internal::pd::memory::checkpoint("before-load");',
                 FIXTURE_AUDIT+'\n    internal::pd::memory::checkpoint("before-load");')
    import difflib
    patch = ''.join(''.join(difflib.unified_diff(originals[name].splitlines(True),
        (source/name).read_text(encoding='utf-8').splitlines(True), fromfile='a/'+name,
        tofile='b/'+name)) for name in paths)
    (folder/'instrumentation.diff').write_text(patch, encoding='utf-8')
    # Enforce that no budget declaration/resolution is touched by the observational patch.
    for name in ('engine/chart/include/cuexis/chart/limits.hpp',
                 'engine/chart/src/packed_limits_internal.hpp'):
        if files.sha(source/name) != files.sha(ROOT/name):
            raise ValueError('Budget contract changed')
    files.save(folder/'plan.json', dict(schema='packed.string-budget-audit.v1', formal=False,
        performance=False, head=head, source=str(source), original_source=original,
        instrumented_source=files.source_identity(source), archive_sha256=files.sha(folder/'source-input.zip'),
        instrumentation_sha256=files.sha(folder/'instrumentation.diff'),
        header_sha256=files.sha(source/'_audit/packed_dictionary_audit.hpp'),
        tool_sha256=files.sha(Path(__file__)),
        configs=CONFIGS, native_processes=6, repeats=1, max_build_jobs=4,
        stop='Unexpected execution/schema/identity/fixture error stops all remaining units; no retries',
        limits=dict(strings=100000,references=100000,section=16777216),
        hash_format='Sorted unique STR0 strings, each framed by u64 little-endian byte length; not encoded STR0 identity'))


def parse_log(text, n, reuse):
    stages = m.records(text, 'AUDIT_STAGE ')
    if [r['stage'] for r in stages] != ['global','requirements','definitions','coordination']:
        raise ValueError('Missing/duplicate dictionary collection stage')
    records = [m.records(text, tag) for tag in ('AUDIT_FIXTURE ','AUDIT_DICT ','AUDIT_RESULT ')]
    if any(len(r) != 1 for r in records):
        raise ValueError('Incomplete dictionary observation')
    fixture, dictionary, result = [r[0] for r in records]
    if fixture != dict(entities=n,requirements=n,reuse=reuse) or dictionary['entities'] != n:
        raise ValueError('Wrong actual fixture size/reuse')
    if (dictionary['strings_limit'],dictionary['references_limit'],dictionary['section_limit']) != (100000,100000,16777216):
        raise ValueError('Frozen limits changed')
    if sum(dictionary['reference_kinds'].values()) != dictionary['references']:
        raise ValueError('Reference histogram disagrees with actual table')
    if dictionary['str0_bytes'] != 12+4*dictionary['strings']+dictionary['utf8_bytes']:
        raise ValueError('STR0 accounting mismatch')
    if result['wire_accepted'] != (n != 40000) or result['performance_acceptance'] is not False:
        raise ValueError('Unexpected admission outcome')
    if n == 40000 and result['code'] != 'packed.budget.strings':
        raise ValueError('Unexpected first rejection')
    return dict(stages=stages,dictionary=dictionary,result=result)


def run(folder, build):
    plan = json.loads((folder/'plan.json').read_text(encoding='utf-8'))
    source = Path(plan['source'])
    if files.source_identity(source) != plan['instrumented_source']:
        raise ValueError('Instrumented source changed')
    if (files.sha(source/'_audit/packed_dictionary_audit.hpp') != plan['header_sha256'] or
            files.sha(Path(__file__)) != plan['tool_sha256']):
        raise ValueError('Audit tool/header changed')
    exe = build/'bin/cuexis_pd_host_probe.exe'
    files.save(folder/'run.claim.json',dict(binary=str(exe),binary_sha256=files.sha(exe),
        plan_sha256=files.sha(folder/'plan.json'),mask=control.affinity(),formal=False))
    rows=[]
    for n,reuse in plan['configs']:
        log=folder/f'{n}-{reuse}.log'
        env={k:v for k,v in os.environ.items() if not k.startswith('CUEXIS_PD')}
        env.update(CUEXIS_PD5_ENTITIES=str(n),CUEXIS_PD5_REUSE=reuse,CUEXIS_DICTIONARY_AUDIT='1')
        record=dict(entities=n,reuse=reuse,log=str(log),exit_code=None)
        try:
            with log.open('x',encoding='utf-8') as stream:
                completed=control.run([str(exe),UNIT,'--rng-seed','1','--reporter','compact'],
                    control.affinity(),cwd=str(source),env=env,output=stream,timeout=900)
                record['exit_code']=completed.returncode
                completed.check_returncode()
            parsed=parse_log(log.read_text(encoding='utf-8-sig'),n,reuse)
            record.update(parsed)
        except Exception as error:
            record['error']=str(error)
            raise
        finally:
            record['log_sha256']=files.sha(log)
            files.save(folder/f'{n}-{reuse}.process.json',record)
        rows.append(record)
        print('completed',n,reuse,flush=True)
    files.save(folder/'analysis.json',dict(formal=False,performance_acceptance=False,records=rows))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['prepare','run'])
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--build',type=Path)
    args=parser.parse_args()
    if args.command == 'prepare': prepare(args.output.resolve())
    else: run(args.output.resolve(),args.build.resolve())
