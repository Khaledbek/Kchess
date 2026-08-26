part of '../../../ui/app_root.dart';

class _EmptySection extends StatelessWidget {
  const _EmptySection({required this.title, this.message});
  final String title;
  final String? message;

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: MediaQuery.sizeOf(context).width >= 900
        ? AppBar(title: Text(title))
        : null,
    body: Center(
      child: Text(message ?? AppLocalizations.of(context).emptySection),
    ),
  );
}

/// A rich game row: outcome accent, time-control badge, both players with
/// piece markers and rating pills, a result pill, accuracy and quick actions.
