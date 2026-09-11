// -----------------------------------------------------------------------------
// Section: Visual projection of native engine evaluations
// -----------------------------------------------------------------------------
import 'package:flutter/material.dart';

import '../../localization/generated/app_localizations.dart';
import '../models/models.dart';

class EvaluationBar extends StatelessWidget {
  const EvaluationBar({required this.line, super.key});

  final EngineLine? line;

  ({double whiteShare, String label}) _value() {
    final cp = line?.evaluationCp;
    final mate = line?.mateIn;

    if (mate != null && mate != 0) {
      return (
        whiteShare: mate > 0 ? 1.0 : 0.0,
        label: mate > 0 ? 'M$mate' : '-M${mate.abs()}',
      );
    }

    if (cp == null) {
      return (whiteShare: 0.5, label: '0.0');
    }

    final whiteShare = (0.5 + cp.clamp(-1000, 1000) / 2000)
        .clamp(0.0, 1.0)
        .toDouble();
    final pawns = cp / 100.0;
    final label = pawns.abs() < 0.05
        ? '0.0'
        : '${pawns > 0 ? '+' : ''}${pawns.toStringAsFixed(1)}';
    return (whiteShare: whiteShare, label: label);
  }

  @override
  Widget build(BuildContext context) {
    final value = _value();
    final scheme = Theme.of(context).colorScheme;
    return Semantics(
      label: '${AppLocalizations.of(context).evaluation}: ${value.label}',
      child: ClipRRect(
        key: const Key('analysis-evaluation-bar'),
        borderRadius: BorderRadius.circular(7),
        child: LayoutBuilder(
          builder: (context, constraints) => TweenAnimationBuilder<double>(
            duration: const Duration(milliseconds: 220),
            curve: Curves.easeOutCubic,
            tween: Tween<double>(begin: 0.5, end: value.whiteShare),
            builder: (context, whiteShare, _) {
              final split = constraints.maxWidth * whiteShare;
              return Stack(
                fit: StackFit.expand,
                children: [
                  const ColoredBox(color: Color(0xFF232624)),
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: SizedBox(
                      width: split,
                      height: double.infinity,
                      child: const ColoredBox(color: Color(0xFFF2EFE7)),
                    ),
                  ),
                  Align(
                    alignment: Alignment.center,
                    child: Container(
                      width: 1,
                      color: scheme.outline.withValues(alpha: 0.75),
                    ),
                  ),
                  Center(
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: scheme.surface.withValues(alpha: 0.88),
                        borderRadius: BorderRadius.circular(5),
                        border: Border.all(
                          color: scheme.outlineVariant.withValues(alpha: 0.8),
                        ),
                      ),
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 7),
                        child: Text(
                          value.label,
                          textDirection: TextDirection.ltr,
                          style: Theme.of(context).textTheme.labelMedium
                              ?.copyWith(
                                fontWeight: FontWeight.w800,
                                height: 1.25,
                              ),
                        ),
                      ),
                    ),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}
