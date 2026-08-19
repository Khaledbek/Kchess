import 'package:flutter/foundation.dart' show defaultTargetPlatform;
import 'package:flutter/material.dart';

/// Central design system for KChess.
///
/// The look is a calm, modern "analysis studio": crisp neutral surfaces, a
/// single confident blue accent that matches the move badges, hairline-bordered
/// cards instead of heavy Material elevation, and softly rounded controls.
class AppTheme {
  const AppTheme._();

  /// Brand blue — shared with the "Critical" move badge and the logo concept.
  static const brand = Color(0xFF2C5CE0);

  /// Semantic colours reused across screens (game results, win-rates …).
  static const success = Color(0xFF2E9E5B);
  static const warning = Color(0xFFE0A43B);

  static ThemeData light() => _build(Brightness.light);
  static ThemeData dark() => _build(Brightness.dark);

  /// Result colour for a provider outcome string (`win` / `loss` / other).
  static Color outcomeColor(BuildContext context, String outcome) =>
      switch (outcome) {
        'win' => success,
        'loss' => Theme.of(context).colorScheme.error,
        _ => Theme.of(context).colorScheme.onSurfaceVariant,
      };

  /// Ambient background gradient used on immersive screens (splash, onboarding).
  static List<Color> ambientGradient(Brightness brightness) =>
      brightness == Brightness.dark
      ? const [Color(0xFF141A2B), Color(0xFF0E1116)]
      : const [Color(0xFFEEF2FE), Color(0xFFF7F8FB)];

  static ColorScheme _scheme(Brightness brightness) {
    final base = ColorScheme.fromSeed(seedColor: brand, brightness: brightness);
    if (brightness == Brightness.light) {
      return base.copyWith(
        primary: brand,
        onPrimary: Colors.white,
        primaryContainer: const Color(0xFFDDE5FF),
        onPrimaryContainer: const Color(0xFF08205C),
        secondary: const Color(0xFF515A68),
        onSecondary: Colors.white,
        secondaryContainer: const Color(0xFFE6EAF2),
        onSecondaryContainer: const Color(0xFF1B2430),
        tertiary: const Color(0xFF9A6B12),
        tertiaryContainer: const Color(0xFFFCE7C2),
        onTertiaryContainer: const Color(0xFF3E2A00),
        surface: const Color(0xFFF7F8FB),
        onSurface: const Color(0xFF191C22),
        onSurfaceVariant: const Color(0xFF565D6B),
        surfaceContainerLowest: Colors.white,
        surfaceContainerLow: const Color(0xFFF2F4F9),
        surfaceContainer: const Color(0xFFEDF0F6),
        surfaceContainerHigh: const Color(0xFFE7EBF2),
        surfaceContainerHighest: const Color(0xFFE0E5EE),
        outline: const Color(0xFFBFC5D0),
        outlineVariant: const Color(0xFFE1E5EE),
        error: const Color(0xFFC9414C),
      );
    }
    return base.copyWith(
      primary: const Color(0xFF7CA2FF),
      onPrimary: const Color(0xFF05132F),
      primaryContainer: const Color(0xFF23407F),
      onPrimaryContainer: const Color(0xFFD9E2FF),
      secondary: const Color(0xFFB4BDCC),
      onSecondary: const Color(0xFF141A24),
      secondaryContainer: const Color(0xFF272E39),
      onSecondaryContainer: const Color(0xFFDCE2EC),
      tertiary: const Color(0xFFE5C07B),
      tertiaryContainer: const Color(0xFF4A3A18),
      onTertiaryContainer: const Color(0xFFFBE7C4),
      surface: const Color(0xFF0F1216),
      onSurface: const Color(0xFFE6EAF1),
      onSurfaceVariant: const Color(0xFF9BA3B1),
      surfaceContainerLowest: const Color(0xFF0A0D11),
      surfaceContainerLow: const Color(0xFF14181F),
      surfaceContainer: const Color(0xFF171C23),
      surfaceContainerHigh: const Color(0xFF1D232B),
      surfaceContainerHighest: const Color(0xFF242B35),
      outline: const Color(0xFF3B434F),
      outlineVariant: const Color(0xFF2A313B),
      error: const Color(0xFFEB6B72),
    );
  }

  static TextTheme _text(ColorScheme scheme) {
    final typography = Typography.material2021(platform: defaultTargetPlatform);
    final base = scheme.brightness == Brightness.dark
        ? typography.white
        : typography.black;
    return base
        .copyWith(
          displaySmall: base.displaySmall?.copyWith(
            fontWeight: FontWeight.w700,
            letterSpacing: -0.5,
          ),
          headlineMedium: base.headlineMedium?.copyWith(
            fontWeight: FontWeight.w700,
            letterSpacing: -0.5,
          ),
          headlineSmall: base.headlineSmall?.copyWith(
            fontWeight: FontWeight.w700,
            letterSpacing: -0.3,
          ),
          titleLarge: base.titleLarge?.copyWith(
            fontWeight: FontWeight.w700,
            letterSpacing: -0.2,
          ),
          titleMedium: base.titleMedium?.copyWith(fontWeight: FontWeight.w600),
          labelLarge: base.labelLarge?.copyWith(
            fontWeight: FontWeight.w600,
            letterSpacing: 0.1,
          ),
        )
        .apply(bodyColor: scheme.onSurface, displayColor: scheme.onSurface);
  }

  static ThemeData _build(Brightness brightness) {
    final scheme = _scheme(brightness);
    final text = _text(scheme);
    final isDark = brightness == Brightness.dark;

    OutlineInputBorder inputBorder(Color color, [double width = 1]) =>
        OutlineInputBorder(
          borderRadius: BorderRadius.circular(14),
          borderSide: BorderSide(color: color, width: width),
        );

    return ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
      textTheme: text,
      scaffoldBackgroundColor: scheme.surface,
      splashFactory: InkSparkle.splashFactory,
      visualDensity: VisualDensity.standard,
      dividerColor: scheme.outlineVariant,

      appBarTheme: AppBarTheme(
        backgroundColor: scheme.surface,
        foregroundColor: scheme.onSurface,
        surfaceTintColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0.5,
        centerTitle: false,
        titleTextStyle: text.titleLarge,
      ),

      cardTheme: CardThemeData(
        elevation: 0,
        margin: EdgeInsets.zero,
        clipBehavior: Clip.antiAlias,
        color: isDark ? scheme.surfaceContainer : scheme.surfaceContainerLowest,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(18),
          side: BorderSide(color: scheme.outlineVariant),
        ),
      ),

      dividerTheme: DividerThemeData(
        color: scheme.outlineVariant,
        thickness: 1,
        space: 1,
      ),

      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: isDark ? scheme.surfaceContainerHigh : scheme.surfaceContainerLow,
        isDense: true,
        contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        border: inputBorder(Colors.transparent),
        enabledBorder: inputBorder(Colors.transparent),
        focusedBorder: inputBorder(scheme.primary, 1.6),
        errorBorder: inputBorder(scheme.error),
        focusedErrorBorder: inputBorder(scheme.error, 1.6),
        prefixIconColor: scheme.onSurfaceVariant,
        labelStyle: TextStyle(color: scheme.onSurfaceVariant),
        hintStyle: TextStyle(color: scheme.onSurfaceVariant),
      ),

      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 22, vertical: 15),
          textStyle: text.labelLarge,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(13),
          ),
        ),
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 22, vertical: 15),
          textStyle: text.labelLarge,
          backgroundColor: scheme.surfaceContainerHighest,
          foregroundColor: scheme.onSurface,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(13),
          ),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 15),
          textStyle: text.labelLarge,
          side: BorderSide(color: scheme.outline),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(13),
          ),
        ),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
          textStyle: text.labelLarge,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(11),
          ),
        ),
      ),

      segmentedButtonTheme: SegmentedButtonThemeData(
        style: ButtonStyle(
          textStyle: WidgetStatePropertyAll(text.labelLarge),
          shape: WidgetStatePropertyAll(
            RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
        ),
      ),

      chipTheme: ChipThemeData(
        backgroundColor: scheme.surfaceContainerHigh,
        selectedColor: scheme.primaryContainer,
        side: BorderSide(color: scheme.outlineVariant),
        labelStyle: text.labelLarge?.copyWith(color: scheme.onSurface),
        secondaryLabelStyle: text.labelLarge,
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(10),
        ),
      ),

      navigationRailTheme: NavigationRailThemeData(
        backgroundColor: scheme.surface,
        indicatorColor: scheme.primaryContainer,
        indicatorShape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        selectedIconTheme: IconThemeData(color: scheme.onPrimaryContainer),
        unselectedIconTheme: IconThemeData(color: scheme.onSurfaceVariant),
        selectedLabelTextStyle: text.labelLarge?.copyWith(
          color: scheme.onSurface,
        ),
        unselectedLabelTextStyle: text.labelLarge?.copyWith(
          color: scheme.onSurfaceVariant,
          fontWeight: FontWeight.w500,
        ),
        useIndicator: true,
      ),

      drawerTheme: DrawerThemeData(
        backgroundColor: scheme.surface,
        surfaceTintColor: Colors.transparent,
        elevation: 1,
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.horizontal(right: Radius.circular(24)),
        ),
      ),

      listTileTheme: ListTileThemeData(
        iconColor: scheme.onSurfaceVariant,
        selectedColor: scheme.onPrimaryContainer,
        selectedTileColor: scheme.primaryContainer.withValues(alpha: 0.55),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      ),

      dialogTheme: DialogThemeData(
        backgroundColor: scheme.surfaceContainerLow,
        surfaceTintColor: Colors.transparent,
        elevation: 6,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(22),
        ),
        titleTextStyle: text.titleLarge,
      ),

      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: scheme.surfaceContainerLow,
        surfaceTintColor: Colors.transparent,
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
        ),
      ),

      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        backgroundColor: scheme.inverseSurface,
        contentTextStyle: text.bodyMedium?.copyWith(color: scheme.onInverseSurface),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
      ),

      floatingActionButtonTheme: FloatingActionButtonThemeData(
        elevation: 2,
        highlightElevation: 4,
        backgroundColor: scheme.primary,
        foregroundColor: scheme.onPrimary,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
        ),
      ),

      menuTheme: MenuThemeData(
        style: MenuStyle(
          surfaceTintColor: const WidgetStatePropertyAll(Colors.transparent),
          backgroundColor: WidgetStatePropertyAll(scheme.surfaceContainerLow),
          shape: WidgetStatePropertyAll(
            RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(14),
              side: BorderSide(color: scheme.outlineVariant),
            ),
          ),
        ),
      ),
      dropdownMenuTheme: DropdownMenuThemeData(
        menuStyle: MenuStyle(
          surfaceTintColor: const WidgetStatePropertyAll(Colors.transparent),
          backgroundColor: WidgetStatePropertyAll(scheme.surfaceContainerLow),
          shape: WidgetStatePropertyAll(
            RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(14),
              side: BorderSide(color: scheme.outlineVariant),
            ),
          ),
        ),
      ),
      popupMenuTheme: PopupMenuThemeData(
        surfaceTintColor: Colors.transparent,
        color: scheme.surfaceContainerLow,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(14),
          side: BorderSide(color: scheme.outlineVariant),
        ),
      ),

      progressIndicatorTheme: ProgressIndicatorThemeData(
        color: scheme.primary,
        linearTrackColor: scheme.surfaceContainerHigh,
        circularTrackColor: scheme.surfaceContainerHigh,
      ),

      tooltipTheme: TooltipThemeData(
        decoration: BoxDecoration(
          color: scheme.inverseSurface,
          borderRadius: BorderRadius.circular(8),
        ),
        textStyle: text.bodySmall?.copyWith(color: scheme.onInverseSurface),
      ),

      bannerTheme: MaterialBannerThemeData(
        backgroundColor: scheme.surfaceContainerHigh,
        contentTextStyle: text.bodyMedium,
        elevation: 0,
      ),
    );
  }
}
