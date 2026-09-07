import 'package:flutter/material.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'package:flutter_svg/flutter_svg.dart';
import 'package:flutter_test/flutter_test.dart';

/// Guards the two ways these icons silently disappear: the asset folder not
/// being registered in pubspec (loadString throws), and markup flutter_svg
/// cannot parse (the picture renders as nothing).
void main() {
  const icons = <String, String>{
    'assets/icons/king_won.svg': '#2E9E5B', // AppTheme.success
    'assets/icons/king_lost.svg': '#D2555F', // mid-tone between the two errors
    'assets/icons/king_draw.svg': '#6E7A90', // shared draw slate
  };

  testWidgets('game-over result icons are registered and render', (
    tester,
  ) async {
    for (final entry in icons.entries) {
      final markup = await rootBundle.loadString(entry.key);
      expect(markup, contains('<svg'), reason: '${entry.key} is not SVG');

      // Colours are the point of the refinement: fail if they drift back to a
      // bright off-the-shelf palette.
      expect(
        markup,
        contains(entry.value),
        reason: '${entry.key} should use the theme token ${entry.value}',
      );
      expect(
        markup.toUpperCase(),
        isNot(anyOf(contains('#10B981'), contains('#EF4444'), contains('#94A3B8'))),
        reason: '${entry.key} still uses a baseline Tailwind colour',
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Center(
            child: SizedBox.square(
              dimension: 64,
              child: SvgPicture.asset(entry.key),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(
        tester.takeException(),
        isNull,
        reason: '${entry.key} failed to parse or render',
      );
      expect(find.byType(SvgPicture), findsOneWidget);
    }
  });
}
