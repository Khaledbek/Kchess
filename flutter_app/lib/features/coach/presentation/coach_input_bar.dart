import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../models/coach_ui_models.dart';

// -----------------------------------------------------------------------------
// Coach input bar
// -----------------------------------------------------------------------------

class CoachInputBar extends StatefulWidget {
  const CoachInputBar({
    required this.depth,
    required this.isLoading,
    required this.onSubmit,
    required this.onDepthChanged,
    this.onHintRequested,
    this.onFenRequested,
    this.onPgnRequested,
    this.hintEnabled = true,
    this.hintBusy = false,
    super.key,
  });

  final CoachResponseDepth depth;
  final bool isLoading;
  final ValueChanged<String>? onSubmit;
  final ValueChanged<CoachResponseDepth>? onDepthChanged;
  final VoidCallback? onHintRequested;
  final VoidCallback? onFenRequested;
  final VoidCallback? onPgnRequested;
  final bool hintEnabled;
  final bool hintBusy;

  @override
  State<CoachInputBar> createState() => _CoachInputBarState();
}

class _CoachInputBarState extends State<CoachInputBar> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    final text = _controller.text.trim();
    if (text.isEmpty || widget.isLoading || widget.onSubmit == null) return;
    widget.onSubmit!(text);
    _controller.clear();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;

    return Material(
      color: scheme.surface,
      child: SafeArea(
        top: false,
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                children: [
                  OutlinedButton.icon(
                    key: const Key('coach-hint'),
                    onPressed: widget.isLoading ||
                            widget.hintBusy ||
                            !widget.hintEnabled
                        ? null
                        : widget.onHintRequested,
                    icon: widget.hintBusy
                        ? const SizedBox.square(
                            dimension: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.lightbulb_outline_rounded),
                    label: Text(strings.coachHint),
                  ),
                  const SizedBox(width: 8),
                  DropdownButton<CoachResponseDepth>(
                    key: const Key('coach-depth'),
                    value: widget.depth,
                    underline: const SizedBox.shrink(),
                    borderRadius: BorderRadius.circular(12),
                    items: CoachResponseDepth.values
                        .map(
                          (depth) => DropdownMenuItem(
                            value: depth,
                            child: Text(_depthLabel(strings, depth)),
                          ),
                        )
                        .toList(growable: false),
                    onChanged: widget.isLoading || widget.onDepthChanged == null
                        ? null
                        : (value) {
                            if (value != null) widget.onDepthChanged!(value);
                          },
                  ),
                  const Spacer(),
                  PopupMenuButton<_CoachContextAction>(
                    key: const Key('coach-context-menu'),
                    tooltip: strings.coachMore,
                    icon: const Icon(Icons.more_vert_rounded),
                    enabled: !widget.isLoading,
                    onSelected: (action) {
                      if (action == _CoachContextAction.fen) {
                        widget.onFenRequested?.call();
                      } else {
                        widget.onPgnRequested?.call();
                      }
                    },
                    itemBuilder: (context) => [
                      PopupMenuItem(
                        value: _CoachContextAction.fen,
                        child: ListTile(
                          dense: true,
                          leading: const Icon(Icons.grid_on_outlined),
                          title: Text(strings.coachFen),
                        ),
                      ),
                      PopupMenuItem(
                        value: _CoachContextAction.pgn,
                        child: ListTile(
                          dense: true,
                          leading: const Icon(Icons.article_outlined),
                          title: Text(strings.coachPgn),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 10),
              Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: [
                  Expanded(
                    child: TextField(
                      key: const Key('coach-input'),
                      controller: _controller,
                      enabled: !widget.isLoading,
                      minLines: 1,
                      maxLines: 4,
                      textInputAction: TextInputAction.send,
                      onSubmitted: (_) => _submit(),
                      onChanged: (_) => setState(() {}),
                      decoration: InputDecoration(
                        hintText: strings.coachInputHint,
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  IconButton.filled(
                    key: const Key('coach-send'),
                    onPressed: _controller.text.trim().isEmpty ||
                            widget.isLoading ||
                            widget.onSubmit == null
                        ? null
                        : _submit,
                    tooltip: strings.coachSend,
                    icon: const Icon(Icons.arrow_upward_rounded),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  String _depthLabel(AppLocalizations strings, CoachResponseDepth depth) =>
      switch (depth) {
        CoachResponseDepth.concise => strings.coachDepthConcise,
        CoachResponseDepth.balanced => strings.coachDepthBalanced,
        CoachResponseDepth.deep => strings.coachDepthDeep,
      };
}

enum _CoachContextAction { fen, pgn }
