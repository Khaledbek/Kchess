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
      'Maximum search depth per position. Higher values usually take longer.';

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
}
