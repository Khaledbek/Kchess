import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';

// -----------------------------------------------------------------------------
// Section: PGN board navigation
// -----------------------------------------------------------------------------

class CoachBoardControls extends StatelessWidget {
  const CoachBoardControls({
    required this.onFirst,
    required this.onPrevious,
    required this.onNext,
    required this.onLast,
    super.key,
  });

  final VoidCallback? onFirst;
  final VoidCallback? onPrevious;
  final VoidCallback? onNext;
  final VoidCallback? onLast;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Material(
      elevation: 8,
      child: SizedBox(
        height: 64,
        width: double.infinity,
        child: Center(
          child: FittedBox(
            fit: BoxFit.scaleDown,
            child: SizedBox(
              width: 300,
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  IconButton(
                    key: const Key('coach-first'),
                    onPressed: onFirst,
                    tooltip: strings.first,
                    icon: const Icon(Icons.first_page, size: 28),
                  ),
                  IconButton(
                    key: const Key('coach-previous'),
                    onPressed: onPrevious,
                    tooltip: strings.previous,
                    icon: const Icon(Icons.chevron_left, size: 30),
                  ),
                  IconButton(
                    key: const Key('coach-next'),
                    onPressed: onNext,
                    tooltip: strings.next,
                    icon: const Icon(Icons.chevron_right, size: 30),
                  ),
                  IconButton(
                    key: const Key('coach-last'),
                    onPressed: onLast,
                    tooltip: strings.last,
                    icon: const Icon(Icons.last_page, size: 28),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
