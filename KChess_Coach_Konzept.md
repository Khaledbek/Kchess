# KChess Coach – finaler adaptiver AI-Schachtrainer

## Ziel und Grundprinzip

KChess behandelt den Coach als nativen, persönlichen Schachtrainer und nicht als freien LLM-Chat. C++ entscheidet, welche Schachfakten gelten, welcher Lernpunkt relevant ist, wie schwer eine Aufgabe sein soll, wann ein Thema wiederholt wird und ob eine automatische Unterbrechung didaktisch sinnvoll ist. Gemini formuliert die bereits geplante und geerdete Lektion natürlich. Flutter bleibt UI, Interaktion und Darstellung. Python bleibt Offline-/Evaluierungswerkzeug.

Die zentrale Regel lautet: **Engine-/Datenbank-/Knowledge-Wahrheit und Lernpolitik bleiben nativ; das LLM formuliert, aber erfindet keine zweite Wahrheit.**

## Finaler Coach-Datenfluss

```text
Nutzerfrage / Hint / Automatic Coach / persönliche Übung
        ↓
CoachService (Integration + Foreground-Priorität)
        ↓
CoachSession (kompakter Gesprächszustand)
        ↓
DomainRouter (+ optional Tiny Intent bei Ambiguität)
        ↓
QueryPlanner (+ optional Tiny Context Planner)
        ↓
ContextBuilder
        ↓
EvidenceRetriever
  cache → bestehende Analyse → lokale Schach-/Konzeptdaten
  → Theory/Opening → Player/Knowledge → nur falls nötig Engine
        ↓
PositionAnalysisStage
  Features + Weaknesses + Exploitation + Pläne + Motive
  mit bounded Exact-FEN-Cache
        ↓
Practicality v2
  objektive Engine-Reihenfolge bleibt Wahrheit; WDL/Expected Score
  bewertet nur Risiko/Praktikabilität im Spieler-Kontext
        ↓
TeachingPlanner
  ein primäres Lernziel, Delivery Mode, Reveal-Level, Limits
        ↓
ProviderInputOptimizer
  nur provider-sichtbare exakte Duplikate/unnötige Evidenz kürzen
  volle Evidenz bleibt für native Validierung erhalten
        ↓
ValidatedResponseCache (nur exact + bereits validiert)
        ↓ miss
Gemini
        ↓
ResponseValidator + maximal ein Repair-Pass
        ↓
Session merken + nur verifizierte Lernversuche persistieren
        ↓
Flutter zeigt Antwort / Frage / Brettmarkierungen
```

Es existiert bewusst **kein zweiter Coach-Pfad** für Automatic Coach, persönliche Übungen oder Tiny Models. Alle Varianten laufen durch dieselbe Orchestrierung und denselben Validator.

## Teaching Planner: KChess entscheidet, was gelehrt wird

Der `TeachingPlanner` liegt vor dem Provider. Er bestimmt nativ einen primären Lernzweck, Delivery Mode, Reveal-Level sowie maximale Konzepte/Empfehlungen. Quiz- und frühe Hint-Schritte dürfen dadurch keine Lösung über Empfehlungen verraten. Gemini bekommt den Unterrichtsvertrag und formuliert ihn, entscheidet aber nicht eigenständig über eine konkurrierende Lernstrategie.

Antworten sollen standardmäßig einen überschaubaren Punkt lehren. Mehrere Nebenprobleme dürfen als Kontext vorkommen, aber der zentrale Lernpunkt bleibt eindeutig, außer der Nutzer fordert ausdrücklich eine tiefe Gesamtanalyse an.

## Feingranulares Skill-Modell

Neue verifizierte Übungen werden unter stabilen namespaced Skill-IDs erfasst, z. B. `tactics.fork`, `tactics.back_rank`, `strategy.plan_choice`, `opening.decision`, `endgame.decision` oder `calculation.candidate_selection`. Ein Motiv wird nur dann fein zugeordnet, wenn die vorhandene native Evidenz ausreichend sicher ist; sonst bleibt der gröbere Skill bestehen.

Die vorhandene SQLite-Tabelle `ai_coach_skill_progress` bleibt die **einzige** persistierte Lernfortschrittsquelle. Sie wurde erweitert, nicht dupliziert. Gespeichert werden u. a. verifizierte Erfolge/schwache Versuche, Streak, Scheduling-Level, Intervall und nächste Fälligkeit. Diese Daten sind Trainingszustand, kein pauschales Rating oder Beweis für Stärke/Schwäche.

## Spaced Repetition

`SpacedRepetitionScheduler` entscheidet nativ, wann ein Skill wiederholt werden soll. Ein selbstständig gelöster, nativ verifizierter Versuch verlängert das Intervall; ein verifizierter schwacher Versuch setzt die Wiederholung näher. Unbewertete legale Alternativen oder reine LLM-Aussagen verändern den Lernstand nicht.

Fälligkeit beeinflusst die Auswahl von Übungen und den Teaching Value des Automatic Coach, wird aber nicht als zusätzliche Spieler-Schwäche ausgegeben.

## Training aus eigenen Partien

Die Aktion „aus meinen Partien trainieren“ wählt ausschließlich reale, bereits gelernte Beispielstellungen aus dem vorhandenen Spielerprofil. `personal_training_selector` kombiniert vorhandene Muster-/Prioritätsdaten mit fälliger Wiederholung und gibt nur eine Referenz auf die passende echte Partie/Stellung zurück. Danach läuft die Stellung wieder durch den normalen Coach-/Quiz-Pfad.

Es gibt keine zweite Puzzle-Datenbank und keine erfundenen Trainingsstellungen.

## Foreground vor Automatic Coach

`CoachService` bleibt die einzige Serialisierungsgrenze. Manuelle Fragen und explizite Hints sind Foreground-Arbeit und erhalten den nächsten freien Coach-Slot. Wartende Automatic-Coach-Jobs werden verdrängt. Ein Provider-Aufruf, der bereits läuft, kann mangels Provider-Cancel-Vertrag noch zu Ende laufen; sein veraltetes Ergebnis wird danach verworfen. Parallele Gemini-Aufrufe werden nicht als Abkürzung eingeführt.

## Automatic Coach: Teaching Value statt bloßer Ereignisstärke

Der Automatic Coach nutzt weiterhin native Schachereignisse wie Fehler, WDL-Verschiebungen, Motive, Phasenwechsel und verifizierte Quiz-Antworten. Zusätzlich bewertet er, ob eine Unterbrechung didaktisch sinnvoll ist: persönliche Relevanz, fällige Wiederholung, Neuigkeit und jüngste automatische Coach-Ausgabe beeinflussen den Teaching Value.

Dadurch soll der Coach weniger häufig, aber gezielter sprechen. Eine verifizierte Antwort auf eine offene Trainerfrage bleibt davon ausgenommen und erhält Feedback.

## Practicality v2

Practicality darf die objektive Engine-Reihenfolge nicht überschreiben. Wenn WDL/Expected Score vorliegt, ist der Verlust an erwarteter Punktzahl das primäre Sicherheits-/Risikosignal; Centipawn-Verlust ist nur Fallback. Spielerprofil, Phase und vorhandener Kontext erklären, welcher objektiv akzeptable Zug praktisch leichter oder riskanter ist. Engine-Rang 1 bleibt immer zulässig.

## Optional Tiny Models und Embeddings

Der Coach kann kleine portable Modelle aus dem vorhandenen Model-Root laden:

- `intent_linear.json` für mehrdeutiges Intent-Routing,
- `context_planner_linear.json` für begrenzte Context-Plan-Verfeinerung,
- `embedding_projection.json` für den bestehenden gemeinsamen `EmbeddingModel`-Vertrag.

Diese Modelle sind **optional**. Fehlen sie oder sind sie ungültig, bleibt der deterministische C++-Pfad vollständig funktionsfähig. Tiny Models dürfen keine Engine-Arbeit erzwingen, keine Schachwahrheit erzeugen und keinen zweiten Router/Retriever bilden. Das Embedding-Modell wird gemeinsam von Concept Retrieval und Knowledge Graph genutzt; exakte/kräftige lexikalische Treffer bleiben bevorzugt.

## Caches und Performance

### Position Intelligence Cache

`PositionAnalysisStage` hält bis zu 128 exakte FEN-Einträge mit rein deterministischen DTOs (Features, Schwächen, Exploitation, Pläne, Motive). Er speichert keine Engine-Ergebnisse und keine Spielerkenntnisse dauerhaft. Follow-up-Fragen zur identischen Stellung vermeiden damit unnötige Wiederberechnung.

### Validated Response Cache

`ValidatedResponseCache` ist prozesslokal und bounded. Ein Treffer ist nur möglich, wenn Provider-Request, provider-sichtbare Evidenz, vollständige Validierungs-Evidenz, Teaching-Plan, Profil-/Session-Kontext, Provider-ID sowie Prompt-/Schema-Version exakt übereinstimmen. Nur bereits nativ validierte Antworten werden gespeichert. Fehler, unvalidierte Inhalte und Lernzustände werden nie gecacht.

Update 166 baut diesen exakten Schlüssel nur einmal pro Provider-Stage und verwendet ihn für Lookup und späteres Store wieder; die Exaktheit wird nicht durch einen lossy Hash ersetzt.

### Provider Input Optimizer

Der Optimizer darf nur provider-sichtbare leere/exakte Duplikate oder bereits als out-of-scope definierte Profil-Evidenz entfernen und bestehende Tokenbudgets anwenden. Die vollständige native Evidenz bleibt für Validierung und Cache-Wahrheit erhalten. Diagnosewerte messen Input-/Selected-Items, geschätzte Tokens und Duplikatreduktion.

## Halluzinationsgrenze und Validation

Gemini liefert strukturierte Inhalte. Die native Validierung prüft u. a. Evidenzreferenzen, Zuglegalität, Brettfakten, Kandidaten-/Engine-Bezug, Eröffnungsdaten und Teaching-Plan-Limits. Bei Fehlern ist höchstens ein Repair-Pass erlaubt. Ein weiterhin ungültiges Ergebnis wird nicht als gültige Coach-Antwort übernommen und nicht als validierter Cache-Eintrag gespeichert.

Natürliche Erklärungen allgemeiner Prinzipien sind dadurch nicht mathematisch bewiesen; KChess begrenzt Halluzinationen durch Fakten-/Schema-/Move-Verträge, verspricht aber keine absolute Fehlerfreiheit freier Sprache.

Update 174 verbindet jede nicht-allgemeine typisierte Behauptung im neuen Provider-Schema mit einem kurzen exakten `answer_quote` aus Antwort oder Trainerfrage. C++ prüft den Textanker und die zugrunde liegende Evidenz getrennt. Für einen nachweislich abgeschlossenen Antwortzug liefert `move.contrast.v1` zusätzlich einen statischen Vorher-/Nachher-Vergleich und, falls vorhanden, den ursprünglich gespeicherten Kandidaten. Material ist ein Brettfakt; Aktivitäts- und Königszonenwerte sind nur transparente Stellungsmerkmale, keine neue Engine-Bewertung. Der Coach darf einen numerischen Vergleich nur mit exakt validiertem `position_contrast_fact` zitieren. Der verbleibende Grenzfall sind faktische Sätze, für die Gemini gar keinen Claim ausgibt; der nächste Schritt ist eine vollständige Segment-zu-Claim-Abdeckung.

Ein offline auswertbarer, synthetischer Dialogkatalog erfasst Faktenstützung, Scope/Unsicherheit, Reaktion auf Spielzüge und Gesprächsnutzen getrennt. Der priorisierte Ausbau und die Kriterien für ein späteres Sprachstil-Tuning stehen in `docs/Coach_Naechste_Verbesserungen.md`.

Update 175 setzt die sichtbare Gemini-Antwort aus geordneten Segmenten zusammen. Ein faktisches Segment braucht genau einen typisierten Claim; dessen `answer_quote` muss dem ganzen Segment entsprechen und der Claim wird gegen genau die an Gemini gesendete Evidenz geprüft. Allgemeine Regeln, ausdrücklich markierte Unsicherheit und kurze Gesprächsübergänge haben eigene Segmentarten. Für persönliche oder aktuelle Stellungsfragen sind ungebundene allgemeine Segmente gesperrt. Diese Struktur verringert verdeckte unbelegte Sätze; ob eine natürliche Schlussfolgerung aus einem Quellwert wirklich folgt, muss zusätzlich an menschlich geprüften Dialogen bewertet werden.

Der Zugkontrast kann nun eine vorhandene vollständige Analyse derselben Partie und desselben Halbzugs lesen, nachdem UCI sowie Ausgangs- und Ziel-FEN übereinstimmen. Nur dann dürfen gespeicherte Klassifikation, vergleichbare erwartete Scores und eine tatsächlich gespeicherte gegnerische Antwort als `verifiedAnalysis` in den kompakten Provider-Kontext gelangen. Ohne diesen Eintrag bleibt die Erklärung auf deterministische Brettmerkmale und den nativen Versuchstatus begrenzt. Der Coach startet dafür keine weitere Suche.

Der Offline-Review kann zusätzlich private, reale Dialogfälle und menschliche Korrekturen aus lokalen JSONL-Dateien auswerten. Eine deterministische Gruppenteilung nach Spieler/Partie hält Entwicklungs- und Held-out-Fälle getrennt. Fakt-, Scope-, Kontinuitäts- und Lehrnutzenfehler werden getrennt gezählt; nur geprüfte reine Tonkorrekturen können später einen kontrollierten Sprachstil-Vergleich begründen. KChess erstellt oder trainiert dabei kein kleines Modell. Die realen Fälle und eine solche Gegenüberstellung liegen noch nicht vor.

## Diagnose

Die Coach-Performance-Diagnose erfasst pro Turn und aggregiert u. a. Session, Routing, Planning, Context, Retrieval, Position Analysis, Practicality, Teaching Planner, Provider-Request-Build, Response-Cache-Key, Provider, Validation und Repair. Zusätzlich werden Cache-Hits, Evidence-/Token-Reduktion, Provider-Calls und Foreground-/Automatic-Wartezustände sichtbar. Diagnose bleibt read-only und beeinflusst keine Coach-Entscheidung.

## UI-Grenze

Flutter zeigt Chat, Trainerfrage, Ereigniskarten, Brettmarkierungen, Hints, Quiz und die lokalisierte persönliche Trainingsaktion. FEN/PGN/Board-Interaktion werden nur als UI-/Transportzustand gehalten; Legalität, Auswahl der Trainingsstellung, Skill-Zuordnung, Scheduling, Routing, Engine-Nutzung, Teaching-Plan und Validation bleiben nativ. Alle festen sichtbaren Texte liegen gemeinsam in EN/DE/AR-ARB.

## Finaler Ownership-Merksatz

```text
Schachwahrheit          → vorhandene native Engine/Analyse/DB/Knowledge-Systeme
Coach-Orchestrierung    → native/ai/coach_orchestrator.*
App-Integration/Priorität → native/src/services/coach_service.*
Unterrichtspolitik      → native/ai/teaching/*
Position Intelligence   → native/ai/position/position_analysis_stage.*
Practicality            → native/ai/practicality/*
Automatic Coach         → native/ai/automatic/*
Provider-Optimierung    → native/ai/optimization/*
Optional Tiny/Embedding → native/ai/models/*
Persistierter Lernstand → bestehende ai_coach_skill_progress-Tabelle
Gemini                  → Formulierung/Dialog nach nativer Planung
Flutter                 → UI/Interaktion/Lokalisierung
```

Keine dieser Komponenten soll durch eine parallele Implementierung in Flutter, Python oder einem zweiten Coach-Pfad dupliziert werden.
