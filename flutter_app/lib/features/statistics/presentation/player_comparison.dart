// -----------------------------------------------------------------------------
// Section: player comparison presentation
// -----------------------------------------------------------------------------

part of '../../../ui/app_root.dart';

/// "Spielervergleich": scout a public Chess.com player and compare their
/// profile, ratings, colour performance, flagging tendency and opening
/// repertoire against the active profile, highlighting exploitable leaks. The
/// opponent's stats are aggregated natively from recent archives; the head-to-
/// head record comes from the user's own stored games.
class _PlayerComparisonScreen extends StatefulWidget {
  const _PlayerComparisonScreen({required this.controller});

  final AppController controller;

  @override
  State<_PlayerComparisonScreen> createState() =>
      _PlayerComparisonScreenState();
}

class _PlayerComparisonScreenState extends State<_PlayerComparisonScreen> {
  final _search = TextEditingController();
  bool _loading = false;
  String? _error;
  ScoutReport? _report;
  StatisticsOverview? _userOverview;
  OpeningsStats? _userOpenings;
  TerminationStats? _userTerminations;
  StatTally? _h2h; // from the active profile's perspective
  bool _isSelf = false;
  int _generation = 0;

  @override
  void dispose() {
    _search.dispose();
    super.dispose();
  }

  Future<void> _compare() async {
    final username = _search.text.trim();
    if (username.isEmpty) return;
    FocusScope.of(context).unfocus();
    final gateway = widget.controller.gateway;
    final generation = ++_generation;
    final profileId = widget.controller.activeProfile?.id;
    setState(() {
      _loading = true;
      _error = null;
      _report = null;
    });
    try {
      // Opponent aggregation (network) first; the user's own stats are local.
      final report = await gateway.scoutReport(username);
      final comparison = report.comparison;
      if (profileId != widget.controller.activeProfile?.id ||
          comparison.profileId != (profileId ?? '')) {
        if (mounted && generation == _generation)
          setState(() => _loading = false);
        return;
      }
      if (!mounted || generation != _generation) return;
      setState(() {
        _isSelf = comparison.isSelf;
        _report = report;
        _userOverview = comparison.userOverview;
        _userOpenings = comparison.userOpenings;
        _userTerminations = comparison.userTerminations;
        _h2h = comparison.headToHead;
        _loading = false;
      });
    } catch (error) {
      if (!mounted || generation != _generation) return;
      setState(() {
        _error = AppLocalizations.of(context).statsOverviewError;
        _loading = false;
      });
      final labels = _comparisonText(context);
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('${labels.errorPrefix}: $_error')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final labels = _comparisonText(context);
    return Scaffold(
      appBar: AppBar(title: Text(labels.title)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 28),
        children: [
          Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 900),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  _searchBar(context, labels),
                  const SizedBox(height: 20),
                  _body(context, labels),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _searchBar(BuildContext context, _ComparisonText labels) {
    return Row(
      children: [
        Expanded(
          child: TextField(
            controller: _search,
            textInputAction: TextInputAction.search,
            onSubmitted: (_) => _compare(),
            decoration: InputDecoration(
              prefixIcon: const Icon(Icons.person_search_outlined),
              labelText: labels.usernameLabel,
              hintText: labels.usernameHint,
            ),
          ),
        ),
        const SizedBox(width: 12),
        FilledButton.icon(
          onPressed: _loading ? null : _compare,
          icon: const Icon(Icons.compare_arrows),
          label: Text(labels.compare),
        ),
      ],
    );
  }

  Widget _body(BuildContext context, _ComparisonText labels) {
    if (_loading) {
      return Padding(
        padding: const EdgeInsets.symmetric(vertical: 48),
        child: Column(
          children: [
            const CircularProgressIndicator(),
            const SizedBox(height: 16),
            Text(
              labels.loadingHint,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      );
    }
    if (_error != null) {
      return _ComparisonMessage(icon: Icons.error_outline, text: _error!);
    }
    final report = _report;
    if (report == null) {
      return _ComparisonMessage(
        icon: Icons.groups_2_outlined,
        text: labels.prompt,
      );
    }
    return _ComparisonResult(
      controller: widget.controller,
      report: report,
      userOverview: _userOverview,
      userOpenings: _userOpenings,
      userTerminations: _userTerminations,
      h2h: _h2h,
      isSelf: _isSelf,
      labels: labels,
    );
  }
}

class _ComparisonMessage extends StatelessWidget {
  const _ComparisonMessage({required this.icon, required this.text});

  final IconData icon;
  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 44),
      child: Column(
        children: [
          Icon(icon, size: 44, color: theme.colorScheme.onSurfaceVariant),
          const SizedBox(height: 14),
          Text(
            text,
            textAlign: TextAlign.center,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

class _ComparisonResult extends StatelessWidget {
  const _ComparisonResult({
    required this.controller,
    required this.report,
    required this.userOverview,
    required this.userOpenings,
    required this.userTerminations,
    required this.h2h,
    required this.isSelf,
    required this.labels,
  });

  final AppController controller;
  final ScoutReport report;
  final StatisticsOverview? userOverview;
  final OpeningsStats? userOpenings;
  final TerminationStats? userTerminations;
  final StatTally? h2h;
  final bool isSelf;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final userProfile = controller.activeProfile;
    final userStats = controller.providerOverview?.stats ?? const [];

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        IntrinsicHeight(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(
                child: _ComparisonProfileCard(
                  profile: userProfile,
                  stats: userStats,
                  caption: labels.you,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: _ComparisonProfileCard(
                  profile: report.profile,
                  stats: report.stats,
                  caption: labels.opponent,
                ),
              ),
            ],
          ),
        ),
        if (isSelf) ...[
          const SizedBox(height: 12),
          Align(child: _SelfComparisonBadge(labels: labels)),
          const SizedBox(height: 16),
          _SelfComparisonH2HCard(labels: labels),
        ] else if (h2h != null && h2h!.games > 0) ...[
          const SizedBox(height: 16),
          _H2HBanner(tally: h2h!, labels: labels),
        ],
        const SizedBox(height: 16),
        _PerformanceComparisonCard(
          userOverview: userOverview,
          userTerminations: userTerminations,
          report: report,
          labels: labels,
        ),
        const SizedBox(height: 16),
        _OpeningMatchupCard(
          userOpenings: userOpenings,
          report: report,
          isSelf: isSelf,
          labels: labels,
        ),
        const SizedBox(height: 12),
        Text(
          labels.gamesAnalyzed(report.gamesAnalyzed, report.monthsFetched),
          textAlign: TextAlign.center,
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
        ),
      ],
    );
  }
}

class _ComparisonProfileCard extends StatelessWidget {
  const _ComparisonProfileCard({
    required this.profile,
    required this.stats,
    required this.caption,
  });

  final AppProfile? profile;
  final List<ProviderPerformance> stats;
  final String caption;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final name = profile?.displayName ?? '—';
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              caption,
              style: theme.textTheme.labelSmall?.copyWith(
                color: scheme.onSurfaceVariant,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                ClipOval(
                  child: SizedBox(
                    width: 44,
                    height: 44,
                    child: profile == null
                        ? Icon(Icons.person, color: scheme.onSurfaceVariant)
                        : _comparisonAvatar(profile!),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        name,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                      if (profile?.title != null)
                        Text(
                          profile!.title!,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: scheme.tertiary,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            for (final key in const ['blitz', 'bullet', 'rapid'])
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Row(
                  children: [
                    Icon(
                      _timeControlIcon(key),
                      size: 15,
                      color: scheme.onSurfaceVariant,
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: Text(
                        _timeControlLabel(context, key),
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    ),
                    Text(
                      _ratingFor(stats, key)?.toString() ?? '—',
                      style: const TextStyle(fontWeight: FontWeight.w700),
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

class _H2HBanner extends StatelessWidget {
  const _H2HBanner({required this.tally, required this.labels});

  final StatTally tally;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: scheme.primaryContainer,
        borderRadius: BorderRadius.circular(16),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(Icons.sports_kabaddi, color: scheme.onPrimaryContainer),
              const SizedBox(width: 8),
              Text(
                labels.h2hTitle,
                style: theme.textTheme.titleSmall?.copyWith(
                  color: scheme.onPrimaryContainer,
                  fontWeight: FontWeight.w800,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            '${tally.wins} ${labels.wins} · ${tally.draws} ${labels.draws} · ${tally.losses} ${labels.losses}',
            style: theme.textTheme.titleMedium?.copyWith(
              color: scheme.onPrimaryContainer,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 10),
          _WinLossDrawRatioBar(
            wins: tally.wins,
            draws: tally.draws,
            losses: tally.losses,
            height: 8,
          ),
          const SizedBox(height: 6),
          Text(
            '${tally.games} ${labels.directGames}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: scheme.onPrimaryContainer.withValues(alpha: 0.8),
            ),
          ),
        ],
      ),
    );
  }
}

/// Side-by-side comparative bars: win rate as White, win rate as Black, and the
/// share of losses that come on time (flagging tendency).
class _PerformanceComparisonCard extends StatelessWidget {
  const _PerformanceComparisonCard({
    required this.userOverview,
    required this.userTerminations,
    required this.report,
    required this.labels,
  });

  final StatisticsOverview? userOverview;
  final TerminationStats? userTerminations;
  final ScoutReport report;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final userWhite = userOverview?.white.winRate;
    final userBlack = userOverview?.black.winRate;
    final userFlag = report.comparison.userFlagRate;
    final oppFlag = report.comparison.opponentFlagRate;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              labels.performanceCompare,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 6),
            Wrap(
              spacing: 16,
              children: [
                _legendDot(context, scheme.primary, labels.you),
                _legendDot(context, scheme.tertiary, labels.opponent),
              ],
            ),
            const SizedBox(height: 6),
            // The two columns are not the same sample: the user's side is the
            // whole local library, the opponent's is a live 6-month window. Say
            // so, otherwise a self-comparison looks broken when the mirrored
            // rates fail to match.
            Text(
              labels.sampleScope(report.monthsFetched, report.gamesAnalyzed),
              style: theme.textTheme.bodySmall?.copyWith(color: scheme.outline),
            ),
            const SizedBox(height: 12),
            _CompareMetric(
              label: labels.winRateWhite,
              userValue: userWhite,
              oppValue: report.white.winRate,
              labels: labels,
            ),
            const SizedBox(height: 14),
            _CompareMetric(
              label: labels.winRateBlack,
              userValue: userBlack,
              oppValue: report.black.winRate,
              labels: labels,
            ),
            const SizedBox(height: 14),
            _CompareMetric(
              label: labels.flagging,
              userValue: userFlag,
              oppValue: oppFlag,
              labels: labels,
            ),
          ],
        ),
      ),
    );
  }

  Widget _legendDot(BuildContext context, Color color, String text) => Row(
    mainAxisSize: MainAxisSize.min,
    children: [
      Container(
        width: 9,
        height: 9,
        decoration: BoxDecoration(color: color, shape: BoxShape.circle),
      ),
      const SizedBox(width: 6),
      Text(text, style: Theme.of(context).textTheme.bodySmall),
    ],
  );
}

class _CompareMetric extends StatelessWidget {
  const _CompareMetric({
    required this.label,
    required this.userValue,
    required this.oppValue,
    required this.labels,
  });

  final String label;
  final double? userValue; // 0..1 fraction
  final double? oppValue;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: const TextStyle(fontWeight: FontWeight.w700)),
        const SizedBox(height: 6),
        _bar(context, labels.you, userValue, scheme.primary),
        const SizedBox(height: 5),
        _bar(context, labels.opponent, oppValue, scheme.tertiary),
      ],
    );
  }

  Widget _bar(BuildContext context, String who, double? value, Color color) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final fraction = (value ?? 0).clamp(0.0, 1.0).toDouble();
    final text = value == null ? '—' : '${(value * 100).round()}%';
    return Row(
      children: [
        SizedBox(
          width: 64,
          child: Text(
            who,
            style: theme.textTheme.bodySmall?.copyWith(
              color: scheme.onSurfaceVariant,
            ),
          ),
        ),
        Expanded(
          child: SizedBox(
            height: 12,
            child: Stack(
              children: [
                Positioned.fill(
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: scheme.surfaceContainerHighest,
                      borderRadius: BorderRadius.circular(6),
                    ),
                  ),
                ),
                FractionallySizedBox(
                  alignment: Alignment.centerLeft,
                  widthFactor: fraction,
                  heightFactor: 1,
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: color,
                      borderRadius: BorderRadius.circular(6),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(width: 10),
        SizedBox(
          width: 42,
          child: Text(
            text,
            textAlign: TextAlign.end,
            style: const TextStyle(fontWeight: FontWeight.w700),
          ),
        ),
      ],
    );
  }
}

/// Opening matchup: user's repertoire vs. the opponent's win rate in the same
/// ECO with the opposite colour, and the exploitable leaks (opponent under 45%).
class _OpeningMatchupCard extends StatelessWidget {
  const _OpeningMatchupCard({
    required this.userOpenings,
    required this.report,
    required this.isSelf,
    required this.labels,
  });

  final OpeningsStats? userOpenings;
  final ScoutReport report;
  final bool isSelf;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final matchups = report.comparison.matchups;
    final leaks = report.comparison.recommendations;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              labels.openingMatchup,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              labels.matchupSubtitle,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              '${labels.scopeNote(report.gamesAnalyzed)} '
              '${labels.minSampleNote}',
              style: theme.textTheme.bodySmall?.copyWith(color: scheme.outline),
            ),
            const SizedBox(height: 14),
            if (matchups.isEmpty)
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 12),
                child: Text(
                  labels.noMatchups,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              )
            else ...[
              Row(
                children: [
                  Expanded(
                    child: Text(labels.openingColumn, style: _colHead(theme)),
                  ),
                  SizedBox(
                    width: 52,
                    child: Text(
                      labels.you,
                      textAlign: TextAlign.end,
                      style: _colHead(theme),
                    ),
                  ),
                  SizedBox(
                    width: 52,
                    child: Text(
                      labels.opponent,
                      textAlign: TextAlign.end,
                      style: _colHead(theme),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 4),
              for (final m in matchups)
                _MatchupRow(matchup: m, isSelf: isSelf, labels: labels),
            ],
            if (isSelf) ...[
              const SizedBox(height: 18),
              _OwnWeaknessSection(
                weaknesses: report.comparison.weaknesses,
                labels: labels,
              ),
            ] else if (leaks.isNotEmpty) ...[
              const SizedBox(height: 18),
              _StrategySection(leaks: leaks, labels: labels),
            ] else if (matchups.isNotEmpty) ...[
              const SizedBox(height: 14),
              Text(
                labels.noLeaks,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  TextStyle? _colHead(ThemeData theme) => theme.textTheme.labelSmall?.copyWith(
    color: theme.colorScheme.onSurfaceVariant,
    fontWeight: FontWeight.w700,
  );
}

class _MatchupRow extends StatelessWidget {
  const _MatchupRow({
    required this.matchup,
    required this.isSelf,
    required this.labels,
  });

  final OpeningMatchup matchup;
  final bool isSelf;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final family = matchup.family;
    final opponent = matchup.opponent;
    // Naming the mirrored side keeps a self-audit from reading as a real
    // opponent with coincidentally identical numbers.
    final opponentLabel = isSelf
        ? '${labels.opponent} (${labels.you})'
        : labels.opponent;
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 3),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
      decoration: BoxDecoration(
        color: matchup.exploitable
            ? AppTheme.success.withValues(alpha: 0.12)
            : scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(
          color: matchup.exploitable
              ? AppTheme.success.withValues(alpha: 0.5)
              : scheme.outlineVariant,
        ),
      ),
      child: Row(
        children: [
          _ColorDot(color: family.color),
          const SizedBox(width: 8),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  family.familyName,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
                // Both sample sizes, so a 54%-from-34 row is never mistaken for
                // a 33%-from-3 row.
                Text(
                  [
                    if (family.baseEco.isNotEmpty) family.baseEco,
                    '${labels.you}: ${family.tally.games} '
                        '${labels.gamesShort} '
                        '(${_formatPercent(family.tally.winRate)})',
                    '$opponentLabel: ${opponent.games} '
                        '${labels.gamesShort} '
                        '(${_formatPercent(opponent.winRate)})',
                  ].join('  ·  '),
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          SizedBox(
            width: 52,
            child: Text(
              _formatPercent(family.tally.winRate),
              textAlign: TextAlign.end,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
          SizedBox(
            width: 52,
            child: Text(
              _formatPercent(opponent.winRate),
              textAlign: TextAlign.end,
              style: TextStyle(
                fontWeight: FontWeight.w700,
                color: matchup.exploitable ? AppTheme.success : null,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// Recommendations, phrased so the side that actually chooses the opening is
/// the one being told to play it: White picks the opening, Black picks the
/// answer to it. The old wording ("play `<line>` with `<colour>`") could tell
/// you to open with a black defence.
class _StrategySection extends StatelessWidget {
  const _StrategySection({required this.leaks, required this.labels});

  final List<OpeningMatchup> leaks;
  final _ComparisonText labels;

  String _sentence(OpeningMatchup leak) {
    final opening = leak.family.familyName;
    final rate = _formatPercent(leak.opponent.winRate);
    final games = leak.opponent.games;
    return leak.family.color == 'white'
        ? labels.openWhite(opening, rate, games)
        : labels.answerBlack(opening, rate, games);
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(
              Icons.tips_and_updates_outlined,
              color: AppTheme.success,
              size: 20,
            ),
            const SizedBox(width: 8),
            Text(
              labels.strategyTitle,
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        for (final leak in leaks)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 4),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(
                  Icons.arrow_right,
                  size: 18,
                  color: scheme.onSurfaceVariant,
                ),
                Expanded(
                  child: Text(
                    _sentence(leak),
                    style: theme.textTheme.bodyMedium,
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }
}

/// Self-audit replacement for the strategy card: against yourself both sides
/// score identically, so there is no edge to exploit — the useful question is
/// which of your own lines score badly.
class _OwnWeaknessSection extends StatelessWidget {
  const _OwnWeaknessSection({required this.weaknesses, required this.labels});

  final List<OpeningFamily> weaknesses;
  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.self_improvement, color: scheme.error, size: 20),
            const SizedBox(width: 8),
            Text(
              labels.ownWeaknessTitle,
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        if (weaknesses.isEmpty)
          Text(
            labels.noOwnWeakness,
            style: theme.textTheme.bodySmall?.copyWith(
              color: scheme.onSurfaceVariant,
            ),
          )
        else
          for (final family in weaknesses)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Icon(
                    Icons.arrow_right,
                    size: 18,
                    color: scheme.onSurfaceVariant,
                  ),
                  Expanded(
                    child: Text(
                      labels.ownWeakness(
                        _colorLabel(labels, family.color),
                        family.familyName,
                        _formatPercent(family.tally.winRate),
                        family.tally.games,
                      ),
                      style: theme.textTheme.bodyMedium,
                    ),
                  ),
                  // Straight from the weakest line into the opening lab.
                  _TrainOpeningButton(family: family),
                ],
              ),
            ),
      ],
    );
  }
}

/// Chip marking that the profile is being compared with itself.
class _SelfComparisonBadge extends StatelessWidget {
  const _SelfComparisonBadge({required this.labels});

  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: scheme.secondaryContainer,
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Text('🔍', style: TextStyle(fontSize: 13)),
          const SizedBox(width: 6),
          Text(
            labels.selfBadge,
            style: theme.textTheme.labelMedium?.copyWith(
              color: scheme.onSecondaryContainer,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    );
  }
}

/// Stands in for the head-to-head banner during a self-comparison, where a
/// 0-0-0 progress bar would imply a played-and-lost record that cannot exist.
class _SelfComparisonH2HCard extends StatelessWidget {
  const _SelfComparisonH2HCard({required this.labels});

  final _ComparisonText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.person_search_outlined, color: scheme.onSurfaceVariant),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  labels.h2hTitle,
                  style: theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  labels.selfH2H,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

Widget _comparisonAvatar(AppProfile profile, {double iconSize = 24}) {
  Widget asset() => Image.asset(
    '../img/${profile.avatarAsset}',
    fit: BoxFit.cover,
    errorBuilder: (_, _, _) => Icon(Icons.person, size: iconSize),
  );
  final url = profile.avatarUrl;
  if (url != null && url.isNotEmpty) {
    return Image.network(
      url,
      fit: BoxFit.cover,
      errorBuilder: (_, _, _) => asset(),
    );
  }
  final file = profile.avatarFile;
  if (file != null && file.isNotEmpty) {
    return Image.file(
      File(file),
      fit: BoxFit.cover,
      errorBuilder: (_, _, _) => asset(),
    );
  }
  return asset();
}

int? _ratingFor(List<ProviderPerformance> stats, String key) {
  for (final performance in stats) {
    if (performance.key == key) return performance.currentRating;
  }
  return null;
}

IconData _timeControlIcon(String key) => switch (key) {
  'bullet' => Icons.bolt,
  'blitz' => Icons.flash_on,
  'rapid' => Icons.timer_outlined,
  _ => Icons.schedule,
};

String _colorLabel(_ComparisonText labels, String color) => switch (color) {
  'white' => labels.colorWhite,
  'black' => labels.colorBlack,
  _ => '—',
};

class _ComparisonText {
  const _ComparisonText({
    required this.title,
    required this.usernameLabel,
    required this.usernameHint,
    required this.compare,
    required this.loadingHint,
    required this.prompt,
    required this.you,
    required this.opponent,
    required this.h2hTitle,
    required this.directGames,
    required this.wins,
    required this.draws,
    required this.losses,
    required this.performanceCompare,
    required this.winRateWhite,
    required this.winRateBlack,
    required this.flagging,
    required this.openingMatchup,
    required this.matchupSubtitle,
    required this.openingColumn,
    required this.gamesShort,
    required this.noMatchups,
    required this.noLeaks,
    required this.strategyTitle,
    required this.colorWhite,
    required this.colorBlack,
    required this.errorPrefix,
    required this.gamesAnalyzed,
    required this.selfBadge,
    required this.selfH2H,
    required this.scopeNote,
    required this.minSampleNote,
    required this.ownWeaknessTitle,
    required this.ownWeakness,
    required this.noOwnWeakness,
    required this.openWhite,
    required this.answerBlack,
    required this.sampleScope,
  });

  final String title;
  final String usernameLabel;
  final String usernameHint;
  final String compare;
  final String loadingHint;
  final String prompt;
  final String you;
  final String opponent;
  final String h2hTitle;
  final String directGames;
  final String wins;
  final String draws;
  final String losses;
  final String performanceCompare;
  final String winRateWhite;
  final String winRateBlack;
  final String flagging;
  final String openingMatchup;
  final String matchupSubtitle;
  final String openingColumn;
  final String gamesShort;
  final String noMatchups;
  final String noLeaks;
  final String strategyTitle;
  final String colorWhite;
  final String colorBlack;
  final String errorPrefix;
  final String Function(int games, int months) gamesAnalyzed;
  final String selfBadge;
  final String selfH2H;
  final String Function(int games) scopeNote;
  final String minSampleNote;
  final String ownWeaknessTitle;
  final String Function(String color, String opening, String rate, int games)
  ownWeakness;
  final String noOwnWeakness;
  final String Function(String opening, String rate, int games) openWhite;
  final String Function(String opening, String rate, int games) answerBlack;
  final String Function(int months, int games) sampleScope;
}

_ComparisonText _comparisonText(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return _ComparisonText(
    title: strings.statsCompareTitle,
    usernameLabel: strings.statsCompareUsernameLabel,
    usernameHint: strings.statsCompareUsernameHint,
    compare: strings.statsCompareCompare,
    loadingHint: strings.statsCompareLoadingHint,
    prompt: strings.statsComparePrompt,
    you: strings.statsCompareYou,
    opponent: strings.statsCompareOpponent,
    h2hTitle: strings.statsCompareH2hTitle,
    directGames: strings.statsCompareDirectGames,
    wins: strings.statsCompareWins,
    draws: strings.statsCompareDraws,
    losses: strings.statsCompareLosses,
    performanceCompare: strings.statsComparePerformanceCompare,
    winRateWhite: strings.statsCompareWinRateWhite,
    winRateBlack: strings.statsCompareWinRateBlack,
    flagging: strings.statsCompareFlagging,
    openingMatchup: strings.statsCompareOpeningMatchup,
    matchupSubtitle: strings.statsCompareMatchupSubtitle,
    openingColumn: strings.statsCompareOpeningColumn,
    gamesShort: strings.statsCompareGamesShort,
    noMatchups: strings.statsCompareNoMatchups,
    noLeaks: strings.statsCompareNoLeaks,
    strategyTitle: strings.statsCompareStrategyTitle,
    colorWhite: strings.statsCompareColorWhite,
    colorBlack: strings.statsCompareColorBlack,
    errorPrefix: strings.statsCompareErrorPrefix,
    gamesAnalyzed: strings.statsCompareGamesAnalyzed,
    selfBadge: strings.statsCompareSelfBadge,
    selfH2H: strings.statsCompareSelfH2H,
    scopeNote: strings.statsCompareScopeNote,
    minSampleNote: strings.statsCompareMinSampleNote,
    ownWeaknessTitle: strings.statsCompareOwnWeaknessTitle,
    ownWeakness: strings.statsCompareOwnWeakness,
    noOwnWeakness: strings.statsCompareNoOwnWeakness,
    openWhite: strings.statsCompareOpenWhite,
    answerBlack: strings.statsCompareAnswerBlack,
    sampleScope: strings.statsCompareSampleScope,
  );
}
