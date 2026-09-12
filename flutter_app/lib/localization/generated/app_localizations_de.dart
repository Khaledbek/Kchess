// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for German (`de`).
class AppLocalizationsDe extends AppLocalizations {
  AppLocalizationsDe([String locale = 'de']) : super(locale);

  @override
  String get coachShowOnBoard => 'Auf dem Brett zeigen';

  @override
  String get coachCompareMoves => 'Züge vergleichen';

  @override
  String get coachChallenge => 'Stell mir eine Aufgabe';

  @override
  String get coachCompareRequest =>
      'Vergleiche die vorliegenden Engine-Kandidaten in dieser Schachstellung. Erkläre für jeden die Idee und die gegnerische Antwort nur anhand geprüfter Belege. Wenn nur ein Kandidat vorliegt, erkläre ihn und sage, dass ein zweiter bewerteter Kandidat fehlt.';

  @override
  String get coachChallengeRequest =>
      'Trainiere mit mir anhand einer gezielten Frage zu dieser Schachstellung. Lass mich die Idee auf dem Brett finden, bevor du den Zug verrätst. Warte auf meinen Versuch und gib mir dann hilfreiches Feedback.';

  @override
  String get appTitle => 'KChess';

  @override
  String get firstRunTitle => 'Dein lokaler Schachbereich';

  @override
  String get chessCom => 'Chess.com';

  @override
  String get lichess => 'Lichess';

  @override
  String get localPgnFen => 'PGN / FEN';

  @override
  String get username => 'Benutzername';

  @override
  String get profileName => 'Profilname';

  @override
  String get continueLabel => 'Weiter';

  @override
  String get requiredField => 'Bitte einen Wert eingeben.';

  @override
  String get games => 'Partien';

  @override
  String get gameSection => 'Partie';

  @override
  String get play => 'Spielen';

  @override
  String get favorites => 'Favoriten';

  @override
  String get favoriteCollectionsTitle => 'Sammlungen';

  @override
  String get favoriteNoCollections =>
      'Noch keine Sammlungen. Erstelle eine Sammlung, um Favoriten zu gruppieren.';

  @override
  String get favoriteLooseTitle => 'Lose Favoriten';

  @override
  String get favoriteCreateCollection => 'Sammlung erstellen';

  @override
  String get favoriteRenameCollection => 'Sammlung umbenennen';

  @override
  String get favoriteDeleteCollection => 'Sammlung löschen';

  @override
  String get favoriteCollectionName => 'Name der Sammlung';

  @override
  String get favoriteDeleteCollectionBody =>
      'Die Sammlung wird gelöscht. Ihre Partien bleiben als lose Favoriten erhalten.';

  @override
  String get favoriteEmptyCollection =>
      'Diese Sammlung enthält noch keine Partien.';

  @override
  String get favoriteNoLooseGames => 'Keine losen Favoriten.';

  @override
  String get favoriteMoveToCollection => 'Sammlung ändern';

  @override
  String get profile => 'Profil';

  @override
  String get settings => 'Einstellungen';

  @override
  String get analysis => 'Analyse';

  @override
  String get addAccount => 'Konto anlegen';

  @override
  String get switchAccount => 'Konto wechseln';

  @override
  String get importData => 'PGN / FEN importieren';

  @override
  String get importPgnFile => 'PGN-Datei auswählen';

  @override
  String get pastePgn => 'PGN-Text einfügen';

  @override
  String get importFen => 'FEN-Stellung importieren';

  @override
  String get pgnText => 'PGN-Text';

  @override
  String get pgnLabel => 'Partie-PGN';

  @override
  String get fenText => 'Vollständige FEN';

  @override
  String get positionName => 'Name der Stellung';

  @override
  String get importAction => 'Importieren';

  @override
  String get noGames =>
      'Noch keine lokalen Partien oder Stellungen. Importiere eine PGN-Datei, PGN-Text oder eine FEN-Stellung.';

  @override
  String get emptySection => 'Dieser Bereich ist für lokale Daten vorbereitet.';

  @override
  String get loading => 'Lokaler Kern wird geladen…';

  @override
  String get coreUnavailable =>
      'Der lokale Kern konnte nicht gestartet werden.';

  @override
  String get retry => 'Erneut versuchen';

  @override
  String get summary => 'Zusammenfassung';

  @override
  String get analyzing => 'Jeder Halbzug wird analysiert…';

  @override
  String get analysisComplete => 'Vollanalyse abgeschlossen';

  @override
  String get analysisCancelled =>
      'Analyse abgebrochen – vorhandene Ergebnisse bleiben gespeichert.';

  @override
  String get cancelAnalysis => 'Analyse abbrechen';

  @override
  String get deleteAnalysis => 'Gespeicherte Analyse löschen';

  @override
  String get deleteAnalysisQuestion => 'Gespeicherte Analyse löschen?';

  @override
  String get deleteAnalysisBody =>
      'Die lokal gespeicherte Analyse und Accuracy dieser Partie werden gelöscht. PGN/FEN und globaler Engine-Cache bleiben erhalten.';

  @override
  String get analysisDeleted => 'Gespeicherte Analyse wurde gelöscht.';

  @override
  String get classificationPending =>
      'Die Klassifizierung für diesen Zug ist noch nicht verfügbar.';

  @override
  String get bestMove => 'Bester Zug';

  @override
  String get evaluation => 'Bewertung';

  @override
  String get engineLines => 'Engine-Linien';

  @override
  String get currentMove => 'Aktueller Zug';

  @override
  String get engine => 'Engine';

  @override
  String get depth => 'Tiefe';

  @override
  String get numberOfLines => 'Anzahl der Linien';

  @override
  String get timeLimitSeconds => 'Zeitlimit (Sekunden)';

  @override
  String get noTimeLimit => 'Aus';

  @override
  String get secondsShort => 's';

  @override
  String get deleteAccount => 'Konto löschen';

  @override
  String get deleteAccountQuestion => 'Konto löschen?';

  @override
  String get deleteOnlineProfileBody =>
      'Dieses Profil und seine lokal gespeicherten Daten werden von KChess entfernt. Das Chess.com-/Lichess-Konto selbst wird nicht verändert.';

  @override
  String get deleteLocalProfileBody =>
      'Dieses Profil und seine lokal gespeicherten PGN-/FEN-Daten werden von KChess entfernt.';

  @override
  String get cancelAction => 'Abbrechen';

  @override
  String get deleteAction => 'Löschen';

  @override
  String get language => 'Sprache';

  @override
  String get theme => 'Design';

  @override
  String get systemTheme => 'System';

  @override
  String get lightTheme => 'Hell';

  @override
  String get darkTheme => 'Dunkel';

  @override
  String get analysisSettingsTitle => 'Analyse';

  @override
  String get analysisBoardGuidance => 'Brett-Hinweise';

  @override
  String get analysisInformation => 'Analyseinformationen';

  @override
  String get bestMoveArrow => 'Bestzug-Pfeil';

  @override
  String get threatArrow => 'Bedrohungs-Pfeil';

  @override
  String get evaluationBarSetting => 'Bewertungsleiste';

  @override
  String get showEngineLinesSetting => 'Engine-Linien anzeigen';

  @override
  String get showClassificationsSetting => 'Zugklassifikationen anzeigen';

  @override
  String get showAccuracySetting => 'Accuracy anzeigen';

  @override
  String get showTheorySetting => 'Theorieinformationen anzeigen';

  @override
  String get showResultSymbolsSetting => 'Ergebnissymbole anzeigen';

  @override
  String get designSettingsTitle => 'Design';

  @override
  String get generalSettingsTitle => 'Allgemein';

  @override
  String get dataStorageSettingsTitle => 'Daten & Speicher';

  @override
  String get licensesAbout => 'Lizenzen & Über';

  @override
  String get stockfishPending => 'Stockfish · lokal · GPLv3';

  @override
  String get provider => 'Anbieter';

  @override
  String get theory => 'Theorie';

  @override
  String get brilliant => 'Brillant';

  @override
  String get critical => 'Großartiger Zug';

  @override
  String get best => 'Bester';

  @override
  String get excellent => 'Ausgezeichnet';

  @override
  String get okay => 'Okay';

  @override
  String get miss => 'Verpasst';

  @override
  String get mistake => 'Fehler';

  @override
  String get blunder => 'Patzer';

  @override
  String get totalMoves => 'Halbzüge';

  @override
  String get localAccuracy => 'Lokale Genauigkeit';

  @override
  String get close => 'Schließen';

  @override
  String get previous => 'Zurück';

  @override
  String get next => 'Weiter';

  @override
  String get first => 'Erster Zug';

  @override
  String get last => 'Letzter Zug';

  @override
  String get playPause => 'Abspielen oder pausieren';

  @override
  String get whitePlayer => 'Weiß';

  @override
  String get blackPlayer => 'Schwarz';

  @override
  String get analyzedMoves => 'Klassifizierte Züge';

  @override
  String get bookGames => 'Book-Partien';

  @override
  String get analyzingGame => 'Partie wird analysiert';

  @override
  String analyzedMovesProgress(int completed, int total) {
    return '$completed / $total Halbzüge analysiert';
  }

  @override
  String get openAnalysis => 'Analyse öffnen';

  @override
  String bestMoveText(String move) {
    return '$move ist der beste Zug.';
  }

  @override
  String get sidelineEngineTitle => 'Side-Line-Engine';

  @override
  String get mainLineLabel => 'Hauptlinie';

  @override
  String get sidelineLabel => 'Eigene Variante';

  @override
  String get liveEngineTheorySkipped => 'Theorie: Live-Analyse übersprungen';

  @override
  String get liveEngineTargetReached => 'Stockfish: Analyseziel erreicht';

  @override
  String liveEngineProgress(int percent) {
    return 'Stockfish analysiert live · $percent%';
  }

  @override
  String illegalOrFailedMove(String message) {
    return 'Der Zug ist nicht legal oder konnte nicht analysiert werden: $message';
  }

  @override
  String get myPlayer => 'Mein Spieler';

  @override
  String get opponent => 'Gegner';

  @override
  String get engineQualityTitle => 'Analysequalität';

  @override
  String get engineResourcesTitle => 'Ressourcen';

  @override
  String get adaptiveEarlyStop => 'Adaptive Analyse';

  @override
  String get threads => 'Threads';

  @override
  String get hashMemory => 'Hash-Speicher';

  @override
  String get boardDisplayTitle => 'Brettdarstellung';

  @override
  String get rotateBoard => 'Brett drehen';

  @override
  String get showBoardCoordinates => 'Brettkoordinaten';

  @override
  String get highlightLastMove => 'Letzten Zug hervorheben';

  @override
  String get highlightSelectedSquare => 'Ausgewähltes Feld hervorheben';

  @override
  String get behaviorTitle => 'Verhalten';

  @override
  String get autoSyncOnline => 'Online-Profile automatisch aktualisieren';

  @override
  String get confirmBeforeDelete => 'Vor dem Löschen bestätigen';

  @override
  String get analysisCacheTitle => 'Analyse-Cache';

  @override
  String get useGlobalAnalysisCache => 'Globalen Stellungs-Cache verwenden';

  @override
  String get clearAnalysisCache => 'Analyse-Cache leeren';

  @override
  String get clearAnalysisCacheQuestion => 'Analyse-Cache leeren?';

  @override
  String get clearAnalysisCacheBody =>
      'Der globale Stellungs-Cache wird gelöscht. Deine Partien, Favoriten, Downloads und vollständigen Partieanalysen bleiben erhalten.';

  @override
  String get analysisCacheCleared => 'Analyse-Cache wurde geleert.';

  @override
  String get diagnosticsTitle => 'Diagnose';

  @override
  String get diagnosticLogging => 'Diagnoseprotokoll';

  @override
  String get deleteLocalGameQuestion => 'Lokalen Eintrag löschen?';

  @override
  String get deleteLocalGameBody =>
      'Die gespeicherte PGN/FEN und ihre lokale Analyse werden dauerhaft entfernt.';

  @override
  String get profileRatings => 'Ratings';

  @override
  String get profileGameOverview => 'Partieübersicht';

  @override
  String get profileWins => 'Siege';

  @override
  String get profileDraws => 'Remis';

  @override
  String get profileLosses => 'Niederlagen';

  @override
  String get ratingRapid => 'Rapid';

  @override
  String get ratingBlitz => 'Blitz';

  @override
  String get ratingBullet => 'Bullet';

  @override
  String get ratingDaily => 'Daily';

  @override
  String get ratingClassical => 'Klassisch';

  @override
  String get ratingChess960 => 'Chess960';

  @override
  String get ratingFide => 'FIDE';

  @override
  String get statsWins => 'Siege';

  @override
  String get statsDraws => 'Remis';

  @override
  String get statsLosses => 'Niederlagen';

  @override
  String get statsAll => 'Alle';

  @override
  String get statsPhaseTitle => 'Nach Spielphase';

  @override
  String get statsPhaseOpening => 'Eröffnung (1–12)';

  @override
  String get statsPhaseMiddlegame => 'Mittelspiel (13–30)';

  @override
  String get statsPhaseEndgame => 'Endspiel (31+)';

  @override
  String get statsPhaseOpeningShort => 'Eröffnung';

  @override
  String get statsPhaseMiddlegameShort => 'Mittelspiel';

  @override
  String get statsPhaseEndgameShort => 'Endspiel';

  @override
  String get statsPhaseGames => 'Partien';

  @override
  String get statsPhaseWinWord => 'Sieg';

  @override
  String get statsPhaseEmpty => 'Nicht genügend Daten zu Spielphasen.';

  @override
  String get statsPhaseNoProfile =>
      'Erstelle oder wähle ein Profil, um Statistiken zu sehen.';

  @override
  String get statsPhaseError => 'Spielphasen konnten nicht geladen werden.';

  @override
  String get statsPhaseRetry => 'Erneut versuchen';

  @override
  String statsPhaseClassifiedNote(int classified, int total) {
    return '$classified von $total Partien';
  }

  @override
  String get statsTitle => 'Statistiken';

  @override
  String get statsIntroTitle => 'Deine Schachleistung';

  @override
  String get statsFormTitle => 'Aktuelle Form';

  @override
  String get statsFormVersus => 'gegen';

  @override
  String get statsFormEmpty => 'Keine aktuellen Partien vorhanden.';

  @override
  String get statsFormError => 'Aktuelle Partien konnten nicht geladen werden.';

  @override
  String get statsFormRetry => 'Erneut versuchen';

  @override
  String get statsOverviewTitle => 'Übersicht';

  @override
  String get statsOverviewGames => 'Partien';

  @override
  String get statsOverviewWinRate => 'Siegquote';

  @override
  String get statsOverviewScore => 'Score';

  @override
  String get statsOverviewRecord => 'Bilanz';

  @override
  String get statsOverviewByColor => 'Nach Farbe';

  @override
  String get statsOverviewByTimeControl => 'Nach Zeitkontrolle';

  @override
  String get statsOverviewWhite => 'Weiß';

  @override
  String get statsOverviewBlack => 'Schwarz';

  @override
  String get statsOverviewEmpty =>
      'Noch keine Partien. Synchronisiere ein Online-Profil oder importiere Partien, um deine Statistik zu sehen.';

  @override
  String get statsOverviewNoProfile =>
      'Erstelle oder wähle ein Profil, um Statistiken zu sehen.';

  @override
  String get statsOverviewNoGamesForFilter =>
      'Keine Partien für die gewählte Zeitkontrolle.';

  @override
  String get statsOverviewError => 'Statistik konnte nicht geladen werden.';

  @override
  String get statsOverviewRetry => 'Erneut versuchen';

  @override
  String get statsTerminationTitle => 'Partie-Ende Statistik';

  @override
  String get statsTerminationCheckmate => 'Matt';

  @override
  String get statsTerminationResignation => 'Aufgabe';

  @override
  String get statsTerminationTimeout => 'Zeit';

  @override
  String get statsTerminationDraw => 'Remis';

  @override
  String get statsTerminationOther => 'Andere';

  @override
  String get statsTerminationWonByCheckmate => 'Gewonnen durch Matt';

  @override
  String get statsTerminationLostByCheckmate => 'Verloren durch Matt';

  @override
  String get statsTerminationOpponentResigned => 'Gegner gab auf';

  @override
  String get statsTerminationSelfResigned => 'Selbst aufgegeben';

  @override
  String get statsTerminationOpponentFlagged => 'Gegner-Zeit abgelaufen';

  @override
  String get statsTerminationSelfFlagged => 'Eigene Zeit abgelaufen';

  @override
  String get statsTerminationWonGeneric => 'Gewonnen';

  @override
  String get statsTerminationLostGeneric => 'Verloren';

  @override
  String get statsTerminationEmpty => 'Nicht genügend Daten zum Partie-Ende.';

  @override
  String get statsTerminationNoProfile =>
      'Erstelle oder wähle ein Profil, um Statistiken zu sehen.';

  @override
  String get statsTerminationError =>
      'Partie-Enden konnten nicht geladen werden.';

  @override
  String get statsTerminationRetry => 'Erneut versuchen';

  @override
  String get statsRatingTitle => 'Rating-Verlauf';

  @override
  String get statsRatingEmpty =>
      'Nicht genügend Rating-Daten für einen Verlauf.';

  @override
  String get statsRatingError => 'Rating-Daten konnten nicht geladen werden.';

  @override
  String get statsRatingRetry => 'Erneut versuchen';

  @override
  String get statsOpeningGamesAll => 'Alle';

  @override
  String get statsOpeningGamesWon => 'Gewonnen';

  @override
  String get statsOpeningGamesLost => 'Verloren';

  @override
  String get statsOpeningGamesByCheckmate => 'durch Matt';

  @override
  String get statsOpeningGamesByResignation => 'durch Aufgabe';

  @override
  String get statsOpeningGamesByTimeout => 'durch Zeit';

  @override
  String get statsOpeningGamesByDraw => 'Remis';

  @override
  String get statsOpeningGamesEmpty => 'Keine Partien für diese Auswahl.';

  @override
  String get statsOpeningGamesError => 'Partien konnten nicht geladen werden.';

  @override
  String get statsOpeningsTitle => 'Erfolgreichste Eröffnungen';

  @override
  String get statsOpeningsMostPlayed => 'Meistgespielt';

  @override
  String get statsOpeningsBestWinRate => 'Beste Siegquote';

  @override
  String get statsOpeningsMinGamesHint => 'Mindestens 3 Partien pro Eröffnung.';

  @override
  String get statsOpeningsClassifiedGames => 'Partien mit benannter Eröffnung';

  @override
  String get statsOpeningsGames => 'Partien';

  @override
  String get statsOpeningsVariations => 'Varianten';

  @override
  String get statsOpeningsBaseLine => 'Grundform';

  @override
  String get statsOpeningsWhite => 'Weiß';

  @override
  String get statsOpeningsBlack => 'Schwarz';

  @override
  String get statsOpeningsUnknownColor => 'Andere';

  @override
  String get statsOpeningsWinRateShort => 'Sieg';

  @override
  String get statsOpeningsNoOpeningsForColor =>
      'Noch keine Eröffnungen für diese Farbe.';

  @override
  String get statsOpeningsNoOpeningsForWinRate =>
      'Keine Eröffnung mit mindestens 3 Partien.';

  @override
  String get statsOpeningsEmpty =>
      'Noch keine benannten Eröffnungen. Synchronisierte und importierte Partien werden automatisch klassifiziert.';

  @override
  String get statsOpeningsNoProfile =>
      'Erstelle oder wähle ein Profil, um Eröffnungen zu sehen.';

  @override
  String get statsOpeningsError => 'Eröffnungen konnten nicht geladen werden.';

  @override
  String get statsOpeningsRetry => 'Erneut versuchen';

  @override
  String get statsCompareTitle => 'Spielervergleich';

  @override
  String get statsCompareUsernameLabel => 'Chess.com-Benutzername';

  @override
  String get statsCompareUsernameHint => 'z. B. hikaru';

  @override
  String get statsCompareCompare => 'Vergleichen';

  @override
  String get statsCompareLoadingHint =>
      'Partien des Gegners werden geladen und ausgewertet…';

  @override
  String get statsComparePrompt =>
      'Gib einen Chess.com-Benutzernamen ein, um Statistiken zu vergleichen.';

  @override
  String get statsCompareYou => 'Du';

  @override
  String get statsCompareOpponent => 'Gegner';

  @override
  String get statsCompareH2hTitle => 'Direktvergleich';

  @override
  String get statsCompareDirectGames => 'direkte Partien';

  @override
  String get statsCompareWins => 'Siege';

  @override
  String get statsCompareDraws => 'Remis';

  @override
  String get statsCompareLosses => 'Niederlagen';

  @override
  String get statsComparePerformanceCompare => 'Leistungsvergleich';

  @override
  String get statsCompareWinRateWhite => 'Siegquote mit Weiß';

  @override
  String get statsCompareWinRateBlack => 'Siegquote mit Schwarz';

  @override
  String get statsCompareFlagging => 'Niederlagen auf Zeit';

  @override
  String get statsCompareOpeningMatchup => 'Eröffnungs-Duelle';

  @override
  String get statsCompareMatchupSubtitle =>
      'Deine Eröffnungen gegen die Siegquote des Gegners mit der Gegenfarbe.';

  @override
  String get statsCompareOpeningColumn => 'Eröffnung';

  @override
  String get statsCompareGamesShort => 'Partien';

  @override
  String get statsCompareNoMatchups =>
      'Keine gemeinsamen Eröffnungen gefunden.';

  @override
  String get statsCompareNoLeaks => 'Keine klaren Schwächen gefunden.';

  @override
  String get statsCompareStrategyTitle => 'Empfohlene Strategie';

  @override
  String get statsCompareColorWhite => 'Weiß';

  @override
  String get statsCompareColorBlack => 'Schwarz';

  @override
  String get statsCompareErrorPrefix => 'Fehler';

  @override
  String statsCompareGamesAnalyzed(int games, int months) {
    return '$games Partien aus $months Monaten ausgewertet';
  }

  @override
  String statsTerminationSpotlight(String label, int share, int lossPercent) {
    return 'Häufigstes Partie-Ende: $label — $share% aller Partien, davon $lossPercent% Niederlagen.';
  }

  @override
  String get statsCompareSelfBadge => 'Selbstvergleich (Spiegelung)';

  @override
  String get statsCompareSelfH2H =>
      'Keine Partien gegen dich selbst · Dies ist eine Selbstanalyse deines eigenen Profils.';

  @override
  String statsCompareScopeNote(int games) {
    return 'Vergleich basiert auf den letzten $games geladenen Partien.';
  }

  @override
  String get statsCompareMinSampleNote =>
      'Nur Eröffnungen mit mindestens 5 Partien auf beiden Seiten.';

  @override
  String get statsCompareOwnWeaknessTitle => 'Eigene Schwachstellen';

  @override
  String statsCompareOwnWeakness(
    String color,
    String opening,
    String rate,
    int games,
  ) {
    return 'Schwachstelle mit $color: $opening — nur $rate Siegquote (aus $games Partien).';
  }

  @override
  String get statsCompareNoOwnWeakness =>
      'Keine klaren eigenen Schwächen bei mindestens 5 Partien pro Eröffnung.';

  @override
  String statsCompareOpenWhite(String opening, String rate, int games) {
    return 'Eröffne mit $opening — dein Gegner erzielt als Schwarz dagegen nur $rate (aus $games Partien).';
  }

  @override
  String statsCompareAnswerBlack(String opening, String rate, int games) {
    return 'Als Schwarz: Wähle $opening — dein Gegner erzielt als Weiß dagegen nur $rate (aus $games Partien).';
  }

  @override
  String statsCompareSampleScope(int months, int games) {
    return 'Du: deine gesamte lokale Bibliothek · Gegner: die letzten $months Monate ($games Partien). Beide Seiten sind unterschiedliche Stichproben — die Werte können sich daher auch beim Selbstvergleich unterscheiden.';
  }

  @override
  String get trainingSection => 'Training';

  @override
  String get trainingIntroTitle => 'Training Arena';

  @override
  String get trainingOpeningTitle => 'Eröffnungs-Labor';

  @override
  String get trainingOpeningAction => 'Linien trainieren';

  @override
  String trainingNemesisBadge(String opening, String rate) {
    return 'Angstgegner: $opening · nur $rate Siegquote';
  }

  @override
  String get trainingNemesisNone =>
      'Noch keine Schwachstelle in deiner Statistik erkannt.';

  @override
  String get trainingTacticsTitle => 'Blunder-Buster';

  @override
  String trainingTacticsSolved(int count) {
    return '$count gelöste Taktiken';
  }

  @override
  String get trainingTacticsAction => 'Taktik starten';

  @override
  String get trainingTacticsEmpty => 'Noch keine Taktik-Aufgaben im Katalog.';

  @override
  String get trainingEndgameTitle => 'Endspiel-Akademie';

  @override
  String trainingEndgameProgress(int mastered, int total, int percent) {
    return '$mastered / $total Stellungen gemeistert ($percent%)';
  }

  @override
  String get trainingEndgameAction => 'Endspiele öffnen';

  @override
  String get trainingMastered => 'Gemeistert';

  @override
  String trainingStreak(int done, int total) {
    return '$done/$total fehlerfreie Wiederholungen';
  }

  @override
  String get trainingOpeningLabSelected => 'Ausgewählte Linie';

  @override
  String get statsTrainOpening => 'Trainieren';

  @override
  String trainingGoalWin(String side) {
    return '$side am Zug — Gewinne die Stellung';
  }

  @override
  String trainingGoalDraw(String side) {
    return '$side am Zug — Halte die Stellung remis';
  }

  @override
  String get trainingWrongMove =>
      'Nicht der beste Zug. Probiere es noch einmal.';

  @override
  String get trainingSolvedTitle => 'Ausgezeichnet! Stellung gelöst.';

  @override
  String get trainingSolvedClean =>
      'Fehlerfrei gelöst — das zählt für die Meisterschaft.';

  @override
  String get trainingSolvedWithErrors =>
      'Gelöst, aber mit Korrekturen. Für „gemeistert“ zählt nur ein fehlerfreier Durchgang.';

  @override
  String get trainingPracticeAgain => 'Nochmal üben';

  @override
  String get trainingNextEndgame => 'Nächstes Endspiel';

  @override
  String get trainingBackToList => 'Zur Übersicht';

  @override
  String trainingMoveProgress(int done, int total) {
    return 'Zug $done von $total';
  }

  @override
  String get trainingBoardError => 'Die Stellung konnte nicht geladen werden.';

  @override
  String get trainingShowHint => 'Tipp anzeigen';

  @override
  String get trainingYourMove => 'Du bist am Zug';

  @override
  String get trainingRestart => 'Neu starten';

  @override
  String get trainingOpeningLabStart => 'Linie üben';

  @override
  String get trainingOpeningLabBackToOverview => 'Zurück zur Übersicht';

  @override
  String get trainingOpeningTreeEmpty =>
      'Noch keine Eröffnungslinien verfügbar.';

  @override
  String get trainingOpeningTreeExpand => 'Varianten zeigen';

  @override
  String get trainingOpeningTreeCollapse => 'Varianten ausblenden';

  @override
  String trainingOpeningTreeVariations(int count) {
    String _temp0 = intl.Intl.pluralLogic(
      count,
      locale: localeName,
      other: '$count Varianten',
      one: '1 Variante',
    );
    return '$_temp0';
  }

  @override
  String get trainingOpeningTreeLoadFailed =>
      'Diese Linie ist noch nicht in der Datenbank.';

  @override
  String get trainingOpeningTreeZoomIn => 'Vergrößern';

  @override
  String get trainingOpeningTreeZoomOut => 'Verkleinern';

  @override
  String get trainingOpeningTreeFit => 'Baum einpassen';

  @override
  String get trainingOpeningTreeHint =>
      'Tippe auf eine Karte, um ihre Varianten aufzuklappen — oder trainiere die Linie direkt.';

  @override
  String get playAgainstBot => 'Gegen einen Bot spielen';

  @override
  String get continueAgainstBot => 'Gegen Bot weiterspielen';

  @override
  String get botPlayerColor => 'Deine Farbe';

  @override
  String get temporaryBotGameNotSaved =>
      'Diese Partie wird nicht gespeichert und beim Schließen oder Abbrechen vollständig verworfen.';

  @override
  String get botGameLog => 'Bot-Spielverlauf';

  @override
  String get botGameLogEmpty => 'Noch keine gespeicherten Bot-Partien.';

  @override
  String get botGameLogLoadFailed =>
      'Der Bot-Spielverlauf konnte nicht geladen werden.';

  @override
  String get botGameHistoryActive => 'Laufend';

  @override
  String get botGameHistoryWin => 'Gewonnen';

  @override
  String get botGameHistoryLoss => 'Verloren';

  @override
  String get botGameHistoryDraw => 'Remis';

  @override
  String get botStrength => 'Bot-Spielstärke';

  @override
  String get botElo => 'Elo';

  @override
  String get botStartGame => 'Partie starten';

  @override
  String get botGameTitle => 'Partie gegen Bot';

  @override
  String get botGameLoading => 'Partie wird vorbereitet …';

  @override
  String get botGameLoadFailed =>
      'Die Bot-Partie konnte nicht vorbereitet werden.';

  @override
  String get botMoveFailed => 'Der Bot-Zug konnte nicht berechnet werden.';

  @override
  String get botYou => 'Du';

  @override
  String get botYourTurn => 'Du bist am Zug.';

  @override
  String get botThinking => 'Stockfish denkt …';

  @override
  String get botApplyingMove => 'Zug wird ausgeführt …';

  @override
  String get botWaiting => 'Warte auf den nächsten Zug …';

  @override
  String get botViewingHistory => 'Du siehst eine frühere Stellung.';

  @override
  String get botGameFinished => 'Partie beendet.';

  @override
  String get botMoveList => 'Zugliste';

  @override
  String get botNoMovesYet => 'Noch keine Züge gespielt.';

  @override
  String get botPreviousMove => 'Einen Zug zurück';

  @override
  String get botNextMove => 'Einen Zug vor';

  @override
  String get botReturnToLive => 'Zur aktuellen Stellung';

  @override
  String get botHintPiece => 'Hinweis 1: Figur anzeigen';

  @override
  String get botHintTarget => 'Hinweis 2: Zielfeld anzeigen';

  @override
  String get botHintsUsed => 'Beide Hinweise verwendet';

  @override
  String get botHintThinking => 'Hinweis wird berechnet …';

  @override
  String get botHintFailed => 'Der Hinweis konnte nicht berechnet werden.';

  @override
  String get botGameSettingsTitle => 'Spieleinstellungen';

  @override
  String botHintCounter(int used) {
    return '$used / 2 Hinweise für diesen Zug';
  }

  @override
  String get exportFen => 'FEN exportieren';

  @override
  String get fenCopiedToClipboard => 'FEN wurde in die Zwischenablage kopiert.';

  @override
  String get engineSelectionTitle => 'Engine-Version';

  @override
  String get engineVersion => 'Schach-Engine';

  @override
  String engineActiveLabel(String engine) {
    return 'Aktiv: $engine';
  }

  @override
  String get stockfish18 => 'Stockfish 18';

  @override
  String get stockfish19 => 'Stockfish 19';

  @override
  String get engineSelectionFailed =>
      'Die ausgewählte Engine konnte nicht aktiviert werden. Die bisherige Engine bleibt ausgewählt.';

  @override
  String get forced => 'Erzwungen';

  @override
  String get good => 'Gut';

  @override
  String get statsTimeControlCorrespondence => 'Fernschach';

  @override
  String get statsTimeControlOther => 'Sonstige';

  @override
  String get botGameDeleteQuestion => 'Bot-Partie löschen?';

  @override
  String get botGameDeleteBody =>
      'Dieser Eintrag im Bot-Spielverlauf wird dauerhaft gelöscht. Eine daraus bereits erstellte Analyse-Partie bleibt in deiner lokalen Spielbibliothek erhalten.';

  @override
  String get promotionTitle => 'Bauernumwandlung';

  @override
  String get promotionChoosePiece =>
      'Wähle die Figur, in die der Bauer umgewandelt werden soll.';

  @override
  String get promotionQueen => 'Dame';

  @override
  String get promotionRook => 'Turm';

  @override
  String get promotionBishop => 'Läufer';

  @override
  String get promotionKnight => 'Springer';

  @override
  String get trainingOppositionTitle => 'Bauern-Opposition';

  @override
  String get trainingOppositionHint =>
      'Nimm zuerst die Opposition, dann umgehe den König. Ziehe den Bauern erst, wenn dein König vor ihm steht.';

  @override
  String get trainingLucenaTitle => 'Lucena-Stellung';

  @override
  String get trainingLucenaHint =>
      'Stelle den Turm auf die vierte Reihe, bevor der König herauskommt; später schützt er vor den Schachs.';

  @override
  String get trainingPhilidorTitle => 'Philidor-Verteidigung';

  @override
  String get trainingPhilidorHint =>
      'Halte den Turm auf der sechsten Reihe, bis der Bauer vorzieht, und schache danach von hinten.';

  @override
  String get practiceDrills => 'Matt-Training';

  @override
  String get practiceStudies => 'Klassische Endspielstudien';

  @override
  String get practiceFailed => 'Versuch beendet. Versuche es erneut.';

  @override
  String get practiceThinking => 'Gegner denkt nach …';

  @override
  String practiceMoves(int count) {
    return 'Gespielte Züge: $count';
  }

  @override
  String practiceLevel(int number) {
    return 'Stufe $number';
  }

  @override
  String practiceStudyNumber(int number) {
    return 'Studie $number';
  }

  @override
  String get practiceStudySource =>
      'Kling & Horwitz · Chess Studies, Or, Endings of Games (1851). Spiele diese Stellungen gegen Stockfish.';

  @override
  String get practiceQueenTitle => 'König und Dame gegen König';

  @override
  String get practiceRookTitle => 'König und Turm gegen König';

  @override
  String get practiceQueenRookTitle => 'Dame gegen Turm';

  @override
  String get practiceQueenHint =>
      'Unterstütze die Dame mit deinem König. Lass ein Fluchtfeld frei, bis du mattsetzen kannst.';

  @override
  String get practiceRookHint =>
      'Schneide den König mit dem Turm ab und bringe deinen König näher.';

  @override
  String get practiceQueenRookHint =>
      'Suche nach Doppelangriffen auf König und Turm. Achte auf Patt.';

  @override
  String get practiceBeginner => 'Anfänger';

  @override
  String get practiceIntermediate => 'Fortgeschritten';

  @override
  String get practiceMaster => 'Meister';

  @override
  String get practiceSectionKingPawn => 'König und Bauer';

  @override
  String get practiceSectionBishops => 'Könige, Läufer und Bauern';

  @override
  String get practiceSectionKnightsBishops => 'Springer, Läufer und Bauern';

  @override
  String get practiceSectionTwoMinor => 'Zwei Leichtfiguren gegen eine';

  @override
  String get practiceSectionRookPawns => 'Turm gegen Bauern';

  @override
  String get practiceSectionRookMinor => 'Turm gegen Leichtfiguren';

  @override
  String get practiceSectionMinorRook => 'Leichtfiguren gegen Turm';

  @override
  String get practiceSectionQueenPawns => 'Dame gegen Bauern';

  @override
  String get practiceSectionQueens => 'Damen und Bauern';

  @override
  String get practiceSectionQueenRook => 'Dame gegen Turm';

  @override
  String get practiceSectionQueenMinor => 'Dame gegen Leichtfiguren';

  @override
  String get coach => 'KI-Schachtrainer';

  @override
  String get coachBoardContext => 'Brettkontext';

  @override
  String get coachNoPosition => 'Keine Stellung geladen';

  @override
  String get coachTrainerOutput => 'Trainer';

  @override
  String get coachWelcome =>
      'Frage zur Stellung oder zu einem beliebigen Schachthema.';

  @override
  String get coachThinking => 'Der Trainer denkt nach …';

  @override
  String get coachHint => 'Hinweis';

  @override
  String get coachInputHint => 'Stelle deine Schachfrage …';

  @override
  String get coachSend => 'Senden';

  @override
  String get coachDepthConcise => 'Kurz';

  @override
  String get coachDepthBalanced => 'Ausgewogen';

  @override
  String get coachDepthDeep => 'Tiefgehend';

  @override
  String get coachUnavailable =>
      'Das Sprachmodell des Trainers ist noch nicht verfügbar.';

  @override
  String get coachOffTopic =>
      'Der Trainer beantwortet ausschließlich Schachfragen.';

  @override
  String get coachValidationFailed =>
      'Die Trainerantwort konnte nicht sicher geprüft werden.';

  @override
  String get coachHintRequest => 'Gib mir einen Hinweis zu dieser Stellung.';

  @override
  String get coachFen => 'FEN';

  @override
  String get coachPgn => 'PGN';

  @override
  String get coachPasteFen => 'FEN einfügen';

  @override
  String get coachPastePgn => 'PGN einfügen';

  @override
  String get coachChooseGame => 'Partie aus „Partien“ auswählen';

  @override
  String get coachUseContext => 'Übernehmen';

  @override
  String get coachNoGames => 'Keine Partien verfügbar.';

  @override
  String get coachMore => 'Mehr';

  @override
  String get coachPlayerWhite => 'Weiß';

  @override
  String get coachPlayerBlack => 'Schwarz';

  @override
  String get coachHintPieceMessage =>
      'Beginne mit der markierten Figur. Überlege zuerst, was sich verbessert, wenn diese Figur zieht.';

  @override
  String get coachHintTargetMessage =>
      'Schau jetzt auf das markierte Zielfeld und den Pfeil. Versuche die Idee zu finden, bevor du die Erklärung aufdeckst.';

  @override
  String get coachHintExplainRequest =>
      'Erkläre, warum der native Engine-Hinweiszug in dieser Stellung stark ist. Konzentriere dich auf die Schachidee und den praktischen Plan, nicht auf Engine-Zahlen.';
}
