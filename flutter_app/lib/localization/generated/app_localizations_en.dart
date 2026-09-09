// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'KChess';

  @override
  String get firstRunTitle => 'Your local chess workspace';

  @override
  String get firstRunBody =>
      'Choose a source. Public online profiles do not require a password.';

  @override
  String get chessCom => 'Chess.com';

  @override
  String get lichess => 'Lichess';

  @override
  String get localPgnFen => 'PGN / FEN';

  @override
  String get username => 'Username';

  @override
  String get profileName => 'Profile name';

  @override
  String get continueLabel => 'Continue';

  @override
  String get requiredField => 'Please enter a value.';

  @override
  String get games => 'Games';

  @override
  String get gameSection => 'Game';

  @override
  String get play => 'Play';

  @override
  String get playPlaceholder =>
      'This section is reserved for future play modes, such as games against bots.';

  @override
  String get downloads => 'Downloads';

  @override
  String get favorites => 'Favorites';

  @override
  String get favoriteCollectionsTitle => 'Collections';

  @override
  String get favoriteNoCollections =>
      'No collections yet. Create one to group your favorite games.';

  @override
  String get favoriteLooseTitle => 'Loose favorites';

  @override
  String get favoriteCreateCollection => 'Create collection';

  @override
  String get favoriteRenameCollection => 'Rename collection';

  @override
  String get favoriteDeleteCollection => 'Delete collection';

  @override
  String get favoriteCollectionName => 'Collection name';

  @override
  String get favoriteDeleteCollectionBody =>
      'The collection will be deleted. Its games will remain as loose favorites.';

  @override
  String get favoriteEmptyCollection =>
      'This collection does not contain any games yet.';

  @override
  String get favoriteNoLooseGames => 'No loose favorites.';

  @override
  String get favoriteCollectionRule =>
      'Collections are one level only and cannot be nested.';

  @override
  String get favoriteMoveToCollection => 'Change collection';

  @override
  String get favoriteMoveHelp =>
      'Games can stay as loose favorites or belong to exactly one collection.';

  @override
  String get profile => 'Profile';

  @override
  String get settings => 'Settings';

  @override
  String get analysis => 'Analysis';

  @override
  String get addAccount => 'Add account';

  @override
  String get switchAccount => 'Switch account';

  @override
  String get demoNotice => 'Local game';

  @override
  String get tapToAnalyze => 'Open and analyze';

  @override
  String get importData => 'Import PGN / FEN';

  @override
  String get importPgnFile => 'Choose PGN file';

  @override
  String get pastePgn => 'Paste PGN text';

  @override
  String get importFen => 'Import FEN position';

  @override
  String get pgnText => 'PGN text';

  @override
  String get pgnLabel => 'Game PGN';

  @override
  String get fenText => 'Complete FEN';

  @override
  String get positionName => 'Position name';

  @override
  String get importAction => 'Import';

  @override
  String get noGames =>
      'No local games or positions yet. Import a PGN file, PGN text, or a FEN position.';

  @override
  String get emptySection => 'This section is ready for local data.';

  @override
  String get loading => 'Loading local core…';

  @override
  String get coreUnavailable => 'The local core could not be started.';

  @override
  String get retry => 'Retry';

  @override
  String get summary => 'Summary';

  @override
  String get analyzing => 'Analyzing every half-move…';

  @override
  String get analysisComplete => 'Full analysis complete';

  @override
  String get analysisCancelled =>
      'Analysis cancelled — existing results remain saved.';

  @override
  String get cancelAnalysis => 'Cancel analysis';

  @override
  String get deleteAnalysis => 'Delete saved analysis';

  @override
  String get deleteAnalysisQuestion => 'Delete saved analysis?';

  @override
  String get deleteAnalysisBody =>
      'The locally saved analysis and accuracy for this game will be removed. The PGN/FEN and global engine cache remain.';

  @override
  String get analysisDeleted => 'Saved analysis deleted.';

  @override
  String get classificationPending =>
      'Classification is not available for this move yet.';

  @override
  String get bestMove => 'Best move';

  @override
  String get evaluation => 'Evaluation';

  @override
  String get engineLines => 'Engine lines';

  @override
  String get currentMove => 'Current move';

  @override
  String get boardArrows => 'Show board arrows';

  @override
  String get boardArrowsHelp =>
      'Display only; changing this never restarts analysis.';

  @override
  String get engine => 'Engine';

  @override
  String get enginePreset => 'Medium · depth 18 · 3 lines';

  @override
  String get depth => 'Depth';

  @override
  String get numberOfLines => 'Number of lines';

  @override
  String get timeLimitSeconds => 'Time Limit (seconds)';

  @override
  String get noTimeLimit => 'Off';

  @override
  String get secondsShort => 's';

  @override
  String get deleteAccount => 'Delete account';

  @override
  String get deleteAccountQuestion => 'Delete account?';

  @override
  String get deleteOnlineProfileBody =>
      'This profile and its locally stored data will be removed from KChess. The Chess.com/Lichess account itself will not be changed.';

  @override
  String get deleteLocalProfileBody =>
      'This profile and its locally stored PGN/FEN data will be removed from KChess.';

  @override
  String get cancelAction => 'Cancel';

  @override
  String get deleteAction => 'Delete';

  @override
  String get language => 'Language';

  @override
  String get theme => 'Theme';

  @override
  String get systemTheme => 'System';

  @override
  String get lightTheme => 'Light';

  @override
  String get darkTheme => 'Dark';

  @override
  String get engineSettingsSubtitle =>
      'Depth, lines, time limit, threads and hash';

  @override
  String get analysisSettingsTitle => 'Analysis';

  @override
  String get analysisSettingsSubtitle =>
      'Arrows, evaluation and analysis display';

  @override
  String get analysisBoardGuidance => 'Board guidance';

  @override
  String get analysisInformation => 'Analysis information';

  @override
  String get bestMoveArrow => 'Best move arrow';

  @override
  String get bestMoveArrowHelp => 'Shows the engine’s best move on the board.';

  @override
  String get threatArrow => 'Threat arrow';

  @override
  String get threatArrowHelp =>
      'Shows the opponent’s strongest next move as a warning arrow when the opponent is to move.';

  @override
  String get evaluationBarSetting => 'Evaluation bar';

  @override
  String get evaluationBarSettingHelp => 'Shows the current engine evaluation.';

  @override
  String get showEngineLinesSetting => 'Show engine lines';

  @override
  String get showEngineLinesSettingHelp =>
      'Shows the calculated principal variations (MultiPV).';

  @override
  String get showClassificationsSetting => 'Show move classifications';

  @override
  String get showClassificationsSettingHelp =>
      'Shows Theory, Brilliant, Critical, Best and the other move labels.';

  @override
  String get showAccuracySetting => 'Show accuracy';

  @override
  String get showAccuracySettingHelp =>
      'Shows the locally calculated accuracy values.';

  @override
  String get showTheorySetting => 'Show theory information';

  @override
  String get showTheorySettingHelp =>
      'Shows opening-book information and theory counts.';

  @override
  String get showResultSymbolsSetting => 'Show result symbols';

  @override
  String get showResultSymbolsSettingHelp =>
      'Shows Win, Loss or Draw symbols above the kings when a game has ended.';

  @override
  String get designSettingsTitle => 'Design';

  @override
  String get designSettingsSubtitle => 'Appearance, theme, board and pieces';

  @override
  String get generalSettingsTitle => 'General';

  @override
  String get generalSettingsSubtitle => 'Language and app behavior';

  @override
  String get dataStorageSettingsTitle => 'Data & storage';

  @override
  String get dataStorageSettingsSubtitle =>
      'Analysis cache, downloads and local data';

  @override
  String get dataStoragePlaceholder =>
      'Storage and cache controls will be added in a following step.';

  @override
  String get licensesAbout => 'Licenses & about';

  @override
  String get stockfishPending => 'Stockfish 18 · local · GPLv3';

  @override
  String get provider => 'Provider';

  @override
  String get localProfile => 'Local profile';

  @override
  String get theory => 'Theory';

  @override
  String get brilliant => 'Brilliant';

  @override
  String get critical => 'Great Move';

  @override
  String get best => 'Best';

  @override
  String get excellent => 'Excellent';

  @override
  String get okay => 'Okay';

  @override
  String get miss => 'Miss';

  @override
  String get mistake => 'Mistake';

  @override
  String get blunder => 'Blunder';

  @override
  String get totalMoves => 'Half-moves';

  @override
  String get localAccuracy => 'Local accuracy';

  @override
  String get close => 'Close';

  @override
  String get previous => 'Previous';

  @override
  String get next => 'Next';

  @override
  String get first => 'First';

  @override
  String get last => 'Last';

  @override
  String get playPause => 'Play or pause';

  @override
  String get player => 'Player';

  @override
  String get bothPlayers => 'Both';

  @override
  String get whitePlayer => 'White';

  @override
  String get blackPlayer => 'Black';

  @override
  String get analyzedMoves => 'Classified moves';

  @override
  String get bookGames => 'Book games';

  @override
  String get expectedLoss => 'Expected-score loss';

  @override
  String get versions => 'Versions';

  @override
  String get classifierVersionLabel => 'Classifier';

  @override
  String get accuracyVersionLabel => 'Accuracy';

  @override
  String get analyzingGame => 'Analyzing game';

  @override
  String analyzedMovesProgress(int completed, int total) {
    return '$completed / $total half-moves analyzed';
  }

  @override
  String get openAnalysis => 'Open analysis';

  @override
  String bestMoveText(String move) {
    return '$move is the best move.';
  }

  @override
  String moveComparisonText(String played, String classification, String best) {
    return '$played was $classification. $best is the best move.';
  }

  @override
  String theoryMoveText(String move) {
    return '$move is a theory move.';
  }

  @override
  String triedMove(String move) {
    return 'You tried $move.';
  }

  @override
  String get sidelineEngineTitle => 'Sideline engine';

  @override
  String get sidelineEngineSubtitle =>
      'These values apply only to live analysis of your sideline.';

  @override
  String get mainLineLabel => 'Main line';

  @override
  String get sidelineLabel => 'Your sideline';

  @override
  String get liveEngineTheorySkipped => 'Theory: live analysis skipped';

  @override
  String get liveEngineTargetReached => 'Stockfish: analysis target reached';

  @override
  String liveEngineProgress(int percent) {
    return 'Stockfish analyzing live · $percent%';
  }

  @override
  String get sidelineAnalysisPaused => 'Live analysis paused';

  @override
  String get analyzingVariation => 'Analyzing the temporary variation…';

  @override
  String evaluationComparison(String before, String after) {
    return 'Evaluation: $before → $after';
  }

  @override
  String bestContinuation(String line) {
    return 'Best continuation: $line';
  }

  @override
  String get returnToMainLine => 'Return to main line';

  @override
  String illegalOrFailedMove(String message) {
    return 'The move is illegal or could not be analyzed: $message';
  }

  @override
  String get myPlayer => 'My player';

  @override
  String get opponent => 'Opponent';

  @override
  String get variationStartingPosition => 'Variation starting position';

  @override
  String get variationStart => 'Start of variation';

  @override
  String get engineQualityTitle => 'Analysis quality';

  @override
  String get engineResourcesTitle => 'Resources';

  @override
  String get depthHelp =>
      'Min = pre-analysis depth. Max = maximum live-analysis depth. Higher values usually take longer.';

  @override
  String get adaptiveEarlyStop => 'Adaptive analysis';

  @override
  String get adaptiveEarlyStopHelp =>
      'Ends quiet pre-analysis and live searches early when the evaluation and principal variations are stable. Critical verification searches still use the configured limit.';

  @override
  String get numberOfLinesHelp =>
      'How many top engine variations Stockfish calculates at the same time.';

  @override
  String get timeLimitHelp =>
      'Optional limit per position. Off uses depth only; otherwise the search stops when depth or time is reached first.';

  @override
  String get threads => 'Threads';

  @override
  String get threadsHelp =>
      'CPU threads per Stockfish worker. Kchess detects your PC automatically and allows at most half of the logical CPU threads.';

  @override
  String get hashMemory => 'Hash memory';

  @override
  String get hashMemoryHelp =>
      'RAM for Stockfish’s transposition table. More memory can improve repeated-position search.';

  @override
  String get boardDisplayTitle => 'Board display';

  @override
  String get rotateBoard => 'Rotate board';

  @override
  String get showBoardCoordinates => 'Board coordinates';

  @override
  String get showBoardCoordinatesHelp =>
      'Shows file and rank labels (a–h / 1–8) on the board.';

  @override
  String get highlightLastMove => 'Highlight last move';

  @override
  String get highlightLastMoveHelp =>
      'Highlights the origin and destination squares of the last played move.';

  @override
  String get highlightSelectedSquare => 'Highlight selected square';

  @override
  String get highlightSelectedSquareHelp =>
      'Highlights the square you selected while exploring a variation.';

  @override
  String get behaviorTitle => 'Behavior';

  @override
  String get autoSyncOnline => 'Automatically sync online profiles';

  @override
  String get autoSyncOnlineHelp =>
      'Synchronizes Chess.com and Lichess automatically at startup and when switching profiles.';

  @override
  String get confirmBeforeDelete => 'Confirm before deleting';

  @override
  String get confirmBeforeDeleteHelp =>
      'Asks for confirmation before deleting profiles or local games.';

  @override
  String get analysisCacheTitle => 'Analysis cache';

  @override
  String get useGlobalAnalysisCache => 'Use shared position cache';

  @override
  String get useGlobalAnalysisCacheHelp =>
      'Reuses compatible analysis of identical positions across different games.';

  @override
  String get clearAnalysisCache => 'Clear analysis cache';

  @override
  String get clearAnalysisCacheHelp =>
      'Clears only the shared position cache. Saved games and completed game analyses are kept.';

  @override
  String get clearAnalysisCacheQuestion => 'Clear analysis cache?';

  @override
  String get clearAnalysisCacheBody =>
      'The shared position cache will be deleted. Your games, favorites, downloads, and completed game analyses will be kept.';

  @override
  String get analysisCacheCleared => 'Analysis cache cleared.';

  @override
  String get diagnosticsTitle => 'Diagnostics';

  @override
  String get diagnosticLogging => 'Diagnostic logging';

  @override
  String get diagnosticLoggingHelp =>
      'Writes bounded technical logs for troubleshooting. Full PGNs, FENs, and provider responses are not logged.';

  @override
  String get deleteLocalGameQuestion => 'Delete local entry?';

  @override
  String get deleteLocalGameBody =>
      'The stored PGN/FEN and its local analysis will be permanently removed.';

  @override
  String get profileRatings => 'Ratings';

  @override
  String get profileGameOverview => 'Game overview';

  @override
  String get profileWins => 'Wins';

  @override
  String get profileDraws => 'Draws';

  @override
  String get profileLosses => 'Losses';

  @override
  String get ratingRapid => 'Rapid';

  @override
  String get ratingBlitz => 'Blitz';

  @override
  String get ratingBullet => 'Bullet';

  @override
  String get ratingDaily => 'Daily';

  @override
  String get ratingClassical => 'Classical';

  @override
  String get ratingChess960 => 'Chess960';

  @override
  String get ratingFide => 'FIDE';

  @override
  String get statsWins => 'Wins';

  @override
  String get statsDraws => 'Draws';

  @override
  String get statsLosses => 'Losses';

  @override
  String get statsAll => 'All';

  @override
  String get statsAllTimeControlsNote => 'All time controls';

  @override
  String get statsPhaseTitle => 'By game phase';

  @override
  String get statsPhaseSubtitle =>
      'Where your games end and how you score there.';

  @override
  String get statsPhaseOpening => 'Opening (1–12)';

  @override
  String get statsPhaseMiddlegame => 'Middlegame (13–30)';

  @override
  String get statsPhaseEndgame => 'Endgame (31+)';

  @override
  String get statsPhaseOpeningShort => 'Opening';

  @override
  String get statsPhaseMiddlegameShort => 'Middlegame';

  @override
  String get statsPhaseEndgameShort => 'Endgame';

  @override
  String get statsPhaseGames => 'games';

  @override
  String get statsPhaseWinWord => 'win';

  @override
  String get statsPhaseEmpty => 'Not enough data on game phases.';

  @override
  String get statsPhaseNoProfile =>
      'Create or select a profile to see statistics.';

  @override
  String get statsPhaseError => 'Could not load game phases.';

  @override
  String get statsPhaseRetry => 'Retry';

  @override
  String statsPhaseClassifiedNote(int classified, int total) {
    return '$classified of $total games';
  }

  @override
  String get statsTitle => 'Statistics';

  @override
  String get statsIntroTitle => 'Your chess performance';

  @override
  String get statsIntroBody =>
      'See your results, recent form and opening record separated by color.';

  @override
  String get statsFormTitle => 'Recent form';

  @override
  String get statsFormHint => 'Tap a result to open the game.';

  @override
  String get statsFormVersus => 'vs';

  @override
  String get statsFormEmpty => 'No recent games to show.';

  @override
  String get statsFormError => 'Could not load recent games.';

  @override
  String get statsFormRetry => 'Retry';

  @override
  String get statsOverviewTitle => 'Overview';

  @override
  String get statsOverviewGames => 'Games';

  @override
  String get statsOverviewWinRate => 'Win rate';

  @override
  String get statsOverviewScore => 'Score';

  @override
  String get statsOverviewRecord => 'Record';

  @override
  String get statsOverviewByColor => 'By color';

  @override
  String get statsOverviewByTimeControl => 'By time control';

  @override
  String get statsOverviewWhite => 'White';

  @override
  String get statsOverviewBlack => 'Black';

  @override
  String get statsOverviewEmpty =>
      'No games yet. Sync an online profile or import games to see your statistics.';

  @override
  String get statsOverviewNoProfile =>
      'Create or select a profile to see statistics.';

  @override
  String get statsOverviewNoGamesForFilter =>
      'No games for the selected time control.';

  @override
  String get statsOverviewError => 'Could not load statistics.';

  @override
  String get statsOverviewRetry => 'Retry';

  @override
  String get statsTerminationTitle => 'How games end';

  @override
  String get statsTerminationCheckmate => 'Checkmate';

  @override
  String get statsTerminationResignation => 'Resignation';

  @override
  String get statsTerminationTimeout => 'On time';

  @override
  String get statsTerminationDraw => 'Draw';

  @override
  String get statsTerminationOther => 'Other';

  @override
  String get statsTerminationWonByCheckmate => 'Won by checkmate';

  @override
  String get statsTerminationLostByCheckmate => 'Lost by checkmate';

  @override
  String get statsTerminationOpponentResigned => 'Opponent resigned';

  @override
  String get statsTerminationSelfResigned => 'Resigned';

  @override
  String get statsTerminationOpponentFlagged => 'Opponent ran out of time';

  @override
  String get statsTerminationSelfFlagged => 'Ran out of time';

  @override
  String get statsTerminationWonGeneric => 'Won';

  @override
  String get statsTerminationLostGeneric => 'Lost';

  @override
  String get statsTerminationEmpty => 'Not enough data on how games ended.';

  @override
  String get statsTerminationNoProfile =>
      'Create or select a profile to see statistics.';

  @override
  String get statsTerminationError => 'Could not load game endings.';

  @override
  String get statsTerminationRetry => 'Retry';

  @override
  String get statsRatingTitle => 'Rating trend';

  @override
  String get statsRatingEmpty => 'Not enough rating data to draw a trend.';

  @override
  String get statsRatingError => 'Could not load rating data.';

  @override
  String get statsRatingRetry => 'Retry';

  @override
  String get statsOpeningGamesAll => 'All';

  @override
  String get statsOpeningGamesWon => 'Won';

  @override
  String get statsOpeningGamesLost => 'Lost';

  @override
  String get statsOpeningGamesByCheckmate => 'by checkmate';

  @override
  String get statsOpeningGamesByResignation => 'by resignation';

  @override
  String get statsOpeningGamesByTimeout => 'on time';

  @override
  String get statsOpeningGamesByDraw => 'draw';

  @override
  String get statsOpeningGamesEmpty => 'No games for this selection.';

  @override
  String get statsOpeningGamesError => 'Could not load games.';

  @override
  String get statsOpeningsTitle => 'Top openings';

  @override
  String get statsOpeningsMostPlayed => 'Most played';

  @override
  String get statsOpeningsBestWinRate => 'Best win rate';

  @override
  String get statsOpeningsMinGamesHint => 'At least 3 games per opening.';

  @override
  String get statsOpeningsClassifiedGames => 'games with a named opening';

  @override
  String get statsOpeningsGames => 'games';

  @override
  String get statsOpeningsVariations => 'variations';

  @override
  String get statsOpeningsBaseLine => 'Base line';

  @override
  String get statsOpeningsWhite => 'White';

  @override
  String get statsOpeningsBlack => 'Black';

  @override
  String get statsOpeningsUnknownColor => 'Other';

  @override
  String get statsOpeningsWinRateShort => 'win';

  @override
  String get statsOpeningsNoOpeningsForColor =>
      'No openings for this color yet.';

  @override
  String get statsOpeningsNoOpeningsForWinRate =>
      'No opening with at least 3 games.';

  @override
  String get statsOpeningsEmpty =>
      'No named openings yet. Synced and imported games are classified automatically.';

  @override
  String get statsOpeningsNoProfile =>
      'Create or select a profile to see openings.';

  @override
  String get statsOpeningsError => 'Could not load openings.';

  @override
  String get statsOpeningsRetry => 'Retry';

  @override
  String get statsCompareTitle => 'Player comparison';

  @override
  String get statsCompareUsernameLabel => 'Chess.com username';

  @override
  String get statsCompareUsernameHint => 'e.g. hikaru';

  @override
  String get statsCompareCompare => 'Compare';

  @override
  String get statsCompareLoadingHint =>
      'Fetching and analysing the opponent\'s games…';

  @override
  String get statsComparePrompt =>
      'Enter a Chess.com username to compare stats.';

  @override
  String get statsCompareYou => 'You';

  @override
  String get statsCompareOpponent => 'Opponent';

  @override
  String get statsCompareH2hTitle => 'Head-to-head';

  @override
  String get statsCompareDirectGames => 'direct games';

  @override
  String get statsCompareWins => 'Wins';

  @override
  String get statsCompareDraws => 'Draws';

  @override
  String get statsCompareLosses => 'Losses';

  @override
  String get statsComparePerformanceCompare => 'Performance comparison';

  @override
  String get statsCompareWinRateWhite => 'Win rate as White';

  @override
  String get statsCompareWinRateBlack => 'Win rate as Black';

  @override
  String get statsCompareFlagging => 'Losses on time';

  @override
  String get statsCompareOpeningMatchup => 'Opening matchup';

  @override
  String get statsCompareMatchupSubtitle =>
      'Your openings vs. the opponent\'s win rate with the opposite colour.';

  @override
  String get statsCompareOpeningColumn => 'Opening';

  @override
  String get statsCompareGamesShort => 'games';

  @override
  String get statsCompareNoMatchups => 'No shared openings found.';

  @override
  String get statsCompareNoLeaks => 'No clear weaknesses found.';

  @override
  String get statsCompareStrategyTitle => 'Recommended strategy';

  @override
  String get statsCompareColorWhite => 'White';

  @override
  String get statsCompareColorBlack => 'Black';

  @override
  String get statsCompareErrorPrefix => 'Error';

  @override
  String statsCompareGamesAnalyzed(int games, int months) {
    return '$games games analysed across $months months';
  }

  @override
  String statsTerminationSpotlight(String label, int share, int lossPercent) {
    return 'Most common ending: $label — $share% of all games, $lossPercent% of them losses.';
  }

  @override
  String get statsCompareSelfBadge => 'Self-comparison (mirror)';

  @override
  String get statsCompareSelfH2H =>
      'No games against yourself · This is a self-analysis of your own profile.';

  @override
  String statsCompareScopeNote(int games) {
    return 'Comparison based on the last $games loaded games.';
  }

  @override
  String get statsCompareMinSampleNote =>
      'Only openings with at least 5 games on both sides.';

  @override
  String get statsCompareOwnWeaknessTitle => 'Your own weak spots';

  @override
  String statsCompareOwnWeakness(
    String color,
    String opening,
    String rate,
    int games,
  ) {
    return 'Weak spot as $color: $opening — only $rate win rate (from $games games).';
  }

  @override
  String get statsCompareNoOwnWeakness =>
      'No clear weak spots with at least 5 games per opening.';

  @override
  String statsCompareOpenWhite(String opening, String rate, int games) {
    return 'Open with $opening — your opponent scores only $rate against it as Black (from $games games).';
  }

  @override
  String statsCompareAnswerBlack(String opening, String rate, int games) {
    return 'As Black: choose $opening — your opponent scores only $rate against it as White (from $games games).';
  }

  @override
  String statsCompareSampleScope(int months, int games) {
    return 'You: your entire local library · Opponent: the last $months months ($games games). The two sides are different samples, so the numbers can differ even when you compare a profile with itself.';
  }

  @override
  String get trainingSection => 'Training';

  @override
  String get trainingIntroTitle => 'Training arena';

  @override
  String get trainingIntroBody =>
      'Pick a training area. Your progress is stored locally on this device.';

  @override
  String get trainingOpeningTitle => 'Opening lab';

  @override
  String get trainingOpeningSubtitle =>
      'Drill your repertoire lines and your weak spots.';

  @override
  String get trainingOpeningAction => 'Train lines';

  @override
  String trainingNemesisBadge(String opening, String rate) {
    return 'Nemesis: $opening — only $rate win rate';
  }

  @override
  String get trainingNemesisNone =>
      'No weak line found in your statistics yet.';

  @override
  String get trainingTacticsTitle => 'Blunder buster';

  @override
  String get trainingTacticsSubtitle =>
      'Find the better move in critical middlegame positions.';

  @override
  String trainingTacticsSolved(int count) {
    return '$count tactics solved';
  }

  @override
  String get trainingTacticsAction => 'Start tactics';

  @override
  String get trainingTacticsEmpty => 'No tactics puzzles in the catalogue yet.';

  @override
  String get trainingEndgameTitle => 'Endgame academy';

  @override
  String get trainingEndgameSubtitle =>
      'Master theoretical endgames step by step.';

  @override
  String trainingEndgameProgress(int mastered, int total, int percent) {
    return '$mastered / $total positions mastered ($percent%)';
  }

  @override
  String get trainingEndgameAction => 'Open endgames';

  @override
  String get trainingMastered => 'Mastered';

  @override
  String trainingStreak(int done, int total) {
    return '$done/$total clean repeats';
  }

  @override
  String trainingLastAttempt(String date) {
    return 'Last practised: $date';
  }

  @override
  String get trainingNeverAttempted => 'Not practised yet';

  @override
  String get trainingHint => 'Hint';

  @override
  String get trainingBoardPending =>
      'The board trainer follows once the core exposes move validation over the native interface.';

  @override
  String get trainingOpeningLabSelected => 'Selected line';

  @override
  String get trainingOpeningLabEmpty =>
      'Pick an opening in the statistics tab and tap “Train” to load it here.';

  @override
  String get trainingOpeningLabPending =>
      'The line trainer follows once the core supplies the opening moves.';

  @override
  String get statsTrainOpening => 'Train';

  @override
  String trainingGoalWin(String side) {
    return '$side to move — win the position';
  }

  @override
  String trainingGoalDraw(String side) {
    return '$side to move — hold the draw';
  }

  @override
  String get trainingWrongMove => 'Not the best move. Give it another try.';

  @override
  String get trainingSolvedTitle => 'Excellent! Position solved.';

  @override
  String get trainingSolvedClean =>
      'Solved without a slip — this one counts towards mastery.';

  @override
  String get trainingSolvedWithErrors =>
      'Solved, but with corrections. Only a clean run counts towards “mastered”.';

  @override
  String get trainingPracticeAgain => 'Practise again';

  @override
  String get trainingNextEndgame => 'Next endgame';

  @override
  String get trainingBackToList => 'Back to the list';

  @override
  String trainingMoveProgress(int done, int total) {
    return 'Move $done of $total';
  }

  @override
  String get trainingBoardError => 'The position could not be loaded.';

  @override
  String get trainingShowHint => 'Show hint';

  @override
  String get trainingYourMove => 'Your move';

  @override
  String get trainingOpponentThinking => 'Reply …';

  @override
  String get trainingRestart => 'Restart';
}
