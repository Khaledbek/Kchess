// -----------------------------------------------------------------------------
// Section: Opening scenarios studio — palette, backdrop and the depth bar
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';


// -----------------------------------------------------------------------------
// Section: Palette
// -----------------------------------------------------------------------------

/// Dark "studio" palette for the opening scenarios dashboard.
///
/// The drill itself follows the app theme like every other training board.
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
    // Tonal buttons and chips; the dark scheme's default is an off-palette teal.
    secondaryContainer: Color(0xFF23364F),
    onSecondaryContainer: OpeningStudio.textPrimary,
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
    this.trackColor = OpeningStudio.track,
    this.glow = true,
    super.key,
  });

  final int done, total;
  final Color color;
  final double height;

  /// Colour of the segments not reached yet.
  final Color trackColor;

  /// A soft halo around the reached run; off on the plain app-themed drill.
  final bool glow;

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
                      color: trackColor,
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
                      boxShadow: glow
                          ? [BoxShadow(color: color.withValues(alpha: 0.5), blurRadius: 10)]
                          : null,
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
