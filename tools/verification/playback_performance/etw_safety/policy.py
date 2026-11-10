"""Pure classification of an already-saved inventory. It grants no trace control."""
BACKGROUND = frozenset({
    ('WPR_initiated_DiagTrackMiniLogger_OneTrace_User_Logger_20261016_1_EC_0', ''),
    ('WPR_initiated_DiagTrackMiniLogger_WPR System Collector', ''),
    ('WPR_initiated_DiagTrackAotLogger_WPR System Collector', ''),
    ('WPR_initiated_DiagTrackAotLogger_Hello - Core', ''),
})


def classify(sessions):
    background, conflicts = [], []
    for row in sessions:
        name = row['name']
        # Never whitelist a PD5 name, even if an upstream record attempts to do so.
        if name.casefold().startswith(('pd5', 'wpr_initiated_pd5')):
            conflicts.append(dict(row, classification='PD5 session requires investigation'))
        elif not name.casefold().startswith('wpr_initiated_'):
            continue
        elif (name, row.get('log_file')) in BACKGROUND:
            background.append(dict(row, classification='recorded background; creator not proven'))
        else:
            conflicts.append(dict(row, classification='unknown or changed WPR session'))
    return {'session_conflict_gate': 'BLOCKED' if conflicts else 'NO UNCLASSIFIED WPR CONFLICT IN SNAPSHOT',
            'background': background, 'conflicts': conflicts,
            'stop_authorized': False, 'capture_authorized': False,
            'quiet_environment_proven': False, 'full_preflight_passed': False,
            'scope': 'Names and logfile tuples from one elevated inventory only; keep raw records.'}
