import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_ar.dart';
import 'app_localizations_de.dart';
import 'app_localizations_en.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'generated/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations)!;
  }

  static const LocalizationsDelegate<AppLocalizations> delegate =
      _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('ar'),
    Locale('de'),
    Locale('en'),
  ];

  /// No description provided for @appTitle.
  ///
  /// In en, this message translates to:
  /// **'KChess'**
  String get appTitle;

  /// No description provided for @firstRunTitle.
  ///
  /// In en, this message translates to:
  /// **'Your local chess workspace'**
  String get firstRunTitle;

  /// No description provided for @chessCom.
  ///
  /// In en, this message translates to:
  /// **'Chess.com'**
  String get chessCom;

  /// No description provided for @lichess.
  ///
  /// In en, this message translates to:
  /// **'Lichess'**
  String get lichess;

  /// No description provided for @localPgnFen.
  ///
  /// In en, this message translates to:
  /// **'PGN / FEN'**
  String get localPgnFen;

  /// No description provided for @username.
  ///
  /// In en, this message translates to:
  /// **'Username'**
  String get username;

  /// No description provided for @profileName.
  ///
  /// In en, this message translates to:
  /// **'Profile name'**
  String get profileName;

  /// No description provided for @continueLabel.
  ///
  /// In en, this message translates to:
  /// **'Continue'**
  String get continueLabel;

  /// No description provided for @requiredField.
  ///
  /// In en, this message translates to:
  /// **'Please enter a value.'**
  String get requiredField;

  /// No description provided for @games.
  ///
  /// In en, this message translates to:
  /// **'Games'**
  String get games;

  /// No description provided for @gameSection.
  ///
  /// In en, this message translates to:
  /// **'Game'**
  String get gameSection;

  /// No description provided for @play.
  ///
  /// In en, this message translates to:
  /// **'Play'**
  String get play;

  /// No description provided for @favorites.
  ///
  /// In en, this message translates to:
  /// **'Favorites'**
  String get favorites;

  /// No description provided for @favoriteCollectionsTitle.
  ///
  /// In en, this message translates to:
  /// **'Collections'**
  String get favoriteCollectionsTitle;

  /// No description provided for @favoriteNoCollections.
  ///
  /// In en, this message translates to:
  /// **'No collections yet. Create one to group your favorite games.'**
  String get favoriteNoCollections;

  /// No description provided for @favoriteLooseTitle.
  ///
  /// In en, this message translates to:
  /// **'Loose favorites'**
  String get favoriteLooseTitle;

  /// No description provided for @favoriteCreateCollection.
  ///
  /// In en, this message translates to:
  /// **'Create collection'**
  String get favoriteCreateCollection;

  /// No description provided for @favoriteRenameCollection.
  ///
  /// In en, this message translates to:
  /// **'Rename collection'**
  String get favoriteRenameCollection;

  /// No description provided for @favoriteDeleteCollection.
  ///
  /// In en, this message translates to:
  /// **'Delete collection'**
  String get favoriteDeleteCollection;

  /// No description provided for @favoriteCollectionName.
  ///
  /// In en, this message translates to:
  /// **'Collection name'**
  String get favoriteCollectionName;

  /// No description provided for @favoriteDeleteCollectionBody.
  ///
  /// In en, this message translates to:
  /// **'The collection will be deleted. Its games will remain as loose favorites.'**
  String get favoriteDeleteCollectionBody;

  /// No description provided for @favoriteEmptyCollection.
  ///
  /// In en, this message translates to:
  /// **'This collection does not contain any games yet.'**
  String get favoriteEmptyCollection;

  /// No description provided for @favoriteNoLooseGames.
  ///
  /// In en, this message translates to:
  /// **'No loose favorites.'**
  String get favoriteNoLooseGames;

  /// No description provided for @favoriteMoveToCollection.
  ///
  /// In en, this message translates to:
  /// **'Change collection'**
  String get favoriteMoveToCollection;

  /// No description provided for @profile.
  ///
  /// In en, this message translates to:
  /// **'Profile'**
  String get profile;

  /// No description provided for @settings.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get settings;

  /// No description provided for @analysis.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get analysis;

  /// No description provided for @addAccount.
  ///
  /// In en, this message translates to:
  /// **'Add account'**
  String get addAccount;

  /// No description provided for @switchAccount.
  ///
  /// In en, this message translates to:
  /// **'Switch account'**
  String get switchAccount;

  /// No description provided for @importData.
  ///
  /// In en, this message translates to:
  /// **'Import PGN / FEN'**
  String get importData;

  /// No description provided for @importPgnFile.
  ///
  /// In en, this message translates to:
  /// **'Choose PGN file'**
  String get importPgnFile;

  /// No description provided for @pastePgn.
  ///
  /// In en, this message translates to:
  /// **'Paste PGN text'**
  String get pastePgn;

  /// No description provided for @importFen.
  ///
  /// In en, this message translates to:
  /// **'Import FEN position'**
  String get importFen;

  /// No description provided for @pgnText.
  ///
  /// In en, this message translates to:
  /// **'PGN text'**
  String get pgnText;

  /// No description provided for @pgnLabel.
  ///
  /// In en, this message translates to:
  /// **'Game PGN'**
  String get pgnLabel;

  /// No description provided for @fenText.
  ///
  /// In en, this message translates to:
  /// **'Complete FEN'**
  String get fenText;

  /// No description provided for @positionName.
  ///
  /// In en, this message translates to:
  /// **'Position name'**
  String get positionName;

  /// No description provided for @importAction.
  ///
  /// In en, this message translates to:
  /// **'Import'**
  String get importAction;

  /// No description provided for @noGames.
  ///
  /// In en, this message translates to:
  /// **'No local games or positions yet. Import a PGN file, PGN text, or a FEN position.'**
  String get noGames;

  /// No description provided for @emptySection.
  ///
  /// In en, this message translates to:
  /// **'This section is ready for local data.'**
  String get emptySection;

  /// No description provided for @loading.
  ///
  /// In en, this message translates to:
  /// **'Loading local core…'**
  String get loading;

  /// No description provided for @coreUnavailable.
  ///
  /// In en, this message translates to:
  /// **'The local core could not be started.'**
  String get coreUnavailable;

  /// No description provided for @retry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get retry;

  /// No description provided for @summary.
  ///
  /// In en, this message translates to:
  /// **'Summary'**
  String get summary;

  /// No description provided for @analyzing.
  ///
  /// In en, this message translates to:
  /// **'Analyzing every half-move…'**
  String get analyzing;

  /// No description provided for @analysisComplete.
  ///
  /// In en, this message translates to:
  /// **'Full analysis complete'**
  String get analysisComplete;

  /// No description provided for @analysisCancelled.
  ///
  /// In en, this message translates to:
  /// **'Analysis cancelled — existing results remain saved.'**
  String get analysisCancelled;

  /// No description provided for @cancelAnalysis.
  ///
  /// In en, this message translates to:
  /// **'Cancel analysis'**
  String get cancelAnalysis;

  /// No description provided for @deleteAnalysis.
  ///
  /// In en, this message translates to:
  /// **'Delete saved analysis'**
  String get deleteAnalysis;

  /// No description provided for @deleteAnalysisQuestion.
  ///
  /// In en, this message translates to:
  /// **'Delete saved analysis?'**
  String get deleteAnalysisQuestion;

  /// No description provided for @deleteAnalysisBody.
  ///
  /// In en, this message translates to:
  /// **'The locally saved analysis and accuracy for this game will be removed. The PGN/FEN and global engine cache remain.'**
  String get deleteAnalysisBody;

  /// No description provided for @analysisDeleted.
  ///
  /// In en, this message translates to:
  /// **'Saved analysis deleted.'**
  String get analysisDeleted;

  /// No description provided for @classificationPending.
  ///
  /// In en, this message translates to:
  /// **'Classification is not available for this move yet.'**
  String get classificationPending;

  /// No description provided for @bestMove.
  ///
  /// In en, this message translates to:
  /// **'Best move'**
  String get bestMove;

  /// No description provided for @evaluation.
  ///
  /// In en, this message translates to:
  /// **'Evaluation'**
  String get evaluation;

  /// No description provided for @engineLines.
  ///
  /// In en, this message translates to:
  /// **'Engine lines'**
  String get engineLines;

  /// No description provided for @currentMove.
  ///
  /// In en, this message translates to:
  /// **'Current move'**
  String get currentMove;

  /// No description provided for @engine.
  ///
  /// In en, this message translates to:
  /// **'Engine'**
  String get engine;

  /// No description provided for @depth.
  ///
  /// In en, this message translates to:
  /// **'Depth'**
  String get depth;

  /// No description provided for @numberOfLines.
  ///
  /// In en, this message translates to:
  /// **'Number of lines'**
  String get numberOfLines;

  /// No description provided for @timeLimitSeconds.
  ///
  /// In en, this message translates to:
  /// **'Time Limit (seconds)'**
  String get timeLimitSeconds;

  /// No description provided for @noTimeLimit.
  ///
  /// In en, this message translates to:
  /// **'Off'**
  String get noTimeLimit;

  /// No description provided for @secondsShort.
  ///
  /// In en, this message translates to:
  /// **'s'**
  String get secondsShort;

  /// No description provided for @deleteAccount.
  ///
  /// In en, this message translates to:
  /// **'Delete account'**
  String get deleteAccount;

  /// No description provided for @deleteAccountQuestion.
  ///
  /// In en, this message translates to:
  /// **'Delete account?'**
  String get deleteAccountQuestion;

  /// No description provided for @deleteOnlineProfileBody.
  ///
  /// In en, this message translates to:
  /// **'This profile and its locally stored data will be removed from KChess. The Chess.com/Lichess account itself will not be changed.'**
  String get deleteOnlineProfileBody;

  /// No description provided for @deleteLocalProfileBody.
  ///
  /// In en, this message translates to:
  /// **'This profile and its locally stored PGN/FEN data will be removed from KChess.'**
  String get deleteLocalProfileBody;

  /// No description provided for @cancelAction.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get cancelAction;

  /// No description provided for @deleteAction.
  ///
  /// In en, this message translates to:
  /// **'Delete'**
  String get deleteAction;

  /// No description provided for @language.
  ///
  /// In en, this message translates to:
  /// **'Language'**
  String get language;

  /// No description provided for @theme.
  ///
  /// In en, this message translates to:
  /// **'Theme'**
  String get theme;

  /// No description provided for @systemTheme.
  ///
  /// In en, this message translates to:
  /// **'System'**
  String get systemTheme;

  /// No description provided for @lightTheme.
  ///
  /// In en, this message translates to:
  /// **'Light'**
  String get lightTheme;

  /// No description provided for @darkTheme.
  ///
  /// In en, this message translates to:
  /// **'Dark'**
  String get darkTheme;

  /// No description provided for @analysisSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get analysisSettingsTitle;

  /// No description provided for @analysisBoardGuidance.
  ///
  /// In en, this message translates to:
  /// **'Board guidance'**
  String get analysisBoardGuidance;

  /// No description provided for @analysisInformation.
  ///
  /// In en, this message translates to:
  /// **'Analysis information'**
  String get analysisInformation;

  /// No description provided for @bestMoveArrow.
  ///
  /// In en, this message translates to:
  /// **'Best move arrow'**
  String get bestMoveArrow;

  /// No description provided for @threatArrow.
  ///
  /// In en, this message translates to:
  /// **'Threat arrow'**
  String get threatArrow;

  /// No description provided for @evaluationBarSetting.
  ///
  /// In en, this message translates to:
  /// **'Evaluation bar'**
  String get evaluationBarSetting;

  /// No description provided for @showEngineLinesSetting.
  ///
  /// In en, this message translates to:
  /// **'Show engine lines'**
  String get showEngineLinesSetting;

  /// No description provided for @showClassificationsSetting.
  ///
  /// In en, this message translates to:
  /// **'Show move classifications'**
  String get showClassificationsSetting;

  /// No description provided for @showAccuracySetting.
  ///
  /// In en, this message translates to:
  /// **'Show accuracy'**
  String get showAccuracySetting;

  /// No description provided for @showTheorySetting.
  ///
  /// In en, this message translates to:
  /// **'Show theory information'**
  String get showTheorySetting;

  /// No description provided for @showResultSymbolsSetting.
  ///
  /// In en, this message translates to:
  /// **'Show result symbols'**
  String get showResultSymbolsSetting;

  /// No description provided for @designSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Design'**
  String get designSettingsTitle;

  /// No description provided for @generalSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'General'**
  String get generalSettingsTitle;

  /// No description provided for @dataStorageSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Data & storage'**
  String get dataStorageSettingsTitle;

  /// No description provided for @licensesAbout.
  ///
  /// In en, this message translates to:
  /// **'Licenses & about'**
  String get licensesAbout;

  /// No description provided for @stockfishPending.
  ///
  /// In en, this message translates to:
  /// **'Stockfish · local · GPLv3'**
  String get stockfishPending;

  /// No description provided for @provider.
  ///
  /// In en, this message translates to:
  /// **'Provider'**
  String get provider;

  /// No description provided for @theory.
  ///
  /// In en, this message translates to:
  /// **'Theory'**
  String get theory;

  /// No description provided for @brilliant.
  ///
  /// In en, this message translates to:
  /// **'Brilliant'**
  String get brilliant;

  /// No description provided for @critical.
  ///
  /// In en, this message translates to:
  /// **'Great Move'**
  String get critical;

  /// No description provided for @best.
  ///
  /// In en, this message translates to:
  /// **'Best'**
  String get best;

  /// No description provided for @excellent.
  ///
  /// In en, this message translates to:
  /// **'Excellent'**
  String get excellent;

  /// No description provided for @okay.
  ///
  /// In en, this message translates to:
  /// **'Okay'**
  String get okay;

  /// No description provided for @miss.
  ///
  /// In en, this message translates to:
  /// **'Miss'**
  String get miss;

  /// No description provided for @mistake.
  ///
  /// In en, this message translates to:
  /// **'Mistake'**
  String get mistake;

  /// No description provided for @blunder.
  ///
  /// In en, this message translates to:
  /// **'Blunder'**
  String get blunder;

  /// No description provided for @totalMoves.
  ///
  /// In en, this message translates to:
  /// **'Half-moves'**
  String get totalMoves;

  /// No description provided for @localAccuracy.
  ///
  /// In en, this message translates to:
  /// **'Local accuracy'**
  String get localAccuracy;

  /// No description provided for @close.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get close;

  /// No description provided for @previous.
  ///
  /// In en, this message translates to:
  /// **'Previous'**
  String get previous;

  /// No description provided for @next.
  ///
  /// In en, this message translates to:
  /// **'Next'**
  String get next;

  /// No description provided for @first.
  ///
  /// In en, this message translates to:
  /// **'First'**
  String get first;

  /// No description provided for @last.
  ///
  /// In en, this message translates to:
  /// **'Last'**
  String get last;

  /// No description provided for @playPause.
  ///
  /// In en, this message translates to:
  /// **'Play or pause'**
  String get playPause;

  /// No description provided for @whitePlayer.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get whitePlayer;

  /// No description provided for @blackPlayer.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get blackPlayer;

  /// No description provided for @analyzedMoves.
  ///
  /// In en, this message translates to:
  /// **'Classified moves'**
  String get analyzedMoves;

  /// No description provided for @bookGames.
  ///
  /// In en, this message translates to:
  /// **'Book games'**
  String get bookGames;

  /// No description provided for @analyzingGame.
  ///
  /// In en, this message translates to:
  /// **'Analyzing game'**
  String get analyzingGame;

  /// No description provided for @analyzedMovesProgress.
  ///
  /// In en, this message translates to:
  /// **'{completed} / {total} half-moves analyzed'**
  String analyzedMovesProgress(int completed, int total);

  /// No description provided for @openAnalysis.
  ///
  /// In en, this message translates to:
  /// **'Open analysis'**
  String get openAnalysis;

  /// No description provided for @bestMoveText.
  ///
  /// In en, this message translates to:
  /// **'{move} is the best move.'**
  String bestMoveText(String move);

  /// No description provided for @sidelineEngineTitle.
  ///
  /// In en, this message translates to:
  /// **'Sideline engine'**
  String get sidelineEngineTitle;

  /// No description provided for @mainLineLabel.
  ///
  /// In en, this message translates to:
  /// **'Main line'**
  String get mainLineLabel;

  /// No description provided for @sidelineLabel.
  ///
  /// In en, this message translates to:
  /// **'Your sideline'**
  String get sidelineLabel;

  /// No description provided for @liveEngineTheorySkipped.
  ///
  /// In en, this message translates to:
  /// **'Theory: live analysis skipped'**
  String get liveEngineTheorySkipped;

  /// No description provided for @liveEngineTargetReached.
  ///
  /// In en, this message translates to:
  /// **'Stockfish: analysis target reached'**
  String get liveEngineTargetReached;

  /// No description provided for @liveEngineProgress.
  ///
  /// In en, this message translates to:
  /// **'Stockfish analyzing live · {percent}%'**
  String liveEngineProgress(int percent);

  /// No description provided for @illegalOrFailedMove.
  ///
  /// In en, this message translates to:
  /// **'The move is illegal or could not be analyzed: {message}'**
  String illegalOrFailedMove(String message);

  /// No description provided for @myPlayer.
  ///
  /// In en, this message translates to:
  /// **'My player'**
  String get myPlayer;

  /// No description provided for @opponent.
  ///
  /// In en, this message translates to:
  /// **'Opponent'**
  String get opponent;

  /// No description provided for @engineQualityTitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis quality'**
  String get engineQualityTitle;

  /// No description provided for @engineResourcesTitle.
  ///
  /// In en, this message translates to:
  /// **'Resources'**
  String get engineResourcesTitle;

  /// No description provided for @adaptiveEarlyStop.
  ///
  /// In en, this message translates to:
  /// **'Adaptive analysis'**
  String get adaptiveEarlyStop;

  /// No description provided for @threads.
  ///
  /// In en, this message translates to:
  /// **'Threads'**
  String get threads;

  /// No description provided for @hashMemory.
  ///
  /// In en, this message translates to:
  /// **'Hash memory'**
  String get hashMemory;

  /// No description provided for @boardDisplayTitle.
  ///
  /// In en, this message translates to:
  /// **'Board display'**
  String get boardDisplayTitle;

  /// No description provided for @rotateBoard.
  ///
  /// In en, this message translates to:
  /// **'Rotate board'**
  String get rotateBoard;

  /// No description provided for @showBoardCoordinates.
  ///
  /// In en, this message translates to:
  /// **'Board coordinates'**
  String get showBoardCoordinates;

  /// No description provided for @highlightLastMove.
  ///
  /// In en, this message translates to:
  /// **'Highlight last move'**
  String get highlightLastMove;

  /// No description provided for @highlightSelectedSquare.
  ///
  /// In en, this message translates to:
  /// **'Highlight selected square'**
  String get highlightSelectedSquare;

  /// No description provided for @behaviorTitle.
  ///
  /// In en, this message translates to:
  /// **'Behavior'**
  String get behaviorTitle;

  /// No description provided for @autoSyncOnline.
  ///
  /// In en, this message translates to:
  /// **'Automatically sync online profiles'**
  String get autoSyncOnline;

  /// No description provided for @confirmBeforeDelete.
  ///
  /// In en, this message translates to:
  /// **'Confirm before deleting'**
  String get confirmBeforeDelete;

  /// No description provided for @analysisCacheTitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis cache'**
  String get analysisCacheTitle;

  /// No description provided for @useGlobalAnalysisCache.
  ///
  /// In en, this message translates to:
  /// **'Use shared position cache'**
  String get useGlobalAnalysisCache;

  /// No description provided for @clearAnalysisCache.
  ///
  /// In en, this message translates to:
  /// **'Clear analysis cache'**
  String get clearAnalysisCache;

  /// No description provided for @clearAnalysisCacheQuestion.
  ///
  /// In en, this message translates to:
  /// **'Clear analysis cache?'**
  String get clearAnalysisCacheQuestion;

  /// No description provided for @clearAnalysisCacheBody.
  ///
  /// In en, this message translates to:
  /// **'The shared position cache will be deleted. Your games, favorites, downloads, and completed game analyses will be kept.'**
  String get clearAnalysisCacheBody;

  /// No description provided for @analysisCacheCleared.
  ///
  /// In en, this message translates to:
  /// **'Analysis cache cleared.'**
  String get analysisCacheCleared;

  /// No description provided for @diagnosticsTitle.
  ///
  /// In en, this message translates to:
  /// **'Diagnostics'**
  String get diagnosticsTitle;

  /// No description provided for @diagnosticLogging.
  ///
  /// In en, this message translates to:
  /// **'Diagnostic logging'**
  String get diagnosticLogging;

  /// No description provided for @deleteLocalGameQuestion.
  ///
  /// In en, this message translates to:
  /// **'Delete local entry?'**
  String get deleteLocalGameQuestion;

  /// No description provided for @deleteLocalGameBody.
  ///
  /// In en, this message translates to:
  /// **'The stored PGN/FEN and its local analysis will be permanently removed.'**
  String get deleteLocalGameBody;

  /// No description provided for @profileRatings.
  ///
  /// In en, this message translates to:
  /// **'Ratings'**
  String get profileRatings;

  /// No description provided for @profileGameOverview.
  ///
  /// In en, this message translates to:
  /// **'Game overview'**
  String get profileGameOverview;

  /// No description provided for @profileWins.
  ///
  /// In en, this message translates to:
  /// **'Wins'**
  String get profileWins;

  /// No description provided for @profileDraws.
  ///
  /// In en, this message translates to:
  /// **'Draws'**
  String get profileDraws;

  /// No description provided for @profileLosses.
  ///
  /// In en, this message translates to:
  /// **'Losses'**
  String get profileLosses;

  /// No description provided for @ratingRapid.
  ///
  /// In en, this message translates to:
  /// **'Rapid'**
  String get ratingRapid;

  /// No description provided for @ratingBlitz.
  ///
  /// In en, this message translates to:
  /// **'Blitz'**
  String get ratingBlitz;

  /// No description provided for @ratingBullet.
  ///
  /// In en, this message translates to:
  /// **'Bullet'**
  String get ratingBullet;

  /// No description provided for @ratingDaily.
  ///
  /// In en, this message translates to:
  /// **'Daily'**
  String get ratingDaily;

  /// No description provided for @ratingClassical.
  ///
  /// In en, this message translates to:
  /// **'Classical'**
  String get ratingClassical;

  /// No description provided for @ratingChess960.
  ///
  /// In en, this message translates to:
  /// **'Chess960'**
  String get ratingChess960;

  /// No description provided for @ratingFide.
  ///
  /// In en, this message translates to:
  /// **'FIDE'**
  String get ratingFide;

  /// No description provided for @statsWins.
  ///
  /// In en, this message translates to:
  /// **'Wins'**
  String get statsWins;

  /// No description provided for @statsDraws.
  ///
  /// In en, this message translates to:
  /// **'Draws'**
  String get statsDraws;

  /// No description provided for @statsLosses.
  ///
  /// In en, this message translates to:
  /// **'Losses'**
  String get statsLosses;

  /// No description provided for @statsAll.
  ///
  /// In en, this message translates to:
  /// **'All'**
  String get statsAll;

  /// No description provided for @statsPhaseTitle.
  ///
  /// In en, this message translates to:
  /// **'By game phase'**
  String get statsPhaseTitle;

  /// No description provided for @statsPhaseOpening.
  ///
  /// In en, this message translates to:
  /// **'Opening (1–12)'**
  String get statsPhaseOpening;

  /// No description provided for @statsPhaseMiddlegame.
  ///
  /// In en, this message translates to:
  /// **'Middlegame (13–30)'**
  String get statsPhaseMiddlegame;

  /// No description provided for @statsPhaseEndgame.
  ///
  /// In en, this message translates to:
  /// **'Endgame (31+)'**
  String get statsPhaseEndgame;

  /// No description provided for @statsPhaseOpeningShort.
  ///
  /// In en, this message translates to:
  /// **'Opening'**
  String get statsPhaseOpeningShort;

  /// No description provided for @statsPhaseMiddlegameShort.
  ///
  /// In en, this message translates to:
  /// **'Middlegame'**
  String get statsPhaseMiddlegameShort;

  /// No description provided for @statsPhaseEndgameShort.
  ///
  /// In en, this message translates to:
  /// **'Endgame'**
  String get statsPhaseEndgameShort;

  /// No description provided for @statsPhaseGames.
  ///
  /// In en, this message translates to:
  /// **'games'**
  String get statsPhaseGames;

  /// No description provided for @statsPhaseWinWord.
  ///
  /// In en, this message translates to:
  /// **'win'**
  String get statsPhaseWinWord;

  /// No description provided for @statsPhaseEmpty.
  ///
  /// In en, this message translates to:
  /// **'Not enough data on game phases.'**
  String get statsPhaseEmpty;

  /// No description provided for @statsPhaseNoProfile.
  ///
  /// In en, this message translates to:
  /// **'Create or select a profile to see statistics.'**
  String get statsPhaseNoProfile;

  /// No description provided for @statsPhaseError.
  ///
  /// In en, this message translates to:
  /// **'Could not load game phases.'**
  String get statsPhaseError;

  /// No description provided for @statsPhaseRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsPhaseRetry;

  /// No description provided for @statsPhaseClassifiedNote.
  ///
  /// In en, this message translates to:
  /// **'{classified} of {total} games'**
  String statsPhaseClassifiedNote(int classified, int total);

  /// No description provided for @statsTitle.
  ///
  /// In en, this message translates to:
  /// **'Statistics'**
  String get statsTitle;

  /// No description provided for @statsIntroTitle.
  ///
  /// In en, this message translates to:
  /// **'Your chess performance'**
  String get statsIntroTitle;

  /// No description provided for @statsFormTitle.
  ///
  /// In en, this message translates to:
  /// **'Recent form'**
  String get statsFormTitle;

  /// No description provided for @statsFormVersus.
  ///
  /// In en, this message translates to:
  /// **'vs'**
  String get statsFormVersus;

  /// No description provided for @statsFormEmpty.
  ///
  /// In en, this message translates to:
  /// **'No recent games to show.'**
  String get statsFormEmpty;

  /// No description provided for @statsFormError.
  ///
  /// In en, this message translates to:
  /// **'Could not load recent games.'**
  String get statsFormError;

  /// No description provided for @statsFormRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsFormRetry;

  /// No description provided for @statsOverviewTitle.
  ///
  /// In en, this message translates to:
  /// **'Overview'**
  String get statsOverviewTitle;

  /// No description provided for @statsOverviewGames.
  ///
  /// In en, this message translates to:
  /// **'Games'**
  String get statsOverviewGames;

  /// No description provided for @statsOverviewWinRate.
  ///
  /// In en, this message translates to:
  /// **'Win rate'**
  String get statsOverviewWinRate;

  /// No description provided for @statsOverviewScore.
  ///
  /// In en, this message translates to:
  /// **'Score'**
  String get statsOverviewScore;

  /// No description provided for @statsOverviewRecord.
  ///
  /// In en, this message translates to:
  /// **'Record'**
  String get statsOverviewRecord;

  /// No description provided for @statsOverviewByColor.
  ///
  /// In en, this message translates to:
  /// **'By color'**
  String get statsOverviewByColor;

  /// No description provided for @statsOverviewByTimeControl.
  ///
  /// In en, this message translates to:
  /// **'By time control'**
  String get statsOverviewByTimeControl;

  /// No description provided for @statsOverviewWhite.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get statsOverviewWhite;

  /// No description provided for @statsOverviewBlack.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get statsOverviewBlack;

  /// No description provided for @statsOverviewEmpty.
  ///
  /// In en, this message translates to:
  /// **'No games yet. Sync an online profile or import games to see your statistics.'**
  String get statsOverviewEmpty;

  /// No description provided for @statsOverviewNoProfile.
  ///
  /// In en, this message translates to:
  /// **'Create or select a profile to see statistics.'**
  String get statsOverviewNoProfile;

  /// No description provided for @statsOverviewNoGamesForFilter.
  ///
  /// In en, this message translates to:
  /// **'No games for the selected time control.'**
  String get statsOverviewNoGamesForFilter;

  /// No description provided for @statsOverviewError.
  ///
  /// In en, this message translates to:
  /// **'Could not load statistics.'**
  String get statsOverviewError;

  /// No description provided for @statsOverviewRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsOverviewRetry;

  /// No description provided for @statsTerminationTitle.
  ///
  /// In en, this message translates to:
  /// **'How games end'**
  String get statsTerminationTitle;

  /// No description provided for @statsTerminationCheckmate.
  ///
  /// In en, this message translates to:
  /// **'Checkmate'**
  String get statsTerminationCheckmate;

  /// No description provided for @statsTerminationResignation.
  ///
  /// In en, this message translates to:
  /// **'Resignation'**
  String get statsTerminationResignation;

  /// No description provided for @statsTerminationTimeout.
  ///
  /// In en, this message translates to:
  /// **'On time'**
  String get statsTerminationTimeout;

  /// No description provided for @statsTerminationDraw.
  ///
  /// In en, this message translates to:
  /// **'Draw'**
  String get statsTerminationDraw;

  /// No description provided for @statsTerminationOther.
  ///
  /// In en, this message translates to:
  /// **'Other'**
  String get statsTerminationOther;

  /// No description provided for @statsTerminationWonByCheckmate.
  ///
  /// In en, this message translates to:
  /// **'Won by checkmate'**
  String get statsTerminationWonByCheckmate;

  /// No description provided for @statsTerminationLostByCheckmate.
  ///
  /// In en, this message translates to:
  /// **'Lost by checkmate'**
  String get statsTerminationLostByCheckmate;

  /// No description provided for @statsTerminationOpponentResigned.
  ///
  /// In en, this message translates to:
  /// **'Opponent resigned'**
  String get statsTerminationOpponentResigned;

  /// No description provided for @statsTerminationSelfResigned.
  ///
  /// In en, this message translates to:
  /// **'Resigned'**
  String get statsTerminationSelfResigned;

  /// No description provided for @statsTerminationOpponentFlagged.
  ///
  /// In en, this message translates to:
  /// **'Opponent ran out of time'**
  String get statsTerminationOpponentFlagged;

  /// No description provided for @statsTerminationSelfFlagged.
  ///
  /// In en, this message translates to:
  /// **'Ran out of time'**
  String get statsTerminationSelfFlagged;

  /// No description provided for @statsTerminationWonGeneric.
  ///
  /// In en, this message translates to:
  /// **'Won'**
  String get statsTerminationWonGeneric;

  /// No description provided for @statsTerminationLostGeneric.
  ///
  /// In en, this message translates to:
  /// **'Lost'**
  String get statsTerminationLostGeneric;

  /// No description provided for @statsTerminationEmpty.
  ///
  /// In en, this message translates to:
  /// **'Not enough data on how games ended.'**
  String get statsTerminationEmpty;

  /// No description provided for @statsTerminationNoProfile.
  ///
  /// In en, this message translates to:
  /// **'Create or select a profile to see statistics.'**
  String get statsTerminationNoProfile;

  /// No description provided for @statsTerminationError.
  ///
  /// In en, this message translates to:
  /// **'Could not load game endings.'**
  String get statsTerminationError;

  /// No description provided for @statsTerminationRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsTerminationRetry;

  /// No description provided for @statsRatingTitle.
  ///
  /// In en, this message translates to:
  /// **'Rating trend'**
  String get statsRatingTitle;

  /// No description provided for @statsRatingEmpty.
  ///
  /// In en, this message translates to:
  /// **'Not enough rating data to draw a trend.'**
  String get statsRatingEmpty;

  /// No description provided for @statsRatingError.
  ///
  /// In en, this message translates to:
  /// **'Could not load rating data.'**
  String get statsRatingError;

  /// No description provided for @statsRatingRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsRatingRetry;

  /// No description provided for @statsOpeningGamesAll.
  ///
  /// In en, this message translates to:
  /// **'All'**
  String get statsOpeningGamesAll;

  /// No description provided for @statsOpeningGamesWon.
  ///
  /// In en, this message translates to:
  /// **'Won'**
  String get statsOpeningGamesWon;

  /// No description provided for @statsOpeningGamesLost.
  ///
  /// In en, this message translates to:
  /// **'Lost'**
  String get statsOpeningGamesLost;

  /// No description provided for @statsOpeningGamesByCheckmate.
  ///
  /// In en, this message translates to:
  /// **'by checkmate'**
  String get statsOpeningGamesByCheckmate;

  /// No description provided for @statsOpeningGamesByResignation.
  ///
  /// In en, this message translates to:
  /// **'by resignation'**
  String get statsOpeningGamesByResignation;

  /// No description provided for @statsOpeningGamesByTimeout.
  ///
  /// In en, this message translates to:
  /// **'on time'**
  String get statsOpeningGamesByTimeout;

  /// No description provided for @statsOpeningGamesByDraw.
  ///
  /// In en, this message translates to:
  /// **'draw'**
  String get statsOpeningGamesByDraw;

  /// No description provided for @statsOpeningGamesEmpty.
  ///
  /// In en, this message translates to:
  /// **'No games for this selection.'**
  String get statsOpeningGamesEmpty;

  /// No description provided for @statsOpeningGamesError.
  ///
  /// In en, this message translates to:
  /// **'Could not load games.'**
  String get statsOpeningGamesError;

  /// No description provided for @statsOpeningsTitle.
  ///
  /// In en, this message translates to:
  /// **'Top openings'**
  String get statsOpeningsTitle;

  /// No description provided for @statsOpeningsMostPlayed.
  ///
  /// In en, this message translates to:
  /// **'Most played'**
  String get statsOpeningsMostPlayed;

  /// No description provided for @statsOpeningsBestWinRate.
  ///
  /// In en, this message translates to:
  /// **'Best win rate'**
  String get statsOpeningsBestWinRate;

  /// No description provided for @statsOpeningsMinGamesHint.
  ///
  /// In en, this message translates to:
  /// **'At least 3 games per opening.'**
  String get statsOpeningsMinGamesHint;

  /// No description provided for @statsOpeningsClassifiedGames.
  ///
  /// In en, this message translates to:
  /// **'games with a named opening'**
  String get statsOpeningsClassifiedGames;

  /// No description provided for @statsOpeningsGames.
  ///
  /// In en, this message translates to:
  /// **'games'**
  String get statsOpeningsGames;

  /// No description provided for @statsOpeningsVariations.
  ///
  /// In en, this message translates to:
  /// **'variations'**
  String get statsOpeningsVariations;

  /// No description provided for @statsOpeningsBaseLine.
  ///
  /// In en, this message translates to:
  /// **'Base line'**
  String get statsOpeningsBaseLine;

  /// No description provided for @statsOpeningsWhite.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get statsOpeningsWhite;

  /// No description provided for @statsOpeningsBlack.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get statsOpeningsBlack;

  /// No description provided for @statsOpeningsUnknownColor.
  ///
  /// In en, this message translates to:
  /// **'Other'**
  String get statsOpeningsUnknownColor;

  /// No description provided for @statsOpeningsWinRateShort.
  ///
  /// In en, this message translates to:
  /// **'win'**
  String get statsOpeningsWinRateShort;

  /// No description provided for @statsOpeningsNoOpeningsForColor.
  ///
  /// In en, this message translates to:
  /// **'No openings for this color yet.'**
  String get statsOpeningsNoOpeningsForColor;

  /// No description provided for @statsOpeningsNoOpeningsForWinRate.
  ///
  /// In en, this message translates to:
  /// **'No opening with at least 3 games.'**
  String get statsOpeningsNoOpeningsForWinRate;

  /// No description provided for @statsOpeningsEmpty.
  ///
  /// In en, this message translates to:
  /// **'No named openings yet. Synced and imported games are classified automatically.'**
  String get statsOpeningsEmpty;

  /// No description provided for @statsOpeningsNoProfile.
  ///
  /// In en, this message translates to:
  /// **'Create or select a profile to see openings.'**
  String get statsOpeningsNoProfile;

  /// No description provided for @statsOpeningsError.
  ///
  /// In en, this message translates to:
  /// **'Could not load openings.'**
  String get statsOpeningsError;

  /// No description provided for @statsOpeningsRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get statsOpeningsRetry;

  /// No description provided for @statsCompareTitle.
  ///
  /// In en, this message translates to:
  /// **'Player comparison'**
  String get statsCompareTitle;

  /// No description provided for @statsCompareUsernameLabel.
  ///
  /// In en, this message translates to:
  /// **'Chess.com username'**
  String get statsCompareUsernameLabel;

  /// No description provided for @statsCompareUsernameHint.
  ///
  /// In en, this message translates to:
  /// **'e.g. hikaru'**
  String get statsCompareUsernameHint;

  /// No description provided for @statsCompareCompare.
  ///
  /// In en, this message translates to:
  /// **'Compare'**
  String get statsCompareCompare;

  /// No description provided for @statsCompareLoadingHint.
  ///
  /// In en, this message translates to:
  /// **'Fetching and analysing the opponent\'s games…'**
  String get statsCompareLoadingHint;

  /// No description provided for @statsComparePrompt.
  ///
  /// In en, this message translates to:
  /// **'Enter a Chess.com username to compare stats.'**
  String get statsComparePrompt;

  /// No description provided for @statsCompareYou.
  ///
  /// In en, this message translates to:
  /// **'You'**
  String get statsCompareYou;

  /// No description provided for @statsCompareOpponent.
  ///
  /// In en, this message translates to:
  /// **'Opponent'**
  String get statsCompareOpponent;

  /// No description provided for @statsCompareH2hTitle.
  ///
  /// In en, this message translates to:
  /// **'Head-to-head'**
  String get statsCompareH2hTitle;

  /// No description provided for @statsCompareDirectGames.
  ///
  /// In en, this message translates to:
  /// **'direct games'**
  String get statsCompareDirectGames;

  /// No description provided for @statsCompareWins.
  ///
  /// In en, this message translates to:
  /// **'Wins'**
  String get statsCompareWins;

  /// No description provided for @statsCompareDraws.
  ///
  /// In en, this message translates to:
  /// **'Draws'**
  String get statsCompareDraws;

  /// No description provided for @statsCompareLosses.
  ///
  /// In en, this message translates to:
  /// **'Losses'**
  String get statsCompareLosses;

  /// No description provided for @statsComparePerformanceCompare.
  ///
  /// In en, this message translates to:
  /// **'Performance comparison'**
  String get statsComparePerformanceCompare;

  /// No description provided for @statsCompareWinRateWhite.
  ///
  /// In en, this message translates to:
  /// **'Win rate as White'**
  String get statsCompareWinRateWhite;

  /// No description provided for @statsCompareWinRateBlack.
  ///
  /// In en, this message translates to:
  /// **'Win rate as Black'**
  String get statsCompareWinRateBlack;

  /// No description provided for @statsCompareFlagging.
  ///
  /// In en, this message translates to:
  /// **'Losses on time'**
  String get statsCompareFlagging;

  /// No description provided for @statsCompareOpeningMatchup.
  ///
  /// In en, this message translates to:
  /// **'Opening matchup'**
  String get statsCompareOpeningMatchup;

  /// No description provided for @statsCompareMatchupSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Your openings vs. the opponent\'s win rate with the opposite colour.'**
  String get statsCompareMatchupSubtitle;

  /// No description provided for @statsCompareOpeningColumn.
  ///
  /// In en, this message translates to:
  /// **'Opening'**
  String get statsCompareOpeningColumn;

  /// No description provided for @statsCompareGamesShort.
  ///
  /// In en, this message translates to:
  /// **'games'**
  String get statsCompareGamesShort;

  /// No description provided for @statsCompareNoMatchups.
  ///
  /// In en, this message translates to:
  /// **'No shared openings found.'**
  String get statsCompareNoMatchups;

  /// No description provided for @statsCompareNoLeaks.
  ///
  /// In en, this message translates to:
  /// **'No clear weaknesses found.'**
  String get statsCompareNoLeaks;

  /// No description provided for @statsCompareStrategyTitle.
  ///
  /// In en, this message translates to:
  /// **'Recommended strategy'**
  String get statsCompareStrategyTitle;

  /// No description provided for @statsCompareColorWhite.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get statsCompareColorWhite;

  /// No description provided for @statsCompareColorBlack.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get statsCompareColorBlack;

  /// No description provided for @statsCompareErrorPrefix.
  ///
  /// In en, this message translates to:
  /// **'Error'**
  String get statsCompareErrorPrefix;

  /// No description provided for @statsCompareGamesAnalyzed.
  ///
  /// In en, this message translates to:
  /// **'{games} games analysed across {months} months'**
  String statsCompareGamesAnalyzed(int games, int months);

  /// No description provided for @statsTerminationSpotlight.
  ///
  /// In en, this message translates to:
  /// **'Most common ending: {label} — {share}% of all games, {lossPercent}% of them losses.'**
  String statsTerminationSpotlight(String label, int share, int lossPercent);

  /// No description provided for @statsCompareSelfBadge.
  ///
  /// In en, this message translates to:
  /// **'Self-comparison (mirror)'**
  String get statsCompareSelfBadge;

  /// No description provided for @statsCompareSelfH2H.
  ///
  /// In en, this message translates to:
  /// **'No games against yourself · This is a self-analysis of your own profile.'**
  String get statsCompareSelfH2H;

  /// No description provided for @statsCompareScopeNote.
  ///
  /// In en, this message translates to:
  /// **'Comparison based on the last {games} loaded games.'**
  String statsCompareScopeNote(int games);

  /// No description provided for @statsCompareMinSampleNote.
  ///
  /// In en, this message translates to:
  /// **'Only openings with at least 5 games on both sides.'**
  String get statsCompareMinSampleNote;

  /// No description provided for @statsCompareOwnWeaknessTitle.
  ///
  /// In en, this message translates to:
  /// **'Your own weak spots'**
  String get statsCompareOwnWeaknessTitle;

  /// No description provided for @statsCompareOwnWeakness.
  ///
  /// In en, this message translates to:
  /// **'Weak spot as {color}: {opening} — only {rate} win rate (from {games} games).'**
  String statsCompareOwnWeakness(
    String color,
    String opening,
    String rate,
    int games,
  );

  /// No description provided for @statsCompareNoOwnWeakness.
  ///
  /// In en, this message translates to:
  /// **'No clear weak spots with at least 5 games per opening.'**
  String get statsCompareNoOwnWeakness;

  /// No description provided for @statsCompareOpenWhite.
  ///
  /// In en, this message translates to:
  /// **'Open with {opening} — your opponent scores only {rate} against it as Black (from {games} games).'**
  String statsCompareOpenWhite(String opening, String rate, int games);

  /// No description provided for @statsCompareAnswerBlack.
  ///
  /// In en, this message translates to:
  /// **'As Black: choose {opening} — your opponent scores only {rate} against it as White (from {games} games).'**
  String statsCompareAnswerBlack(String opening, String rate, int games);

  /// No description provided for @statsCompareSampleScope.
  ///
  /// In en, this message translates to:
  /// **'You: your entire local library · Opponent: the last {months} months ({games} games). The two sides are different samples, so the numbers can differ even when you compare a profile with itself.'**
  String statsCompareSampleScope(int months, int games);

  /// No description provided for @trainingSection.
  ///
  /// In en, this message translates to:
  /// **'Training'**
  String get trainingSection;

  /// No description provided for @trainingIntroTitle.
  ///
  /// In en, this message translates to:
  /// **'Training arena'**
  String get trainingIntroTitle;

  /// No description provided for @trainingOpeningTitle.
  ///
  /// In en, this message translates to:
  /// **'Opening lab'**
  String get trainingOpeningTitle;

  /// No description provided for @trainingOpeningAction.
  ///
  /// In en, this message translates to:
  /// **'Train lines'**
  String get trainingOpeningAction;

  /// No description provided for @trainingNemesisBadge.
  ///
  /// In en, this message translates to:
  /// **'Nemesis: {opening} — only {rate} win rate'**
  String trainingNemesisBadge(String opening, String rate);

  /// No description provided for @trainingNemesisNone.
  ///
  /// In en, this message translates to:
  /// **'No weak line found in your statistics yet.'**
  String get trainingNemesisNone;

  /// No description provided for @trainingTacticsTitle.
  ///
  /// In en, this message translates to:
  /// **'Blunder buster'**
  String get trainingTacticsTitle;

  /// No description provided for @trainingTacticsSolved.
  ///
  /// In en, this message translates to:
  /// **'{count} tactics solved'**
  String trainingTacticsSolved(int count);

  /// No description provided for @trainingTacticsAction.
  ///
  /// In en, this message translates to:
  /// **'Start tactics'**
  String get trainingTacticsAction;

  /// No description provided for @trainingTacticsEmpty.
  ///
  /// In en, this message translates to:
  /// **'No tactics puzzles in the catalogue yet.'**
  String get trainingTacticsEmpty;

  /// No description provided for @trainingEndgameTitle.
  ///
  /// In en, this message translates to:
  /// **'Endgame academy'**
  String get trainingEndgameTitle;

  /// No description provided for @trainingEndgameProgress.
  ///
  /// In en, this message translates to:
  /// **'{mastered} / {total} positions mastered ({percent}%)'**
  String trainingEndgameProgress(int mastered, int total, int percent);

  /// No description provided for @trainingEndgameAction.
  ///
  /// In en, this message translates to:
  /// **'Open endgames'**
  String get trainingEndgameAction;

  /// No description provided for @trainingMastered.
  ///
  /// In en, this message translates to:
  /// **'Mastered'**
  String get trainingMastered;

  /// No description provided for @trainingStreak.
  ///
  /// In en, this message translates to:
  /// **'{done}/{total} clean repeats'**
  String trainingStreak(int done, int total);

  /// No description provided for @trainingOpeningLabSelected.
  ///
  /// In en, this message translates to:
  /// **'Selected line'**
  String get trainingOpeningLabSelected;

  /// No description provided for @statsTrainOpening.
  ///
  /// In en, this message translates to:
  /// **'Train'**
  String get statsTrainOpening;

  /// No description provided for @trainingGoalWin.
  ///
  /// In en, this message translates to:
  /// **'{side} to move — win the position'**
  String trainingGoalWin(String side);

  /// No description provided for @trainingGoalDraw.
  ///
  /// In en, this message translates to:
  /// **'{side} to move — hold the draw'**
  String trainingGoalDraw(String side);

  /// No description provided for @trainingWrongMove.
  ///
  /// In en, this message translates to:
  /// **'Not the best move. Give it another try.'**
  String get trainingWrongMove;

  /// No description provided for @trainingSolvedTitle.
  ///
  /// In en, this message translates to:
  /// **'Excellent! Position solved.'**
  String get trainingSolvedTitle;

  /// No description provided for @trainingSolvedClean.
  ///
  /// In en, this message translates to:
  /// **'Solved without a slip — this one counts towards mastery.'**
  String get trainingSolvedClean;

  /// No description provided for @trainingSolvedWithErrors.
  ///
  /// In en, this message translates to:
  /// **'Solved, but with corrections. Only a clean run counts towards mastery.'**
  String get trainingSolvedWithErrors;

  /// No description provided for @trainingPracticeAgain.
  ///
  /// In en, this message translates to:
  /// **'Practise again'**
  String get trainingPracticeAgain;

  /// No description provided for @trainingNextEndgame.
  ///
  /// In en, this message translates to:
  /// **'Next endgame'**
  String get trainingNextEndgame;

  /// No description provided for @trainingBackToList.
  ///
  /// In en, this message translates to:
  /// **'Back to the list'**
  String get trainingBackToList;

  /// No description provided for @trainingMoveProgress.
  ///
  /// In en, this message translates to:
  /// **'Move {done} of {total}'**
  String trainingMoveProgress(int done, int total);

  /// No description provided for @trainingBoardError.
  ///
  /// In en, this message translates to:
  /// **'The position could not be loaded.'**
  String get trainingBoardError;

  /// No description provided for @trainingShowHint.
  ///
  /// In en, this message translates to:
  /// **'Show hint'**
  String get trainingShowHint;

  /// No description provided for @trainingYourMove.
  ///
  /// In en, this message translates to:
  /// **'Your move'**
  String get trainingYourMove;

  /// No description provided for @trainingRestart.
  ///
  /// In en, this message translates to:
  /// **'Restart'**
  String get trainingRestart;

  /// No description provided for @trainingOpeningLabStart.
  ///
  /// In en, this message translates to:
  /// **'Train line'**
  String get trainingOpeningLabStart;

  /// No description provided for @trainingOpeningLabBackToOverview.
  ///
  /// In en, this message translates to:
  /// **'Back to the overview'**
  String get trainingOpeningLabBackToOverview;

  /// No description provided for @trainingOpeningTreeEmpty.
  ///
  /// In en, this message translates to:
  /// **'No opening lines available yet.'**
  String get trainingOpeningTreeEmpty;

  /// No description provided for @trainingOpeningTreeExpand.
  ///
  /// In en, this message translates to:
  /// **'Show variations'**
  String get trainingOpeningTreeExpand;

  /// No description provided for @trainingOpeningTreeCollapse.
  ///
  /// In en, this message translates to:
  /// **'Hide variations'**
  String get trainingOpeningTreeCollapse;

  /// No description provided for @trainingOpeningTreeVariations.
  ///
  /// In en, this message translates to:
  /// **'{count,plural, =1{1 variation} other{{count} variations}}'**
  String trainingOpeningTreeVariations(int count);

  /// No description provided for @trainingOpeningTreeLoadFailed.
  ///
  /// In en, this message translates to:
  /// **'That line is not in the database yet.'**
  String get trainingOpeningTreeLoadFailed;

  /// No description provided for @trainingOpeningTreeZoomIn.
  ///
  /// In en, this message translates to:
  /// **'Zoom in'**
  String get trainingOpeningTreeZoomIn;

  /// No description provided for @trainingOpeningTreeZoomOut.
  ///
  /// In en, this message translates to:
  /// **'Zoom out'**
  String get trainingOpeningTreeZoomOut;

  /// No description provided for @trainingOpeningTreeFit.
  ///
  /// In en, this message translates to:
  /// **'Fit the tree'**
  String get trainingOpeningTreeFit;

  /// No description provided for @trainingOpeningTreeHint.
  ///
  /// In en, this message translates to:
  /// **'Tap a card to unfold its variations, or train the line right away.'**
  String get trainingOpeningTreeHint;

  /// No description provided for @trainingOpeningIncorrectMove.
  ///
  /// In en, this message translates to:
  /// **'Incorrect move'**
  String get trainingOpeningIncorrectMove;

  /// No description provided for @trainingOpeningTryAgain.
  ///
  /// In en, this message translates to:
  /// **'Try again.'**
  String get trainingOpeningTryAgain;

  /// No description provided for @trainingOpeningPlayInstead.
  ///
  /// In en, this message translates to:
  /// **'The move was {move}'**
  String trainingOpeningPlayInstead(String move);

  /// No description provided for @trainingOpeningScenariosTitle.
  ///
  /// In en, this message translates to:
  /// **'Opening scenarios'**
  String get trainingOpeningScenariosTitle;

  /// No description provided for @trainingOpeningScenariosCaption.
  ///
  /// In en, this message translates to:
  /// **'Play the book move against every reply the book can throw at you.'**
  String get trainingOpeningScenariosCaption;

  /// No description provided for @trainingOpeningScenarioDepth.
  ///
  /// In en, this message translates to:
  /// **'Depth mastery: {done}/{total} moves'**
  String trainingOpeningScenarioDepth(int done, int total);

  /// No description provided for @trainingOpeningPlayAs.
  ///
  /// In en, this message translates to:
  /// **'Play as'**
  String get trainingOpeningPlayAs;

  /// No description provided for @trainingOpeningOpponentPlayed.
  ///
  /// In en, this message translates to:
  /// **'Opponent played {move}.'**
  String trainingOpeningOpponentPlayed(String move);

  /// No description provided for @trainingOpeningScenarioReady.
  ///
  /// In en, this message translates to:
  /// **'Position after {move}.'**
  String trainingOpeningScenarioReady(String move);

  /// No description provided for @trainingOpeningFindBest.
  ///
  /// In en, this message translates to:
  /// **'Find the best engine response.'**
  String get trainingOpeningFindBest;

  /// No description provided for @trainingOpeningCurrentDepth.
  ///
  /// In en, this message translates to:
  /// **'Current depth: move {done} / {total}'**
  String trainingOpeningCurrentDepth(int done, int total);

  /// No description provided for @trainingOpeningCorrect.
  ///
  /// In en, this message translates to:
  /// **'{move} — that is the book move.'**
  String trainingOpeningCorrect(String move);

  /// No description provided for @trainingOpeningAlternative.
  ///
  /// In en, this message translates to:
  /// **'{move} works too — book choice #{rank}.'**
  String trainingOpeningAlternative(String move, int rank);

  /// No description provided for @trainingOpeningDepthReached.
  ///
  /// In en, this message translates to:
  /// **'Depth {depth} reached'**
  String trainingOpeningDepthReached(int depth);

  /// No description provided for @trainingOpeningBookExhausted.
  ///
  /// In en, this message translates to:
  /// **'The book ends here — you answered everything it knows.'**
  String get trainingOpeningBookExhausted;

  /// No description provided for @trainingOpeningNoBook.
  ///
  /// In en, this message translates to:
  /// **'The opening book has no moves for this position.'**
  String get trainingOpeningNoBook;

  /// No description provided for @trainingOpeningDrillClean.
  ///
  /// In en, this message translates to:
  /// **'No slips — this run counts towards mastery.'**
  String get trainingOpeningDrillClean;

  /// No description provided for @trainingOpeningDrillWithErrors.
  ///
  /// In en, this message translates to:
  /// **'Finished with corrections. Only a clean run counts towards mastery.'**
  String get trainingOpeningDrillWithErrors;

  /// No description provided for @trainingOpeningDrillAgain.
  ///
  /// In en, this message translates to:
  /// **'Drill again'**
  String get trainingOpeningDrillAgain;

  /// No description provided for @trainingOpeningBookChoices.
  ///
  /// In en, this message translates to:
  /// **'{count, plural, =1{the only book reply} other{one of {count} book replies}}'**
  String trainingOpeningBookChoices(int count);

  /// No description provided for @playAgainstBot.
  ///
  /// In en, this message translates to:
  /// **'Play against a bot'**
  String get playAgainstBot;

  /// No description provided for @continueAgainstBot.
  ///
  /// In en, this message translates to:
  /// **'Continue against bot'**
  String get continueAgainstBot;

  /// No description provided for @botPlayerColor.
  ///
  /// In en, this message translates to:
  /// **'Your color'**
  String get botPlayerColor;

  /// No description provided for @temporaryBotGameNotSaved.
  ///
  /// In en, this message translates to:
  /// **'This game is not saved and is discarded completely when you close or cancel it.'**
  String get temporaryBotGameNotSaved;

  /// No description provided for @botGameLog.
  ///
  /// In en, this message translates to:
  /// **'Bot game history'**
  String get botGameLog;

  /// No description provided for @botGameLogEmpty.
  ///
  /// In en, this message translates to:
  /// **'No saved bot games yet.'**
  String get botGameLogEmpty;

  /// No description provided for @botGameLogLoadFailed.
  ///
  /// In en, this message translates to:
  /// **'The bot game history could not be loaded.'**
  String get botGameLogLoadFailed;

  /// No description provided for @botGameHistoryActive.
  ///
  /// In en, this message translates to:
  /// **'In progress'**
  String get botGameHistoryActive;

  /// No description provided for @botGameHistoryWin.
  ///
  /// In en, this message translates to:
  /// **'Won'**
  String get botGameHistoryWin;

  /// No description provided for @botGameHistoryLoss.
  ///
  /// In en, this message translates to:
  /// **'Lost'**
  String get botGameHistoryLoss;

  /// No description provided for @botGameHistoryDraw.
  ///
  /// In en, this message translates to:
  /// **'Draw'**
  String get botGameHistoryDraw;

  /// No description provided for @botStrength.
  ///
  /// In en, this message translates to:
  /// **'Bot strength'**
  String get botStrength;

  /// No description provided for @botElo.
  ///
  /// In en, this message translates to:
  /// **'Elo'**
  String get botElo;

  /// No description provided for @botStartGame.
  ///
  /// In en, this message translates to:
  /// **'Start game'**
  String get botStartGame;

  /// No description provided for @botGameTitle.
  ///
  /// In en, this message translates to:
  /// **'Game against bot'**
  String get botGameTitle;

  /// No description provided for @botGameLoading.
  ///
  /// In en, this message translates to:
  /// **'Preparing game …'**
  String get botGameLoading;

  /// No description provided for @botGameLoadFailed.
  ///
  /// In en, this message translates to:
  /// **'The bot game could not be prepared.'**
  String get botGameLoadFailed;

  /// No description provided for @botMoveFailed.
  ///
  /// In en, this message translates to:
  /// **'The bot move could not be calculated.'**
  String get botMoveFailed;

  /// No description provided for @botYou.
  ///
  /// In en, this message translates to:
  /// **'You'**
  String get botYou;

  /// No description provided for @botYourTurn.
  ///
  /// In en, this message translates to:
  /// **'Your turn.'**
  String get botYourTurn;

  /// No description provided for @botThinking.
  ///
  /// In en, this message translates to:
  /// **'Stockfish is thinking …'**
  String get botThinking;

  /// No description provided for @botApplyingMove.
  ///
  /// In en, this message translates to:
  /// **'Applying move …'**
  String get botApplyingMove;

  /// No description provided for @botWaiting.
  ///
  /// In en, this message translates to:
  /// **'Waiting for the next move …'**
  String get botWaiting;

  /// No description provided for @botViewingHistory.
  ///
  /// In en, this message translates to:
  /// **'You are viewing an earlier position.'**
  String get botViewingHistory;

  /// No description provided for @botGameFinished.
  ///
  /// In en, this message translates to:
  /// **'Game finished.'**
  String get botGameFinished;

  /// No description provided for @botMoveList.
  ///
  /// In en, this message translates to:
  /// **'Move list'**
  String get botMoveList;

  /// No description provided for @botNoMovesYet.
  ///
  /// In en, this message translates to:
  /// **'No moves played yet.'**
  String get botNoMovesYet;

  /// No description provided for @botPreviousMove.
  ///
  /// In en, this message translates to:
  /// **'Previous move'**
  String get botPreviousMove;

  /// No description provided for @botNextMove.
  ///
  /// In en, this message translates to:
  /// **'Next move'**
  String get botNextMove;

  /// No description provided for @botReturnToLive.
  ///
  /// In en, this message translates to:
  /// **'Return to current position'**
  String get botReturnToLive;

  /// No description provided for @botHintPiece.
  ///
  /// In en, this message translates to:
  /// **'Hint 1: Show piece'**
  String get botHintPiece;

  /// No description provided for @botHintTarget.
  ///
  /// In en, this message translates to:
  /// **'Hint 2: Show target square'**
  String get botHintTarget;

  /// No description provided for @botHintsUsed.
  ///
  /// In en, this message translates to:
  /// **'Both hints used'**
  String get botHintsUsed;

  /// No description provided for @botHintThinking.
  ///
  /// In en, this message translates to:
  /// **'Calculating hint …'**
  String get botHintThinking;

  /// No description provided for @botHintFailed.
  ///
  /// In en, this message translates to:
  /// **'The hint could not be calculated.'**
  String get botHintFailed;

  /// No description provided for @botGameSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Game settings'**
  String get botGameSettingsTitle;

  /// No description provided for @botHintCounter.
  ///
  /// In en, this message translates to:
  /// **'{used} / 2 hints for this move'**
  String botHintCounter(int used);

  /// No description provided for @exportFen.
  ///
  /// In en, this message translates to:
  /// **'Export FEN'**
  String get exportFen;

  /// No description provided for @fenCopiedToClipboard.
  ///
  /// In en, this message translates to:
  /// **'FEN copied to clipboard.'**
  String get fenCopiedToClipboard;

  /// No description provided for @engineSelectionTitle.
  ///
  /// In en, this message translates to:
  /// **'Engine version'**
  String get engineSelectionTitle;

  /// No description provided for @engineVersion.
  ///
  /// In en, this message translates to:
  /// **'Chess engine'**
  String get engineVersion;

  /// No description provided for @engineActiveLabel.
  ///
  /// In en, this message translates to:
  /// **'Active: {engine}'**
  String engineActiveLabel(String engine);

  /// No description provided for @stockfish18.
  ///
  /// In en, this message translates to:
  /// **'Stockfish 18'**
  String get stockfish18;

  /// No description provided for @stockfish19.
  ///
  /// In en, this message translates to:
  /// **'Stockfish 19'**
  String get stockfish19;

  /// No description provided for @engineSelectionFailed.
  ///
  /// In en, this message translates to:
  /// **'The selected engine could not be activated. The previous engine remains selected.'**
  String get engineSelectionFailed;

  /// No description provided for @forced.
  ///
  /// In en, this message translates to:
  /// **'Forced'**
  String get forced;

  /// No description provided for @good.
  ///
  /// In en, this message translates to:
  /// **'Good'**
  String get good;

  /// No description provided for @statsTimeControlCorrespondence.
  ///
  /// In en, this message translates to:
  /// **'Correspondence'**
  String get statsTimeControlCorrespondence;

  /// No description provided for @statsTimeControlOther.
  ///
  /// In en, this message translates to:
  /// **'Other'**
  String get statsTimeControlOther;

  /// No description provided for @botGameDeleteQuestion.
  ///
  /// In en, this message translates to:
  /// **'Delete bot game?'**
  String get botGameDeleteQuestion;

  /// No description provided for @botGameDeleteBody.
  ///
  /// In en, this message translates to:
  /// **'This bot-game history entry will be permanently deleted. Any analysis game already created from it remains in your local game library.'**
  String get botGameDeleteBody;

  /// No description provided for @promotionTitle.
  ///
  /// In en, this message translates to:
  /// **'Pawn promotion'**
  String get promotionTitle;

  /// No description provided for @promotionChoosePiece.
  ///
  /// In en, this message translates to:
  /// **'Choose the piece the pawn should promote to.'**
  String get promotionChoosePiece;

  /// No description provided for @promotionQueen.
  ///
  /// In en, this message translates to:
  /// **'Queen'**
  String get promotionQueen;

  /// No description provided for @promotionRook.
  ///
  /// In en, this message translates to:
  /// **'Rook'**
  String get promotionRook;

  /// No description provided for @promotionBishop.
  ///
  /// In en, this message translates to:
  /// **'Bishop'**
  String get promotionBishop;

  /// No description provided for @promotionKnight.
  ///
  /// In en, this message translates to:
  /// **'Knight'**
  String get promotionKnight;

  /// No description provided for @trainingOppositionTitle.
  ///
  /// In en, this message translates to:
  /// **'Pawn opposition'**
  String get trainingOppositionTitle;

  /// No description provided for @trainingOppositionHint.
  ///
  /// In en, this message translates to:
  /// **'Take the opposition first, then outflank the king. Push the pawn only after your king is in front of it.'**
  String get trainingOppositionHint;

  /// No description provided for @trainingLucenaTitle.
  ///
  /// In en, this message translates to:
  /// **'Lucena position'**
  String get trainingLucenaTitle;

  /// No description provided for @trainingLucenaHint.
  ///
  /// In en, this message translates to:
  /// **'Place the rook on the fourth rank before bringing out the king; it will later shield the checks.'**
  String get trainingLucenaHint;

  /// No description provided for @trainingPhilidorTitle.
  ///
  /// In en, this message translates to:
  /// **'Philidor defence'**
  String get trainingPhilidorTitle;

  /// No description provided for @trainingPhilidorHint.
  ///
  /// In en, this message translates to:
  /// **'Keep the rook on the sixth rank until the pawn advances, then check from behind.'**
  String get trainingPhilidorHint;

  /// No description provided for @practiceDrills.
  ///
  /// In en, this message translates to:
  /// **'Checkmate drills'**
  String get practiceDrills;

  /// No description provided for @practiceStudies.
  ///
  /// In en, this message translates to:
  /// **'Classical endgame studies'**
  String get practiceStudies;

  /// No description provided for @practiceFailed.
  ///
  /// In en, this message translates to:
  /// **'Attempt ended. Try again.'**
  String get practiceFailed;

  /// No description provided for @practiceThinking.
  ///
  /// In en, this message translates to:
  /// **'Opponent is thinking…'**
  String get practiceThinking;

  /// No description provided for @practiceMoves.
  ///
  /// In en, this message translates to:
  /// **'Moves played: {count}'**
  String practiceMoves(int count);

  /// No description provided for @practiceLevel.
  ///
  /// In en, this message translates to:
  /// **'Level {number}'**
  String practiceLevel(int number);

  /// No description provided for @practiceStudyNumber.
  ///
  /// In en, this message translates to:
  /// **'Study {number}'**
  String practiceStudyNumber(int number);

  /// No description provided for @practiceStudySource.
  ///
  /// In en, this message translates to:
  /// **'Kling & Horwitz · Chess Studies, Or, Endings of Games (1851). Practise these positions against Stockfish.'**
  String get practiceStudySource;

  /// No description provided for @practiceQueenTitle.
  ///
  /// In en, this message translates to:
  /// **'King and queen vs. king'**
  String get practiceQueenTitle;

  /// No description provided for @practiceRookTitle.
  ///
  /// In en, this message translates to:
  /// **'King and rook vs. king'**
  String get practiceRookTitle;

  /// No description provided for @practiceQueenRookTitle.
  ///
  /// In en, this message translates to:
  /// **'Queen vs. rook'**
  String get practiceQueenRookTitle;

  /// No description provided for @practiceQueenHint.
  ///
  /// In en, this message translates to:
  /// **'Use your king to support the queen. Leave an escape square until you can deliver mate.'**
  String get practiceQueenHint;

  /// No description provided for @practiceRookHint.
  ///
  /// In en, this message translates to:
  /// **'Cut off the king with your rook and bring your king closer.'**
  String get practiceRookHint;

  /// No description provided for @practiceQueenRookHint.
  ///
  /// In en, this message translates to:
  /// **'Look for forks against the king and rook. Watch out for stalemate.'**
  String get practiceQueenRookHint;

  /// No description provided for @practiceBeginner.
  ///
  /// In en, this message translates to:
  /// **'Beginner'**
  String get practiceBeginner;

  /// No description provided for @practiceIntermediate.
  ///
  /// In en, this message translates to:
  /// **'Intermediate'**
  String get practiceIntermediate;

  /// No description provided for @practiceMaster.
  ///
  /// In en, this message translates to:
  /// **'Master'**
  String get practiceMaster;

  /// No description provided for @practiceSectionKingPawn.
  ///
  /// In en, this message translates to:
  /// **'King and pawn'**
  String get practiceSectionKingPawn;

  /// No description provided for @practiceSectionBishops.
  ///
  /// In en, this message translates to:
  /// **'Kings, bishops and pawns'**
  String get practiceSectionBishops;

  /// No description provided for @practiceSectionKnightsBishops.
  ///
  /// In en, this message translates to:
  /// **'Knights, bishops and pawns'**
  String get practiceSectionKnightsBishops;

  /// No description provided for @practiceSectionTwoMinor.
  ///
  /// In en, this message translates to:
  /// **'Two minor pieces vs. one'**
  String get practiceSectionTwoMinor;

  /// No description provided for @practiceSectionRookPawns.
  ///
  /// In en, this message translates to:
  /// **'Rook vs. pawns'**
  String get practiceSectionRookPawns;

  /// No description provided for @practiceSectionRookMinor.
  ///
  /// In en, this message translates to:
  /// **'Rook vs. minor pieces'**
  String get practiceSectionRookMinor;

  /// No description provided for @practiceSectionMinorRook.
  ///
  /// In en, this message translates to:
  /// **'Minor pieces vs. rook'**
  String get practiceSectionMinorRook;

  /// No description provided for @practiceSectionQueenPawns.
  ///
  /// In en, this message translates to:
  /// **'Queen vs. pawns'**
  String get practiceSectionQueenPawns;

  /// No description provided for @practiceSectionQueens.
  ///
  /// In en, this message translates to:
  /// **'Queens and pawns'**
  String get practiceSectionQueens;

  /// No description provided for @practiceSectionQueenRook.
  ///
  /// In en, this message translates to:
  /// **'Queen vs. rook'**
  String get practiceSectionQueenRook;

  /// No description provided for @practiceSectionQueenMinor.
  ///
  /// In en, this message translates to:
  /// **'Queen vs. minor pieces'**
  String get practiceSectionQueenMinor;
}

class _AppLocalizationsDelegate
    extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['ar', 'de', 'en'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'ar':
      return AppLocalizationsAr();
    case 'de':
      return AppLocalizationsDe();
    case 'en':
      return AppLocalizationsEn();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
