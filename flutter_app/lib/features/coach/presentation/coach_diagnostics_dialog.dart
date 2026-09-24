import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';

// -----------------------------------------------------------------------------
// Read-only native Coach diagnostics
// -----------------------------------------------------------------------------

Future<void> showCoachDiagnosticsDialog(
  BuildContext context,
  CoreGateway gateway,
) async {
  final strings = AppLocalizations.of(context);
  Map<String, Object?>? snapshot;
  var loading = true;
  var failed = false;
  var requested = false;

  await showDialog<void>(
    context: context,
    builder: (dialogContext) => StatefulBuilder(
      builder: (dialogContext, setDialogState) {
        Future<void> load() async {
          setDialogState(() {
            loading = true;
            failed = false;
          });
          try {
            final next = await gateway.coachPerformanceDiagnostics();
            if (!dialogContext.mounted) return;
            setDialogState(() {
              snapshot = next;
              loading = false;
            });
          } catch (_) {
            if (!dialogContext.mounted) return;
            setDialogState(() {
              loading = false;
              failed = true;
            });
          }
        }

        if (!requested) {
          requested = true;
          WidgetsBinding.instance.addPostFrameCallback((_) {
            if (dialogContext.mounted) unawaited(load());
          });
        }
        final rendered = snapshot == null
            ? ''
            : const JsonEncoder.withIndent('  ').convert(snapshot);
        return AlertDialog(
          title: Text(strings.coachDiagnostics),
          content: SizedBox(
            width: 760,
            height: 540,
            child: loading
                ? const Center(child: CircularProgressIndicator())
                : failed
                    ? Center(child: Text(strings.coachDiagnosticsUnavailable))
                    : SingleChildScrollView(
                        child: Align(
                          alignment: Alignment.topLeft,
                          child: SelectableText(
                            rendered,
                            style: Theme.of(dialogContext).textTheme.bodySmall
                                ?.copyWith(fontFamily: 'monospace'),
                          ),
                        ),
                      ),
          ),
          actions: [
            TextButton.icon(
              onPressed: loading ? null : () => unawaited(load()),
              icon: const Icon(Icons.refresh_rounded),
              label: Text(strings.coachDiagnosticsRefresh),
            ),
            TextButton.icon(
              onPressed: rendered.isEmpty
                  ? null
                  : () => unawaited(
                    Clipboard.setData(ClipboardData(text: rendered)),
                  ),
              icon: const Icon(Icons.copy_outlined),
              label: Text(strings.coachCopyDiagnostics),
            ),
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: Text(strings.close),
            ),
          ],
        );
      },
    ),
  );
}
