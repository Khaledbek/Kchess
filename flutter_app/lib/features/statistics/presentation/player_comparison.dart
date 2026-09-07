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

  /// The handle the active profile plays under. Comparing it with the searched
  /// handle is what separates a self-audit from a real opponent.
  String get _userHandle {
    final profile = widget.controller.activeProfile;
    if (profile == null) return '';
    final provider = profile.providerUsername?.trim() ?? '';
    return provider.isNotEmpty ? provider : profile.displayName.trim();
  }

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
    setState(() {
      _loading = true;
      _error = null;
      _report = null;
    });
    try {
      // Opponent aggregation (network) first; the user's own stats are local.
      final report = await gateway.scoutReport(username);
      final userOverview = await gateway.statisticsOverview();
      final userOpenings = await gateway.openingsStats();
      final userTerminations = await gateway.terminationStats();
      final opponentHandle = report.profile.providerUsername ?? username;
      final userHandle = _userHandle;
      final isSelf = userHandle.isNotEmpty &&
          opponentHandle.trim().toLowerCase() == userHandle.toLowerCase();
      final h2h = await _headToHead(opponentHandle, userHandle);
      if (!mounted || generation != _generation) return;
      setState(() {
        _isSelf = isSelf;
        _report = report;
        _userOverview = userOverview;
        _userOpenings = userOpenings;
        _userTerminations = userTerminations;
        _h2h = h2h;
        _loading = false;
      });
    } catch (error) {
      if (!mounted || generation != _generation) return;
      setState(() {
        _error = error.toString();
        _loading = false;
      });
      final labels = _comparisonText(context);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('${labels.errorPrefix}: ${error.toString()}')),
      );
    }
  }

  /// Head-to-head from the active profile's own stored games, tallied from the
  /// profile's perspective. A direct game needs the two sides to be *different*
  /// handles: matching on "is this handle present" matched every game in the
  /// library when the searched handle was the profile's own, inventing hundreds
  /// of phantom meetings with itself.
  Future<StatTally> _headToHead(String opponentHandle, String userHandle) async {
    final opponent = opponentHandle.trim().toLowerCase();
    final user = userHandle.trim().toLowerCase();
    if (user.isEmpty || opponent.isEmpty || user == opponent) {
      return const StatTally();
    }
    final games = await widget.controller.queryGames(
      const GameQuery(sort: 'newest'),
    );
    var wins = 0;
    var draws = 0;
    var losses = 0;
    for (final game in games) {
      final white = game.whiteName.trim().toLowerCase();
      final black = game.blackName.trim().toLowerCase();
      final isDirect = (white == user && black == opponent) ||
          (black == user && white == opponent);
      if (!isDirect) continue;
      switch (_statGameOutcome(game)) {
        case 'win':
          wins += 1;
        case 'loss':
          losses += 1;
        case 'draw':
          draws += 1;
      }
    }
    return StatTally(
      games: wins + draws + losses,
      wins: wins,
      draws: draws,
      losses: losses,
    );
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
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: Theme.of(context).colorScheme.onSurfaceVariant,
          ),
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
                        _timeControlLabel(key),
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

  double? _flagRate(List<GameTermination> terms, int totalLosses) {
    if (totalLosses <= 0) return null;
    for (final t in terms) {
      if (t.type == 'timeout') return t.tally.losses / totalLosses;
    }
    return 0;
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final userWhite = userOverview?.white.winRate;
    final userBlack = userOverview?.black.winRate;
    final userFlag = _flagRate(
      userTerminations?.terminations ?? const [],
      userOverview?.overall.losses ?? 0,
    );
    final oppFlag = _flagRate(report.terminations, report.overall.losses);

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

  /// Games needed on *each* side before an opening may say anything at all.
  /// Without this a single 1/1 line reads as a 100% edge.
  static const _minGames = 5;

  /// A 20-point gap counts on its own; smaller gaps must clear the z-test.
  static const _minRateGap = 0.20;

  /// One-sided 95% critical value.
  static const _zThreshold = 1.645;

  /// Where one of your own lines counts as a weakness in a self-audit.
  static const _weakRate = 0.45;
  static const _maxRows = 12;

  String _opposite(String color) => switch (color) {
    'white' => 'black',
    'black' => 'white',
    _ => 'unknown',
  };

  /// Sum every scouted line sharing an ECO and colour, so a family and its
  /// sub-variations ("Blackmar-Diemer", "BDG Accepted", "BDG Declined") count
  /// once against the parent. The previous putIfAbsent kept only the first and
  /// discarded the rest, which both duplicated recommendations and understated
  /// the opponent's sample.
  Map<String, StatTally> _opponentByEcoColor() {
    final merged = <String, StatTally>{};
    for (final opening in report.openings) {
      if (opening.eco.isEmpty) continue;
      final key = '${opening.eco}|${opening.color}';
      final current = merged[key];
      final tally = opening.tally;
      merged[key] = current == null
          ? tally
          : StatTally(
              games: current.games + tally.games,
              wins: current.wins + tally.wins,
              draws: current.draws + tally.draws,
              losses: current.losses + tally.losses,
            );
    }
    return merged;
  }

  /// Whether the user really scores better here than the opponent does with the
  /// other colour. Needs a usable sample on both sides, then either a 20-point
  /// gap or a significant two-proportion z-test. Compared squared, so no square
  /// root is needed.
  static bool _isEdge(StatTally user, StatTally opponent) {
    final userDecided = user.wins + user.draws + user.losses;
    final oppDecided = opponent.wins + opponent.draws + opponent.losses;
    if (userDecided < _minGames || oppDecided < _minGames) return false;
    final gap = user.wins / userDecided - opponent.wins / oppDecided;
    if (gap <= 0) return false;
    if (gap >= _minRateGap) return true;
    final pooled = (user.wins + opponent.wins) / (userDecided + oppDecided);
    final variance = pooled * (1 - pooled) * (1 / userDecided + 1 / oppDecided);
    if (variance <= 0) return false;
    return gap * gap >= _zThreshold * _zThreshold * variance;
  }

  List<_Matchup> _matchups() {
    final openings = userOpenings;
    if (openings == null) return const [];
    final opponentByKey = _opponentByEcoColor();
    final families = [...openings.families]
      ..sort((a, b) => b.tally.games.compareTo(a.tally.games));
    final matchups = <_Matchup>[];
    for (final family in families) {
      if (family.baseEco.isEmpty) continue;
      final opponentColor = _opposite(family.color);
      final opponent = opponentByKey['${family.baseEco}|$opponentColor'];
      if (opponent == null) continue;
      // Both sides need a real sample before the row means anything.
      if (family.tally.games < _minGames || opponent.games < _minGames) {
        continue;
      }
      matchups.add(
        _Matchup(
          family: family,
          opponent: opponent,
          opponentColor: opponentColor,
          // Mirroring yourself gives identical rates, so no edge can exist.
          exploitable: !isSelf && _isEdge(family.tally, opponent),
        ),
      );
      if (matchups.length >= _maxRows) break;
    }
    return matchups;
  }

  /// Self-audit view: your own lines that score badly on a usable sample.
  List<OpeningFamily> _ownWeaknesses() {
    final openings = userOpenings;
    if (openings == null) return const [];
    final weak = [
      for (final family in openings.families)
        if (family.tally.games >= _minGames &&
            (_tallyRate(family.tally) ?? 1) < _weakRate)
          family,
    ]..sort(
      (a, b) => (_tallyRate(a.tally) ?? 1).compareTo(_tallyRate(b.tally) ?? 1),
    );
    return weak.take(5).toList();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final matchups = _matchups();
    final leaks = matchups.where((m) => m.exploitable).toList();

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
                  Expanded(child: Text(labels.openingColumn, style: _colHead(theme))),
                  SizedBox(width: 52, child: Text(labels.you, textAlign: TextAlign.end, style: _colHead(theme))),
                  SizedBox(width: 52, child: Text(labels.opponent, textAlign: TextAlign.end, style: _colHead(theme))),
                ],
              ),
              const SizedBox(height: 4),
              for (final m in matchups)
                _MatchupRow(matchup: m, isSelf: isSelf, labels: labels),
            ],
            if (isSelf) ...[
              const SizedBox(height: 18),
              _OwnWeaknessSection(
                weaknesses: _ownWeaknesses(),
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

/// One row of the matchup table: the user's opening family against the
/// opponent's merged record in the same ECO with the opposite colour.
class _Matchup {
  const _Matchup({
    required this.family,
    required this.opponent,
    required this.opponentColor,
    required this.exploitable,
  });

  final OpeningFamily family;
  final StatTally opponent;
  final String opponentColor;
  final bool exploitable;
}

/// Win rate computed from the tally itself. Merged opponent tallies carry no
/// pre-computed rate, so deriving it here keeps both sides comparable.
double? _tallyRate(StatTally tally) {
  final decided = tally.wins + tally.draws + tally.losses;
  return decided == 0 ? null : tally.wins / decided;
}

class _MatchupRow extends StatelessWidget {
  const _MatchupRow({
    required this.matchup,
    required this.isSelf,
    required this.labels,
  });

  final _Matchup matchup;
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
    final opponentLabel =
        isSelf ? '${labels.opponent} (${labels.you})' : labels.opponent;
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
                        '(${_formatPercent(_tallyRate(family.tally))})',
                    '$opponentLabel: ${opponent.games} '
                        '${labels.gamesShort} '
                        '(${_formatPercent(_tallyRate(opponent))})',
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
              _formatPercent(_tallyRate(family.tally)),
              textAlign: TextAlign.end,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
          SizedBox(
            width: 52,
            child: Text(
              _formatPercent(_tallyRate(opponent)),
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

  final List<_Matchup> leaks;
  final _ComparisonText labels;

  String _sentence(_Matchup leak) {
    final opening = leak.family.familyName;
    final rate = _formatPercent(_tallyRate(leak.opponent));
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
                        _formatPercent(_tallyRate(family.tally)),
                        family.tally.games,
                      ),
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
  );
}
