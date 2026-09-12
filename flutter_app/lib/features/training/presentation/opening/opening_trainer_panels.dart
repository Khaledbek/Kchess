// -----------------------------------------------------------------------------
// Section: Opening drill studio — palette, chrome and panels
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../../localization/generated/app_localizations.dart';

// -----------------------------------------------------------------------------
// Section: Palette
// -----------------------------------------------------------------------------

/// Dark "studio" palette for the opening scenarios and their drill.
///
/// The drill is a focus surface — one banner, the board and a depth meter — so
/// it keeps its own deep backdrop instead of following the app's surfaces.
class OpeningStudio {
  const OpeningStudio._();

  static const backdrop = Color(0xFF0B0E13);
  static const backdropGlow = Color(0xFF1B2C4A);
  static const sheet = Color(0xFF161A21);
  static const frame = Color(0xFF1A1F27);
  static const raised = Color(0xFF20262F);
  static const track = Color(0xFF262D38);
  static const hairline = Color(0x1FFFFFFF);
  static const textPrimary = Color(0xFFF1F4F9);
  static const textMuted = Color(0xFF8B95A6);

  /// The drill's voice: opponent moves, the current depth.
  static const accent = Color(0xFF5AB4F0);

  /// Stored mastery on the scenario cards.
  static const mastery = Color(0xFFE2B84A);

  static const danger = Color(0xFFE5484D);
  static const success = Color(0xFF3FD07E);
}

// -----------------------------------------------------------------------------
// Section: Backdrop
// -----------------------------------------------------------------------------

/// Deep backdrop with a soft ambient glow at the top.
class OpeningStudioBackdrop extends StatelessWidget {
  const OpeningStudioBackdrop({required this.child, super.key});

  final Widget child;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: const BoxDecoration(color: OpeningStudio.backdrop),
    child: Stack(
      children: [
        const Positioned(
          top: -140,
          left: -80,
          right: -80,
          height: 480,
          child: DecoratedBox(
            decoration: BoxDecoration(
              gradient: RadialGradient(
                colors: [OpeningStudio.backdropGlow, Color(0x000B0E13)],
                radius: 0.75,
              ),
            ),
          ),
        ),
        child,
      ],
    ),
  );
}

/// The studio's dark theme, for screens that sit on [OpeningStudioBackdrop].
ThemeData openingStudioTheme() => ThemeData.dark(useMaterial3: true).copyWith(
  scaffoldBackgroundColor: Colors.transparent,
  colorScheme: const ColorScheme.dark(
    primary: OpeningStudio.accent,
    surface: OpeningStudio.sheet,
    error: OpeningStudio.danger,
  ),
);

// -----------------------------------------------------------------------------
// Section: Segmented meter
// -----------------------------------------------------------------------------

/// A bar split into one segment per move, the reached part fused into a single
/// glowing run: the depth meter on the drill and on every scenario card.
class OpeningSegmentedMeter extends StatelessWidget {
  const OpeningSegmentedMeter({
    required this.done,
    required this.total,
    required this.color,
    this.height = 7,
    super.key,
  });

  final int done, total;
  final Color color;
  final double height;

  static const _gap = 4.0;

  @override
  Widget build(BuildContext context) {
    final segments = total < 1 ? 1 : total;
    final reached = done.clamp(0, segments);
    return LayoutBuilder(
      builder: (context, constraints) {
        final segment = (constraints.maxWidth - _gap * (segments - 1)) / segments;
        return SizedBox(
          height: height,
          child: Stack(
            children: [
              for (var index = reached; index < segments; index++)
                Positioned(
                  left: index * (segment + _gap),
                  width: segment,
                  top: 0,
                  bottom: 0,
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: OpeningStudio.track,
                      borderRadius: BorderRadius.circular(height / 2),
                    ),
                  ),
                ),
              if (reached > 0)
                AnimatedPositioned(
                  duration: const Duration(milliseconds: 280),
                  curve: Curves.easeOutCubic,
                  left: 0,
                  width: reached * segment + (reached - 1) * _gap,
                  top: 0,
                  bottom: 0,
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: color,
                      borderRadius: BorderRadius.circular(height / 2),
                      boxShadow: [
                        BoxShadow(
                          color: color.withValues(alpha: 0.5),
                          blurRadius: 10,
                        ),
                      ],
                    ),
                  ),
                ),
            ],
          ),
        );
      },
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Board chrome
// -----------------------------------------------------------------------------

/// Rounded frame the board sits in, with its own drop shadow.
class OpeningBoardFrame extends StatelessWidget {
  const OpeningBoardFrame({
    required this.side,
    required this.child,
    this.accent,
    super.key,
  });

  static const _padding = 8.0;

  final double side;
  final Widget child;

  /// Colour the frame glows in for a beat, e.g. green for a correct answer.
  final Color? accent;

  @override
  Widget build(BuildContext context) => AnimatedContainer(
    key: const Key('opening-trainer-board-frame'),
    duration: const Duration(milliseconds: 220),
    width: side,
    height: side,
    padding: const EdgeInsets.all(_padding),
    decoration: BoxDecoration(
      color: OpeningStudio.frame,
      borderRadius: BorderRadius.circular(22),
      border: Border.all(
        color: accent ?? OpeningStudio.hairline,
        width: accent == null ? 1 : 2,
      ),
      boxShadow: [
        const BoxShadow(
          color: Color(0x9E000000),
          blurRadius: 34,
          offset: Offset(0, 16),
        ),
        if (accent != null)
          BoxShadow(
            color: accent!.withValues(alpha: 0.45),
            blurRadius: 30,
            spreadRadius: -6,
          ),
      ],
    ),
    child: ClipRRect(borderRadius: BorderRadius.circular(14), child: child),
  );
}

/// Neon halo around board squares.
///
/// [squares] maps a board square (`e4`) to the colour of its glow.
class OpeningSquareGlow extends StatelessWidget {
  const OpeningSquareGlow({
    required this.squares,
    required this.blackAtBottom,
    super.key,
  });

  final Map<String, Color> squares;
  final bool blackAtBottom;

  @override
  Widget build(BuildContext context) => IgnorePointer(
    child: RepaintBoundary(
      child: CustomPaint(painter: _GlowPainter(squares, blackAtBottom)),
    ),
  );
}

class _GlowPainter extends CustomPainter {
  const _GlowPainter(this.squares, this.blackAtBottom);

  final Map<String, Color> squares;
  final bool blackAtBottom;

  @override
  void paint(Canvas canvas, Size size) {
    final side = size.width / 8;
    for (final entry in squares.entries) {
      final square = entry.key;
      if (square.length < 2) continue;
      final file = square.codeUnitAt(0) - 0x61;
      final rank = square.codeUnitAt(1) - 0x30;
      if (file < 0 || file > 7 || rank < 1 || rank > 8) continue;
      final column = blackAtBottom ? 7 - file : file;
      final row = blackAtBottom ? rank - 1 : 8 - rank;
      final rect = Rect.fromLTWH(column * side, row * side, side, side)
          .deflate(side * 0.06);
      final shape = RRect.fromRectAndRadius(rect, Radius.circular(side * 0.22));
      canvas
        ..drawRRect(
          shape,
          Paint()
            ..color = entry.value.withValues(alpha: 0.55)
            ..maskFilter = MaskFilter.blur(BlurStyle.normal, side * 0.28)
            ..style = PaintingStyle.stroke
            ..strokeWidth = side * 0.14,
        )
        ..drawRRect(
          shape,
          Paint()
            ..color = entry.value.withValues(alpha: 0.85)
            ..style = PaintingStyle.stroke
            ..strokeWidth = side * 0.055,
        );
    }
  }

  @override
  bool shouldRepaint(covariant _GlowPainter oldDelegate) =>
      oldDelegate.blackAtBottom != blackAtBottom ||
      !mapEquals(oldDelegate.squares, squares);

  static bool mapEquals(Map<String, Color> a, Map<String, Color> b) {
    if (a.length != b.length) return false;
    for (final entry in a.entries) {
      if (b[entry.key] != entry.value) return false;
    }
    return true;
  }
}

// -----------------------------------------------------------------------------
// Section: Drill banner
// -----------------------------------------------------------------------------

/// What the banner above the board is saying.
enum OpeningBannerTone { prompt, miss, done }

/// Glass banner above the board: the move just played and what to do about it.
///
/// [headline] carries the move itself; it is split around [highlight] so the
/// move — and only the move — lights up in the banner's colour, whatever the
/// language puts around it.
class OpeningDrillBanner extends StatelessWidget {
  const OpeningDrillBanner({
    required this.tone,
    required this.headline,
    required this.instruction,
    this.highlight,
    this.footnote,
    super.key,
  });

  final OpeningBannerTone tone;
  final String headline;
  final String? highlight;
  final String instruction;

  /// A quiet third line, e.g. the verdict on the previous answer.
  final String? footnote;

  Color get _color => switch (tone) {
    OpeningBannerTone.prompt => OpeningStudio.accent,
    OpeningBannerTone.miss => OpeningStudio.danger,
    OpeningBannerTone.done => OpeningStudio.success,
  };

  @override
  Widget build(BuildContext context) {
    final color = _color;
    const base = TextStyle(
      fontSize: 17,
      height: 1.3,
      fontWeight: FontWeight.w700,
      letterSpacing: 0.2,
      color: OpeningStudio.textPrimary,
    );
    return AnimatedContainer(
      key: const Key('opening-trainer-banner'),
      duration: const Duration(milliseconds: 240),
      width: double.infinity,
      padding: const EdgeInsets.fromLTRB(20, 16, 20, 16),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [
            Color.alphaBlend(color.withValues(alpha: 0.10), OpeningStudio.raised),
            OpeningStudio.sheet,
          ],
        ),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withValues(alpha: 0.45)),
        boxShadow: [
          BoxShadow(
            color: color.withValues(alpha: 0.22),
            blurRadius: 28,
            spreadRadius: -6,
            offset: const Offset(0, 10),
          ),
        ],
      ),
      child: Column(
        children: [
          Text.rich(
            key: const Key('opening-trainer-banner-headline'),
            _highlighted(headline, highlight, base, base.copyWith(color: color)),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 4),
          Text(
            instruction,
            key: const Key('opening-trainer-banner-instruction'),
            textAlign: TextAlign.center,
            style: base.copyWith(
              fontSize: 14.5,
              fontWeight: FontWeight.w600,
              color: OpeningStudio.textMuted,
            ),
          ),
          if (footnote != null) ...[
            const SizedBox(height: 8),
            Text(
              footnote!,
              key: const Key('opening-trainer-banner-footnote'),
              textAlign: TextAlign.center,
              style: const TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
                color: OpeningStudio.success,
              ),
            ),
          ],
        ],
      ),
    );
  }

  static TextSpan _highlighted(
    String text,
    String? highlight,
    TextStyle style,
    TextStyle accent,
  ) {
    final at = highlight == null || highlight.isEmpty ? -1 : text.indexOf(highlight);
    if (at < 0) return TextSpan(text: text, style: style);
    return TextSpan(
      style: style,
      children: [
        TextSpan(text: text.substring(0, at)),
        // Notation reads left to right even inside an Arabic sentence.
        TextSpan(text: '\u2066$highlight\u2069', style: accent),
        TextSpan(text: text.substring(at + highlight!.length)),
      ],
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Depth meter
// -----------------------------------------------------------------------------

/// "Current depth: move 2 / 10" over the segmented bar, plus the streak.
class OpeningDepthMeter extends StatelessWidget {
  const OpeningDepthMeter({
    required this.depth,
    required this.targetDepth,
    required this.finished,
    this.streak = 0,
    super.key,
  });

  /// Answers given so far.
  final int depth;
  final int targetDepth;

  /// Once the run is over the meter reports what was reached, not what is next.
  final bool finished;
  final int streak;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final total = targetDepth < 1 ? 1 : targetDepth;
    final current = finished ? depth.clamp(0, total) : (depth + 1).clamp(1, total);
    return Container(
      key: const Key('opening-trainer-progress'),
      padding: const EdgeInsets.fromLTRB(16, 12, 12, 14),
      decoration: BoxDecoration(
        color: OpeningStudio.sheet,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: OpeningStudio.hairline),
      ),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  strings.trainingOpeningCurrentDepth(current, total).toUpperCase(),
                  key: const Key('opening-trainer-depth-label'),
                  style: const TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 0.8,
                    color: OpeningStudio.textMuted,
                  ),
                ),
                const SizedBox(height: 9),
                OpeningSegmentedMeter(
                  done: depth,
                  total: total,
                  color: OpeningStudio.accent,
                ),
              ],
            ),
          ),
          if (streak > 1) ...[
            const SizedBox(width: 12),
            OpeningStreakChip(streak: streak),
          ],
        ],
      ),
    );
  }
}

/// Run of answers found without a miss, shown once it is worth bragging about.
class OpeningStreakChip extends StatelessWidget {
  const OpeningStreakChip({required this.streak, super.key});

  final int streak;

  @override
  Widget build(BuildContext context) => Container(
    key: const Key('opening-trainer-streak'),
    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
    decoration: BoxDecoration(
      color: OpeningStudio.success.withValues(alpha: 0.16),
      borderRadius: BorderRadius.circular(20),
      border: Border.all(color: OpeningStudio.success.withValues(alpha: 0.5)),
    ),
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        const Icon(
          Icons.local_fire_department_rounded,
          size: 15,
          color: OpeningStudio.success,
        ),
        const SizedBox(width: 5),
        Text(
          '$streak',
          textDirection: TextDirection.ltr,
          style: const TextStyle(
            fontSize: 13,
            fontWeight: FontWeight.w800,
            color: OpeningStudio.success,
          ),
        ),
      ],
    ),
  );
}

// -----------------------------------------------------------------------------
// Section: Result
// -----------------------------------------------------------------------------

/// End-of-run card: whether the run counts towards mastery, and another go.
/// How deep it went and why it stopped are the banner's job.
class OpeningDrillResultCard extends StatelessWidget {
  const OpeningDrillResultCard({
    required this.depth,
    required this.clean,
    required this.bookExhausted,
    required this.onDrillAgain,
    super.key,
  });

  final int depth;
  final bool clean;
  final bool bookExhausted;
  final VoidCallback onDrillAgain;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    // A scenario the book does not cover asked nothing, so there is no verdict.
    final nothingAsked = bookExhausted && depth == 0;
    final color = clean ? OpeningStudio.success : OpeningStudio.mastery;
    return Container(
      key: const Key('opening-trainer-completed'),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: OpeningStudio.sheet,
        borderRadius: BorderRadius.circular(18),
        border: Border.all(
          color: nothingAsked ? OpeningStudio.hairline : color.withValues(alpha: 0.5),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!nothingAsked)
            Row(
              children: [
                Icon(
                  clean ? Icons.emoji_events_rounded : Icons.flag_rounded,
                  color: color,
                  size: 24,
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Text(
                    clean
                        ? strings.trainingOpeningDrillClean
                        : strings.trainingOpeningDrillWithErrors,
                    style: const TextStyle(
                      fontSize: 14,
                      height: 1.35,
                      fontWeight: FontWeight.w600,
                      color: OpeningStudio.textPrimary,
                    ),
                  ),
                ),
              ],
            ),
          if (!nothingAsked) const SizedBox(height: 14),
          SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              key: const Key('opening-trainer-practise-again'),
              style: FilledButton.styleFrom(
                backgroundColor: OpeningStudio.accent,
                foregroundColor: OpeningStudio.backdrop,
                padding: const EdgeInsets.symmetric(vertical: 13),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
                textStyle: const TextStyle(
                  fontWeight: FontWeight.w800,
                  fontSize: 15,
                ),
              ),
              onPressed: onDrillAgain,
              icon: const Icon(Icons.replay_rounded, size: 18),
              label: Text(strings.trainingOpeningDrillAgain),
            ),
          ),
        ],
      ),
    );
  }
}
