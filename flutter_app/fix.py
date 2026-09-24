import sys

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Fix total -> games
    content = content.replace('tally.total', 'tally.games')
    
    # Fix missing interpolation in opening_cards.dart
    content = content.replace(
        "Text('% Win', style: theme.textTheme.labelSmall?.copyWith(color: Colors.green)),",
        "Text('${(winPct * 100).round()}% Win', style: theme.textTheme.labelSmall?.copyWith(color: Colors.green)),"
    )
    content = content.replace(
        "Text('% Draw', style: theme.textTheme.labelSmall?.copyWith(color: Colors.grey)),",
        "Text('${(drawPct * 100).round()}% Draw', style: theme.textTheme.labelSmall?.copyWith(color: Colors.grey)),"
    )
    content = content.replace(
        "Text('% Loss', style: theme.textTheme.labelSmall?.copyWith(color: Colors.red)),",
        "Text('${(lossPct * 100).round()}% Loss', style: theme.textTheme.labelSmall?.copyWith(color: Colors.red)),"
    )
    
    # Fix missing interpolation in opening_family_hub.dart
    content = content.replace(
        "Text('% Win', style: theme.textTheme.labelMedium?.copyWith(color: Colors.green, fontWeight: FontWeight.bold)),",
        "Text('${(winPct * 100).round()}% Win', style: theme.textTheme.labelMedium?.copyWith(color: Colors.green, fontWeight: FontWeight.bold)),"
    )
    content = content.replace(
        "Text('% Draw', style: theme.textTheme.labelMedium?.copyWith(color: Colors.grey, fontWeight: FontWeight.bold)),",
        "Text('${(drawPct * 100).round()}% Draw', style: theme.textTheme.labelMedium?.copyWith(color: Colors.grey, fontWeight: FontWeight.bold)),"
    )
    content = content.replace(
        "Text('% Loss', style: theme.textTheme.labelMedium?.copyWith(color: Colors.red, fontWeight: FontWeight.bold)),",
        "Text('${(lossPct * 100).round()}% Loss', style: theme.textTheme.labelMedium?.copyWith(color: Colors.red, fontWeight: FontWeight.bold)),"
    )
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

fix_file('lib/features/training/presentation/opening/opening_cards.dart')
fix_file('lib/features/training/presentation/opening/opening_family_hub.dart')
