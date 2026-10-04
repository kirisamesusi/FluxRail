"""保存本轮软件证据与当前源码摘要；不执行设备操作。"""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    build = HERE / 'build'
    full = read(build / 'full_results.json')
    firmware = read(build / 'firmware_results.json')
    descriptors = read(build / 'usb_descriptor_results.json')
    audit = read(build / 'source_link_audit.json')
    assert full['passed'] and full['status'] == 'complete'
    assert full['formal_build_included'] and len(full['steps']) == 13
    assert firmware['exit_code'] == 0 and firmware['source_link_audit_passed']
    assert firmware['firmware_metadata']['verified']
    assert all(result['passed'] for result in descriptors['results'])
    assert descriptors['elf_sha256'] == digest(build / 'arduino/FluxRail.ino.elf')
    assert audit['passed'] and audit['storage_partition_reserved']
    assert audit['stack_frame_budget_passed'] and audit['core1_entry_calls_project']
    # 同一正式产物所对应的源码，不用历史报告证明修改后的源文件。
    for rel, expected in firmware['source_sha256'].items():
        assert digest(ROOT / rel) == expected, rel
    paths = subprocess.check_output(
        ['rg', '--files', '-g', '*.md', '-g', '*.txt', '-g', '*.rst', '-g', '*.adoc',
         '-g', '!**/build/**', '-g', '!dependencies/generated/**',
         '-g', '!backups/firmware-before-bugfix-2026-10-04/**',
         '-g', '!backups/firmware-before-absolute-xy-2026-10-04/**'],
        cwd=ROOT, text=True, encoding='utf-8').splitlines()
    docs = {Path(p).as_posix(): digest(ROOT / p) for p in paths
            if Path(p).name != 'CMakeLists.txt'}
    reports = []
    for rel in ['tests/module1/build/simulation_results.json',
                'tests/module2/build/simulation_results.json',
                'tests/module3/build/simulation_results.json',
                'tests/integration/build/simulation_results.json',
                'tests/integration/build/transport/simulation_results.json',
                'tests/integration/build/protocol/simulation_results.json',
                'tests/integration/build/storage/simulation_results.json',
                'tests/integration/build/app_storage/simulation_results.json',
                'tests/integration/build/io4/simulation_results.json',
                'tests/integration/build/motion/simulation_results.json']:
        path = ROOT / rel
        simulation = read(path)
        results = simulation['results']
        assert all(r['passed'] and r['completed'] for r in results), rel
        for source, expected in simulation.get('source_sha256', {}).items():
            assert digest(ROOT / source) == expected, source
        reports.append({'path': rel, 'sha256': digest(path),
                        'groups': len(results), 'checks': sum(r['checks'] for r in results),
                        'results': results,
                        'source_sha256': simulation.get('source_sha256', {})})
    uf2 = build / 'arduino/FluxRail.ino.uf2'
    report = {
        'date_local': datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(),
        'scope': 'Hall fixed-phase scheduling and standard absolute-XY mouse software candidate; board/Windows/game verification pending',
        'hall_software_fixed': True, 'windows_code10_software_fix_implemented': True,
        'windows_code10_fixed': None, 'windows_host_verified': False,
        'hall_hardware_hz': None,
        'hardware_tested': False, 'game_tested': False, 'uploaded': False,
        'usb_choice_pending': False, 'release_blocked_by_windows_compatibility': True,
        'absolute_y_policy': 'Initialize to 16384/32767 once per firmware boot; retain device knob Y on Hall X reports; no host helper',
        'full_validation': full, 'firmware': firmware, 'descriptors': descriptors,
        'source_link_audit': audit,
        'arm_reports': reports,
        'arm_groups': sum(r['groups'] for r in reports),
        'arm_checks': sum(r['checks'] for r in reports),
        'uf2': {'path': uf2.relative_to(ROOT).as_posix(), 'bytes': uf2.stat().st_size,
                'sha256': digest(uf2)},
        'documentation_sha256': docs,
        'documentation_scope': 'Project docs, module/test/Web docs, reference README and license, old Web backup docs; identical content read once; generated build/library copies excluded',
        'review': {'model': 'gpt-6-luna', 'read_only': True,
                   'path': 'tests/integration/REVIEW_BUGFIX_2026-10-04.md',
                   'sha256': digest(HERE / 'REVIEW_BUGFIX_2026-10-04.md')},
        'limitations': ['No new board or Windows host/game validation',
                        'ELF descriptor checks validate structure, not Windows mouhid startup or actual Y/button handling',
                        'Other mice cursor changes are not available to firmware; absolute reports restore device-owned Y',
                        'Synthetic execution costs do not prove worst-case real-time performance',
                        'Original combined timing case exhausted simulator budget; split cases pass'],
    }
    target = HERE / 'validation_bugfix_2026-10-04.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'PASS: saved {report["arm_groups"]} ARM groups/{report["arm_checks"]} checks; Windows/board/game verification pending')


if __name__ == '__main__':
    main()
