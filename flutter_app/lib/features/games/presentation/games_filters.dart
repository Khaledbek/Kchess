part of '../../../ui/app_root.dart';

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
              DropdownMenuItem(
                value: 'newest',
                child: Text(labels.newestFirst),
              ),
              DropdownMenuItem(
                value: 'oldest',
                child: Text(labels.oldestFirst),
              ),
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

