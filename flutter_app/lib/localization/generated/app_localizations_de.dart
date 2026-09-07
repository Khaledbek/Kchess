// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for German (`de`).
class AppLocalizationsDe extends AppLocalizations {
  AppLocalizationsDe([String locale = 'de']) : super(locale);

  @override
  String get appTitle => 'KChess';

  @override
  String get firstRunTitle => 'Dein lokaler Schachbereich';

  @override
  String get firstRunBody =>
      'Wähle eine Quelle. Öffentliche Online-Profile benötigen kein Passwort.';

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
  String get playPlaceholder =>
      'Dieser Bereich ist als Platzhalter für zukünftige Spielmodi vorbereitet, z. B. Partien gegen Bots.';

  @override
  String get downloads => 'Downloads';

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
  String get favoriteCollectionRule =>
      'Sammlungen sind nur eine Ebene tief und können nicht verschachtelt werden.';

  @override
  String get favoriteMoveToCollection => 'Sammlung ändern';

  @override
  String get favoriteMoveHelp =>
      'Partien können lose in Favoriten liegen oder genau einer Sammlung zugeordnet werden.';

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
  String get demoNotice => 'Lokale Partie';

  @override
  String get tapToAnalyze => 'Öffnen und analysieren';

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
  String get boardArrows => 'Brett-Pfeile anzeigen';

  @override
  String get boardArrowsHelp =>
      'Nur Darstellung; Umschalten startet keine neue Analyse.';

  @override
  String get engine => 'Engine';

  @override
  String get enginePreset => 'Mittel · Tiefe 18 · 3 Linien';

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
  String get engineSettingsSubtitle =>
      'Tiefe, Linien, Zeitlimit, Threads und Hash';

  @override
  String get analysisSettingsTitle => 'Analyse';

  @override
  String get analysisSettingsSubtitle => 'Pfeile, Bewertung und Analyseanzeige';

  @override
  String get analysisBoardGuidance => 'Brett-Hinweise';

  @override
  String get analysisInformation => 'Analyseinformationen';

  @override
  String get bestMoveArrow => 'Bestzug-Pfeil';

  @override
  String get bestMoveArrowHelp => 'Zeigt den besten Engine-Zug auf dem Brett.';

  @override
  String get threatArrow => 'Bedrohungs-Pfeil';

  @override
  String get threatArrowHelp =>
      'Zeigt den stärksten gegnerischen Zug als Warnpfeil, wenn der Gegner am Zug ist.';

  @override
  String get evaluationBarSetting => 'Bewertungsleiste';

  @override
  String get evaluationBarSettingHelp => 'Zeigt die aktuelle Engine-Bewertung.';

  @override
  String get showEngineLinesSetting => 'Engine-Linien anzeigen';

  @override
  String get showEngineLinesSettingHelp =>
      'Zeigt die berechneten Hauptvarianten (MultiPV).';

  @override
  String get showClassificationsSetting => 'Zugklassifikationen anzeigen';

  @override
  String get showClassificationsSettingHelp =>
      'Zeigt Theorie, Brillant, Kritisch, Bester und die weiteren Zugkategorien.';

  @override
  String get showAccuracySetting => 'Accuracy anzeigen';

  @override
  String get showAccuracySettingHelp =>
      'Zeigt die lokal berechneten Accuracy-Werte.';

  @override
  String get showTheorySetting => 'Theorieinformationen anzeigen';

  @override
  String get showTheorySettingHelp =>
      'Zeigt Eröffnungsbuch-Informationen und Theorie-Zähler.';

  @override
  String get showResultSymbolsSetting => 'Ergebnissymbole anzeigen';

  @override
  String get showResultSymbolsSettingHelp =>
      'Zeigt bei beendeten Partien Win-, Loss- oder Draw-Symbole über den Königen.';

  @override
  String get designSettingsTitle => 'Design';

  @override
  String get designSettingsSubtitle => 'Darstellung, Theme, Brett und Figuren';

  @override
  String get generalSettingsTitle => 'Allgemein';

  @override
  String get generalSettingsSubtitle => 'Sprache und App-Verhalten';

  @override
  String get dataStorageSettingsTitle => 'Daten & Speicher';

  @override
  String get dataStorageSettingsSubtitle =>
      'Analyse-Cache, Downloads und lokale Daten';

  @override
  String get dataStoragePlaceholder =>
      'Speicher- und Cache-Optionen werden in einem nächsten Schritt ergänzt.';

  @override
  String get licensesAbout => 'Lizenzen & Über';

  @override
  String get stockfishPending => 'Stockfish 18 · lokal · GPLv3';

  @override
  String get provider => 'Anbieter';

  @override
  String get localProfile => 'Lokales Profil';

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
  String get player => 'Spieler';

  @override
  String get bothPlayers => 'Beide';

  @override
  String get whitePlayer => 'Weiß';

  @override
  String get blackPlayer => 'Schwarz';

  @override
  String get analyzedMoves => 'Klassifizierte Züge';

  @override
  String get bookGames => 'Book-Partien';

  @override
  String get expectedLoss => 'Erwartungswertverlust';

  @override
  String get versions => 'Versionen';

  @override
  String get classifierVersionLabel => 'Klassifikator';

  @override
  String get accuracyVersionLabel => 'Accuracy';

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
  String moveComparisonText(String played, String classification, String best) {
    return '$played war $classification. $best ist der beste Zug.';
  }

  @override
  String theoryMoveText(String move) {
    return '$move ist ein Theorie-Zug.';
  }

  @override
  String triedMove(String move) {
    return 'Du hast $move ausprobiert.';
  }

  @override
  String get sidelineEngineTitle => 'Side-Line-Engine';

  @override
  String get sidelineEngineSubtitle =>
      'Diese Werte gelten nur für die Live-Analyse eigener Varianten.';

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
  String get sidelineAnalysisPaused => 'Live-Analyse pausiert';

  @override
  String get analyzingVariation => 'Die temporäre Variante wird analysiert…';

  @override
  String evaluationComparison(String before, String after) {
    return 'Bewertung: $before → $after';
  }

  @override
  String bestContinuation(String line) {
    return 'Beste Fortsetzung: $line';
  }

  @override
  String get returnToMainLine => 'Zur Hauptlinie zurückkehren';

  @override
  String illegalOrFailedMove(String message) {
    return 'Der Zug ist nicht legal oder konnte nicht analysiert werden: $message';
  }

  @override
  String get myPlayer => 'Mein Spieler';

  @override
  String get opponent => 'Gegner';

  @override
  String get variationStartingPosition => 'Ausgangsstellung der Variante';

  @override
  String get variationStart => 'Anfang der Variante';

  @override
  String get engineQualityTitle => 'Analysequalität';

  @override
  String get engineResourcesTitle => 'Ressourcen';

  @override
  String get depthHelp =>
      'Min = Tiefe der Voranalyse. Max = maximale Tiefe der Liveanalyse. Höhere Werte dauern in der Regel länger.';

  @override
  String get adaptiveEarlyStop => 'Adaptive Analyse';

  @override
  String get adaptiveEarlyStopHelp =>
      'Beendet ruhige Vor- und Liveanalysen früher, wenn Bewertung und Hauptvarianten stabil sind. Kritische Verifikationen rechnen weiterhin bis zum eingestellten Limit.';

  @override
  String get numberOfLinesHelp =>
      'Wie viele beste Varianten Stockfish gleichzeitig berechnet.';

  @override
  String get timeLimitHelp =>
      'Optionales Limit pro Stellung. Aus nutzt nur die Tiefe; sonst endet die Suche, sobald Tiefe oder Zeit zuerst erreicht ist.';

  @override
  String get threads => 'Threads';

  @override
  String get threadsHelp =>
      'CPU-Threads pro Stockfish-Worker. Kchess erkennt deinen PC automatisch und erlaubt höchstens die Hälfte der logischen CPU-Threads.';

  @override
  String get hashMemory => 'Hash-Speicher';

  @override
  String get hashMemoryHelp =>
      'Arbeitsspeicher für Stockfishs Transposition Table. Mehr Speicher kann die Suche in wiederkehrenden Stellungen verbessern.';

  @override
  String get boardDisplayTitle => 'Brettdarstellung';

  @override
  String get rotateBoard => 'Brett drehen';

  @override
  String get showBoardCoordinates => 'Brettkoordinaten';

  @override
  String get showBoardCoordinatesHelp =>
      'Zeigt Linien- und Reihennamen (a–h / 1–8) am Brett.';

  @override
  String get highlightLastMove => 'Letzten Zug hervorheben';

  @override
  String get highlightLastMoveHelp =>
      'Markiert Start- und Zielfeld des zuletzt gespielten Zuges.';

  @override
  String get highlightSelectedSquare => 'Ausgewähltes Feld hervorheben';

  @override
  String get highlightSelectedSquareHelp =>
      'Markiert das Feld, das du für eine Variante ausgewählt hast.';

  @override
  String get behaviorTitle => 'Verhalten';

  @override
  String get autoSyncOnline => 'Online-Profile automatisch aktualisieren';

  @override
  String get autoSyncOnlineHelp =>
      'Synchronisiert Chess.com und Lichess beim Start und beim Profilwechsel automatisch.';

  @override
  String get confirmBeforeDelete => 'Vor dem Löschen bestätigen';

  @override
  String get confirmBeforeDeleteHelp =>
      'Fragt vor dem Löschen von Profilen oder lokalen Partien nach.';

  @override
  String get analysisCacheTitle => 'Analyse-Cache';

  @override
  String get useGlobalAnalysisCache => 'Globalen Stellungs-Cache verwenden';

  @override
  String get useGlobalAnalysisCacheHelp =>
      'Verwendet bereits analysierte identische Stellungen auch in anderen Partien wieder.';

  @override
  String get clearAnalysisCache => 'Analyse-Cache leeren';

  @override
  String get clearAnalysisCacheHelp =>
      'Löscht nur den globalen Stellungs-Cache. Gespeicherte Partien und vollständige Partieanalysen bleiben erhalten.';

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
  String get diagnosticLoggingHelp =>
      'Schreibt begrenzte technische Logs für die Fehlersuche. Vollständige PGNs, FENs und Provider-Antworten werden nicht protokolliert.';

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
  String get statsAllTimeControlsNote => 'Alle Zeitkontrollen';

  @override
  String get statsPhaseTitle => 'Nach Spielphase';

  @override
  String get statsPhaseSubtitle =>
      'In welcher Phase deine Partien enden und wie du abschneidest.';

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
  String get statsIntroBody =>
      'Sieh deine Ergebnisse, aktuelle Form und Eröffnungsbilanz getrennt nach Farbe.';

  @override
  String get statsFormTitle => 'Aktuelle Form';

  @override
  String get statsFormHint =>
      'Tippe auf ein Ergebnis, um die Partie zu öffnen.';

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
}
