import sys

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Fix total -> games
    content = content.replace('tally.total', 'tally.games')
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

fix_file('lib/features/training/presentation/opening_lab_screen.dart')
fix_file('lib/features/training/presentation/opening/opening_cards.dart')
fix_file('lib/features/training/presentation/opening/opening_family_hub.dart')
