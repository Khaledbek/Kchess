import 'dart:convert';
import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../localization/generated/app_localizations.dart';
import '../shared/models/models.dart';
import '../shared/theme/app_theme.dart';
import '../features/app/application/app_controller.dart';
import '../features/analysis/presentation/analysis_screen.dart';

class AppRoot extends StatelessWidget {
  const AppRoot({required this.controller, super.key});

  final AppController controller;

  @override
  Widget build(BuildContext context) => switch (controller.phase) {
    AppPhase.loading => const _SplashScreen(),
    AppPhase.firstRun => FirstRunScreen(controller: controller),
    AppPhase.ready => HomeShell(controller: controller),
    AppPhase.error => _ErrorScreen(controller: controller),
  };
}

/// The app logo, framed in a rounded card so it reads cleanly on any surface.
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
          TextSpan(text: 'K', style: TextStyle(color: scheme.primary)),
          TextSpan(text: 'Chess', style: TextStyle(color: scheme.onSurface)),
        ],
      ),
    );
  }
}

class _SplashScreen extends StatelessWidget {
  const _SplashScreen();

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final brightness = Theme.of(context).brightness;
    return Scaffold(
      body: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: AppTheme.ambientGradient(brightness),
          ),
        ),
        child: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const BrandLogo(size: 116),
              const SizedBox(height: 30),
              const BrandWordmark(fontSize: 40),
              const SizedBox(height: 10),
              Text(
                strings.stockfishPending,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 44),
              Semantics(
                label: strings.loading,
                child: SizedBox(
                  width: 168,
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(4),
                    child: const LinearProgressIndicator(minHeight: 4),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _ErrorScreen extends StatelessWidget {
  const _ErrorScreen({required this.controller});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 460),
          child: Padding(
            padding: const EdgeInsets.all(28),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  padding: const EdgeInsets.all(18),
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: scheme.errorContainer.withValues(alpha: 0.5),
                  ),
                  child: Icon(
                    Icons.cloud_off_rounded,
                    size: 40,
                    color: scheme.error,
                  ),
                ),
                const SizedBox(height: 22),
                Text(
                  strings.coreUnavailable,
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                const SizedBox(height: 22),
                FilledButton.icon(
                  onPressed: controller.initialize,
                  icon: const Icon(Icons.refresh),
                  label: Text(strings.retry),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class FirstRunScreen extends StatefulWidget {
  const FirstRunScreen({required this.controller, super.key});

  final AppController controller;

  @override
  State<FirstRunScreen> createState() => _FirstRunScreenState();
}

class _FirstRunScreenState extends State<FirstRunScreen> {
  final _formKey = GlobalKey<FormState>();
  final _input = TextEditingController();
  ProfileType _type = ProfileType.chessCom;
  bool _saving = false;

  @override
  void dispose() {
    _input.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    if (!_formKey.currentState!.validate()) {
      return;
    }
    setState(() => _saving = true);
    try {
      await widget.controller.createProfile(_type, _input.text);
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    } finally {
      if (mounted) {
        setState(() => _saving = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    return Scaffold(
      body: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: AppTheme.ambientGradient(theme.brightness),
          ),
        ),
        child: SafeArea(
          child: Center(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(24),
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 560),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const BrandLogo(size: 88),
                    const SizedBox(height: 18),
                    const BrandWordmark(fontSize: 32),
                    const SizedBox(height: 28),
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.all(24),
                        child: Form(
                          key: _formKey,
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              Text(
                                strings.firstRunTitle,
                                textAlign: TextAlign.center,
                                style: theme.textTheme.headlineSmall,
                              ),
                              const SizedBox(height: 8),
                              Text(
                                strings.firstRunBody,
                                textAlign: TextAlign.center,
                                style: theme.textTheme.bodyMedium?.copyWith(
                                  color: theme.colorScheme.onSurfaceVariant,
                                ),
                              ),
                              const SizedBox(height: 26),
                              SegmentedButton<ProfileType>(
                                key: const Key('profile-type-selector'),
                                showSelectedIcon: false,
                                segments: [
                                  ButtonSegment(
                                    value: ProfileType.chessCom,
                                    icon: const Icon(Icons.public),
                                    label: Text(strings.chessCom),
                                  ),
                                  ButtonSegment(
                                    value: ProfileType.lichess,
                                    icon: const Icon(Icons.language),
                                    label: Text(strings.lichess),
                                  ),
                                  ButtonSegment(
                                    value: ProfileType.localPgnFen,
                                    icon: const Icon(Icons.upload_file),
                                    label: Text(strings.localPgnFen),
                                  ),
                                ],
                                selected: {_type},
                                onSelectionChanged: (selection) => setState(() {
                                  _type = selection.first;
                                  _input.clear();
                                }),
                              ),
                              const SizedBox(height: 20),
                              TextFormField(
                                key: const Key('profile-input'),
                                controller: _input,
                                textInputAction: TextInputAction.done,
                                onFieldSubmitted: (_) => _submit(),
                                decoration: InputDecoration(
                                  prefixIcon: Icon(
                                    _type == ProfileType.localPgnFen
                                        ? Icons.badge_outlined
                                        : Icons.alternate_email,
                                  ),
                                  labelText: _type == ProfileType.localPgnFen
                                      ? strings.profileName
                                      : strings.username,
                                ),
                                validator: (value) =>
                                    value == null || value.trim().isEmpty
                                    ? strings.requiredField
                                    : null,
                              ),
                              const SizedBox(height: 18),
                              FilledButton(
                                key: const Key('create-profile'),
                                onPressed: _saving ? null : _submit,
                                child: _saving
                                    ? const SizedBox.square(
                                        dimension: 20,
                                        child: CircularProgressIndicator(
                                          strokeWidth: 2,
                                        ),
                                      )
                                    : Text(strings.continueLabel),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class HomeShell extends StatefulWidget {
  const HomeShell({required this.controller, super.key});

  final AppController controller;

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _selectedIndex = 0;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final destinations = [
      _Destination(
        strings.games,
        Icons.sports_esports_outlined,
        Icons.sports_esports,
      ),
      _Destination(strings.favorites, Icons.favorite_border, Icons.favorite),
      _Destination(
        _statisticsText(context).title,
        Icons.insights_outlined,
        Icons.insights,
      ),
      _Destination(strings.settings, Icons.settings_outlined, Icons.settings),
    ];
    final content = switch (_selectedIndex) {
      0 => GamesScreen(controller: widget.controller),
      1 => FavoritesScreen(controller: widget.controller),
      2 => StatisticsScreen(controller: widget.controller),
      3 => SettingsScreen(controller: widget.controller),
      _ => _EmptySection(title: destinations[_selectedIndex].label),
    };

    void openProfile() {
      Navigator.of(context).push(
        MaterialPageRoute<void>(
          builder: (_) => ProfileScreen(controller: widget.controller),
        ),
      );
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth >= 900) {
          return Scaffold(
            body: Row(
              children: [
                SafeArea(
                  child: Container(
                    width: 264,
                    color: Theme.of(context).colorScheme.surfaceContainerLow,
                    child: Column(
                      children: [
                        _ProfileHeader(
                          controller: widget.controller,
                          onOpenProfile: openProfile,
                        ),
                        const SizedBox(height: 4),
                        Expanded(
                          child: NavigationRail(
                            backgroundColor: Colors.transparent,
                            extended: true,
                            selectedIndex: _selectedIndex,
                            onDestinationSelected: (value) =>
                                setState(() => _selectedIndex = value),
                            destinations: [
                              for (final destination in destinations)
                                NavigationRailDestination(
                                  icon: Icon(destination.icon),
                                  selectedIcon: Icon(destination.selectedIcon),
                                  label: Text(destination.label),
                                ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const VerticalDivider(width: 1),
                Expanded(child: content),
              ],
            ),
          );
        }
        return Scaffold(
          appBar: AppBar(title: Text(destinations[_selectedIndex].label)),
          drawer: Drawer(
            child: SafeArea(
              child: Column(
                children: [
                  _ProfileHeader(
                    controller: widget.controller,
                    onOpenProfile: () {
                      Navigator.pop(context);
                      openProfile();
                    },
                  ),
                  Expanded(
                    child: ListView.builder(
                      itemCount: destinations.length,
                      itemBuilder: (context, index) => ListTile(
                        selected: index == _selectedIndex,
                        leading: Icon(destinations[index].icon),
                        title: Text(destinations[index].label),
                        onTap: () {
                          setState(() => _selectedIndex = index);
                          Navigator.pop(context);
                        },
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
          body: content,
        );
      },
    );
  }
}

class _Destination {
  const _Destination(this.label, this.icon, this.selectedIcon);
  final String label;
  final IconData icon;
  final IconData selectedIcon;
}

class _ProfileHeader extends StatelessWidget {
  const _ProfileHeader({required this.controller, required this.onOpenProfile});

  final AppController controller;
  final VoidCallback onOpenProfile;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final profile = controller.activeProfile!;
    return Padding(
      padding: const EdgeInsets.fromLTRB(14, 18, 14, 10),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 4, bottom: 16),
            child: Row(
              children: [
                const BrandLogo(size: 30),
                const SizedBox(width: 10),
                const BrandWordmark(fontSize: 21),
              ],
            ),
          ),
          Material(
            color: scheme.surfaceContainerHigh,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(
              onTap: onOpenProfile,
              borderRadius: BorderRadius.circular(16),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Row(
                  children: [
                    Container(
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        border: Border.all(color: scheme.outlineVariant, width: 2),
                      ),
                      padding: const EdgeInsets.all(2),
                      child: CircleAvatar(
                        radius: 22,
                        backgroundColor: scheme.primaryContainer,
                        child: _profileAvatar(profile),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            profile.displayName,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: theme.textTheme.titleMedium,
                          ),
                          const SizedBox(height: 2),
                          Text(
                            _profileTypeLabel(strings, profile.type),
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: scheme.onSurfaceVariant,
                            ),
                          ),
                        ],
                      ),
                    ),
                    Icon(
                      Icons.chevron_right,
                      color: scheme.onSurfaceVariant,
                    ),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: TextButton.icon(
                  onPressed: () => showDialog<void>(
                    context: context,
                    builder: (context) =>
                        _AddProfileDialog(controller: controller),
                  ),
                  icon: const Icon(Icons.person_add_alt_1, size: 18),
                  label: Text(strings.addAccount),
                ),
              ),
              if (controller.profiles.length > 1)
                Expanded(
                  child: TextButton.icon(
                    onPressed: () => _showProfileSwitcher(context, controller),
                    icon: const Icon(Icons.swap_horiz, size: 18),
                    label: Text(strings.switchAccount),
                  ),
                ),
            ],
          ),
        ],
      ),
    );
  }
}

class _AddProfileDialog extends StatefulWidget {
  const _AddProfileDialog({required this.controller});

  final AppController controller;

  @override
  State<_AddProfileDialog> createState() => _AddProfileDialogState();
}

class _AddProfileDialogState extends State<_AddProfileDialog> {
  final _input = TextEditingController();
  ProfileType _type = ProfileType.chessCom;
  bool _saving = false;
  bool _showRequiredError = false;

  @override
  void dispose() {
    _input.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    if (_input.text.trim().isEmpty) {
      setState(() => _showRequiredError = true);
      return;
    }
    setState(() => _saving = true);
    try {
      await widget.controller.createProfile(_type, _input.text);
      if (mounted) Navigator.pop(context);
    } catch (error) {
      if (mounted) {
        setState(() => _saving = false);
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return AlertDialog(
      title: Text(strings.addAccount),
      content: SizedBox(
        width: 480,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            DropdownButtonFormField<ProfileType>(
              initialValue: _type,
              decoration: InputDecoration(labelText: strings.provider),
              items: [
                DropdownMenuItem(
                  value: ProfileType.chessCom,
                  child: Text(strings.chessCom),
                ),
                DropdownMenuItem(
                  value: ProfileType.lichess,
                  child: Text(strings.lichess),
                ),
                DropdownMenuItem(
                  value: ProfileType.localPgnFen,
                  child: Text(strings.localPgnFen),
                ),
              ],
              onChanged: (value) {
                if (value != null) setState(() => _type = value);
              },
            ),
            const SizedBox(height: 16),
            TextField(
              key: const Key('additional-profile-input'),
              controller: _input,
              autofocus: true,
              decoration: InputDecoration(
                labelText: _type == ProfileType.localPgnFen
                    ? strings.profileName
                    : strings.username,
                errorText: _showRequiredError ? strings.requiredField : null,
              ),
              onSubmitted: (_) => _save(),
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: _saving ? null : () => Navigator.pop(context),
          child: Text(strings.close),
        ),
        FilledButton(
          key: const Key('create-additional-profile'),
          onPressed: _saving ? null : _save,
          child: Text(strings.continueLabel),
        ),
      ],
    );
  }
}

Future<void> _showProfileSwitcher(
  BuildContext context,
  AppController controller,
) async {
  final selected = await showDialog<AppProfile>(
    context: context,
    builder: (context) => SimpleDialog(
      title: Text(AppLocalizations.of(context).switchAccount),
      children: [
        for (final profile in controller.profiles)
          SimpleDialogOption(
            onPressed: () => Navigator.pop(context, profile),
            child: ListTile(
              leading: const Icon(Icons.account_circle),
              title: Text(profile.displayName),
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (profile.id == controller.activeProfile?.id)
                    const Icon(Icons.check),
                  IconButton(
                    key: Key('delete-profile-${profile.id}'),
                    tooltip: AppLocalizations.of(context).deleteAccount,
                    onPressed: () async {
                      final deleted = await _confirmDeleteProfile(
                        context,
                        controller,
                        profile,
                      );
                      if (deleted && context.mounted) Navigator.pop(context);
                    },
                    icon: const Icon(Icons.delete_outline),
                  ),
                ],
              ),
            ),
          ),
      ],
    ),
  );
  if (selected != null) {
    await controller.switchProfile(selected);
  }
}

Future<bool> _confirmDeleteProfile(
  BuildContext context,
  AppController controller,
  AppProfile profile,
) async {
  final strings = AppLocalizations.of(context);
  if (controller.settings.confirmBeforeDelete) {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.deleteAccountQuestion),
        content: Text(
          profile.type == ProfileType.localPgnFen
              ? strings.deleteLocalProfileBody
              : strings.deleteOnlineProfileBody,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(strings.cancelAction),
          ),
          FilledButton(
            key: const Key('confirm-delete-profile'),
            onPressed: () => Navigator.pop(context, true),
            child: Text(strings.deleteAction),
          ),
        ],
      ),
    );
    if (confirmed != true) return false;
  }
  try {
    await controller.deleteProfile(profile);
    return true;
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
    return false;
  }
}


String _profileMergeText(
  BuildContext context, {
  required String de,
  required String en,
  required String ar,
}) => switch (Localizations.localeOf(context).languageCode) {
  'ar' => ar,
  'en' => en,
  _ => de,
};

Future<bool> _showMergeLocalProfileDialog(
  BuildContext context,
  AppController controller,
  AppProfile source,
) async {
  final targets = controller.profiles
      .where((profile) => profile.type != ProfileType.localPgnFen)
      .toList(growable: false);
  if (targets.isEmpty) return false;

  final target = await showDialog<AppProfile>(
    context: context,
    builder: (context) => SimpleDialog(
      title: Text(
        _profileMergeText(
          context,
          de: 'Mit Online-Profil zusammenführen',
          en: 'Merge with online profile',
          ar: 'دمج مع ملف شخصي عبر الإنترنت',
        ),
      ),
      children: [
        for (final profile in targets)
          SimpleDialogOption(
            onPressed: () => Navigator.pop(context, profile),
            child: ListTile(
              leading: const Icon(Icons.merge_type),
              title: Text(profile.displayName),
              subtitle: Text(
                _profileTypeLabel(AppLocalizations.of(context), profile.type),
              ),
            ),
          ),
      ],
    ),
  );
  if (target == null || !context.mounted) return false;

  final gameCount = controller.games.length;
  final confirmed = await showDialog<bool>(
    context: context,
    builder: (context) => AlertDialog(
      title: Text(
        _profileMergeText(
          context,
          de: 'Lokales Profil zusammenführen?',
          en: 'Merge local profile?',
          ar: 'دمج الملف الشخصي المحلي؟',
        ),
      ),
      content: Text(
        _profileMergeText(
          context,
          de:
              '$gameCount lokale Partien/Positionen werden zu „${target.displayName}“ verschoben. Doppelte Partien werden erkannt; Favoriten, Sammlungen und die beste vorhandene Analyse bleiben erhalten. Das lokale Profil wird danach entfernt.',
          en:
              '$gameCount local games/positions will be moved to “${target.displayName}”. Duplicate games are detected; favorites, collections and the best available analysis are kept. The local profile is removed afterwards.',
          ar:
              'سيتم نقل $gameCount من المباريات/الوضعيات المحلية إلى «${target.displayName}». سيتم اكتشاف المباريات المكررة مع الاحتفاظ بالمفضلة والمجموعات وأفضل تحليل متاح. بعد ذلك سيتم حذف الملف الشخصي المحلي.',
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, false),
          child: Text(AppLocalizations.of(context).cancelAction),
        ),
        FilledButton(
          onPressed: () => Navigator.pop(context, true),
          child: Text(
            _profileMergeText(
              context,
              de: 'Zusammenführen',
              en: 'Merge',
              ar: 'دمج',
            ),
          ),
        ),
      ],
    ),
  );
  if (confirmed != true) return false;

  try {
    await controller.mergeLocalProfile(source, target);
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            _profileMergeText(
              context,
              de: 'Lokales Profil wurde mit „${target.displayName}“ zusammengeführt.',
              en: 'Local profile was merged into “${target.displayName}”.',
              ar: 'تم دمج الملف الشخصي المحلي مع «${target.displayName}».',
            ),
          ),
        ),
      );
      Navigator.of(context).pop();
    }
    return true;
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
    return false;
  }
}

String _profileTypeLabel(AppLocalizations strings, ProfileType type) =>
    switch (type) {
      ProfileType.chessCom => strings.chessCom,
      ProfileType.lichess => strings.lichess,
      ProfileType.localPgnFen => strings.localPgnFen,
    };

Widget _profileAvatar(AppProfile profile, {double iconSize = 24}) {
  Widget fallback() => Image.asset(
    '../img/${profile.avatarAsset}',
    errorBuilder: (_, _, _) => Icon(Icons.person, size: iconSize),
  );
  final file = profile.avatarFile;
  if (file == null || file.isEmpty) return fallback();
  return Image.file(
    File(file),
    fit: BoxFit.cover,
    errorBuilder: (_, _, _) => fallback(),
  );
}

Future<void> _openAnalysisAndRefreshSettings({
  required BuildContext context,
  required AppController controller,
  required GameSummary game,
}) async {
  await openAnalysisWorkflow(
    context: context,
    gateway: controller.gateway,
    game: game,
    settings: controller.settings,
  );
  await controller.reloadSettings();
  await controller.refreshAfterAnalysis();
}

class GamesScreen extends StatefulWidget {
  const GamesScreen({required this.controller, this.savedFilter, super.key});
  final AppController controller;
  final String? savedFilter;

  @override
  State<GamesScreen> createState() => _GamesScreenState();
}

/// In-memory only: filters survive opening a game and switching sections, but
/// intentionally reset when the app process is started again.
class _GamesFilterSession {
  String query = '';
  String outcome = 'all';
  String color = 'all';
  final Set<String> timeControls = <String>{};
  String sort = 'newest';
  double scrollOffset = 0;
}

final Map<String, _GamesFilterSession> _gamesFilterSessions = {};

class _GamesScreenState extends State<GamesScreen> {
  AppController get controller => widget.controller;
  late final _GamesFilterSession _filterSession;
  late final TextEditingController _search;
  late final ScrollController _scrollController;
  late String _outcome;
  late String _color;
  late Set<String> _timeControls;
  late String _sort;

  @override
  void initState() {
    super.initState();
    final sessionKey =
        '${controller.activeProfile?.id ?? 'no-profile'}:${widget.savedFilter ?? 'games'}';
    _filterSession = _gamesFilterSessions.putIfAbsent(
      sessionKey,
      _GamesFilterSession.new,
    );
    _search = TextEditingController(text: _filterSession.query);
    _scrollController = ScrollController(
      initialScrollOffset: _filterSession.scrollOffset,
    )..addListener(_persistScrollOffset);
    _outcome = _filterSession.outcome;
    _color = _filterSession.color;
    _timeControls = Set<String>.from(_filterSession.timeControls);
    _sort = _filterSession.sort;
  }

  void _persistFilters() {
    _filterSession
      ..query = _search.text
      ..outcome = _outcome
      ..color = _color
      ..sort = _sort;
    _filterSession.timeControls
      ..clear()
      ..addAll(_timeControls);
  }

  void _persistScrollOffset() {
    if (_scrollController.hasClients) {
      _filterSession.scrollOffset = _scrollController.offset;
    }
  }

  void _resetAllFilters() {
    setState(() {
      _search.clear();
      _outcome = 'all';
      _color = 'all';
      _timeControls.clear();
      _sort = 'newest';
      _persistFilters();
    });
  }

  int get _activeFilterCount =>
      [
        _outcome != 'all',
        _color != 'all',
        _sort != 'newest',
      ].where((active) => active).length +
      _timeControls.length;

  List<Widget> _activeFilterChips(_GameFilterLabels labels) {
    final chips = <Widget>[];
    void addChip(String label, VoidCallback onDeleted) {
      chips.add(
        InputChip(
          label: Text(label),
          visualDensity: VisualDensity.compact,
          onDeleted: onDeleted,
        ),
      );
    }

    if (_outcome != 'all') {
      addChip(
        switch (_outcome) {
          'win' => labels.won,
          'loss' => labels.lost,
          'draw' => labels.draw,
          _ => _outcome,
        },
        () => setState(() {
          _outcome = 'all';
          _persistFilters();
        }),
      );
    }
    if (_color != 'all') {
      addChip(
        _color == 'white' ? labels.white : labels.black,
        () => setState(() {
          _color = 'all';
          _persistFilters();
        }),
      );
    }
    for (final value in _orderedTimeAndStatusValues(_timeControls)) {
      addChip(
        _timeAndStatusLabel(labels, value),
        () => setState(() {
          _timeControls.remove(value);
          _persistFilters();
        }),
      );
    }
    if (_sort != 'newest') {
      addChip(
        switch (_sort) {
          'oldest' => labels.oldestFirst,
          'accuracyHigh' => labels.accuracyDescending,
          'accuracyLow' => labels.accuracyAscending,
          _ => _sort,
        },
        () => setState(() {
          _sort = 'newest';
          _persistFilters();
        }),
      );
    }
    return chips;
  }

  Future<void> _openFilters(BuildContext context) async {
    final result = MediaQuery.sizeOf(context).width >= 700
        ? await showDialog<_GameFilterSelection>(
            context: context,
            builder: (context) => Dialog(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 500),
                child: _GameFilterPanel(
                  initial: _GameFilterSelection(
                    outcome: _outcome,
                    color: _color,
                    timeControls: _timeControls,
                    sort: _sort,
                  ),
                ),
              ),
            ),
          )
        : await showModalBottomSheet<_GameFilterSelection>(
            context: context,
            showDragHandle: true,
            isScrollControlled: true,
            builder: (context) => SafeArea(
              child: _GameFilterPanel(
                initial: _GameFilterSelection(
                  outcome: _outcome,
                  color: _color,
                  timeControls: _timeControls,
                  sort: _sort,
                ),
              ),
            ),
          );
    if (result == null || !mounted) return;
    setState(() {
      _outcome = result.outcome;
      _color = result.color;
      _timeControls = Set<String>.from(result.timeControls);
      _sort = result.sort;
      _persistFilters();
    });
  }

  @override
  void dispose() {
    _persistFilters();
    _persistScrollOffset();
    _scrollController
      ..removeListener(_persistScrollOffset)
      ..dispose();
    _search.dispose();
    super.dispose();
  }

  Future<void> _import(BuildContext context) async {
    final strings = AppLocalizations.of(context);
    final source = await showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.file_open_outlined),
              title: Text(strings.importPgnFile),
              onTap: () => Navigator.pop(context, 'file'),
            ),
            ListTile(
              leading: const Icon(Icons.content_paste_outlined),
              title: Text(strings.pastePgn),
              onTap: () => Navigator.pop(context, 'text'),
            ),
            ListTile(
              leading: const Icon(Icons.grid_on_outlined),
              title: Text(strings.importFen),
              onTap: () => Navigator.pop(context, 'fen'),
            ),
          ],
        ),
      ),
    );
    if (!context.mounted || source == null) return;
    try {
      late final GameSummary game;
      if (source == 'file') {
        final file = await FilePicker.pickFile(
          type: FileType.custom,
          allowedExtensions: const ['pgn'],
        );
        if (file == null) return;
        final text = utf8.decode(await file.readAsBytes());
        game = await controller.importPgn(text);
      } else if (source == 'text') {
        final pgn = await showDialog<String>(
          context: context,
          builder: (_) => const _PgnImportDialog(),
        );
        if (pgn == null) return;
        game = await controller.importPgn(pgn);
      } else {
        final value = await showDialog<({String fen, String name})>(
          context: context,
          builder: (_) => const _FenImportDialog(),
        );
        if (value == null) return;
        game = await controller.importFen(fen: value.fen, name: value.name);
      }
      if (context.mounted) {
        await _openAnalysisAndRefreshSettings(
          context: context,
          controller: controller,
          game: game,
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    }
  }

  Future<void> _deleteLocalEntry(BuildContext context, GameSummary game) async {
    if (controller.settings.confirmBeforeDelete) {
      final strings = AppLocalizations.of(context);
      final confirmed = await showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: Text(strings.deleteLocalGameQuestion),
          content: Text(strings.deleteLocalGameBody),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: Text(strings.cancelAction),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: Text(strings.deleteAction),
            ),
          ],
        ),
      );
      if (confirmed != true) return;
    }
    await controller.deleteLocalGame(game);
  }

  Future<void> _clearMonthCache(BuildContext context, String month) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Monatscache löschen?'),
        content: Text(
          '$month wird aus dem normalen Cache entfernt. Favoriten und bereits analysierte Partien bleiben erhalten.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Abbrechen'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Cache löschen'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    await controller.clearCachedMonth(month);
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Cache für $month wurde bereinigt.')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final online = controller.activeProfile?.type != ProfileType.localPgnFen;
    final librarySection = widget.savedFilter != null;
    final screenTitle = switch (widget.savedFilter) {
      'favorite' => strings.favorites,
      _ => strings.games,
    };
    final selectedMonth = controller.selectedMonth;
    final playerIdentity =
        (controller.activeProfile?.providerUsername ??
                controller.activeProfile?.displayName ??
                '')
            .trim();
    final filtered = controller.games.where((game) {
      if (widget.savedFilter == 'favorite' && !game.favorite) return false;
      final query = _search.text.trim().toLowerCase();
      if (query.isNotEmpty &&
          !game.whiteName.toLowerCase().contains(query) &&
          !game.blackName.toLowerCase().contains(query)) {
        return false;
      }
      if (_outcome != 'all' && game.providerOutcome != _outcome) return false;
      if (_color != 'all') {
        final identity = playerIdentity.toLowerCase();
        final isWhite = game.whiteName.toLowerCase() == identity;
        final isBlack = game.blackName.toLowerCase() == identity;
        if ((_color == 'white' && !isWhite) ||
            (_color == 'black' && !isBlack)) {
          return false;
        }
      }
      final selectedTimeControls = _timeControls.where(
        (value) => !_statusFilterValues.contains(value),
      );
      if (selectedTimeControls.isNotEmpty &&
          !selectedTimeControls.contains(game.timeControlType)) {
        return false;
      }
      if (_timeControls.contains('analyzed') && !game.analyzed) return false;
      if (_timeControls.contains('notAnalyzed') && game.analyzed) return false;
      if (online &&
          !librarySection &&
          selectedMonth != null &&
          game.endedAt > 0) {
        final date = DateTime.fromMillisecondsSinceEpoch(
          game.endedAt * 1000,
          isUtc: true,
        );
        final value =
            '${date.year.toString().padLeft(4, '0')}-${date.month.toString().padLeft(2, '0')}';
        if (value != selectedMonth) return false;
      }
      return true;
    }).toList();
    filtered.sort(
      (left, right) => switch (_sort) {
        'oldest' => left.endedAt.compareTo(right.endedAt),
        'accuracyHigh' => (right.accuracy ?? -1).compareTo(left.accuracy ?? -1),
        'accuracyLow' => (left.accuracy ?? 101).compareTo(
          right.accuracy ?? 101,
        ),
        _ => right.endedAt.compareTo(left.endedAt),
      },
    );
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(
              title: Text(screenTitle),
              actions: [
                if (online)
                  IconButton(
                    tooltip: 'Synchronisieren',
                    onPressed: controller.providerSyncing
                        ? null
                        : controller.syncProvider,
                    icon: const Icon(Icons.sync),
                  ),
              ],
            )
          : null,
      floatingActionButton: FloatingActionButton.extended(
        key: const Key('import-pgn-fen'),
        onPressed: () => _import(context),
        icon: const Icon(Icons.add),
        label: Text(strings.importData),
      ),
      body: Column(
        children: [
          if (controller.providerSyncing) const LinearProgressIndicator(),
          if (controller.providerNotice != null)
            MaterialBanner(
              content: Text(controller.providerNotice!),
              leading: const Icon(Icons.cloud_off_outlined),
              actions: [
                TextButton(
                  onPressed: () =>
                      controller.syncProvider(month: controller.selectedMonth),
                  child: Text(strings.retry),
                ),
              ],
            ),
          Container(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.surfaceContainerLow,
              border: Border(
                bottom: BorderSide(
                  color: Theme.of(context).colorScheme.outlineVariant,
                ),
              ),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                LayoutBuilder(
                  builder: (context, constraints) {
                    final labels = _GameFilterLabels.of(context);
                    final hasMonths = online &&
                        !librarySection &&
                        (controller.providerOverview?.availableMonths.isNotEmpty ??
                            false);
                    final months = hasMonths
                        ? ([...controller.providerOverview!.availableMonths]
                          ..sort((left, right) => right.compareTo(left)))
                        : <String>[];
                    final currentMonth = months.isEmpty
                        ? null
                        : months.contains(controller.selectedMonth)
                        ? controller.selectedMonth!
                        : months.first;

                Widget searchField() => TextField(
                      key: const Key('game-search'),
                      controller: _search,
                      onChanged: (_) {
                        _persistFilters();
                        setState(() {});
                      },
                      decoration: InputDecoration(
                        prefixIcon: const Icon(Icons.search),
                        labelText: labels.searchOpponent,
                        isDense: true,
                      ),
                    );

                Widget filterButton() => Stack(
                      clipBehavior: Clip.none,
                      children: [
                        IconButton.filledTonal(
                          key: const Key('open-game-filters'),
                          tooltip: labels.filters,
                          onPressed: () => _openFilters(context),
                          icon: Icon(
                            _activeFilterCount == 0
                                ? Icons.filter_alt_outlined
                                : Icons.filter_alt,
                          ),
                        ),
                        if (_activeFilterCount > 0)
                          PositionedDirectional(
                            end: -3,
                            top: -3,
                            child: Container(
                              constraints: const BoxConstraints(
                                minWidth: 18,
                                minHeight: 18,
                              ),
                              padding: const EdgeInsets.symmetric(horizontal: 4),
                              alignment: Alignment.center,
                              decoration: BoxDecoration(
                                color: Theme.of(context).colorScheme.primary,
                                borderRadius: BorderRadius.circular(10),
                                border: Border.all(
                                  color: Theme.of(context)
                                      .colorScheme
                                      .surfaceContainerLow,
                                  width: 2,
                                ),
                              ),
                              child: Text(
                                '$_activeFilterCount',
                                style: Theme.of(context)
                                    .textTheme
                                    .labelSmall
                                    ?.copyWith(
                                      color: Theme.of(context)
                                          .colorScheme
                                          .onPrimary,
                                      fontWeight: FontWeight.w700,
                                    ),
                              ),
                            ),
                          ),
                      ],
                    );

                    Widget monthControls() {
                      if (currentMonth == null) return const SizedBox.shrink();
                      final index = months.indexOf(currentMonth);
                      final olderMonth = index + 1 < months.length
                          ? months[index + 1]
                          : null;
                      final newerMonth = index > 0 ? months[index - 1] : null;
                      return Wrap(
                        spacing: 4,
                        runSpacing: 4,
                        crossAxisAlignment: WrapCrossAlignment.center,
                        children: [
                          IconButton(
                            tooltip: labels.previousMonth,
                            visualDensity: VisualDensity.compact,
                            onPressed: olderMonth == null || controller.providerSyncing
                                ? null
                                : () => controller.syncProvider(month: olderMonth),
                            icon: const Icon(Icons.chevron_left),
                          ),
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 12),
                            decoration: BoxDecoration(
                              color: Theme.of(context)
                                  .colorScheme
                                  .surfaceContainerHigh,
                              borderRadius: BorderRadius.circular(999),
                              border: Border.all(
                                color: Theme.of(context).colorScheme.outlineVariant,
                              ),
                            ),
                            child: DropdownButtonHideUnderline(
                              child: DropdownButton<String>(
                                key: const Key('month-selector'),
                                value: currentMonth,
                                borderRadius: BorderRadius.circular(14),
                                icon: const Icon(Icons.expand_more, size: 18),
                                items: [
                                  for (final month in months)
                                    DropdownMenuItem(
                                      value: month,
                                      child: Text(_formatGameMonthLabel(context, month)),
                                    ),
                                ],
                                onChanged: controller.providerSyncing
                                    ? null
                                    : (value) {
                                        if (value != null) {
                                          controller.syncProvider(month: value);
                                        }
                                      },
                              ),
                            ),
                          ),
                          IconButton(
                            tooltip: labels.nextMonth,
                            visualDensity: VisualDensity.compact,
                            onPressed: newerMonth == null || controller.providerSyncing
                                ? null
                                : () => controller.syncProvider(month: newerMonth),
                            icon: const Icon(Icons.chevron_right),
                          ),
                          IconButton(
                            key: const Key('clear-month-cache'),
                            tooltip: labels.clearMonthCache,
                            visualDensity: VisualDensity.compact,
                            onPressed: controller.providerSyncing
                                ? null
                                : () => _clearMonthCache(context, currentMonth),
                            icon: const Icon(Icons.cleaning_services_outlined),
                          ),
                        ],
                      );
                    }

                if (constraints.maxWidth >= 720) {
                  return Row(
                    children: [
                      Expanded(child: searchField()),
                      const SizedBox(width: 10),
                      filterButton(),
                      if (hasMonths) ...[
                        const SizedBox(width: 16),
                        monthControls(),
                      ],
                    ],
                  );
                }

                    return Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Row(
                          children: [
                            Expanded(child: searchField()),
                            const SizedBox(width: 10),
                            filterButton(),
                          ],
                        ),
                        if (hasMonths) ...[
                          const SizedBox(height: 8),
                          Align(
                            alignment: AlignmentDirectional.centerStart,
                            child: monthControls(),
                          ),
                        ],
                      ],
                    );
                  },
                ),
                if (_activeFilterCount > 0) ...[
                  const SizedBox(height: 10),
                  Wrap(
                    spacing: 8,
                    runSpacing: 6,
                    children: _activeFilterChips(_GameFilterLabels.of(context)),
                  ),
                ],
              ],
            ),
          ),
          Expanded(
            child: filtered.isEmpty
                ? _GamesEmptyState(
                    labels: _GameFilterLabels.of(context),
                    fallbackText: strings.noGames,
                    hasActiveFilters:
                        _activeFilterCount > 0 || _search.text.trim().isNotEmpty,
                    month: online && !librarySection ? selectedMonth : null,
                    onResetFilters: _resetAllFilters,
                  )
                : ListView.separated(
                    controller: _scrollController,
                    padding: const EdgeInsets.all(16),
                    itemCount: filtered.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 12),
                    itemBuilder: (context, index) {
                      final game = filtered[index];
                      return _GameCard(
                        game: game,
                        online: online,
                        playerIdentity: playerIdentity,
                        onOpen: () => _openAnalysisAndRefreshSettings(
                          context: context,
                          controller: controller,
                          game: game,
                        ),
                        onToggleFavorite: () => controller.toggleFavorite(game),
                        onSaveToDownloads: game.downloaded
                            ? null
                            : () => controller.saveToDownloads(game),
                        onDelete: () => _deleteLocalEntry(context, game),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}


String _formatGameMonthLabel(BuildContext context, String value) {
  final parts = value.split('-');
  if (parts.length != 2) return value;
  final year = int.tryParse(parts[0]);
  final month = int.tryParse(parts[1]);
  if (year == null || month == null || month < 1 || month > 12) return value;
  final language = Localizations.localeOf(context).languageCode;
  const de = [
    'Januar', 'Februar', 'März', 'April', 'Mai', 'Juni',
    'Juli', 'August', 'September', 'Oktober', 'November', 'Dezember',
  ];
  const en = [
    'January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December',
  ];
  const ar = [
    'يناير', 'فبراير', 'مارس', 'أبريل', 'مايو', 'يونيو',
    'يوليو', 'أغسطس', 'سبتمبر', 'أكتوبر', 'نوفمبر', 'ديسمبر',
  ];
  final names = language == 'ar' ? ar : language == 'en' ? en : de;
  return '${names[month - 1]} $year';
}


class _GameFilterSelection {
  const _GameFilterSelection({
    required this.outcome,
    required this.color,
    required this.timeControls,
    required this.sort,
  });

  final String outcome;
  final String color;
  final Set<String> timeControls;
  final String sort;
}

const _timeControlFilterValues = <String>[
  'bullet',
  'blitz',
  'rapid',
  'daily',
  'classical',
  'correspondence',
];

const _statusFilterValues = <String>{
  'analyzed',
  'notAnalyzed',
};

const _timeAndStatusFilterOrder = <String>[
  ..._timeControlFilterValues,
  'analyzed',
  'notAnalyzed',
];

Iterable<String> _orderedTimeAndStatusValues(Set<String> values) =>
    _timeAndStatusFilterOrder.where(values.contains);

String _timeAndStatusLabel(_GameFilterLabels labels, String value) =>
    switch (value) {
      'bullet' => 'Bullet',
      'blitz' => 'Blitz',
      'rapid' => 'Rapid',
      'daily' => 'Daily',
      'classical' => labels.classical,
      'correspondence' => labels.correspondence,
      'analyzed' => labels.analyzed,
      'notAnalyzed' => labels.notAnalyzed,
      _ => value,
    };

class _GameFilterPanel extends StatefulWidget {
  const _GameFilterPanel({required this.initial});

  final _GameFilterSelection initial;

  @override
  State<_GameFilterPanel> createState() => _GameFilterPanelState();
}

class _GameFilterPanelState extends State<_GameFilterPanel> {
  late String _outcome;
  late String _color;
  late Set<String> _timeControls;
  late String _sort;

  @override
  void initState() {
    super.initState();
    _outcome = widget.initial.outcome;
    _color = widget.initial.color;
    _timeControls = Set<String>.from(widget.initial.timeControls);
    _sort = widget.initial.sort;
  }

  void _reset() {
    setState(() {
      _outcome = 'all';
      _color = 'all';
      _timeControls.clear();
      _sort = 'newest';
    });
  }

  void _apply() {
    Navigator.of(context).pop(
      _GameFilterSelection(
        outcome: _outcome,
        color: _color,
        timeControls: Set<String>.unmodifiable(_timeControls),
        sort: _sort,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final labels = _GameFilterLabels.of(context);
    final theme = Theme.of(context);
    return SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(
        20,
        12,
        20,
        20 + MediaQuery.viewInsetsOf(context).bottom,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Container(
                width: 40,
                height: 40,
                decoration: BoxDecoration(
                  color: theme.colorScheme.primaryContainer,
                  borderRadius: BorderRadius.circular(12),
                ),
                alignment: Alignment.center,
                child: Icon(
                  Icons.filter_alt_outlined,
                  color: theme.colorScheme.onPrimaryContainer,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  labels.filters,
                  style: theme.textTheme.titleLarge?.copyWith(
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ),
              IconButton(
                tooltip: labels.close,
                onPressed: () => Navigator.of(context).pop(),
                icon: const Icon(Icons.close),
              ),
            ],
          ),
          const SizedBox(height: 18),
          DropdownButtonFormField<String>(
            initialValue: _outcome,
            decoration: InputDecoration(
              labelText: labels.result,
              prefixIcon: const Icon(Icons.emoji_events_outlined),
            ),
            items: [
              DropdownMenuItem(value: 'all', child: Text(labels.allResults)),
              DropdownMenuItem(value: 'win', child: Text(labels.won)),
              DropdownMenuItem(value: 'loss', child: Text(labels.lost)),
              DropdownMenuItem(value: 'draw', child: Text(labels.draw)),
            ],
            onChanged: (value) {
              if (value != null) setState(() => _outcome = value);
            },
          ),
          const SizedBox(height: 14),
          DropdownButtonFormField<String>(
            initialValue: _color,
            decoration: InputDecoration(
              labelText: labels.color,
              prefixIcon: const Icon(Icons.contrast),
            ),
            items: [
              DropdownMenuItem(value: 'all', child: Text(labels.allColors)),
              DropdownMenuItem(value: 'white', child: Text(labels.white)),
              DropdownMenuItem(value: 'black', child: Text(labels.black)),
            ],
            onChanged: (value) {
              if (value != null) setState(() => _color = value);
            },
          ),
          const SizedBox(height: 14),
          InputDecorator(
            decoration: InputDecoration(
              labelText: labels.timeAndStatus,
              prefixIcon: const Icon(Icons.schedule_outlined),
            ),
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                FilterChip(
                  label: Text(labels.allTimeAndStatus),
                  selected: _timeControls.isEmpty,
                  onSelected: (_) => setState(_timeControls.clear),
                ),
                for (final value in _timeAndStatusFilterOrder)
                  FilterChip(
                    label: Text(_timeAndStatusLabel(labels, value)),
                    selected: _timeControls.contains(value),
                    onSelected: (selected) {
                      setState(() {
                        if (selected) {
                          // Opposing status choices are mutually exclusive.
                          if (value == 'analyzed') {
                            _timeControls.remove('notAnalyzed');
                          } else if (value == 'notAnalyzed') {
                            _timeControls.remove('analyzed');
                          }
                          _timeControls.add(value);
                        } else {
                          _timeControls.remove(value);
                        }
                      });
                    },
                  ),
              ],
            ),
          ),
          const SizedBox(height: 14),
          DropdownButtonFormField<String>(
            initialValue: _sort,
            decoration: InputDecoration(
              labelText: labels.sort,
              prefixIcon: const Icon(Icons.sort),
            ),
            items: [
              DropdownMenuItem(value: 'newest', child: Text(labels.newestFirst)),
              DropdownMenuItem(value: 'oldest', child: Text(labels.oldestFirst)),
              DropdownMenuItem(
                value: 'accuracyHigh',
                child: Text(labels.accuracyDescending),
              ),
              DropdownMenuItem(
                value: 'accuracyLow',
                child: Text(labels.accuracyAscending),
              ),
            ],
            onChanged: (value) {
              if (value != null) setState(() => _sort = value);
            },
          ),
          const SizedBox(height: 22),
          Row(
            mainAxisAlignment: MainAxisAlignment.end,
            children: [
              TextButton.icon(
                onPressed: _reset,
                icon: const Icon(Icons.restart_alt),
                label: Text(labels.reset),
              ),
              const SizedBox(width: 8),
              FilledButton.icon(
                key: const Key('apply-game-filters'),
                onPressed: _apply,
                icon: const Icon(Icons.check),
                label: Text(labels.apply),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _GameFilterLabels {
  const _GameFilterLabels({
    required this.filters,
    required this.searchOpponent,
    required this.clearMonthCache,
    required this.previousMonth,
    required this.nextMonth,
    required this.noMatchingGames,
    required this.noMatchingGamesHelp,
    required this.resetFilters,
    required this.noGamesForMonth,
    required this.result,
    required this.allResults,
    required this.won,
    required this.lost,
    required this.draw,
    required this.color,
    required this.allColors,
    required this.white,
    required this.black,
    required this.timeAndStatus,
    required this.allTimeAndStatus,
    required this.classical,
    required this.correspondence,
    required this.analyzed,
    required this.notAnalyzed,
    required this.sort,
    required this.newestFirst,
    required this.oldestFirst,
    required this.accuracyDescending,
    required this.accuracyAscending,
    required this.reset,
    required this.apply,
    required this.close,
  });

  final String filters;
  final String searchOpponent;
  final String clearMonthCache;
  final String previousMonth;
  final String nextMonth;
  final String noMatchingGames;
  final String noMatchingGamesHelp;
  final String resetFilters;
  final String noGamesForMonth;
  final String result;
  final String allResults;
  final String won;
  final String lost;
  final String draw;
  final String color;
  final String allColors;
  final String white;
  final String black;
  final String timeAndStatus;
  final String allTimeAndStatus;
  final String classical;
  final String correspondence;
  final String analyzed;
  final String notAnalyzed;
  final String sort;
  final String newestFirst;
  final String oldestFirst;
  final String accuracyDescending;
  final String accuracyAscending;
  final String reset;
  final String apply;
  final String close;

  static _GameFilterLabels of(BuildContext context) {
    final language = Localizations.localeOf(context).languageCode;
    if (language == 'ar') {
      return const _GameFilterLabels(
        filters: 'التصفية',
        searchOpponent: 'البحث عن خصم',
        clearMonthCache: 'مسح ذاكرة الشهر المؤقتة',
        previousMonth: 'الشهر السابق',
        nextMonth: 'الشهر التالي',
        noMatchingGames: 'لا توجد مباريات مطابقة',
        noMatchingGamesHelp: 'غيّر البحث أو أزل بعض عوامل التصفية.',
        resetFilters: 'إعادة ضبط البحث والتصفية',
        noGamesForMonth: 'لا توجد مباريات في {month}',
        result: 'النتيجة',
        allResults: 'كل النتائج',
        won: 'فوز',
        lost: 'خسارة',
        draw: 'تعادل',
        color: 'اللون',
        allColors: 'كل الألوان',
        white: 'أبيض',
        black: 'أسود',
        timeAndStatus: 'الوقت والحالة',
        allTimeAndStatus: 'الكل',
        classical: 'كلاسيكي',
        correspondence: 'مراسلة',
        analyzed: 'تم تحليلها',
        notAnalyzed: 'غير محللة',
        sort: 'الترتيب',
        newestFirst: 'الأحدث أولاً',
        oldestFirst: 'الأقدم أولاً',
        accuracyDescending: 'الدقة: من الأعلى',
        accuracyAscending: 'الدقة: من الأدنى',
        reset: 'إعادة ضبط',
        apply: 'تطبيق',
        close: 'إغلاق',
      );
    }
    if (language == 'en') {
      return const _GameFilterLabels(
        filters: 'Filters',
        searchOpponent: 'Search opponent',
        clearMonthCache: 'Clear month cache',
        previousMonth: 'Previous month',
        nextMonth: 'Next month',
        noMatchingGames: 'No matching games',
        noMatchingGamesHelp: 'Change the search or remove some filters.',
        resetFilters: 'Reset search and filters',
        noGamesForMonth: 'No games in {month}',
        result: 'Result',
        allResults: 'All results',
        won: 'Won',
        lost: 'Lost',
        draw: 'Draw',
        color: 'Color',
        allColors: 'All colors',
        white: 'White',
        black: 'Black',
        timeAndStatus: 'Time control & status',
        allTimeAndStatus: 'All',
        classical: 'Classical',
        correspondence: 'Correspondence',
        analyzed: 'Analyzed',
        notAnalyzed: 'Not analyzed',
        sort: 'Sort',
        newestFirst: 'Newest first',
        oldestFirst: 'Oldest first',
        accuracyDescending: 'Accuracy descending',
        accuracyAscending: 'Accuracy ascending',
        reset: 'Reset',
        apply: 'Apply',
        close: 'Close',
      );
    }
    return const _GameFilterLabels(
      filters: 'Filter',
      searchOpponent: 'Gegner suchen',
      clearMonthCache: 'Monatscache löschen',
      previousMonth: 'Vorheriger Monat',
      nextMonth: 'Nächster Monat',
      noMatchingGames: 'Keine passenden Partien',
      noMatchingGamesHelp: 'Ändere die Suche oder entferne einzelne Filter.',
      resetFilters: 'Suche und Filter zurücksetzen',
      noGamesForMonth: 'Keine Partien im {month}',
      result: 'Ergebnis',
      allResults: 'Alle Ergebnisse',
      won: 'Gewonnen',
      lost: 'Verloren',
      draw: 'Remis',
      color: 'Farbe',
      allColors: 'Alle Farben',
      white: 'Weiß',
      black: 'Schwarz',
      timeAndStatus: 'Zeitkontrolle & Status',
      allTimeAndStatus: 'Alle',
      classical: 'Klassisch',
      correspondence: 'Korrespondenz',
      analyzed: 'Analysiert',
      notAnalyzed: 'Nicht analysiert',
      sort: 'Sortierung',
      newestFirst: 'Neueste zuerst',
      oldestFirst: 'Älteste zuerst',
      accuracyDescending: 'Accuracy absteigend',
      accuracyAscending: 'Accuracy aufsteigend',
      reset: 'Zurücksetzen',
      apply: 'Übernehmen',
      close: 'Schließen',
    );
  }
}

class _GamesEmptyState extends StatelessWidget {
  const _GamesEmptyState({
    required this.labels,
    required this.fallbackText,
    required this.hasActiveFilters,
    required this.month,
    required this.onResetFilters,
  });

  final _GameFilterLabels labels;
  final String fallbackText;
  final bool hasActiveFilters;
  final String? month;
  final VoidCallback onResetFilters;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final monthLabel = month == null ? null : _formatGameMonthLabel(context, month!);
    final title = hasActiveFilters
        ? labels.noMatchingGames
        : monthLabel != null
            ? labels.noGamesForMonth.replaceAll('{month}', monthLabel)
            : fallbackText;
    final subtitle = hasActiveFilters ? labels.noMatchingGamesHelp : null;
    return Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 420),
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Container(
                width: 56,
                height: 56,
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: scheme.surfaceContainerHigh,
                  borderRadius: BorderRadius.circular(18),
                ),
                child: Icon(
                  hasActiveFilters
                      ? Icons.filter_alt_off_outlined
                      : Icons.event_busy_outlined,
                  color: scheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 16),
              Text(
                title,
                textAlign: TextAlign.center,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              if (subtitle != null) ...[
                const SizedBox(height: 6),
                Text(
                  subtitle,
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
              if (hasActiveFilters) ...[
                const SizedBox(height: 18),
                OutlinedButton.icon(
                  onPressed: onResetFilters,
                  icon: const Icon(Icons.restart_alt),
                  label: Text(labels.resetFilters),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}


class FavoritesScreen extends StatelessWidget {
  const FavoritesScreen({required this.controller, super.key});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final looseFavorites = controller.favoriteGames
        .where((game) => game.favorite && game.favoriteCollectionId == null)
        .toList(growable: false);

    Future<void> createCollection() async {
      final name = await _showFavoriteCollectionNameDialog(
        context,
        title: strings.favoriteCreateCollection,
        label: strings.favoriteCollectionName,
      );
      if (name == null || !context.mounted) return;
      try {
        await controller.createFavoriteCollection(name);
      } catch (error) {
        if (context.mounted) {
          ScaffoldMessenger.of(context)
              .showSnackBar(SnackBar(content: Text(error.toString())));
        }
      }
    }

    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.favorites))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      strings.favoriteCollectionsTitle,
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      strings.favoriteCollectionRule,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: Theme.of(context).colorScheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 12),
              FilledButton.icon(
                key: const Key('create-favorite-collection'),
                onPressed: createCollection,
                icon: const Icon(Icons.create_new_folder_outlined),
                label: Text(strings.favoriteCreateCollection),
              ),
            ],
          ),
          const SizedBox(height: 14),
          if (controller.favoriteCollections.isEmpty)
            _FavoriteEmptyCard(
              icon: Icons.folder_open_outlined,
              text: strings.favoriteNoCollections,
            )
          else
            LayoutBuilder(
              builder: (context, constraints) {
                final width = constraints.maxWidth >= 760
                    ? (constraints.maxWidth - 12) / 2
                    : constraints.maxWidth;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final collection in controller.favoriteCollections)
                      SizedBox(
                        width: width,
                        child: _FavoriteCollectionCard(
                          collection: collection,
                          onOpen: () => Navigator.of(context).push(
                            MaterialPageRoute<void>(
                              builder: (_) => FavoriteCollectionScreen(
                                controller: controller,
                                collection: collection,
                              ),
                            ),
                          ),
                          onRename: () async {
                            final name = await _showFavoriteCollectionNameDialog(
                              context,
                              title: strings.favoriteRenameCollection,
                              label: strings.favoriteCollectionName,
                              initialValue: collection.name,
                            );
                            if (name == null || !context.mounted) return;
                            try {
                              await controller.renameFavoriteCollection(
                                collection,
                                name,
                              );
                            } catch (error) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(content: Text(error.toString())),
                                );
                              }
                            }
                          },
                          onDelete: () async {
                            final confirmed = await showDialog<bool>(
                              context: context,
                              builder: (dialogContext) => AlertDialog(
                                title: Text(strings.favoriteDeleteCollection),
                                content: Text(strings.favoriteDeleteCollectionBody),
                                actions: [
                                  TextButton(
                                    onPressed: () =>
                                        Navigator.pop(dialogContext, false),
                                    child: Text(strings.cancelAction),
                                  ),
                                  FilledButton(
                                    onPressed: () =>
                                        Navigator.pop(dialogContext, true),
                                    child: Text(strings.deleteAction),
                                  ),
                                ],
                              ),
                            );
                            if (confirmed != true || !context.mounted) return;
                            try {
                              await controller.deleteFavoriteCollection(collection);
                            } catch (error) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(content: Text(error.toString())),
                                );
                              }
                            }
                          },
                        ),
                      ),
                  ],
                );
              },
            ),
          const SizedBox(height: 26),
          Text(
            strings.favoriteLooseTitle,
            style: Theme.of(context).textTheme.titleLarge,
          ),
          const SizedBox(height: 12),
          if (looseFavorites.isEmpty)
            _FavoriteEmptyCard(
              icon: Icons.favorite_border,
              text: strings.favoriteNoLooseGames,
            )
          else
            ...[
              for (final game in looseFavorites) ...[
                _GameCard(
                  game: game,
                  online: game.providerGameId != null,
                  onOpen: () => _openAnalysisAndRefreshSettings(
                    context: context,
                    controller: controller,
                    game: game,
                  ),
                  onToggleFavorite: () => controller.toggleFavorite(game),
                  onSaveToDownloads: null,
                  onMoveFavorite: () => _showFavoriteCollectionPicker(
                    context,
                    controller,
                    game,
                  ),
                  onDelete: null,
                ),
                const SizedBox(height: 12),
              ],
            ],
        ],
      ),
    );
  }
}

class FavoriteCollectionScreen extends StatelessWidget {
  const FavoriteCollectionScreen({
    required this.controller,
    required this.collection,
    super.key,
  });

  final AppController controller;
  final FavoriteCollection collection;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final games = controller.favoriteGames
        .where(
          (game) =>
              game.favorite && game.favoriteCollectionId == collection.id,
        )
        .toList(growable: false);
    return Scaffold(
      appBar: AppBar(title: Text(collection.name)),
      body: games.isEmpty
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(28),
                child: _FavoriteEmptyCard(
                  icon: Icons.folder_open_outlined,
                  text: strings.favoriteEmptyCollection,
                ),
              ),
            )
          : ListView.separated(
              padding: const EdgeInsets.all(16),
              itemCount: games.length,
              separatorBuilder: (_, _) => const SizedBox(height: 12),
              itemBuilder: (context, index) {
                final game = games[index];
                return _GameCard(
                  game: game,
                  online: game.providerGameId != null,
                  onOpen: () => _openAnalysisAndRefreshSettings(
                    context: context,
                    controller: controller,
                    game: game,
                  ),
                  onToggleFavorite: () => controller.toggleFavorite(game),
                  onSaveToDownloads: null,
                  onMoveFavorite: () => _showFavoriteCollectionPicker(
                    context,
                    controller,
                    game,
                  ),
                  onDelete: null,
                );
              },
            ),
    );
  }
}

class _FavoriteCollectionCard extends StatelessWidget {
  const _FavoriteCollectionCard({
    required this.collection,
    required this.onOpen,
    required this.onRename,
    required this.onDelete,
  });

  final FavoriteCollection collection;
  final VoidCallback onOpen;
  final VoidCallback onRename;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    return Card(
      child: ListTile(
        onTap: onOpen,
        contentPadding: const EdgeInsets.fromLTRB(16, 8, 8, 8),
        leading: Container(
          width: 44,
          height: 44,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: scheme.primaryContainer,
            borderRadius: BorderRadius.circular(12),
          ),
          child: Icon(
            Icons.folder_special_outlined,
            color: scheme.onPrimaryContainer,
          ),
        ),
        title: Text(
          collection.name,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(fontWeight: FontWeight.w700),
        ),
        subtitle: Text('${collection.gameCount} ${strings.games}'),
        trailing: PopupMenuButton<String>(
          onSelected: (value) {
            if (value == 'rename') onRename();
            if (value == 'delete') onDelete();
          },
          itemBuilder: (context) => [
            PopupMenuItem(
              value: 'rename',
              child: ListTile(
                dense: true,
                leading: const Icon(Icons.edit_outlined),
                title: Text(strings.favoriteRenameCollection),
              ),
            ),
            PopupMenuItem(
              value: 'delete',
              child: ListTile(
                dense: true,
                leading: const Icon(Icons.delete_outline),
                title: Text(strings.favoriteDeleteCollection),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FavoriteEmptyCard extends StatelessWidget {
  const _FavoriteEmptyCard({required this.icon, required this.text});

  final IconData icon;
  final String text;

  @override
  Widget build(BuildContext context) => Card(
    child: Padding(
      padding: const EdgeInsets.all(22),
      child: Row(
        children: [
          Icon(icon, color: Theme.of(context).colorScheme.onSurfaceVariant),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              text,
              style: TextStyle(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ),
    ),
  );
}

Future<void> _showFavoriteCollectionPicker(
  BuildContext context,
  AppController controller,
  GameSummary game,
) async {
  final strings = AppLocalizations.of(context);
  final currentId = game.favoriteCollectionId ?? '';
  final selectedId = await showDialog<String>(
    context: context,
    builder: (dialogContext) => SimpleDialog(
      title: Text(strings.favoriteMoveToCollection),
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(24, 0, 24, 8),
          child: Text(
            strings.favoriteMoveHelp,
            style: Theme.of(dialogContext).textTheme.bodySmall?.copyWith(
              color: Theme.of(dialogContext).colorScheme.onSurfaceVariant,
            ),
          ),
        ),
        SimpleDialogOption(
          key: const Key('favorite-collection-loose'),
          onPressed: () => Navigator.pop(dialogContext, ''),
          child: ListTile(
            contentPadding: EdgeInsets.zero,
            leading: const Icon(Icons.favorite_outline),
            title: Text(strings.favoriteLooseTitle),
            trailing: currentId.isEmpty ? const Icon(Icons.check) : null,
          ),
        ),
        for (final collection in controller.favoriteCollections)
          SimpleDialogOption(
            key: Key('favorite-collection-${collection.id}'),
            onPressed: () => Navigator.pop(dialogContext, collection.id),
            child: ListTile(
              contentPadding: EdgeInsets.zero,
              leading: const Icon(Icons.folder_outlined),
              title: Text(
                collection.name,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              trailing: currentId == collection.id
                  ? const Icon(Icons.check)
                  : null,
            ),
          ),
      ],
    ),
  );
  if (selectedId == null || selectedId == currentId || !context.mounted) {
    return;
  }

  FavoriteCollection? target;
  if (selectedId.isNotEmpty) {
    for (final collection in controller.favoriteCollections) {
      if (collection.id == selectedId) {
        target = collection;
        break;
      }
    }
    if (target == null) return;
  }

  try {
    await controller.moveFavoriteToCollection(game, target);
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
  }
}

Future<String?> _showFavoriteCollectionNameDialog(
  BuildContext context, {
  required String title,
  required String label,
  String initialValue = '',
}) async {
  final strings = AppLocalizations.of(context);
  final controller = TextEditingController(text: initialValue);
  final result = await showDialog<String>(
    context: context,
    builder: (dialogContext) => AlertDialog(
      title: Text(title),
      content: TextField(
        controller: controller,
        autofocus: true,
        maxLength: 80,
        textInputAction: TextInputAction.done,
        decoration: InputDecoration(labelText: label),
        onSubmitted: (value) {
          final trimmed = value.trim();
          if (trimmed.isNotEmpty) Navigator.pop(dialogContext, trimmed);
        },
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(dialogContext),
          child: Text(strings.cancelAction),
        ),
        FilledButton(
          onPressed: () {
            final trimmed = controller.text.trim();
            if (trimmed.isNotEmpty) Navigator.pop(dialogContext, trimmed);
          },
          child: Text(strings.continueLabel),
        ),
      ],
    ),
  );
  controller.dispose();
  return result;
}

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

class ProfileScreen extends StatelessWidget {
  const ProfileScreen({required this.controller, super.key});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final profile = controller.activeProfile!;
    final mergeTargets = controller.profiles
        .where((value) => value.type != ProfileType.localPgnFen)
        .toList(growable: false);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.profile))
          : null,
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Container(
                        padding: const EdgeInsets.all(3),
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          border: Border.all(
                            color: Theme.of(context).colorScheme.outlineVariant,
                            width: 2,
                          ),
                        ),
                        child: CircleAvatar(
                          radius: 36,
                          backgroundColor:
                              Theme.of(context).colorScheme.primaryContainer,
                          child: _profileAvatar(profile, iconSize: 36),
                        ),
                      ),
                      const SizedBox(width: 18),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              [
                                if (profile.title != null) profile.title,
                                profile.displayName,
                              ].whereType<String>().join(' '),
                              style: Theme.of(context).textTheme.headlineSmall,
                            ),
                            const SizedBox(height: 8),
                            Wrap(
                              spacing: 8,
                              runSpacing: 6,
                              children: [
                                _ProfileTag(
                                  icon: Icons.hub_outlined,
                                  label: _profileTypeLabel(
                                    strings,
                                    profile.type,
                                  ),
                                ),
                                if (profile.providerUsername != null)
                                  _ProfileTag(
                                    icon: Icons.alternate_email,
                                    label: profile.providerUsername!,
                                  ),
                                if (profile.flair != null)
                                  _ProfileTag(
                                    icon: Icons.auto_awesome,
                                    label: profile.flair!,
                                  ),
                              ],
                            ),
                            if (profile.providerDisabled) ...[
                              const SizedBox(height: 8),
                              Text(
                                'Profil deaktiviert',
                                style: TextStyle(
                                  color:
                                      Theme.of(context).colorScheme.error,
                                ),
                              ),
                            ],
                          ],
                        ),
                      ),
                      if (profile.type != ProfileType.localPgnFen)
                        IconButton.filledTonal(
                          tooltip: 'Synchronisieren',
                          onPressed: controller.providerSyncing
                              ? null
                              : controller.syncProvider,
                          icon: const Icon(Icons.sync),
                        ),
                    ],
                  ),
                  if (profile.type == ProfileType.localPgnFen &&
                      mergeTargets.isNotEmpty)
                    Align(
                      alignment: AlignmentDirectional.centerEnd,
                      child: FilledButton.tonalIcon(
                        key: const Key('merge-local-profile'),
                        onPressed: () => _showMergeLocalProfileDialog(
                          context,
                          controller,
                          profile,
                        ),
                        icon: const Icon(Icons.merge_type, size: 18),
                        label: Text(
                          _profileMergeText(
                            context,
                            de: 'Mit Online-Profil zusammenführen',
                            en: 'Merge with online profile',
                            ar: 'دمج مع ملف شخصي عبر الإنترنت',
                          ),
                        ),
                      ),
                    ),
                  Align(
                    alignment: AlignmentDirectional.centerEnd,
                    child: TextButton.icon(
                      key: const Key('delete-active-profile'),
                      onPressed: () =>
                          _confirmDeleteProfile(context, controller, profile),
                      icon: const Icon(Icons.delete_outline, size: 18),
                      label: Text(strings.deleteAccount),
                      style: TextButton.styleFrom(
                        foregroundColor: Theme.of(context).colorScheme.error,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (controller.providerNotice != null)
            Padding(
              padding: const EdgeInsets.only(top: 12),
              child: Text(controller.providerNotice!),
            ),
          if (controller.providerSyncing)
            const Padding(
              padding: EdgeInsets.only(top: 12),
              child: LinearProgressIndicator(),
            ),
          const SizedBox(height: 8),
          if (_ratingEntries(profile).isNotEmpty) ...[
            _ProfileSectionTitle(label: strings.profileRatings),
            LayoutBuilder(
              builder: (context, constraints) {
                final cardWidth = constraints.maxWidth >= 760
                    ? (constraints.maxWidth - 36) / 4
                    : constraints.maxWidth >= 480
                    ? (constraints.maxWidth - 12) / 2
                    : constraints.maxWidth;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final entry in _ratingEntries(profile))
                      SizedBox(
                        width: cardWidth,
                        child: _RatingCard(
                          label: _ratingLabel(strings, entry.$1),
                          rating: entry.$2,
                        ),
                      ),
                  ],
                );
              },
            ),
          ],
          const SizedBox(height: 8),
          _ProfileSectionTitle(label: strings.profileGameOverview),
          LayoutBuilder(
            builder: (context, constraints) {
              final overview = _gameOverview(profile);
              final values = <(IconData, String, int?)>[
                (Icons.sports_esports_outlined, strings.games, overview.games),
                (Icons.emoji_events_outlined, strings.profileWins, overview.wins),
                (Icons.balance_outlined, strings.profileDraws, overview.draws),
                (Icons.close_rounded, strings.profileLosses, overview.losses),
              ].where((entry) => entry.$3 != null).toList(growable: false);
              final width = constraints.maxWidth >= 760
                  ? (constraints.maxWidth - 36) / 4
                  : constraints.maxWidth >= 480
                  ? (constraints.maxWidth - 12) / 2
                  : constraints.maxWidth;
              return Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  for (final value in values)
                    SizedBox(
                      width: width,
                      child: _ProfileMetricCard(
                        icon: value.$1,
                        label: value.$2,
                        value: '${value.$3}',
                      ),
                    ),
                ],
              );
            },
          ),
        ],
      ),
    );
  }

  List<(String, int)> _ratingEntries(AppProfile profile) {
    final entries = <(String, int)>[];
    for (final performance in controller.providerOverview?.stats ?? const <ProviderPerformance>[]) {
      final rating = performance.currentRating;
      if (rating != null) entries.add((performance.key, rating));
    }
    if (profile.fide != null) entries.add(('fide', profile.fide!));
    return entries;
  }

  String _ratingLabel(AppLocalizations strings, String key) {
    final normalized = key.toLowerCase().replaceAll('chess_', '');
    return switch (normalized) {
      'rapid' => strings.ratingRapid,
      'blitz' => strings.ratingBlitz,
      'bullet' => strings.ratingBullet,
      'daily' => strings.ratingDaily,
      'correspondence' => strings.ratingDaily,
      'classical' => strings.ratingClassical,
      'chess960' => strings.ratingChess960,
      '960' => strings.ratingChess960,
      'fide' => strings.ratingFide,
      _ => key.replaceAll('_', ' ').toUpperCase(),
    };
  }

  ({int? games, int? wins, int? draws, int? losses}) _gameOverview(
    AppProfile profile,
  ) {
    final stats = controller.providerOverview?.stats ?? const <ProviderPerformance>[];
    int? sum(Iterable<int?> values) {
      final present = values.whereType<int>().toList(growable: false);
      return present.isEmpty ? null : present.fold<int>(0, (a, b) => a + b);
    }

    final summedGames = sum(stats.map((value) => value.games));
    final games = profile.providerGames ??
        summedGames ??
        (profile.type == ProfileType.localPgnFen ? controller.games.length : null);
    return (
      games: games,
      wins: profile.providerWins ?? sum(stats.map((value) => value.wins)),
      draws: profile.providerDraws ?? sum(stats.map((value) => value.draws)),
      losses: profile.providerLosses ?? sum(stats.map((value) => value.losses)),
    );
  }
}

class _ProfileSectionTitle extends StatelessWidget {
  const _ProfileSectionTitle({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(4, 14, 4, 10),
    child: Text(
      label.toUpperCase(),
      style: Theme.of(context).textTheme.labelMedium?.copyWith(
        color: Theme.of(context).colorScheme.onSurfaceVariant,
        fontWeight: FontWeight.w700,
        letterSpacing: 1.1,
      ),
    ),
  );
}

class _RatingCard extends StatelessWidget {
  const _RatingCard({required this.label, required this.rating});

  final String label;
  final int rating;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
        child: Row(
          children: [
            Icon(
              Icons.speed_rounded,
              color: theme.colorScheme.primary,
              size: 22,
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    label,
                    style: theme.textTheme.labelMedium?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    '$rating',
                    style: theme.textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ProfileMetricCard extends StatelessWidget {
  const _ProfileMetricCard({
    required this.icon,
    required this.label,
    required this.value,
  });

  final IconData icon;
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Container(
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: theme.colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(12),
              ),
              alignment: Alignment.center,
              child: Icon(icon, size: 20),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    value,
                    style: theme.textTheme.titleLarge?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  Text(
                    label,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class StatisticsScreen extends StatefulWidget {
  const StatisticsScreen({required this.controller, super.key});

  final AppController controller;

  @override
  State<StatisticsScreen> createState() => _StatisticsScreenState();
}

class _StatisticsScreenState extends State<StatisticsScreen> {
  late Future<StatisticsOverview> _overview;
  late Future<OpeningsStats> _openings;

  @override
  void initState() {
    super.initState();
    _load();
  }

  void _load() {
    _overview = widget.controller.gateway.statisticsOverview();
    _openings = widget.controller.gateway.openingsStats();
  }

  void _reload() {
    setState(_load);
  }

  @override
  Widget build(BuildContext context) {
    final text = _statisticsText(context);
    final theme = Theme.of(context);
    final sections = <({IconData icon, String title, String body})>[
      (
        icon: Icons.show_chart_rounded,
        title: text.rating,
        body: text.ratingBody,
      ),
      (
        icon: Icons.analytics_outlined,
        title: text.gameQuality,
        body: text.gameQualityBody,
      ),
      (
        icon: Icons.contrast_rounded,
        title: text.whiteBlack,
        body: text.whiteBlackBody,
      ),
      (
        icon: Icons.groups_outlined,
        title: text.opponents,
        body: text.opponentsBody,
      ),
    ];

    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(text.title))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 960),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  text.introTitle,
                  style: theme.textTheme.headlineSmall?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  text.introBody,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 20),
                _OverviewCard(
                  future: _overview,
                  onRetry: _reload,
                ),
                const SizedBox(height: 20),
                _OpeningsCard(
                  future: _openings,
                  onRetry: _reload,
                ),
                const SizedBox(height: 20),
                LayoutBuilder(
                  builder: (context, constraints) {
                    final width = constraints.maxWidth >= 760
                        ? (constraints.maxWidth - 12) / 2
                        : constraints.maxWidth;
                    return Wrap(
                      spacing: 12,
                      runSpacing: 12,
                      children: [
                        for (final section in sections)
                          SizedBox(
                            width: width,
                            child: Card(
                              child: ListTile(
                                contentPadding: const EdgeInsets.symmetric(
                                  horizontal: 18,
                                  vertical: 10,
                                ),
                                leading: Container(
                                  width: 44,
                                  height: 44,
                                  decoration: BoxDecoration(
                                    color: theme.colorScheme.primaryContainer,
                                    borderRadius: BorderRadius.circular(13),
                                  ),
                                  alignment: Alignment.center,
                                  child: Icon(
                                    section.icon,
                                    color: theme.colorScheme.onPrimaryContainer,
                                  ),
                                ),
                                title: Text(
                                  section.title,
                                  style: const TextStyle(
                                    fontWeight: FontWeight.w700,
                                  ),
                                ),
                                subtitle: Padding(
                                  padding: const EdgeInsets.only(top: 4),
                                  child: Text(section.body),
                                ),
                                trailing: Tooltip(
                                  message: text.comingSoon,
                                  child: Icon(
                                    Icons.lock_clock_outlined,
                                    color: theme.colorScheme.onSurfaceVariant,
                                  ),
                                ),
                              ),
                            ),
                          ),
                      ],
                    );
                  },
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _OverviewCard extends StatelessWidget {
  const _OverviewCard({required this.future, required this.onRetry});

  final Future<StatisticsOverview> future;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final labels = _overviewText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: FutureBuilder<StatisticsOverview>(
          future: future,
          builder: (context, snapshot) {
            if (snapshot.connectionState != ConnectionState.done) {
              return const SizedBox(
                height: 140,
                child: Center(child: CircularProgressIndicator()),
              );
            }
            if (snapshot.hasError || !snapshot.hasData) {
              return _OverviewMessage(
                icon: Icons.error_outline,
                text: labels.error,
                action: TextButton(
                  onPressed: onRetry,
                  child: Text(labels.retry),
                ),
              );
            }
            final overview = snapshot.data!;
            if (!overview.hasProfile) {
              return _OverviewMessage(
                icon: Icons.person_outline,
                text: labels.noProfile,
              );
            }
            if (overview.isEmpty) {
              return _OverviewMessage(
                icon: Icons.insights_outlined,
                text: labels.empty,
              );
            }
            return _OverviewContent(overview: overview, labels: labels);
          },
        ),
      ),
    );
  }
}

class _OverviewMessage extends StatelessWidget {
  const _OverviewMessage({required this.icon, required this.text, this.action});

  final IconData icon;
  final String text;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 24),
      child: Column(
        children: [
          Icon(icon, size: 40, color: theme.colorScheme.onSurfaceVariant),
          const SizedBox(height: 12),
          Text(
            text,
            textAlign: TextAlign.center,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
          if (action != null) ...[const SizedBox(height: 8), action!],
        ],
      ),
    );
  }
}

class _OverviewContent extends StatelessWidget {
  const _OverviewContent({required this.overview, required this.labels});

  final StatisticsOverview overview;
  final _OverviewText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final overall = overview.overall;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.dashboard_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Text(
              labels.title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 16),
        Wrap(
          spacing: 28,
          runSpacing: 12,
          children: [
            _StatBig(value: '${overview.totalGames}', label: labels.games),
            _StatBig(value: _formatPercent(overall.winRate), label: labels.winRate),
            _StatBig(
              value: _formatPercent(overall.scorePercent),
              label: labels.score,
            ),
            _StatBig(value: _formatRecord(overall), label: labels.record),
          ],
        ),
        if (overview.recentForm.isNotEmpty) ...[
          const SizedBox(height: 18),
          Text(labels.recentForm, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          Wrap(
            spacing: 4,
            runSpacing: 4,
            children: [
              for (final outcome in overview.recentForm)
                _ResultIcon(outcome: outcome, size: 30),
            ],
          ),
        ],
        if (overview.white.games > 0 || overview.black.games > 0) ...[
          const SizedBox(height: 18),
          Text(labels.byColor, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          if (overview.white.games > 0)
            _TallyLine(label: labels.white, tally: overview.white),
          if (overview.black.games > 0)
            _TallyLine(label: labels.black, tally: overview.black),
        ],
        if (overview.byTimeControl.isNotEmpty) ...[
          const SizedBox(height: 18),
          Text(labels.byTimeControl, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          for (final control in overview.byTimeControl)
            _TallyLine(
              label: _timeControlLabel(control.type),
              tally: control.tally,
            ),
        ],
      ],
    );
  }
}

class _StatBig extends StatelessWidget {
  const _StatBig({required this.value, required this.label});

  final String value;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          value,
          style: theme.textTheme.headlineSmall?.copyWith(
            fontWeight: FontWeight.w800,
          ),
        ),
        Text(
          label,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

class _ResultIcon extends StatelessWidget {
  const _ResultIcon({required this.outcome, this.size = 30});

  final String outcome; // win | loss | draw
  final double size;

  @override
  Widget build(BuildContext context) {
    final asset = switch (outcome) {
      'win' => 'assets/analysis_img/result_win.svg',
      'loss' => 'assets/analysis_img/result_loss.svg',
      'draw' => 'assets/analysis_img/result_draw.svg',
      _ => null,
    };
    if (asset == null) return SizedBox(width: size, height: size);
    final label = switch (outcome) {
      'win' => 'Win',
      'loss' => 'Loss',
      _ => 'Draw',
    };
    return SizedBox(
      width: size,
      height: size,
      child: SvgPicture.asset(
        asset,
        fit: BoxFit.contain,
        semanticsLabel: label,
      ),
    );
  }
}

class _TallyLine extends StatelessWidget {
  const _TallyLine({required this.label, required this.tally});

  final String label;
  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
      child: Row(
        children: [
          SizedBox(
            width: 92,
            child: Text(
              label,
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          Expanded(child: _WdlBar(tally: tally)),
          const SizedBox(width: 12),
          Text(
            '${tally.games} · ${_formatPercent(tally.scorePercent)}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

class _WdlBar extends StatelessWidget {
  const _WdlBar({required this.tally});

  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    if (tally.decided == 0) {
      return Text('—', style: Theme.of(context).textTheme.bodySmall);
    }
    return ClipRRect(
      borderRadius: BorderRadius.circular(4),
      child: SizedBox(
        height: 10,
        child: Row(
          children: [
            if (tally.wins > 0)
              Expanded(
                flex: tally.wins,
                child: const ColoredBox(color: Color(0xFF2E7D32)),
              ),
            if (tally.draws > 0)
              Expanded(
                flex: tally.draws,
                child: const ColoredBox(color: Color(0xFF757575)),
              ),
            if (tally.losses > 0)
              Expanded(
                flex: tally.losses,
                child: const ColoredBox(color: Color(0xFFC62828)),
              ),
          ],
        ),
      ),
    );
  }
}

String _formatPercent(double? value) =>
    value == null ? '—' : '${(value * 100).round()}%';

String _formatRecord(StatTally tally) =>
    '${tally.wins}–${tally.draws}–${tally.losses}';

TextStyle? _overviewSectionLabel(ThemeData theme) =>
    theme.textTheme.labelLarge?.copyWith(
      fontWeight: FontWeight.w700,
      color: theme.colorScheme.onSurfaceVariant,
    );

String _timeControlLabel(String type) => switch (type) {
  'bullet' => 'Bullet',
  'blitz' => 'Blitz',
  'rapid' => 'Rapid',
  'classical' => 'Classical',
  'daily' => 'Daily',
  'correspondence' => 'Correspondence',
  _ => 'Other',
};

class _OverviewText {
  const _OverviewText({
    required this.title,
    required this.games,
    required this.winRate,
    required this.score,
    required this.record,
    required this.recentForm,
    required this.byColor,
    required this.byTimeControl,
    required this.white,
    required this.black,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String games;
  final String winRate;
  final String score;
  final String record;
  final String recentForm;
  final String byColor;
  final String byTimeControl;
  final String white;
  final String black;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
}

_OverviewText _overviewText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _OverviewText(
        title: 'نظرة عامة',
        games: 'المباريات',
        winRate: 'نسبة الفوز',
        score: 'النتيجة',
        record: 'السجل',
        recentForm: 'الأداء الأخير',
        byColor: 'حسب اللون',
        byTimeControl: 'حسب نوع الوقت',
        white: 'أبيض',
        black: 'أسود',
        empty:
            'لا توجد مباريات بعد. زامِن حسابًا على الإنترنت أو استورد مباريات لعرض إحصاءاتك.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
        error: 'تعذّر تحميل الإحصاءات.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _OverviewText(
        title: 'Overview',
        games: 'Games',
        winRate: 'Win rate',
        score: 'Score',
        record: 'Record',
        recentForm: 'Recent form',
        byColor: 'By color',
        byTimeControl: 'By time control',
        white: 'White',
        black: 'Black',
        empty:
            'No games yet. Sync an online profile or import games to see your statistics.',
        noProfile: 'Create or select a profile to see statistics.',
        error: 'Could not load statistics.',
        retry: 'Retry',
      );
    default:
      return const _OverviewText(
        title: 'Übersicht',
        games: 'Partien',
        winRate: 'Siegquote',
        score: 'Score',
        record: 'Bilanz',
        recentForm: 'Aktuelle Form',
        byColor: 'Nach Farbe',
        byTimeControl: 'Nach Zeitkontrolle',
        white: 'Weiß',
        black: 'Schwarz',
        empty:
            'Noch keine Partien. Synchronisiere ein Online-Profil oder importiere Partien, um deine Statistik zu sehen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Statistik konnte nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}

class _OpeningsCard extends StatelessWidget {
  const _OpeningsCard({required this.future, required this.onRetry});

  final Future<OpeningsStats> future;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final labels = _openingsText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: FutureBuilder<OpeningsStats>(
          future: future,
          builder: (context, snapshot) {
            if (snapshot.connectionState != ConnectionState.done) {
              return const SizedBox(
                height: 140,
                child: Center(child: CircularProgressIndicator()),
              );
            }
            if (snapshot.hasError || !snapshot.hasData) {
              return _OverviewMessage(
                icon: Icons.error_outline,
                text: labels.error,
                action: TextButton(
                  onPressed: onRetry,
                  child: Text(labels.retry),
                ),
              );
            }
            final stats = snapshot.data!;
            if (!stats.hasProfile) {
              return _OverviewMessage(
                icon: Icons.person_outline,
                text: labels.noProfile,
              );
            }
            if (stats.isEmpty) {
              return _OverviewMessage(
                icon: Icons.account_tree_outlined,
                text: labels.empty,
              );
            }
            return _OpeningsContent(stats: stats, labels: labels);
          },
        ),
      ),
    );
  }
}

class _OpeningsContent extends StatelessWidget {
  const _OpeningsContent({required this.stats, required this.labels});

  final OpeningsStats stats;
  final _OpeningsText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final mostPlayed = stats.openings.take(8).toList();
    final qualified = stats.openings
        .where((opening) => opening.tally.decided >= 5)
        .toList();
    OpeningStat? best;
    OpeningStat? worst;
    if (qualified.length >= 2) {
      final byScore = [...qualified]..sort(
        (a, b) =>
            (b.tally.scorePercent ?? 0).compareTo(a.tally.scorePercent ?? 0),
      );
      best = byScore.first;
      worst = byScore.last;
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.account_tree_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Text(
              labels.title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Text(
          '${stats.gamesWithOpening} ${labels.classifiedGames}',
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        if (best != null && worst != null) ...[
          const SizedBox(height: 16),
          Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(
                child: _OpeningHighlight(label: labels.strongest, opening: best),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _OpeningHighlight(label: labels.weakest, opening: worst),
              ),
            ],
          ),
        ],
        const SizedBox(height: 16),
        Text(labels.mostPlayed, style: _overviewSectionLabel(theme)),
        const SizedBox(height: 4),
        for (final opening in mostPlayed)
          _OpeningLine(opening: opening, labels: labels),
      ],
    );
  }
}

class _OpeningHighlight extends StatelessWidget {
  const _OpeningHighlight({required this.label, required this.opening});

  final String label;
  final OpeningStat opening;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label.toUpperCase(),
            style: theme.textTheme.labelSmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
              letterSpacing: 0.5,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            opening.name,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(fontWeight: FontWeight.w700),
          ),
          const SizedBox(height: 6),
          Row(
            children: [
              _ColorDot(color: opening.color),
              const SizedBox(width: 6),
              Text(
                _formatPercent(opening.tally.scorePercent),
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
              const SizedBox(width: 6),
              Text(
                '(${opening.tally.games})',
                style: theme.textTheme.bodySmall?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _OpeningLine extends StatelessWidget {
  const _OpeningLine({required this.opening, required this.labels});

  final OpeningStat opening;
  final _OpeningsText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final tally = opening.tally;
    final subtitle = <String>[
      if (opening.eco.isNotEmpty) opening.eco,
      '${tally.games} ${labels.games}',
    ].join(' · ');
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          _ColorDot(color: opening.color),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  opening.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
                const SizedBox(height: 2),
                Text(
                  subtitle,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          SizedBox(width: 84, child: _WdlBar(tally: tally)),
          const SizedBox(width: 10),
          SizedBox(
            width: 42,
            child: Text(
              _formatPercent(tally.scorePercent),
              textAlign: TextAlign.end,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}

class _ColorDot extends StatelessWidget {
  const _ColorDot({required this.color});

  final String color; // white | black | unknown

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final Color fill = switch (color) {
      'white' => Colors.white,
      'black' => Colors.black,
      _ => theme.colorScheme.onSurfaceVariant,
    };
    return Container(
      width: 14,
      height: 14,
      decoration: BoxDecoration(
        color: fill,
        shape: BoxShape.circle,
        border: Border.all(color: theme.colorScheme.outline),
      ),
    );
  }
}

class _OpeningsText {
  const _OpeningsText({
    required this.title,
    required this.mostPlayed,
    required this.strongest,
    required this.weakest,
    required this.classifiedGames,
    required this.games,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String mostPlayed;
  final String strongest;
  final String weakest;
  final String classifiedGames;
  final String games;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
}

_OpeningsText _openingsText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _OpeningsText(
        title: 'الافتتاحيات',
        mostPlayed: 'الأكثر لعبًا',
        strongest: 'الأفضل',
        weakest: 'الأسوأ',
        classifiedGames: 'مباراة بافتتاحية معروفة',
        games: 'مباراة',
        empty:
            'لا توجد افتتاحيات مُصنّفة بعد. تُصنَّف المباريات المستوردة والمتزامنة تلقائيًا.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الافتتاحيات.',
        error: 'تعذّر تحميل الافتتاحيات.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _OpeningsText(
        title: 'Openings',
        mostPlayed: 'Most played',
        strongest: 'Best',
        weakest: 'Worst',
        classifiedGames: 'games with a named opening',
        games: 'games',
        empty:
            'No named openings yet. Synced and imported games are classified automatically.',
        noProfile: 'Create or select a profile to see openings.',
        error: 'Could not load openings.',
        retry: 'Retry',
      );
    default:
      return const _OpeningsText(
        title: 'Eröffnungen',
        mostPlayed: 'Meistgespielt',
        strongest: 'Beste',
        weakest: 'Schwächste',
        classifiedGames: 'Partien mit benannter Eröffnung',
        games: 'Partien',
        empty:
            'Noch keine benannten Eröffnungen. Synchronisierte und importierte Partien werden automatisch klassifiziert.',
        noProfile: 'Erstelle oder wähle ein Profil, um Eröffnungen zu sehen.',
        error: 'Eröffnungen konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}

({
  String title,
  String introTitle,
  String introBody,
  String overview,
  String overviewBody,
  String rating,
  String ratingBody,
  String gameQuality,
  String gameQualityBody,
  String whiteBlack,
  String whiteBlackBody,
  String openings,
  String openingsBody,
  String opponents,
  String opponentsBody,
  String comingSoon,
}) _statisticsText(BuildContext context) {
  return switch (Localizations.localeOf(context).languageCode) {
    'ar' => (
      title: 'الإحصائيات',
      introTitle: 'أداؤك في الشطرنج',
      introBody:
          'سيجمع هذا القسم لاحقًا إحصائيات المباريات والتقييم وتحليل KChess في مكان واحد. تتم إضافة الوحدات خطوة بخطوة.',
      overview: 'نظرة عامة',
      overviewBody: 'المباريات، النتائج، الدقة، الأداء الحالي والاتجاه العام.',
      rating: 'التقييم',
      ratingBody: 'تطور التقييم، أعلى تقييم، والتغير حسب نوع الوقت.',
      gameQuality: 'جودة اللعب',
      gameQualityBody: 'Brilliant وCritical وBest والأخطاء وفق تحليل KChess المحلي.',
      whiteBlack: 'الأبيض / الأسود',
      whiteBlackBody: 'مقارنة النتائج وجودة اللعب عند اللعب بكل لون.',
      openings: 'الافتتاحيات',
      openingsBody: 'الافتتاحيات الأكثر استخدامًا والأقوى والأضعف.',
      opponents: 'الخصوم',
      opponentsBody: 'الأداء أمام خصوم أقوى أو أضعف ومقارنة فروق التقييم.',
      comingSoon: 'سيتم إضافته في الخطوات التالية',
    ),
    'en' => (
      title: 'Statistics',
      introTitle: 'Your chess performance',
      introBody:
          'This area will bring game statistics, rating data and local KChess analysis together. Each module will be added step by step.',
      overview: 'Overview',
      overviewBody: 'Games, results, accuracy, recent form and overall trend.',
      rating: 'Rating',
      ratingBody: 'Rating history, peaks and changes by time control.',
      gameQuality: 'Game quality',
      gameQualityBody: 'Brilliant, Critical, Best and mistakes from local KChess analysis.',
      whiteBlack: 'White / Black',
      whiteBlackBody: 'Compare results and playing quality by color.',
      openings: 'Openings',
      openingsBody: 'Most played, strongest and weakest personal openings.',
      opponents: 'Opponents',
      opponentsBody: 'Performance against stronger and weaker opponents and rating gaps.',
      comingSoon: 'Will be added in the next steps',
    ),
    _ => (
      title: 'Statistiken',
      introTitle: 'Deine Schachleistung',
      introBody:
          'Hier werden später Partiestatistiken, Ratingdaten und lokale KChess-Analysen zusammengeführt. Die Module kommen Schritt für Schritt hinzu.',
      overview: 'Übersicht',
      overviewBody: 'Partien, Ergebnisse, Accuracy, aktuelle Form und Gesamttrend.',
      rating: 'Rating',
      ratingBody: 'Ratingverlauf, Höchstwerte und Veränderungen nach Zeitkontrolle.',
      gameQuality: 'Spielqualität',
      gameQualityBody: 'Brilliant, Critical, Best und Fehler aus der lokalen KChess-Analyse.',
      whiteBlack: 'Weiß / Schwarz',
      whiteBlackBody: 'Ergebnisse und Spielqualität getrennt nach Farbe vergleichen.',
      openings: 'Eröffnungen',
      openingsBody: 'Häufigste, stärkste und schwächste persönliche Eröffnungen.',
      opponents: 'Gegner',
      opponentsBody: 'Leistung gegen stärkere und schwächere Gegner sowie Ratingabstände.',
      comingSoon: 'Wird in den nächsten Schritten ergänzt',
    ),
  };
}

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({required this.controller, super.key});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.settings))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsCategoryTile(
            key: const Key('settings-category-engine'),
            icon: Icons.memory_outlined,
            title: strings.engine,
            subtitle: strings.engineSettingsSubtitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _EngineSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-analysis'),
            icon: Icons.analytics_outlined,
            title: strings.analysisSettingsTitle,
            subtitle: strings.analysisSettingsSubtitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _AnalysisSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-design'),
            icon: Icons.palette_outlined,
            title: strings.designSettingsTitle,
            subtitle: strings.designSettingsSubtitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _DesignSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-general'),
            icon: Icons.tune_outlined,
            title: strings.generalSettingsTitle,
            subtitle: strings.generalSettingsSubtitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _GeneralSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-data-storage'),
            icon: Icons.storage_outlined,
            title: strings.dataStorageSettingsTitle,
            subtitle: strings.dataStorageSettingsSubtitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _DataStorageSettingsPage(controller: controller),
            ),
          ),
        ],
      ),
    );
  }

  static void _openSettingsPage(
    BuildContext context,
    AppController controller,
    Widget Function() pageBuilder,
  ) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => AnimatedBuilder(
          animation: controller,
          builder: (context, _) => pageBuilder(),
        ),
      ),
    );
  }
}

class _SettingsCategoryTile extends StatelessWidget {
  const _SettingsCategoryTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
    super.key,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Card(
      margin: const EdgeInsets.only(bottom: 10),
      clipBehavior: Clip.antiAlias,
      child: ListTile(
        leading: Container(
          width: 44,
          height: 44,
          decoration: BoxDecoration(
            color: scheme.primaryContainer,
            borderRadius: BorderRadius.circular(12),
          ),
          alignment: Alignment.center,
          child: Icon(icon, color: scheme.onPrimaryContainer),
        ),
        title: Text(title),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 3),
          child: Text(subtitle),
        ),
        trailing: const Icon(Icons.chevron_right_rounded),
        onTap: onTap,
      ),
    );
  }
}

class _EngineSettingsPage extends StatelessWidget {
  const _EngineSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.engine)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.engineQualityTitle,
            child: Column(
              children: [
                _DepthRangeSettingTile(
                  key: const Key('engine-depth-range'),
                  title: strings.depth,
                  minimumDepth: controller.settings.minAnalysisDepth,
                  maximumDepth: controller.settings.depth,
                  onMinimumChanged: controller.setMinAnalysisDepth,
                  onMaximumChanged: controller.setDepth,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-lines'),
                  icon: Icons.format_list_numbered,
                  title: strings.numberOfLines,
                  description: strings.numberOfLinesHelp,
                  value: controller.settings.multiPv,
                  minimum: 1,
                  maximum: 8,
                  onChanged: controller.setMultiPv,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-time-limit'),
                  icon: Icons.timer_outlined,
                  title: strings.timeLimitSeconds,
                  description: strings.timeLimitHelp,
                  value: controller.settings.timeLimitSeconds,
                  minimum: 0,
                  maximum: 60,
                  valueLabelBuilder: (value) => value == 0
                      ? strings.noTimeLimit
                      : '$value ${strings.secondsShort}',
                  onChanged: controller.setTimeLimitSeconds,
                ),
              ],
            ),
          ),
          _SettingsSection(
            title: strings.engineResourcesTitle,
            child: Column(
              children: [
                _IntegerSettingTile(
                  key: const Key('engine-threads'),
                  icon: Icons.memory_outlined,
                  title: strings.threads,
                  description: strings.threadsHelp,
                  value: AppController.clampEngineWorkerThreads(
                    controller.settings.threads,
                  ),
                  minimum: 1,
                  maximum: AppController.maximumEngineWorkerThreads,
                  onChanged: controller.setThreads,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-hash'),
                  icon: Icons.storage_outlined,
                  title: strings.hashMemory,
                  description: strings.hashMemoryHelp,
                  value: controller.settings.hashMb,
                  minimum: 16,
                  maximum: 2048,
                  allowedValues: const [16, 32, 64, 128, 256, 512, 1024, 2048],
                  valueLabelBuilder: (value) => '$value MB',
                  onChanged: controller.setHashMb,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _AnalysisSettingsPage extends StatelessWidget {
  const _AnalysisSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.analysisSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.analysisBoardGuidance,
            child: Column(
              children: [
                SwitchListTile(
                  key: const Key('show-best-move-arrow'),
                  secondary: const Icon(Icons.trending_flat),
                  title: Text(strings.bestMoveArrow),
                  subtitle: Text(strings.bestMoveArrowHelp),
                  value: controller.settings.showBestMoveArrow,
                  onChanged: controller.setShowBestMoveArrow,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-threat-arrow'),
                  secondary: const Icon(Icons.warning_amber_rounded),
                  title: Text(strings.threatArrow),
                  subtitle: Text(strings.threatArrowHelp),
                  value: controller.settings.showThreatArrow,
                  onChanged: controller.setShowThreatArrow,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-evaluation-bar'),
                  secondary: const Icon(Icons.stacked_bar_chart),
                  title: Text(strings.evaluationBarSetting),
                  subtitle: Text(strings.evaluationBarSettingHelp),
                  value: controller.settings.showEvaluationBar,
                  onChanged: controller.setShowEvaluationBar,
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          _SettingsSection(
            title: strings.analysisInformation,
            child: Column(
              children: [
                SwitchListTile(
                  key: const Key('show-engine-lines'),
                  secondary: const Icon(Icons.account_tree_outlined),
                  title: Text(strings.showEngineLinesSetting),
                  subtitle: Text(strings.showEngineLinesSettingHelp),
                  value: controller.settings.showEngineLines,
                  onChanged: controller.setShowEngineLines,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-classifications'),
                  secondary: const Icon(Icons.auto_awesome_outlined),
                  title: Text(strings.showClassificationsSetting),
                  subtitle: Text(strings.showClassificationsSettingHelp),
                  value: controller.settings.showClassifications,
                  onChanged: controller.setShowClassifications,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-accuracy'),
                  secondary: const Icon(Icons.percent),
                  title: Text(strings.showAccuracySetting),
                  subtitle: Text(strings.showAccuracySettingHelp),
                  value: controller.settings.showAccuracy,
                  onChanged: controller.setShowAccuracy,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-theory'),
                  secondary: const Icon(Icons.menu_book_outlined),
                  title: Text(strings.showTheorySetting),
                  subtitle: Text(strings.showTheorySettingHelp),
                  value: controller.settings.showTheory,
                  onChanged: controller.setShowTheory,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DesignSettingsPage extends StatelessWidget {
  const _DesignSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.designSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.designSettingsTitle,
            child: ListTile(
              leading: const Icon(Icons.contrast),
              title: Text(strings.theme),
              trailing: DropdownButtonHideUnderline(
                child: DropdownButton<AppThemeMode>(
                  value: controller.settings.themeMode,
                  onChanged: (value) {
                    if (value != null) controller.setThemeMode(value);
                  },
                  items: [
                    DropdownMenuItem(
                      value: AppThemeMode.system,
                      child: Text(strings.systemTheme),
                    ),
                    DropdownMenuItem(
                      value: AppThemeMode.light,
                      child: Text(strings.lightTheme),
                    ),
                    DropdownMenuItem(
                      value: AppThemeMode.dark,
                      child: Text(strings.darkTheme),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.boardDisplayTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.grid_on_outlined),
                  title: Text(strings.showBoardCoordinates),
                  subtitle: Text(strings.showBoardCoordinatesHelp),
                  value: controller.settings.showBoardCoordinates,
                  onChanged: controller.setShowBoardCoordinates,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.compare_arrows),
                  title: Text(strings.highlightLastMove),
                  subtitle: Text(strings.highlightLastMoveHelp),
                  value: controller.settings.highlightLastMove,
                  onChanged: controller.setHighlightLastMove,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.touch_app),
                  title: Text(strings.highlightSelectedSquare),
                  subtitle: Text(strings.highlightSelectedSquareHelp),
                  value: controller.settings.highlightSelectedSquare,
                  onChanged: controller.setHighlightSelectedSquare,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _GeneralSettingsPage extends StatelessWidget {
  const _GeneralSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.generalSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.language,
            child: ListTile(
              leading: const Icon(Icons.translate),
              title: Text(strings.language),
              trailing: DropdownButtonHideUnderline(
                child: DropdownButton<String>(
                  value: controller.settings.locale,
                  onChanged: (value) {
                    if (value != null) controller.setLocale(value);
                  },
                  items: const [
                    DropdownMenuItem(value: 'de', child: Text('Deutsch')),
                    DropdownMenuItem(value: 'en', child: Text('English')),
                    DropdownMenuItem(value: 'ar', child: Text('العربية')),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.behaviorTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.sync),
                  title: Text(strings.autoSyncOnline),
                  subtitle: Text(strings.autoSyncOnlineHelp),
                  value: controller.settings.autoSyncOnline,
                  onChanged: controller.setAutoSyncOnline,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.delete_outline),
                  title: Text(strings.confirmBeforeDelete),
                  subtitle: Text(strings.confirmBeforeDeleteHelp),
                  value: controller.settings.confirmBeforeDelete,
                  onChanged: controller.setConfirmBeforeDelete,
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.licensesAbout,
            child: ListTile(
              leading: const Icon(Icons.info_outline),
              title: Text(strings.licensesAbout),
              subtitle: const Text(
                'KChess 0.1.0 · SQLite 3.53.4 · Stockfish GPLv3',
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DataStorageSettingsPage extends StatelessWidget {
  const _DataStorageSettingsPage({required this.controller});
  final AppController controller;

  Future<void> _clearEngineCache(BuildContext context) async {
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.clearAnalysisCacheQuestion),
        content: Text(strings.clearAnalysisCacheBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(strings.cancelAction),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(strings.clearAnalysisCache),
          ),
        ],
      ),
    );
    if (confirmed != true || !context.mounted) return;
    try {
      await controller.clearEngineCache();
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(strings.analysisCacheCleared)),
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(error.toString())),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.dataStorageSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.analysisCacheTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.cached),
                  title: Text(strings.useGlobalAnalysisCache),
                  subtitle: Text(strings.useGlobalAnalysisCacheHelp),
                  value: controller.settings.useGlobalAnalysisCache,
                  onChanged: controller.setUseGlobalAnalysisCache,
                ),
                const Divider(height: 1),
                ListTile(
                  leading: const Icon(Icons.delete_sweep),
                  title: Text(strings.clearAnalysisCache),
                  subtitle: Text(strings.clearAnalysisCacheHelp),
                  onTap: () => _clearEngineCache(context),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.diagnosticsTitle,
            child: SwitchListTile(
              secondary: const Icon(Icons.bug_report),
              title: Text(strings.diagnosticLogging),
              subtitle: Text(strings.diagnosticLoggingHelp),
              value: controller.settings.diagnosticLogging,
              onChanged: controller.setDiagnosticLogging,
            ),
          ),
        ],
      ),
    );
  }
}

class _DepthRangeSettingTile extends StatelessWidget {
  const _DepthRangeSettingTile({
    required this.title,
    required this.minimumDepth,
    required this.maximumDepth,
    required this.onMinimumChanged,
    required this.onMaximumChanged,
    super.key,
  });

  final String title;
  final int minimumDepth;
  final int maximumDepth;
  final Future<void> Function(int value) onMinimumChanged;
  final Future<void> Function(int value) onMaximumChanged;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Row(
        children: [
          const Icon(Icons.account_tree_outlined),
          const SizedBox(width: 16),
          Text(title, style: Theme.of(context).textTheme.bodyLarge),
          const SizedBox(width: 12),
          Expanded(
            child: FittedBox(
              alignment: Alignment.centerRight,
              fit: BoxFit.scaleDown,
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  _CompactIntegerControl(
                    key: const Key('engine-min-depth'),
                    label: 'Min',
                    value: minimumDepth,
                    minimum: 1,
                    maximum: maximumDepth,
                    onChanged: onMinimumChanged,
                  ),
                  const SizedBox(width: 12),
                  _CompactIntegerControl(
                    key: const Key('engine-max-depth'),
                    label: 'Max',
                    value: maximumDepth,
                    minimum: minimumDepth,
                    maximum: 64,
                    onChanged: onMaximumChanged,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _CompactIntegerControl extends StatefulWidget {
  const _CompactIntegerControl({
    required this.label,
    required this.value,
    required this.minimum,
    required this.maximum,
    required this.onChanged,
    super.key,
  });

  final String label;
  final int value;
  final int minimum;
  final int maximum;
  final Future<void> Function(int value) onChanged;

  @override
  State<_CompactIntegerControl> createState() => _CompactIntegerControlState();
}

class _CompactIntegerControlState extends State<_CompactIntegerControl> {
  late int _value;
  int _pendingWrites = 0;
  Future<void> _writeQueue = Future<void>.value();

  @override
  void initState() {
    super.initState();
    _value = widget.value;
  }

  @override
  void didUpdateWidget(covariant _CompactIntegerControl oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (_pendingWrites == 0 && widget.value != _value) {
      _value = widget.value;
    }
    if (_value < widget.minimum) _value = widget.minimum;
    if (_value > widget.maximum) _value = widget.maximum;
  }

  void _changeValue(int next) {
    final bounded = next.clamp(widget.minimum, widget.maximum).toInt();
    if (bounded == _value) return;
    setState(() {
      _value = bounded;
      _pendingWrites += 1;
    });
    final previousWrite = _writeQueue;
    _writeQueue = () async {
      try {
        await previousWrite;
      } catch (_) {}
      await widget.onChanged(bounded);
    }();
    _writeQueue.then(
      (_) => _finishWrite(),
      onError: (Object _, StackTrace __) => _finishWrite(),
    );
  }

  void _finishWrite() {
    if (!mounted) return;
    setState(() {
      if (_pendingWrites > 0) _pendingWrites -= 1;
      if (_pendingWrites == 0) _value = widget.value;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(widget.label, style: Theme.of(context).textTheme.labelMedium),
        const SizedBox(width: 4),
        IconButton(
          visualDensity: VisualDensity.compact,
          constraints: const BoxConstraints(minWidth: 34, minHeight: 34),
          padding: EdgeInsets.zero,
          onPressed: _value > widget.minimum ? () => _changeValue(_value - 1) : null,
          icon: const Icon(Icons.remove, size: 18),
        ),
        SizedBox(
          width: 44,
          child: TextFormField(
            key: ValueKey('${widget.key}-$_value'),
            initialValue: '$_value',
            textAlign: TextAlign.center,
            keyboardType: TextInputType.number,
            textInputAction: TextInputAction.done,
            decoration: const InputDecoration(
              isDense: true,
              contentPadding: EdgeInsets.symmetric(vertical: 8, horizontal: 4),
            ),
            onFieldSubmitted: (text) {
              final parsed = int.tryParse(text);
              if (parsed != null) _changeValue(parsed);
            },
          ),
        ),
        IconButton(
          visualDensity: VisualDensity.compact,
          constraints: const BoxConstraints(minWidth: 34, minHeight: 34),
          padding: EdgeInsets.zero,
          onPressed: _value < widget.maximum ? () => _changeValue(_value + 1) : null,
          icon: const Icon(Icons.add, size: 18),
        ),
      ],
    );
  }
}

class _IntegerSettingTile extends StatefulWidget {
  const _IntegerSettingTile({
    required this.icon,
    required this.title,
    required this.value,
    required this.minimum,
    required this.maximum,
    required this.onChanged,
    this.description,
    this.valueLabel,
    this.valueLabelBuilder,
    this.allowedValues,
    this.editable = false,
    super.key,
  });

  final IconData icon;
  final String title;
  final String? description;
  final int value;
  final int minimum;
  final int maximum;
  final String? valueLabel;
  final String Function(int value)? valueLabelBuilder;
  final List<int>? allowedValues;
  final bool editable;
  final Future<void> Function(int value) onChanged;

  @override
  State<_IntegerSettingTile> createState() => _IntegerSettingTileState();
}

class _IntegerSettingTileState extends State<_IntegerSettingTile> {
  late int _value;
  int _pendingWrites = 0;
  Future<void> _writeQueue = Future<void>.value();

  @override
  void initState() {
    super.initState();
    _value = widget.value;
  }

  @override
  void didUpdateWidget(covariant _IntegerSettingTile oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (_pendingWrites == 0 && widget.value != _value) {
      _value = widget.value;
    }
  }

  int? get _previousValue {
    final values = widget.allowedValues;
    if (values == null) {
      return _value > widget.minimum ? _value - 1 : null;
    }
    final index = values.indexOf(_value);
    if (index > 0) return values[index - 1];
    if (index == -1) {
      final lower = values.where((candidate) => candidate < _value).toList();
      return lower.isEmpty ? null : lower.last;
    }
    return null;
  }

  int? get _nextValue {
    final values = widget.allowedValues;
    if (values == null) {
      return _value < widget.maximum ? _value + 1 : null;
    }
    final index = values.indexOf(_value);
    if (index >= 0 && index < values.length - 1) return values[index + 1];
    if (index == -1) {
      final higher = values.where((candidate) => candidate > _value).toList();
      return higher.isEmpty ? null : higher.first;
    }
    return null;
  }

  void _changeValue(int next) {
    if (next == _value) return;
    setState(() {
      _value = next;
      _pendingWrites += 1;
    });

    final previousWrite = _writeQueue;
    _writeQueue = () async {
      try {
        await previousWrite;
      } catch (_) {
        // A failed earlier write must not block later user input.
      }
      await widget.onChanged(next);
    }();

    _writeQueue.then(
      (_) => _finishWrite(),
      onError: (Object _, StackTrace __) => _finishWrite(),
    );
  }

  void _finishWrite() {
    if (!mounted) return;
    setState(() {
      if (_pendingWrites > 0) _pendingWrites -= 1;
      if (_pendingWrites == 0) {
        _value = widget.value;
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final previous = _previousValue;
    final next = _nextValue;
    final range = '${widget.minimum}–${widget.maximum}';
    final label = widget.valueLabelBuilder?.call(_value) ??
        widget.valueLabel ??
        '$_value';
    return ListTile(
      leading: Icon(widget.icon),
      title: Text(widget.title),
      subtitle: widget.description == null
          ? Text(range)
          : Text('${widget.description}\n$range'),
      isThreeLine: widget.description != null,
      trailing: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          IconButton(
            onPressed: previous == null ? null : () => _changeValue(previous),
            icon: const Icon(Icons.remove),
          ),
          SizedBox(
            width: 72,
            child: widget.editable
                ? TextFormField(
                    key: ValueKey('integer-${widget.key}-$_value'),
                    initialValue: '$_value',
                    textAlign: TextAlign.center,
                    keyboardType: TextInputType.number,
                    textInputAction: TextInputAction.done,
                    decoration: const InputDecoration(isDense: true),
                    onFieldSubmitted: (text) {
                      final parsed = int.tryParse(text);
                      if (parsed != null) {
                        final bounded = parsed < widget.minimum
                            ? widget.minimum
                            : (parsed > widget.maximum ? widget.maximum : parsed);
                        _changeValue(bounded);
                      }
                    },
                  )
                : Text(label, textAlign: TextAlign.center, textDirection: TextDirection.ltr),
          ),
          IconButton(
            onPressed: next == null ? null : () => _changeValue(next),
            icon: const Icon(Icons.add),
          ),
        ],
      ),
    );
  }
}

class _EmptySection extends StatelessWidget {
  const _EmptySection({required this.title});
  final String title;

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: MediaQuery.sizeOf(context).width >= 900
        ? AppBar(title: Text(title))
        : null,
    body: Center(child: Text(AppLocalizations.of(context).emptySection)),
  );
}

/// A rich game row: outcome accent, time-control badge, both players with
/// piece markers and rating pills, a result pill, accuracy and quick actions.
class _GameCard extends StatelessWidget {
  const _GameCard({
    required this.game,
    required this.online,
    this.playerIdentity,
    required this.onOpen,
    required this.onToggleFavorite,
    this.onSaveToDownloads,
    this.onMoveFavorite,
    this.onDelete,
  });

  final GameSummary game;
  final bool online;
  final String? playerIdentity;
  final VoidCallback onOpen;
  final VoidCallback onToggleFavorite;
  final VoidCallback? onSaveToDownloads;
  final VoidCallback? onMoveFavorite;
  final VoidCallback? onDelete;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final outcome = AppTheme.outcomeColor(context, game.providerOutcome);
    final accuracy = game.accuracy == null
        ? '—'
        : '${game.accuracy!.toStringAsFixed(1)}%';
    final meta = [
      if (game.event.isNotEmpty) game.event,
      if (game.date.isNotEmpty) game.date,
    ].join('  ·  ');
    final identity = playerIdentity?.trim().toLowerCase() ?? '';
    final isCurrentWhite =
        identity.isNotEmpty && game.whiteName.trim().toLowerCase() == identity;
    final isCurrentBlack =
        identity.isNotEmpty && game.blackName.trim().toLowerCase() == identity;

    return Card(
      child: InkWell(
        key: Key('game-${game.id}'),
        onTap: onOpen,
        child: DecoratedBox(
          decoration: BoxDecoration(
            border: Border(left: BorderSide(color: outcome, width: 4)),
          ),
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 6, 12),
            child: Row(
              children: [
                _TimeControlBadge(game: game),
                const SizedBox(width: 14),
                Expanded(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _PlayerLine(
                        name: game.whiteName,
                        rating: game.whiteRating,
                        isWhitePiece: true,
                        isCurrentPlayer: isCurrentWhite,
                        emphasized: isCurrentWhite,
                      ),
                      const SizedBox(height: 5),
                      _PlayerLine(
                        name: game.blackName,
                        rating: game.blackRating,
                        isWhitePiece: false,
                        isCurrentPlayer: isCurrentBlack,
                        emphasized: isCurrentBlack,
                      ),
                      if (meta.isNotEmpty) ...[
                        const SizedBox(height: 7),
                        Text(
                          meta,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: scheme.onSurfaceVariant,
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: [
                    Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 10,
                        vertical: 4,
                      ),
                      decoration: BoxDecoration(
                        color: outcome.withValues(alpha: 0.14),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Text(
                        game.result,
                        style: theme.textTheme.labelLarge?.copyWith(
                          color: outcome,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                    const SizedBox(height: 6),
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          Icons.insights_rounded,
                          size: 14,
                          color: scheme.onSurfaceVariant,
                        ),
                        const SizedBox(width: 4),
                        Text(
                          accuracy,
                          style: theme.textTheme.labelMedium?.copyWith(
                            color: scheme.onSurface,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
                Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    IconButton(
                      key: Key('favorite-${game.id}'),
                      tooltip: 'Favorit',
                      visualDensity: VisualDensity.compact,
                      onPressed: onToggleFavorite,
                      icon: Icon(
                        game.favorite ? Icons.favorite : Icons.favorite_border,
                        color: game.favorite
                            ? scheme.error
                            : scheme.onSurfaceVariant,
                      ),
                    ),
                    if (onMoveFavorite != null)
                      IconButton(
                        key: Key('move-favorite-${game.id}'),
                        tooltip: AppLocalizations.of(context)
                            .favoriteMoveToCollection,
                        visualDensity: VisualDensity.compact,
                        onPressed: onMoveFavorite,
                        icon: Icon(
                          Icons.drive_file_move_outline,
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    if (online &&
                        game.providerGameId != null &&
                        onSaveToDownloads != null)
                      IconButton(
                        key: Key('download-${game.id}'),
                        tooltip: 'In Downloads speichern',
                        visualDensity: VisualDensity.compact,
                        onPressed: onSaveToDownloads,
                        icon: Icon(
                          Icons.download,
                          color: scheme.onSurfaceVariant,
                        ),
                      )
                    else if (game.providerGameId == null && onDelete != null)
                      IconButton(
                        key: Key('delete-local-${game.id}'),
                        tooltip: 'Lokalen Eintrag löschen',
                        visualDensity: VisualDensity.compact,
                        onPressed: onDelete,
                        icon: Icon(
                          Icons.delete_outline,
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// Rounded, tinted badge that conveys a game's time control at a glance.
class _TimeControlBadge extends StatelessWidget {
  const _TimeControlBadge({required this.game});

  final GameSummary game;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final (icon, color) = _style(scheme);
    return Container(
      width: 46,
      height: 46,
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.14),
        borderRadius: BorderRadius.circular(13),
      ),
      child: Icon(icon, color: color, size: 22),
    );
  }

  (IconData, Color) _style(ColorScheme scheme) =>
      switch (game.timeControlType) {
        'bullet' => (Icons.bolt, const Color(0xFFE0663B)),
        'blitz' => (Icons.flash_on, AppTheme.warning),
        'rapid' => (Icons.timer_outlined, AppTheme.success),
        'classical' => (Icons.hourglass_bottom, scheme.primary),
        'daily' ||
        'correspondence' => (Icons.calendar_today, const Color(0xFF7C6FF0)),
        _ => (
          game.kind == 'fen'
              ? Icons.grid_on_outlined
              : Icons.description_outlined,
          scheme.onSurfaceVariant,
        ),
      };
}

/// One player row: a hollow (white) or solid (black) piece marker, the name,
/// and an optional rating pill.
class _PlayerLine extends StatelessWidget {
  const _PlayerLine({
    required this.name,
    required this.rating,
    required this.isWhitePiece,
    required this.isCurrentPlayer,
    required this.emphasized,
  });

  final String name;
  final int? rating;
  final bool isWhitePiece;
  final bool isCurrentPlayer;
  final bool emphasized;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Row(
      children: [
        SizedBox(
          width: 20,
          child: isCurrentPlayer
              ? Text(
                  isWhitePiece ? '♙' : '♟',
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 18,
                    height: 1,
                    color: scheme.onSurface,
                  ),
                )
              : null,
        ),
        const SizedBox(width: 7),
        Flexible(
          child: Text(
            name,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: emphasized
                ? theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w700,
                  )
                : theme.textTheme.bodyMedium?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
          ),
        ),
        if (rating != null) ...[
          const SizedBox(width: 7),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 1),
            decoration: BoxDecoration(
              color: scheme.surfaceContainerHigh,
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(
              '$rating',
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ],
    );
  }
}

/// A small labelled tag used in the profile header (provider, handle, flair …).
class _ProfileTag extends StatelessWidget {
  const _ProfileTag({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(9),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: scheme.onSurfaceVariant),
          const SizedBox(width: 6),
          Text(
            label,
            style: theme.textTheme.bodySmall?.copyWith(
              color: scheme.onSurface,
            ),
          ),
        ],
      ),
    );
  }
}

/// Proportional win / draw / loss ribbon for a performance card.
class _WinLossBar extends StatelessWidget {
  const _WinLossBar({
    required this.wins,
    required this.draws,
    required this.losses,
  });

  final int wins;
  final int draws;
  final int losses;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final segments = <Widget>[
      if (wins > 0)
        Expanded(flex: wins, child: const ColoredBox(color: AppTheme.success)),
      if (draws > 0)
        Expanded(
          flex: draws,
          child: ColoredBox(color: scheme.onSurfaceVariant),
        ),
      if (losses > 0)
        Expanded(flex: losses, child: ColoredBox(color: scheme.error)),
    ];
    return ClipRRect(
      borderRadius: BorderRadius.circular(6),
      child: SizedBox(
        height: 8,
        child: segments.isEmpty
            ? ColoredBox(color: scheme.surfaceContainerHighest)
            : Row(children: segments),
      ),
    );
  }
}

/// A titled group of settings rendered as a bordered card.
class _SettingsSection extends StatelessWidget {
  const _SettingsSection({required this.title, required this.child});

  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(4, 18, 4, 8),
          child: Text(
            title.toUpperCase(),
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
              fontWeight: FontWeight.w700,
              letterSpacing: 1.1,
            ),
          ),
        ),
        Card(child: child),
      ],
    );
  }
}
