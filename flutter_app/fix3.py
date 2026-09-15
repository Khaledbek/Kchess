import sys

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Fix unnecessary underscores
    content = content.replace('(_, __) {}', '(_, _) {}')
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

fix_file('lib/features/training/presentation/opening/opening_cards.dart')
