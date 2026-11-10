"""Validate the sealed WPR profiles. No Power fallback is permitted.

The canary profile is the minimal lifecycle profile. The F1 profile is the
sealed diagnostic scheduler/processor-power profile the F1 collector runs under.
Both are checked by exact XML structure, and any change fails closed.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

from common import sha

PROFILE_NAME = 'PD5ToolLifecycle'
PROFILE_FILE = 'lifecycle.wprp'
F1_PROFILE_NAME = 'PD5F1Scale'
F1_PROFILE_FILE = 'f1_scale.wprp'
F1_PROFILE_ID = 'PD5F1Scale.Verbose.File'
F1_PROFILE_DESCRIPTION = 'Diagnostic scheduler and processor-power attribution for the F1 read-scale workload'
F1_SYSTEM_KEYWORDS = ['CSwitch', 'ReadyThread', 'DPC', 'Interrupt', 'Power', 'IdleStates']


def _entry(binding, key):
    profiles = binding.get('profiles')
    if isinstance(profiles, dict) and key in profiles:
        return profiles[key]
    if key == 'canary' and 'profile' in binding:
        return binding['profile']
    raise RuntimeError('sealed package has no ' + key + ' profile binding')


def profile_argument(here, binding, key='canary'):
    if key == 'canary':
        return _canary(here, binding)
    if key == 'f1-scale':
        return _f1_scale(here, binding)
    raise RuntimeError('unknown fixed profile: ' + repr(key))


def _canary(here, binding):
    entry = _entry(binding, 'canary')
    if entry['file'] != PROFILE_FILE or entry['name'] != PROFILE_NAME:
        raise RuntimeError('not the fixed tool-lifecycle profile')
    path = Path(here) / PROFILE_FILE
    if sha(path) != entry['sha256']:
        raise RuntimeError('tool-lifecycle profile hash changed')
    root = ET.parse(path).getroot()
    profiles = root.find('Profiles')
    if root.tag != 'WindowsPerformanceRecorder' or profiles is None:
        raise RuntimeError('invalid lifecycle WPRP root')
    expected_tags = ['SystemCollector', 'EventCollector', 'SystemProvider', 'EventProvider', 'Profile']
    if [element.tag for element in profiles] != expected_tags:
        raise RuntimeError('unexpected lifecycle WPRP definitions')
    for tag, identifier, name in (
        ('SystemCollector', 'LifecycleSystem', 'WPR System Collector'),
        ('EventCollector', 'LifecycleEvents', 'WPR Event Collector'),
    ):
        collector = profiles.find(tag)
        if collector.attrib != {'Id': identifier, 'Name': name}:
            raise RuntimeError('lifecycle collector identity changed')
        if [(item.tag, item.attrib) for item in collector] != [
            ('BufferSize', {'Value': '64'}), ('Buffers', {'Value': '16'})]:
            raise RuntimeError('lifecycle buffers changed')
    system, event, profile = (profiles.find(tag) for tag in ('SystemProvider', 'EventProvider', 'Profile'))
    if system.attrib != {'Id': 'LifecycleProcessThread'} or event.attrib != {
            'Id': 'LifecycleWprControl', 'Name': '36b6f488-aad7-48c2-afe3-d4ec2c8b46fa'}:
        raise RuntimeError('lifecycle providers changed')
    for provider, keyword in ((system, 'ProcessThread'), (event, '0x40000')):
        if [child.tag for child in provider] != ['Keywords'] or [row.attrib for row in provider.findall('Keywords/Keyword')] != [{'Value': keyword}]:
            raise RuntimeError('lifecycle keywords/stacks changed')
    if profile.attrib != {'Id': PROFILE_NAME + '.Light.File', 'Name': PROFILE_NAME,
            'Description': 'Tool lifecycle only; no performance attribution', 'DetailLevel': 'Light', 'LoggingMode': 'File'}:
        raise RuntimeError('lifecycle profile inheritance/mode changed')
    references = [(item.tag, item.get('Value')) for item in profile.iter() if item.get('Value') is not None]
    if references != [('SystemCollectorId', 'LifecycleSystem'), ('SystemProviderId', 'LifecycleProcessThread'),
                       ('EventCollectorId', 'LifecycleEvents'), ('EventProviderId', 'LifecycleWprControl')]:
        raise RuntimeError('lifecycle collector/provider references changed')
    return str(path.resolve()) + '!' + PROFILE_NAME


def _f1_scale(here, binding):
    entry = _entry(binding, 'f1-scale')
    if entry['file'] != F1_PROFILE_FILE or entry['name'] != F1_PROFILE_NAME:
        raise RuntimeError('not the fixed F1 scale profile')
    path = Path(here) / F1_PROFILE_FILE
    if sha(path) != entry['sha256']:
        raise RuntimeError('F1 scale profile hash changed')
    root = ET.parse(path).getroot()
    profiles = root.find('Profiles')
    if root.tag != 'WindowsPerformanceRecorder' or profiles is None:
        raise RuntimeError('invalid F1 scale WPRP root')
    expected_tags = ['SystemCollector', 'EventCollector', 'SystemProvider', 'EventProvider', 'Profile']
    if [element.tag for element in profiles] != expected_tags:
        raise RuntimeError('unexpected F1 scale WPRP definitions')
    for tag, identifier, size, count in (
        ('SystemCollector', 'F1ScaleSystem', '128', '64'),
        ('EventCollector', 'F1ScaleEvents', '128', '32'),
    ):
        collector = profiles.find(tag)
        if collector.attrib != {'Id': identifier, 'Name': 'WPR ' + ('System' if tag == 'SystemCollector' else 'Event') + ' Collector'}:
            raise RuntimeError('F1 scale collector identity changed')
        if [(item.tag, item.attrib) for item in collector] != [
            ('BufferSize', {'Value': size}), ('Buffers', {'Value': count})]:
            raise RuntimeError('F1 scale buffers changed')
    system, event, profile = (profiles.find(tag) for tag in ('SystemProvider', 'EventProvider', 'Profile'))
    if system.attrib != {'Id': 'F1ScaleScheduler'} or event.attrib != {
            'Id': 'F1ScaleProcessorPower', 'Name': 'Microsoft-Windows-Kernel-Processor-Power'}:
        raise RuntimeError('F1 scale providers changed')
    if [child.tag for child in system] != ['Keywords'] or [
            row.get('Value') for row in system.findall('Keywords/Keyword')] != F1_SYSTEM_KEYWORDS:
        raise RuntimeError('F1 scale scheduler keywords changed')
    if [child.tag for child in event] != ['Keywords'] or [
            row.get('Value') for row in event.findall('Keywords/Keyword')] != ['0x2ff']:
        raise RuntimeError('F1 scale processor-power keywords changed')
    if profile.attrib != {'Id': F1_PROFILE_ID, 'Name': F1_PROFILE_NAME,
            'Description': F1_PROFILE_DESCRIPTION, 'DetailLevel': 'Verbose', 'LoggingMode': 'File'}:
        raise RuntimeError('F1 scale profile detail/mode changed')
    references = [(item.tag, item.get('Value')) for item in profile.iter() if item.get('Value') is not None]
    if references != [('SystemCollectorId', 'F1ScaleSystem'), ('SystemProviderId', 'F1ScaleScheduler'),
                       ('EventCollectorId', 'F1ScaleEvents'), ('EventProviderId', 'F1ScaleProcessorPower')]:
        raise RuntimeError('F1 scale collector/provider references changed')
    return str(path.resolve()) + '!' + F1_PROFILE_NAME
