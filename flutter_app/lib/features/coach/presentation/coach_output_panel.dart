import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../models/coach_ui_models.dart';

// -----------------------------------------------------------------------------
// Coach output panel
// -----------------------------------------------------------------------------

class CoachOutputPanel extends StatefulWidget {
  const CoachOutputPanel({
    required this.messages,
    required this.isLoading,
    this.errorMessage,
    this.onShowBoard,
    super.key,
  });

  final List<CoachUiMessage> messages;
  final bool isLoading;
  final String? errorMessage;
  final ValueChanged<CoachUiMessage>? onShowBoard;

  @override
  State<CoachOutputPanel> createState() => _CoachOutputPanelState();
}

class _CoachOutputPanelState extends State<CoachOutputPanel> {
  final ScrollController _scrollController = ScrollController();

  @override
  void initState() {
    super.initState();
    _scheduleScrollToLatest();
  }

  @override
  void didUpdateWidget(CoachOutputPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.messages.length != widget.messages.length ||
        oldWidget.isLoading != widget.isLoading ||
        oldWidget.errorMessage != widget.errorMessage) {
      _scheduleScrollToLatest();
    }
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  void _scheduleScrollToLatest() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted || !_scrollController.hasClients) return;
      _scrollController.animateTo(
        _scrollController.position.maxScrollExtent,
        duration: const Duration(milliseconds: 180),
        curve: Curves.easeOut,
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final hasError = widget.errorMessage?.trim().isNotEmpty ?? false;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 14, 16, 10),
          child: Text(
            strings.coachTrainerOutput,
            style: theme.textTheme.titleMedium,
          ),
        ),
        const Divider(height: 1),
        Expanded(
          child: widget.messages.isEmpty && !widget.isLoading && !hasError
              ? _EmptyCoachOutput(text: strings.coachWelcome)
              : ListView.builder(
                  controller: _scrollController,
                  key: const Key('coach-message-list'),
                  padding: const EdgeInsets.all(16),
                  itemCount:
                      widget.messages.length +
                      (widget.isLoading ? 1 : 0) +
                      (hasError ? 1 : 0),
                  itemBuilder: (context, index) {
                    if (index < widget.messages.length) {
                      final message = widget.messages[index];
                      return _CoachMessageSection(
                        message: message,
                        onShowBoard:
                            !widget.isLoading &&
                                (message.positionFen?.isNotEmpty ?? false)
                            ? widget.onShowBoard
                            : null,
                      );
                    }
                    final extraIndex = index - widget.messages.length;
                    if (widget.isLoading && extraIndex == 0) {
                      return _CoachThinking(text: strings.coachThinking);
                    }
                    return _CoachError(message: widget.errorMessage!.trim());
                  },
                ),
        ),
      ],
    );
  }
}

// -----------------------------------------------------------------------------
// Output states
// -----------------------------------------------------------------------------

class _EmptyCoachOutput extends StatelessWidget {
  const _EmptyCoachOutput({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(28),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.school_outlined,
              size: 36,
              color: theme.colorScheme.primary,
            ),
            const SizedBox(height: 12),
            Text(
              text,
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _CoachThinking extends StatelessWidget {
  const _CoachThinking({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.only(top: 4, bottom: 12),
    child: Row(
      children: [
        const SizedBox.square(
          dimension: 18,
          child: CircularProgressIndicator(strokeWidth: 2.2),
        ),
        const SizedBox(width: 10),
        Text(text),
      ],
    ),
  );
}

class _CoachError extends StatelessWidget {
  const _CoachError({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      margin: const EdgeInsets.only(top: 4, bottom: 12),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: scheme.errorContainer,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.error_outline_rounded, color: scheme.onErrorContainer),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              message,
              style: TextStyle(color: scheme.onErrorContainer),
            ),
          ),
        ],
      ),
    );
  }
}

class _CoachMessageSection extends StatelessWidget {
  const _CoachMessageSection({required this.message, this.onShowBoard});

  final CoachUiMessage message;
  final ValueChanged<CoachUiMessage>? onShowBoard;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final user = message.role == CoachMessageRole.user;
    final strings = AppLocalizations.of(context);
    final Color? eventColor = user
        ? null
        : switch (message.eventKind) {
            'blunder' => const Color(0xffef5350),
            'best' => const Color(0xff43a047),
            'brilliant' => const Color(0xff42a5f5),
            _ => null,
          };
    final eventLabel = switch (message.eventKind) {
      'blunder' => strings.blunder,
      'best' => strings.bestMove,
      'brilliant' => strings.brilliant,
      _ => '',
    };
    return Align(
      alignment: user ? Alignment.centerRight : Alignment.centerLeft,
      child: Container(
        constraints: const BoxConstraints(maxWidth: 560),
        margin: const EdgeInsets.only(bottom: 24),
        padding: EdgeInsets.symmetric(
          horizontal: eventColor == null ? 0 : 14,
          vertical: 10,
        ),
        decoration: eventColor == null
            ? null
            : BoxDecoration(
                color: eventColor.withValues(alpha: 0.09),
                border: BorderDirectional(
                  start: BorderSide(color: eventColor, width: 3),
                ),
              ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            if (eventColor != null) ...[
              Row(
                children: [
                  Icon(
                    message.eventKind == 'blunder'
                        ? Icons.error_outline
                        : Icons.stars_outlined,
                    color: eventColor,
                    size: 20,
                  ),
                  const SizedBox(width: 8),
                  Text(
                    eventLabel,
                    style: TextStyle(
                      color: eventColor,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
            ],
            SelectableText(
              message.text,
              style: TextStyle(
                color: user ? scheme.onSurfaceVariant : scheme.onSurface,
                height: 1.5,
              ),
            ),
            if (message.question.isNotEmpty) ...[
              const SizedBox(height: 12),
              SelectableText(
                message.question,
                style: TextStyle(
                  color: scheme.primary,
                  fontWeight: FontWeight.w600,
                  height: 1.5,
                ),
              ),
            ],
            if (message.boardMoves.isNotEmpty ||
                message.focusSquares.isNotEmpty)
              TextButton.icon(
                onPressed: onShowBoard == null
                    ? null
                    : () => onShowBoard!(message),
                icon: const Icon(Icons.center_focus_strong, size: 18),
                label: Text(strings.coachShowOnBoard),
              ),
          ],
        ),
      ),
    );
  }
}
