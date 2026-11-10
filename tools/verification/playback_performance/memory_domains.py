"""Attribute the PD-5 allocation envelope to owner x domain cells; static checks only.

The lifetime observer emits ``PD5_MEMORY_DOMAIN`` rows per non-zero cell and one
``PD5_MEMORY_PARTITION`` row per checkpoint.  This module never runs a product, never
resets a peak and never turns an instrumented timing into acceptance evidence.  It
answers one question: do the per-cell books add up to the process books, and does each
observed cell map to a declared cap cell?

Two subcommands:

* ``collect-domains`` parses a captured log into ``pd5.memory.domains.v1`` JSON.
* ``check-domains`` collects and validates partition integrity, coverage, untagged
  bytes and (when a caps file is given) per-cell cap compliance.

Classification is static and total: every (owner, domain) pair is either mapped to one
cap cell, excluded from the reference-workload envelope by the B0 decision, unrated
(no declared cap), or the untagged bucket.  Unknown owners or domains fail closed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import measurement as measure

OWNERS = ('probe', 'main', 'playback-worker', 'renderer-owner', 'audio-owner',
          'audio-worker', 'unknown')
DOMAINS = ('fixture', 'load', 'decode', 'prepare', 'snapshot', 'advance', 'current',
           'candidate', 'scratch', 'index', 'ready', 'history', 'receipt', 'journal',
           'replay', 'gpu', 'audio', 'retained', 'static', 'other')

# Product ownership -> cap cell.  A cap cell may receive more than one domain.
PRODUCT_MAPPED = {
    'candidate': 'state.candidate',
    'current': 'state.current',
    'scratch': 'state.scratch',
    'index': 'state.index',
    'history': 'history.request',
    'receipt': 'delivery.receipt',
    'journal': 'journal.bytes',
    'ready': 'delivery.ready',
    'gpu': 'gpu.inflight',
    'audio': 'audio.live',
}
# The B0 envelope excludes owning content (decode/fixture/load) and the Snapshot /
# Replay archive.  Those bytes are real but are not part of the product envelope.
PRODUCT_EXCLUDED = ('fixture', 'load', 'decode', 'snapshot', 'replay')
# Product domains with no declared per-cell cap: transient frame work, fixed structures
# and the retained bucket.  They stay visible but are not compared against a cap.
PRODUCT_UNRATED = ('prepare', 'advance', 'retained', 'static', 'other')

PARTITION_FIELDS = ('sum_live_count', 'global_live_count', 'sum_requested_cpp_bytes',
                    'global_requested_cpp_bytes', 'sum_crt_usable_storage_bytes',
                    'global_crt_usable_storage_bytes', 'cells', 'untagged_live_count')


class DomainError(ValueError):
    """A domain record cannot be trusted as attribution evidence."""


def classify(owner, domain):
    """Return (classification, cap_cell or None) for one observed cell."""
    if owner not in OWNERS:
        raise DomainError('Unknown owner axis value: ' + owner)
    if domain not in DOMAINS:
        raise DomainError('Unknown domain axis value: ' + domain)
    if owner == 'unknown':
        return 'untagged', None
    if owner == 'probe':
        return 'harness', None
    if domain in PRODUCT_MAPPED:
        return 'mapped', PRODUCT_MAPPED[domain]
    if domain in PRODUCT_EXCLUDED:
        return 'excluded', None
    if domain in PRODUCT_UNRATED:
        return 'unrated', None
    raise DomainError('Unclassified product cell: ' + owner + '.' + domain)


def _unsigned(row, key, label):
    value = row.get(key)
    if type(value) is not int or value < 0:
        raise DomainError('Negative or non-integer ' + key + ' at ' + label)
    return value


def _require_schema(row, schema, label):
    if row.get('schema') != schema:
        raise DomainError('Unexpected schema at ' + label + ': ' + str(row.get('schema')))
    if not isinstance(row.get('label'), str) or not row['label']:
        raise DomainError('Missing checkpoint label')


def collect(text, source=None):
    """Parse domain and partition records; never consults caps."""
    domains = measure.records(text, 'PD5_MEMORY_DOMAIN ')
    partitions = measure.records(text, 'PD5_MEMORY_PARTITION ')
    memory = measure.records(text, 'PD5_MEMORY ')
    if not partitions:
        raise DomainError('No PD5_MEMORY_PARTITION records')
    labels = []
    by_label = {}
    for row in partitions:
        _require_schema(row, 'pd5.memory.partition.v1', str(row.get('label')))
        label = row['label']
        if label in by_label:
            raise DomainError('Duplicate partition label: ' + label)
        by_label[label] = row
        labels.append(label)
    memory_by_label = {}
    for row in memory:
        _require_schema(row, 'pd5.memory.v1', str(row.get('label')))
        memory_by_label[row['label']] = row
    orphan_memory = sorted(set(memory_by_label) - set(by_label))
    if orphan_memory:
        raise DomainError('Memory checkpoint without a partition: ' + orphan_memory[0])

    cells = []
    seen = set()
    for row in domains:
        _require_schema(row, 'pd5.memory.domain.v1', str(row.get('label')))
        label, owner, domain = row['label'], row.get('owner'), row.get('domain')
        if label not in by_label:
            raise DomainError('Domain row without a partition: ' + label)
        if (label, owner, domain) in seen:
            raise DomainError('Duplicate domain cell ' + str(owner) + '.' + str(domain) +
                              ' at ' + label)
        seen.add((label, owner, domain))
        if row.get('cell') != str(owner) + '.' + str(domain):
            raise DomainError('Cell id does not match owner and domain: ' + str(row.get('cell')))
        kind, cap_cell = classify(owner, domain)
        live = _unsigned(row, 'live_count', label)
        peak = _unsigned(row, 'peak_live_count', label)
        requested = _unsigned(row, 'requested_cpp_bytes', label)
        peak_requested = _unsigned(row, 'peak_requested_cpp_bytes', label)
        storage = _unsigned(row, 'crt_usable_storage_bytes', label)
        peak_storage = _unsigned(row, 'peak_crt_usable_storage_bytes', label)
        age = _unsigned(row, 'oldest_live_age_us', label)
        if live > peak:
            raise DomainError('Live count exceeds peak count at ' + label)
        if requested > peak_requested or storage > peak_storage:
            raise DomainError('Live bytes exceed peak bytes at ' + label)
        if requested > storage:
            raise DomainError('Requested bytes exceed CRT usable storage at ' + label)
        if not live and not peak:
            raise DomainError('Empty cell was emitted at ' + label)
        cells.append({
            'label': label, 'owner': owner, 'domain': domain, 'cell': row['cell'],
            'classification': kind, 'cap_cell': cap_cell,
            'live_count': live, 'requested_cpp_bytes': requested,
            'crt_usable_storage_bytes': storage, 'peak_live_count': peak,
            'peak_requested_cpp_bytes': peak_requested,
            'peak_crt_usable_storage_bytes': peak_storage,
            'oldest_live_age_us': age,
        })

    result = {'schema': 'pd5.memory.domains.v1', 'labels': labels,
              'partitions': [], 'cells': cells}
    for label in labels:
        row = by_label[label]
        entry = {'label': label, 'exact': row.get('exact')}
        for key in PARTITION_FIELDS:
            entry[key] = _unsigned(row, key, label)
        entry['observed_cells'] = sum(1 for cell in cells if cell['label'] == label)
        if label in memory_by_label:
            global_row = memory_by_label[label]
            entry['global_memory'] = {
                key: _unsigned(global_row, key, label) for key in
                ('live_count', 'requested_cpp_bytes', 'crt_usable_storage_bytes',
                 'peak_live_count', 'peak_requested_cpp_bytes',
                 'peak_crt_usable_storage_bytes', 'oldest_live_age_us')}
            entry['global_memory']['exceeded'] = global_row.get('exceeded')
        else:
            entry['global_memory'] = None
        result['partitions'].append(entry)
    coverage = {key: 0 for key in ('mapped', 'excluded', 'unrated', 'harness', 'untagged')}
    for cell in cells:
        coverage[cell['classification']] += 1
    result['coverage'] = coverage
    result['totals'] = {
        'labels': len(labels), 'cells': len(cells),
        'peak_requested_cpp_bytes': max((c['peak_requested_cpp_bytes'] for c in cells),
                                        default=0),
        'untagged_peak_requested_cpp_bytes': max(
            (c['peak_requested_cpp_bytes'] for c in cells if c['classification'] == 'untagged'),
            default=0),
    }
    if source is not None:
        data = Path(source).read_bytes()
        result['source'] = {'path': str(source), 'bytes': len(data),
                            'sha256': hashlib.sha256(data).hexdigest()}
    return result


def _check_partition(entry, cells):
    label = entry['label']
    if entry['exact'] is not True:
        raise DomainError('Partition is not exact at ' + label)
    for value, total in (('sum_live_count', 'global_live_count'),
                         ('sum_requested_cpp_bytes', 'global_requested_cpp_bytes'),
                         ('sum_crt_usable_storage_bytes', 'global_crt_usable_storage_bytes')):
        if entry[value] != entry[total]:
            raise DomainError('Partition sum differs from global ' + value + ' at ' + label)
    if entry['cells'] != entry['observed_cells']:
        raise DomainError('Partition cell count differs from emitted rows at ' + label)
    untagged = [c for c in cells if c['classification'] == 'untagged']
    untagged_live = sum(c['live_count'] for c in untagged)
    if untagged_live != entry['untagged_live_count']:
        raise DomainError('Untagged live count differs from unknown.other at ' + label)
    if entry['untagged_live_count'] > entry['global_live_count']:
        raise DomainError('Untagged live count exceeds global live count at ' + label)
    global_memory = entry.get('global_memory')
    if global_memory is not None:
        for value, total in (('sum_live_count', 'live_count'),
                             ('sum_requested_cpp_bytes', 'requested_cpp_bytes'),
                             ('sum_crt_usable_storage_bytes', 'crt_usable_storage_bytes')):
            if entry[value] != global_memory[total]:
                raise DomainError('Domain books differ from PD5_MEMORY ' + value + ' at ' + label)


def _check_cells(cells):
    peak_by_cell = {}
    for cell in cells:
        key = (cell['owner'], cell['domain'])
        peak = peak_by_cell.get(key)
        current = (cell['peak_live_count'], cell['peak_requested_cpp_bytes'],
                   cell['peak_crt_usable_storage_bytes'])
        if peak is not None and any(a < b for a, b in zip(current, peak)):
            raise DomainError('Cell high-water lost a transient at ' + cell['label'] + ':' +
                              cell['cell'])
        peak_by_cell[key] = current


def _cap_cells(caps, profile, entities):
    contents = caps.get('contents')
    if not isinstance(contents, list):
        raise DomainError('Caps file has no contents list')
    matches = [c for c in contents if c.get('profile') == profile]
    if not matches:
        raise DomainError('Caps file does not register profile: ' + str(profile))
    if len(matches) > 1:
        raise DomainError('Caps file registers profile more than once: ' + str(profile))
    content = matches[0]
    declared = content.get('content_entities')
    if entities is not None and declared != entities:
        raise DomainError('Caps profile declares ' + str(declared) + ' entities, run has ' +
                          str(entities))
    return {cell['cell']: cell for cell in content.get('cells', [])}


def _check_caps(collected, caps, profile, entities, uncaptured_ok, warnings):
    declared = _cap_cells(caps, profile, entities)
    cells = collected['cells']
    observed = {}
    for cell in cells:
        if cell['classification'] != 'mapped':
            continue
        key = (cell['cap_cell'], cell['owner'], cell['domain'])
        previous = observed.get(key)
        if previous is None or cell['peak_requested_cpp_bytes'] > previous['bytes']:
            observed[key] = {'count': cell['peak_live_count'],
                             'bytes': cell['peak_requested_cpp_bytes']}
    by_cap = {}
    for (cap_cell, _owner, _domain), values in observed.items():
        bucket = by_cap.setdefault(cap_cell, {'count': 0, 'bytes': 0})
        bucket['count'] += values['count']
        bucket['bytes'] += values['bytes']
    observed = by_cap
    uncaptured = []
    for name, cap in declared.items():
        kind = cap.get('cap_kind')
        observed_cell = observed.get(name)
        if kind == 'duration':
            continue
        if kind not in ('count', 'bytes', 'both'):
            raise DomainError('Unknown cap kind for ' + name + ': ' + str(kind))
        if cap.get('coverage', '').startswith('NA'):
            if observed_cell and (observed_cell['count'] or observed_cell['bytes']):
                raise DomainError('Observed bytes in unallocated cap cell: ' + name)
            continue
        # Count-kind caps describe structural slot counts, which PD5_STRUCTURE covers and
        # the allocation axis cannot see.  Only byte-kind caps need an observable cell.
        if observed_cell is None:
            if kind != 'count':
                uncaptured.append(name)
            continue
        if kind in ('count', 'both'):
            limit = cap.get('cap_count')
            if limit is None:
                if observed_cell['count']:
                    raise DomainError('Cap cell has no declared count: ' + name)
            elif observed_cell['count'] > limit:
                raise DomainError('Observed count exceeds cap ' + name + ': ' +
                                  str(observed_cell['count']) + ' > ' + str(limit))
        if kind in ('bytes', 'both'):
            limit = cap.get('cap_requested_bytes')
            if limit is None:
                if observed_cell['bytes']:
                    raise DomainError('Cap cell has no declared bytes: ' + name)
            elif observed_cell['bytes'] > limit:
                raise DomainError('Observed bytes exceed cap ' + name + ': ' +
                                  str(observed_cell['bytes']) + ' > ' + str(limit))
    allowed = set(uncaptured_ok)
    blocking = [name for name in uncaptured if name not in allowed]
    for name in uncaptured:
        if name not in allowed:
            warnings.append('Cap cell has no observable domain: ' + name)
    return {'profile': profile, 'entities': entities, 'observed_cells': sorted(observed),
            'uncaptured_cells': uncaptured, 'declared_exceptions': sorted(allowed),
            'blocking_uncaptured_cells': blocking}


def check(collected, caps=None, profile=None, entities=None, max_age_us=None,
          max_untagged_bytes=None, uncaptured_ok=(), strict=False, require_labels=(),
          expect_zero=()):
    """Validate one collected attribution document; return a verdict document."""
    labels = collected.get('labels')
    if not labels:
        raise DomainError('No checkpoints to check')
    missing = [label for label in require_labels if label not in labels]
    if missing:
        raise DomainError('Missing required checkpoint: ' + missing[0])
    partitions = {entry['label']: entry for entry in collected['partitions']}
    for label in expect_zero:
        if label not in partitions:
            raise DomainError('Zero checkpoint is not present: ' + label)
        partition = partitions[label]
        if partition['global_live_count'] or partition['global_requested_cpp_bytes'] or \
                partition['global_crt_usable_storage_bytes']:
            raise DomainError('Partition is not zero after release at ' + label)
    cells = collected['cells']
    cells_by_label = {label: [] for label in labels}
    for cell in cells:
        cells_by_label[cell['label']].append(cell)
    for entry in collected['partitions']:
        _check_partition(entry, cells_by_label[entry['label']])
    _check_cells(cells)
    warnings = []
    exceeded = [entry['label'] for entry in collected['partitions']
                if (entry.get('global_memory') or {}).get('exceeded') is True]
    if exceeded:
        warnings.append('PD5_MEMORY envelope exceeded at ' + exceeded[0])
    verdict = {'schema': 'pd5.memory.domains.check.v1', 'labels': labels,
               'cells': len(cells), 'warnings': warnings, 'strict': bool(strict),
               'required_labels': list(require_labels),
               'zero_labels': list(expect_zero),
               'envelope_exceeded_labels': exceeded}
    if max_age_us is not None:
        live_cells = [c for c in cells if c['oldest_live_age_us'] > max_age_us]
        if live_cells:
            raise DomainError('Allocation age exceeds declared cap at ' + live_cells[0]['label'] +
                              ':' + live_cells[0]['cell'])
    untagged = [c for c in cells if c['classification'] == 'untagged']
    worst = max(untagged, key=lambda c: c['peak_requested_cpp_bytes'], default=None)
    verdict['untagged'] = {'cells': len(untagged),
                           'peak_requested_cpp_bytes': worst['peak_requested_cpp_bytes']
                           if worst else 0,
                           'worst_cell': worst['cell'] if worst else None}
    if max_untagged_bytes is not None and verdict['untagged']['peak_requested_cpp_bytes'] > \
            max_untagged_bytes:
        raise DomainError('Untagged peak exceeds declared allowance: ' +
                          str(verdict['untagged']['peak_requested_cpp_bytes']) + ' > ' +
                          str(max_untagged_bytes))
    verdict['coverage'] = collected.get('coverage')
    verdict['totals'] = collected.get('totals')
    if caps is not None:
        if not profile:
            raise DomainError('Caps check requires a profile')
        verdict['caps'] = _check_caps(collected, caps, profile, entities,
                                      tuple(uncaptured_ok), warnings)
    else:
        verdict['caps'] = None
        if uncaptured_ok:
            raise DomainError('Uncaptured exceptions are only meaningful with caps')
    if strict and warnings:
        raise DomainError('Strict attribution failed: ' + warnings[0])
    verdict['passed'] = True
    return verdict


def _load_caps(path):
    caps = json.loads(Path(path).read_text(encoding='utf-8'))
    if caps.get('schema') != 'pd5.memory.caps.v1':
        raise DomainError('Unexpected caps schema: ' + str(caps.get('schema')))
    return caps


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest='command', required=True)
    collect_parser = commands.add_parser('collect-domains')
    check_parser = commands.add_parser('check-domains')
    for sub in (collect_parser, check_parser):
        sub.add_argument('log', type=Path)
        sub.add_argument('--json', type=Path, default=None)
    check_parser.add_argument('--caps', type=Path, default=None)
    check_parser.add_argument('--profile', default=None)
    check_parser.add_argument('--entities', type=int, default=None)
    check_parser.add_argument('--max-age-us', type=int, default=None)
    check_parser.add_argument('--max-untagged-bytes', type=int, default=None)
    check_parser.add_argument('--uncaptured-ok', action='append', default=[])
    check_parser.add_argument('--require-labels', default='before-load,after-close',
                              help='Comma separated checkpoints that must be present')
    check_parser.add_argument('--expect-zero', action='append', default=[],
                              help='Checkpoint whose live partition must be zero')
    check_parser.add_argument('--strict', action='store_true')
    arguments = parser.parse_args(argv)
    try:
        text = arguments.log.read_text(encoding='utf-8-sig')
        collected = collect(text, source=arguments.log)
        if arguments.command == 'collect-domains':
            result = collected
        else:
            caps = _load_caps(arguments.caps) if arguments.caps else None
            required = [label.strip() for label in arguments.require_labels.split(',')
                        if label.strip()]
            result = check(collected, caps, arguments.profile, arguments.entities,
                           arguments.max_age_us, arguments.max_untagged_bytes,
                           arguments.uncaptured_ok, arguments.strict, required,
                           arguments.expect_zero)
            for warning in result['warnings']:
                print('memory-domains: warning: ' + warning, file=sys.stderr)
            print('memory-domains: ' + str(result['cells']) + ' cells over ' +
                  str(len(result['labels'])) + ' checkpoints, untagged peak ' +
                  str(result['untagged']['peak_requested_cpp_bytes']) + ' B')
    except (DomainError, OSError, json.JSONDecodeError, KeyError) as error:
        print('memory-domains: ' + str(error), file=sys.stderr)
        return 1
    if arguments.json:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(json.dumps(result, ensure_ascii=False, sort_keys=True) + '\n',
                                  encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
