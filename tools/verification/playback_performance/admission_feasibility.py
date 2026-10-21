"""Stage A only: exact proposed-layout sizing and unchanged semantic-work demand."""
import argparse
import difflib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import finite_followup as f, measurement as m
from tools.verification.playback_performance import process_control as control
from tools.verification.playback_performance.string_budget_audit import replace_once

ROOT = Path(__file__).resolve().parents[3]
CONFIGS = [(n, reuse) for n in (1, 64, 40000) for reuse in ('low', 'high')]
UNIT = 'PD5 full content Snapshot Seek Replay boundary'
SUPPORT = Path(__file__).parent
CPP = 'engine/gameplay_packed/src/gameplay_capsule.cpp'
FIXTURE = 'tests/pd/pd5_capacity_probe.hpp'
LIMITS = {'file': 16777216, 'decoded': 16777216, 'section': 16777216,
          'entities': 40000, 'requirements': 40000, 'strings': 100000, 'references': 100000}


def instrument(source):
    audit = source / '_audit'
    audit.mkdir()
    for name in ('admission_counting.hpp', 'admission_counting.inc'):
        (audit / name).write_bytes((SUPPORT / name).read_bytes())
    cpp = source / CPP
    replace_once(cpp, '#include <algorithm>', '#include "../../../_audit/admission_counting.hpp"\n#include <algorithm>')
    replace_once(cpp, '    bool sizing{};', '''    bool sizing{};
    bool admissionProposed{};
    admission_audit::Counters* admissionCounters{};
    core::detail::Sha256* admissionDigest{};''')
    replace_once(cpp, '        add(bytes.size());', '''        add(bytes.size());
        if (admissionDigest) admissionDigest->update(bytes);''')
    replace_once(cpp, '        add(1);', '''        add(1);
        if (admissionDigest) {
            const std::array byte{static_cast<std::byte>(v)};
            admissionDigest->update(byte);
        }''')
    replace_once(cpp, '            add(wide ? 8 : 4);', '''            add(wide ? 8 : 4);
            if (admissionDigest) {
                std::array<std::byte, 8> bytes{};
                const auto width = wide ? 8U : 4U;
                for (unsigned i = 0; i < width; ++i)
                    bytes[i] = static_cast<std::byte>((v >> (8U * i)) & 255U);
                admissionDigest->update(std::span{bytes}.first(width));
            }''')
    replace_once(cpp, '            tag(kind);\n            text(v);', '''            if (admissionCounters) admissionCounters->projection(v);
            tag(kind);
            text(v);''')
    replace_once(cpp, '    auto expected = projection(subdomain, tokens, identity), actual = expected;', '''    if constexpr (!A::reading) {
        if (a.admissionProposed && ((kind == 12 && subdomain == 0) || (kind == 13 && subdomain == 1)))
            return;
    }
    auto expected = projection(subdomain, tokens, identity), actual = expected;''')
    replace_once(cpp, '    a.value(hasKey);\n    require(hasKey', '''    bool physicalKey = true;
    if constexpr (!A::reading) physicalKey = !a.admissionProposed;
    if (physicalKey) a.value(hasKey);
    require(hasKey''')
    replace_once(cpp, '    if (hasKey) {\n        if constexpr (defersGraphLinks<A>())',
                 '    if (hasKey && physicalKey) {\n        if constexpr (defersGraphLinks<A>())')
    replace_once(cpp, '        row(a, table[i]);', '''        const auto before = a.size;
        row(a, table[i]);
        if (a.admissionCounters) {
            std::string_view kind = "other";
            if constexpr (std::is_same_v<T, PatternDeclaration>) kind = "pattern";
            else if constexpr (std::is_same_v<T, MeasureDefinition>) kind = "measure";
            else if constexpr (std::is_same_v<T, JudgementDomainRecord>) kind = "domain";
            else if constexpr (std::is_same_v<T, SolverProfileDeclaration>) kind = "solver";
            else if constexpr (std::is_same_v<T, FactBindingRef>) kind = "binding";
            else if constexpr (std::is_same_v<T, ClaimRow>) kind = "claim";
            else if constexpr (std::is_same_v<T, RequirementRecord>) kind = "requirement-key";
            else if constexpr (std::is_same_v<T, ResourceRecord>) kind = "resource-key";
            a.admissionCounters->expanded(kind, a.size - before);
        }''')
    replace_once(cpp, 'template <typename T, typename F> auto boundary(F operation)',
                 '#include "../../../_audit/admission_counting.inc"\n\ntemplate <typename T, typename F> auto boundary(F operation)')
    replace_once(cpp, 'auto encode(const EncodeRequest& request, chart::PackedChartLimits limits) -> core::Result<Bytes> {',
                 '''auto admissionAudit(const EncodeRequest& request) -> core::Result<void> {
    return boundary<void>([&] { admissionAuditImpl(request); });
}

auto encode(const EncodeRequest& request, chart::PackedChartLimits limits) -> core::Result<Bytes> {''')
    fixture = source / FIXTURE
    replace_once(fixture, '#if defined(_WIN32)', '''#if defined(_WIN32)
#include "../../_audit/admission_counting.hpp"
namespace cuexis::gameplay_packed {
auto admissionAudit(const EncodeRequest&) -> core::Result<void>;
}
TEST_CASE("PD5 admission checked arithmetic", "[.][pd][admission][math]") {
    REQUIRE(admission_audit::add(UINT64_MAX - 1, 1) == UINT64_MAX);
    REQUIRE_THROWS_AS(admission_audit::add(UINT64_MAX, 1), std::overflow_error);
    REQUIRE(admission_audit::multiply(UINT64_MAX, 1) == UINT64_MAX);
    REQUIRE_THROWS_AS(admission_audit::multiply(UINT64_MAX, 2), std::overflow_error);
    REQUIRE(admission_audit::leb(127) == 1);
    REQUIRE(admission_audit::leb(128) == 2);
    REQUIRE(admission_audit::leb(UINT64_MAX) == 10);
}''')
    replace_once(fixture, '    internal::pd::memory::checkpoint("before-load");', '''    if (test_support::environmentVariable("CUEXIS_ADMISSION_AUDIT") == "1") {
        auto prepared = judgement::prepareResolvedGameplay(fixture->request);
        REQUIRE(prepared);
        const auto audit = gameplay_packed::admissionAudit(
            {fixture->chart, *prepared, fixture->owners,
             {"normalization.one", "coordinator.policy.greedy_v1"}, {}, {}, 3});
        INFO(audit ? "" : std::string{audit.error().message()});
        REQUIRE(audit);
        return;
    }
    internal::pd::memory::checkpoint("before-load");''')


def prepare(folder):
    folder.mkdir(parents=True, exist_ok=False)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    data = subprocess.check_output(['git', 'archive', '--format=zip', head], cwd=ROOT)
    (folder / 'source-input.zip').write_bytes(data)
    source = folder / 'source'
    with zipfile.ZipFile(io.BytesIO(data)) as stream:
        stream.extractall(source)
    original = f.source_identity(source)
    before = {name: (source / name).read_text(encoding='utf-8') for name in (CPP, FIXTURE)}
    instrument(source)
    patch = ''.join(''.join(difflib.unified_diff(before[name].splitlines(True),
                    (source / name).read_text(encoding='utf-8').splitlines(True),
                    fromfile='a/' + name, tofile='b/' + name)) for name in before)
    (folder / 'instrumentation.diff').write_text(patch, encoding='utf-8')
    f.save(folder / 'plan.json', dict(schema='pd5.admission-feasibility.v1', stage='A',
        formal=False, performance_acceptance=False, source=str(source), head=head,
        configs=CONFIGS, native_processes=6, repeats=1,
        original_source=original, instrumented_source=f.source_identity(source),
        fixture_base_sha256=original['files'][FIXTURE],
        tools={str(p):f.sha(p) for p in (Path(__file__), SUPPORT/'admission_counting.hpp', SUPPORT/'admission_counting.inc')},
        audit_files={str(p):f.sha(p) for p in audit_files(source)},
        archive_sha256=f.sha(folder/'source-input.zip'), instrumentation_sha256=f.sha(folder/'instrumentation.diff'),
        frozen_limits=LIMITS, work_budget_accepted=False,
        scope='Count demand and proposed layout only; existing Writer acceptance unchanged; no revision-4 artifact or Host success',
        stop='Any execution/schema/identity mismatch stops remaining configurations; no retries. Physical infeasibility stops B/C, not the six observations.'))


def audit_files(source):
    return sorted((source / '_audit').glob('*'))


def bind(folder,build):
    plan=json.loads((folder/'plan.json').read_text())
    source=Path(plan['source'])
    cache=(build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    from tools.verification.playback_performance.read_result_candidate import verify_build_source
    verify_build_source(source,cache)
    for entry in ('CMAKE_BUILD_TYPE:STRING=Release','CUEXIS_LIBRARY_TYPE:STRING=STATIC',
                  'CUEXIS_PD_METRICS_TEST_ONLY:BOOL=OFF','CUEXIS_PLAYBACK_TRACE_HARNESS:BOOL=OFF',
                  'CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=OFF','CUEXIS_ENABLE_CHART_V5_CANDIDATE:BOOL=ON'):
        if entry not in cache:raise ValueError('Wrong isolated build configuration: '+entry)
    paths=[build/name for name in ('CMakeCache.txt','compile_commands.json','build.ninja')]
    paths+=sorted((build/'lib').glob('*.lib'))
    paths+=[build/'bin/cuexis_pd_host_probe.exe']
    f.save(folder/'build-binding.json',dict(plan_sha256=f.sha(folder/'plan.json'),
        source_identity=f.source_identity(source),files={str(p):f.sha(p) for p in paths}))


def parse_log(text, n):
    def one(tag):
        rows=m.records(text, tag+' ')
        if len(rows)!=1: raise ValueError('Missing/duplicate '+tag)
        return rows[0]
    dictionaries=m.records(text,'ADMISSION_DICT ')
    if [r['layout'] for r in dictionaries]!=['legacy3','proposed4']:
        raise ValueError('Missing physical dictionary accounting')
    counts=one('ADMISSION_COUNTS');result=one('ADMISSION_RESULT')
    if (counts['entities'],counts['requirements'],counts['claims'],counts['occupying_claims'])!=(n,n,n,n):
        raise ValueError('Original fixture changed')
    if counts['logical_derived_projections']!=3*n:
        raise ValueError('Derived projection ownership/count mismatch')
    for row,strings,refs in zip(dictionaries,(4*n+48,n+48),(3*n+13,13)):
        if (row['strings'],row['references'])!=(strings,refs):raise ValueError('Dictionary count mismatch')
        if row['str0_bytes']!=12+4*strings+row['utf8_bytes']:raise ValueError('STR0 byte mismatch')
    sections=m.records(text,'ADMISSION_SECTION ')
    codes=[r['code'] for r in sections]
    if len(codes)!=len(set(codes)) or not {'META','TIME','STR0','REF0','IDN0','ARCH','ENT0','REQ0','CNS0','GPH0','GPR0','GPD0','GRC0'}<=set(codes):
        raise ValueError('Incomplete/duplicate section accounting')
    if sum(r['bytes'] for r in sections)!=counts['decoded_bytes'] or counts['header_bytes']!=96 or counts['directory_bytes']!=32*len(sections):
        raise ValueError('Envelope accounting mismatch')
    if counts['proposed_file_bytes']!=96+32*len(sections)+counts['decoded_bytes']:
        raise ValueError('Complete proposed file size mismatch')
    expected_fit=(counts['proposed_file_bytes']<=LIMITS['file'] and counts['decoded_bytes']<=LIMITS['decoded'] and all(r['bytes']<=LIMITS['section'] for r in sections))
    if counts['proposed_physical_fit']!=expected_fit:raise ValueError('Wrong physical budget result')
    stages=m.records(text,'ADMISSION_HASH_STAGE ')
    if [r['stage'] for r in stages]!=['coordination','definitions','global','requirements','static_prefix'] or sum(r['bytes'] for r in stages)!=counts['semantic_preimage_bytes']:
        raise ValueError('Incomplete semantic demand accounting')
    if len(counts['semantic_digest'])!=64 or counts['hash_output_buffer_bytes']!=0:
        raise ValueError('Missing streaming count/digest or materialized preimage')
    if (counts['proposed_wire_accepted'] or counts['performance_acceptance'] or result['proposed_wire_accepted'] or result['host_reached']):
        raise ValueError('Diagnostic counter falsely claims acceptance or Host execution')
    if result['legacy_wire_accepted']!=(n<=64) or result['legacy_code']!=('' if n<=64 else 'packed.budget.strings'):
        raise ValueError('Original Writer admission changed')
    if result['small_sizing_and_hash_verified']!=(n<=64):raise ValueError('Wrong validation scope')
    if n<=64 and not all(r['serialized_validation'] for r in sections):raise ValueError('Small sizing oracle missing')
    return dict(dictionaries=dictionaries,counts=counts,sections=sections,hash_stages=stages,
                expansions=m.records(text,'ADMISSION_EXPANSION '),result=result)


def run(folder,build):
    plan=json.loads((folder/'plan.json').read_text())
    binding=json.loads((folder/'build-binding.json').read_text())
    if plan['frozen_limits']!=LIMITS or binding['plan_sha256']!=f.sha(folder/'plan.json'):
        raise ValueError('Frozen plan/limits changed')
    source=Path(plan['source'])
    if f.source_identity(source)!=plan['instrumented_source']:raise ValueError('Source identity changed')
    for path,expected in {**plan['tools'],**plan['audit_files']}.items():
        if f.sha(Path(path))!=expected:raise ValueError('Counting tool changed')
    for path,expected in binding['files'].items():
        if f.sha(Path(path))!=expected:raise ValueError('Diagnostic build changed')
    if binding['source_identity']!=plan['instrumented_source']:raise ValueError('Build/source binding mismatch')
    exe=build/'bin/cuexis_pd_host_probe.exe'
    f.save(folder/'run.claim.json',dict(binary=str(exe),binary_sha256=f.sha(exe),plan_sha256=f.sha(folder/'plan.json'),formal=False))
    records=[]
    for n,reuse in plan['configs']:
        log=folder/f'{n}-{reuse}.log'; record=dict(entities=n,reuse=reuse,exit_code=None)
        env={k:v for k,v in os.environ.items() if not k.startswith(('CUEXIS_PD','CUEXIS_ADMISSION'))}
        env.update(CUEXIS_PD5_ENTITIES=str(n),CUEXIS_PD5_REUSE=reuse,CUEXIS_ADMISSION_AUDIT='1')
        try:
            with log.open('x',encoding='utf-8') as output:
                result=control.run([str(exe),UNIT,'--rng-seed','1','--reporter','compact'],control.affinity(),cwd=str(source),env=env,output=output,timeout=1200)
                record['exit_code']=result.returncode;result.check_returncode()
            record.update(parse_log(log.read_text(encoding='utf-8-sig'),n))
        except Exception as error:
            record['error']=str(error);raise
        finally:
            record['log_sha256']=f.sha(log);f.save(folder/f'{n}-{reuse}.process.json',record)
        records.append(record);print('completed',n,reuse,flush=True)
    f.save(folder/'analysis.json',dict(formal=False,work_budget_accepted=False,records=records))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['prepare','bind','run'])
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--build',type=Path)
    args=parser.parse_args()
    if args.command=='prepare':prepare(args.output.resolve())
    elif args.command=='bind':bind(args.output.resolve(),args.build.resolve())
    else:run(args.output.resolve(),args.build.resolve())
