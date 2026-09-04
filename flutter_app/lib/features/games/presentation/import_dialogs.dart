part of '../../../ui/app_root.dart';

class _PgnImportDialog extends StatefulWidget {
  const _PgnImportDialog();

  @override
  State<_PgnImportDialog> createState() => _PgnImportDialogState();
}

class _PgnImportDialogState extends State<_PgnImportDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return AlertDialog(
      title: Text(strings.pastePgn),
      content: SizedBox(
        width: 620,
        child: TextField(
          key: const Key('pgn-text'),
          controller: _controller,
          minLines: 10,
          maxLines: 18,
          autofocus: true,
          textDirection: TextDirection.ltr,
          decoration: InputDecoration(labelText: strings.pgnText),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(strings.close),
        ),
        FilledButton(
          onPressed: () {
            if (_controller.text.trim().isNotEmpty) {
              Navigator.pop(context, _controller.text);
            }
          },
          child: Text(strings.importAction),
        ),
      ],
    );
  }
}

class _FenImportDialog extends StatefulWidget {
  const _FenImportDialog();

  @override
  State<_FenImportDialog> createState() => _FenImportDialogState();
}

class _FenImportDialogState extends State<_FenImportDialog> {
  final _name = TextEditingController();
  final _fen = TextEditingController();

  @override
  void dispose() {
    _name.dispose();
    _fen.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return AlertDialog(
      title: Text(strings.importFen),
      content: SizedBox(
        width: 620,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              key: const Key('fen-name'),
              controller: _name,
              autofocus: true,
              decoration: InputDecoration(labelText: strings.positionName),
            ),
            const SizedBox(height: 16),
            TextField(
              key: const Key('fen-text'),
              controller: _fen,
              textDirection: TextDirection.ltr,
              decoration: InputDecoration(labelText: strings.fenText),
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(strings.close),
        ),
        FilledButton(
          onPressed: () {
            if (_name.text.trim().isNotEmpty && _fen.text.trim().isNotEmpty) {
              Navigator.pop(context, (
                fen: _fen.text.trim(),
                name: _name.text.trim(),
              ));
            }
          },
          child: Text(strings.importAction),
        ),
      ],
    );
  }
}

