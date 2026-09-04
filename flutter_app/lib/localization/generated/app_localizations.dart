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

  /// No description provided for @firstRunBody.
  ///
  /// In en, this message translates to:
  /// **'Choose a source. Public online profiles do not require a password.'**
  String get firstRunBody;

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

  /// No description provided for @playPlaceholder.
  ///
  /// In en, this message translates to:
  /// **'This section is reserved for future play modes, such as games against bots.'**
  String get playPlaceholder;

  /// No description provided for @downloads.
  ///
  /// In en, this message translates to:
  /// **'Downloads'**
  String get downloads;

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

  /// No description provided for @favoriteCollectionRule.
  ///
  /// In en, this message translates to:
  /// **'Collections are one level only and cannot be nested.'**
  String get favoriteCollectionRule;

  /// No description provided for @favoriteMoveToCollection.
  ///
  /// In en, this message translates to:
  /// **'Change collection'**
  String get favoriteMoveToCollection;

  /// No description provided for @favoriteMoveHelp.
  ///
  /// In en, this message translates to:
  /// **'Games can stay as loose favorites or belong to exactly one collection.'**
  String get favoriteMoveHelp;

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

  /// No description provided for @demoNotice.
  ///
  /// In en, this message translates to:
  /// **'Local game'**
  String get demoNotice;

  /// No description provided for @tapToAnalyze.
  ///
  /// In en, this message translates to:
  /// **'Open and analyze'**
  String get tapToAnalyze;

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

  /// No description provided for @boardArrows.
  ///
  /// In en, this message translates to:
  /// **'Show board arrows'**
  String get boardArrows;

  /// No description provided for @boardArrowsHelp.
  ///
  /// In en, this message translates to:
  /// **'Display only; changing this never restarts analysis.'**
  String get boardArrowsHelp;

  /// No description provided for @engine.
  ///
  /// In en, this message translates to:
  /// **'Engine'**
  String get engine;

  /// No description provided for @enginePreset.
  ///
  /// In en, this message translates to:
  /// **'Medium · depth 18 · 3 lines'**
  String get enginePreset;

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

  /// No description provided for @engineSettingsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Depth, lines, time limit, threads and hash'**
  String get engineSettingsSubtitle;

  /// No description provided for @analysisSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get analysisSettingsTitle;

  /// No description provided for @analysisSettingsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Arrows, evaluation and analysis display'**
  String get analysisSettingsSubtitle;

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

  /// No description provided for @bestMoveArrowHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows the engine’s best move on the board.'**
  String get bestMoveArrowHelp;

  /// No description provided for @threatArrow.
  ///
  /// In en, this message translates to:
  /// **'Threat arrow'**
  String get threatArrow;

  /// No description provided for @threatArrowHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows the opponent’s strongest next move as a warning arrow when the opponent is to move.'**
  String get threatArrowHelp;

  /// No description provided for @evaluationBarSetting.
  ///
  /// In en, this message translates to:
  /// **'Evaluation bar'**
  String get evaluationBarSetting;

  /// No description provided for @evaluationBarSettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows the current engine evaluation.'**
  String get evaluationBarSettingHelp;

  /// No description provided for @showEngineLinesSetting.
  ///
  /// In en, this message translates to:
  /// **'Show engine lines'**
  String get showEngineLinesSetting;

  /// No description provided for @showEngineLinesSettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows the calculated principal variations (MultiPV).'**
  String get showEngineLinesSettingHelp;

  /// No description provided for @showClassificationsSetting.
  ///
  /// In en, this message translates to:
  /// **'Show move classifications'**
  String get showClassificationsSetting;

  /// No description provided for @showClassificationsSettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows Theory, Brilliant, Critical, Best and the other move labels.'**
  String get showClassificationsSettingHelp;

  /// No description provided for @showAccuracySetting.
  ///
  /// In en, this message translates to:
  /// **'Show accuracy'**
  String get showAccuracySetting;

  /// No description provided for @showAccuracySettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows the locally calculated accuracy values.'**
  String get showAccuracySettingHelp;

  /// No description provided for @showTheorySetting.
  ///
  /// In en, this message translates to:
  /// **'Show theory information'**
  String get showTheorySetting;

  /// No description provided for @showTheorySettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows opening-book information and theory counts.'**
  String get showTheorySettingHelp;

  /// No description provided for @showResultSymbolsSetting.
  ///
  /// In en, this message translates to:
  /// **'Show result symbols'**
  String get showResultSymbolsSetting;

  /// No description provided for @showResultSymbolsSettingHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows Win, Loss or Draw symbols above the kings when a game has ended.'**
  String get showResultSymbolsSettingHelp;

  /// No description provided for @designSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Design'**
  String get designSettingsTitle;

  /// No description provided for @designSettingsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Appearance, theme, board and pieces'**
  String get designSettingsSubtitle;

  /// No description provided for @generalSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'General'**
  String get generalSettingsTitle;

  /// No description provided for @generalSettingsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Language and app behavior'**
  String get generalSettingsSubtitle;

  /// No description provided for @dataStorageSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Data & storage'**
  String get dataStorageSettingsTitle;

  /// No description provided for @dataStorageSettingsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis cache, downloads and local data'**
  String get dataStorageSettingsSubtitle;

  /// No description provided for @dataStoragePlaceholder.
  ///
  /// In en, this message translates to:
  /// **'Storage and cache controls will be added in a following step.'**
  String get dataStoragePlaceholder;

  /// No description provided for @licensesAbout.
  ///
  /// In en, this message translates to:
  /// **'Licenses & about'**
  String get licensesAbout;

  /// No description provided for @stockfishPending.
  ///
  /// In en, this message translates to:
  /// **'Stockfish 18 · local · GPLv3'**
  String get stockfishPending;

  /// No description provided for @provider.
  ///
  /// In en, this message translates to:
  /// **'Provider'**
  String get provider;

  /// No description provided for @localProfile.
  ///
  /// In en, this message translates to:
  /// **'Local profile'**
  String get localProfile;

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

  /// No description provided for @player.
  ///
  /// In en, this message translates to:
  /// **'Player'**
  String get player;

  /// No description provided for @bothPlayers.
  ///
  /// In en, this message translates to:
  /// **'Both'**
  String get bothPlayers;

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

  /// No description provided for @expectedLoss.
  ///
  /// In en, this message translates to:
  /// **'Expected-score loss'**
  String get expectedLoss;

  /// No description provided for @versions.
  ///
  /// In en, this message translates to:
  /// **'Versions'**
  String get versions;

  /// No description provided for @classifierVersionLabel.
  ///
  /// In en, this message translates to:
  /// **'Classifier'**
  String get classifierVersionLabel;

  /// No description provided for @accuracyVersionLabel.
  ///
  /// In en, this message translates to:
  /// **'Accuracy'**
  String get accuracyVersionLabel;

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

  /// No description provided for @moveComparisonText.
  ///
  /// In en, this message translates to:
  /// **'{played} was {classification}. {best} is the best move.'**
  String moveComparisonText(String played, String classification, String best);

  /// No description provided for @theoryMoveText.
  ///
  /// In en, this message translates to:
  /// **'{move} is a theory move.'**
  String theoryMoveText(String move);

  /// No description provided for @triedMove.
  ///
  /// In en, this message translates to:
  /// **'You tried {move}.'**
  String triedMove(String move);

  /// No description provided for @sidelineEngineTitle.
  ///
  /// In en, this message translates to:
  /// **'Sideline engine'**
  String get sidelineEngineTitle;

  /// No description provided for @sidelineEngineSubtitle.
  ///
  /// In en, this message translates to:
  /// **'These values apply only to live analysis of your sideline.'**
  String get sidelineEngineSubtitle;

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

  /// No description provided for @sidelineAnalysisPaused.
  ///
  /// In en, this message translates to:
  /// **'Live analysis paused'**
  String get sidelineAnalysisPaused;

  /// No description provided for @analyzingVariation.
  ///
  /// In en, this message translates to:
  /// **'Analyzing the temporary variation…'**
  String get analyzingVariation;

  /// No description provided for @evaluationComparison.
  ///
  /// In en, this message translates to:
  /// **'Evaluation: {before} → {after}'**
  String evaluationComparison(String before, String after);

  /// No description provided for @bestContinuation.
  ///
  /// In en, this message translates to:
  /// **'Best continuation: {line}'**
  String bestContinuation(String line);

  /// No description provided for @returnToMainLine.
  ///
  /// In en, this message translates to:
  /// **'Return to main line'**
  String get returnToMainLine;

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

  /// No description provided for @variationStartingPosition.
  ///
  /// In en, this message translates to:
  /// **'Variation starting position'**
  String get variationStartingPosition;

  /// No description provided for @variationStart.
  ///
  /// In en, this message translates to:
  /// **'Start of variation'**
  String get variationStart;

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

  /// No description provided for @depthHelp.
  ///
  /// In en, this message translates to:
  /// **'Min = pre-analysis depth. Max = maximum live-analysis depth. Higher values usually take longer.'**
  String get depthHelp;

  /// No description provided for @adaptiveEarlyStop.
  ///
  /// In en, this message translates to:
  /// **'Adaptive analysis'**
  String get adaptiveEarlyStop;

  /// No description provided for @adaptiveEarlyStopHelp.
  ///
  /// In en, this message translates to:
  /// **'Ends quiet pre-analysis and live searches early when the evaluation and principal variations are stable. Critical verification searches still use the configured limit.'**
  String get adaptiveEarlyStopHelp;

  /// No description provided for @numberOfLinesHelp.
  ///
  /// In en, this message translates to:
  /// **'How many top engine variations Stockfish calculates at the same time.'**
  String get numberOfLinesHelp;

  /// No description provided for @timeLimitHelp.
  ///
  /// In en, this message translates to:
  /// **'Optional limit per position. Off uses depth only; otherwise the search stops when depth or time is reached first.'**
  String get timeLimitHelp;

  /// No description provided for @threads.
  ///
  /// In en, this message translates to:
  /// **'Threads'**
  String get threads;

  /// No description provided for @threadsHelp.
  ///
  /// In en, this message translates to:
  /// **'CPU threads per Stockfish worker. Kchess detects your PC automatically and allows at most half of the logical CPU threads.'**
  String get threadsHelp;

  /// No description provided for @hashMemory.
  ///
  /// In en, this message translates to:
  /// **'Hash memory'**
  String get hashMemory;

  /// No description provided for @hashMemoryHelp.
  ///
  /// In en, this message translates to:
  /// **'RAM for Stockfish’s transposition table. More memory can improve repeated-position search.'**
  String get hashMemoryHelp;

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

  /// No description provided for @showBoardCoordinatesHelp.
  ///
  /// In en, this message translates to:
  /// **'Shows file and rank labels (a–h / 1–8) on the board.'**
  String get showBoardCoordinatesHelp;

  /// No description provided for @highlightLastMove.
  ///
  /// In en, this message translates to:
  /// **'Highlight last move'**
  String get highlightLastMove;

  /// No description provided for @highlightLastMoveHelp.
  ///
  /// In en, this message translates to:
  /// **'Highlights the origin and destination squares of the last played move.'**
  String get highlightLastMoveHelp;

  /// No description provided for @highlightSelectedSquare.
  ///
  /// In en, this message translates to:
  /// **'Highlight selected square'**
  String get highlightSelectedSquare;

  /// No description provided for @highlightSelectedSquareHelp.
  ///
  /// In en, this message translates to:
  /// **'Highlights the square you selected while exploring a variation.'**
  String get highlightSelectedSquareHelp;

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

  /// No description provided for @autoSyncOnlineHelp.
  ///
  /// In en, this message translates to:
  /// **'Synchronizes Chess.com and Lichess automatically at startup and when switching profiles.'**
  String get autoSyncOnlineHelp;

  /// No description provided for @confirmBeforeDelete.
  ///
  /// In en, this message translates to:
  /// **'Confirm before deleting'**
  String get confirmBeforeDelete;

  /// No description provided for @confirmBeforeDeleteHelp.
  ///
  /// In en, this message translates to:
  /// **'Asks for confirmation before deleting profiles or local games.'**
  String get confirmBeforeDeleteHelp;

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

  /// No description provided for @useGlobalAnalysisCacheHelp.
  ///
  /// In en, this message translates to:
  /// **'Reuses compatible analysis of identical positions across different games.'**
  String get useGlobalAnalysisCacheHelp;

  /// No description provided for @clearAnalysisCache.
  ///
  /// In en, this message translates to:
  /// **'Clear analysis cache'**
  String get clearAnalysisCache;

  /// No description provided for @clearAnalysisCacheHelp.
  ///
  /// In en, this message translates to:
  /// **'Clears only the shared position cache. Saved games and completed game analyses are kept.'**
  String get clearAnalysisCacheHelp;

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

  /// No description provided for @diagnosticLoggingHelp.
  ///
  /// In en, this message translates to:
  /// **'Writes bounded technical logs for troubleshooting. Full PGNs, FENs, and provider responses are not logged.'**
  String get diagnosticLoggingHelp;

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
