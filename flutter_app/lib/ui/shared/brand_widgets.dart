part of '../app_root.dart';

class BrandLogo extends StatelessWidget {
  const BrandLogo({this.size = 96, super.key});

  final double size;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(size * 0.26),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.22),
            blurRadius: size * 0.22,
            offset: Offset(0, size * 0.08),
          ),
        ],
      ),
      clipBehavior: Clip.antiAlias,
      foregroundDecoration: BoxDecoration(
        borderRadius: BorderRadius.circular(size * 0.26),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(size * 0.26),
        child: Image.asset(
          '../img/app_logo.png',
          width: size,
          height: size,
          fit: BoxFit.cover,
          errorBuilder: (_, _, _) => ColoredBox(
            color: scheme.primary,
            child: Icon(
              Icons.grid_view_rounded,
              size: size * 0.5,
              color: scheme.onPrimary,
            ),
          ),
        ),
      ),
    );
  }
}

/// "KChess" wordmark with the leading K in the brand accent (K = King).
class BrandWordmark extends StatelessWidget {
  const BrandWordmark({this.fontSize = 34, super.key});

  final double fontSize;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Text.rich(
      TextSpan(
        style: TextStyle(
          fontSize: fontSize,
          fontWeight: FontWeight.w800,
          letterSpacing: -0.5,
          height: 1,
        ),
        children: [
          TextSpan(
            text: 'K',
            style: TextStyle(color: scheme.primary),
          ),
          TextSpan(
            text: 'Chess',
            style: TextStyle(color: scheme.onSurface),
          ),
        ],
      ),
    );
  }
}

