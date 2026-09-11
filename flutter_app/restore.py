import json

log_path = r'C:\Users\USER\.gemini\antigravity\brain\ec32a1c2-83f3-4e09-bbf1-f0b2c932a84c\.system_generated\logs\transcript_full.jsonl'
with open(log_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

for line in reversed(lines):
    if '"name":"write_to_file"' in line and 'opening_lab_screen.dart' in line:
        data = json.loads(line)
        for c in data.get('tool_calls', []):
            if c.get('name') == 'write_to_file' and 'opening_lab_screen.dart' in c.get('args', {}).get('TargetFile', ''):
                content = c['args']['CodeContent']
                with open('lib/features/training/presentation/opening_lab_screen.dart', 'w', encoding='utf-8') as out:
                    out.write(content)
                print("RESTORED!")
                exit(0)
