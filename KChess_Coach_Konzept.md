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

## Grounded Coach Architecture (Updates 187–198)

Der aktuelle Coach trennt Schachwahrheit, Unterrichtslogik und Sprache strikt. Eine konkrete Nutzerfrage hat Vorrang vor einem älteren Trainingsmotiv und wird nativ in einen `PositionAnalysisMode` aufgelöst. Dazu gehören unter anderem `best_move`, `worst_move`, `fastest_loss`, `avoid_trade`, `threat`, `what_if_move`, Kandidatenvergleich und die Bewertung eines verifizierten Nutzerzuges.

Für `worst_move` und `fastest_loss` genügt ein gewöhnlicher Top-N-Hint nicht. KChess prüft deshalb alle legalen Root-Züge mit einem begrenzten Scout und vertieft nur die schlechtesten Kandidaten. Ein nachgewiesener Verlust durch Matt hat Vorrang vor Centipawn-Verlust; bei `fastest_loss` wird der kürzeste nachgewiesene Verlust bevorzugt. Fertige Extreme-Ergebnisse dürfen nur prozesslokal für exakt dieselbe FEN, Engine-Einstellung und denselben Analysemodus wiederverwendet werden.

Konkrete Schachbehauptungen werden als native `coach.chess_facts.v1`-Fakten mit stabilen Fact-/Candidate-IDs transportiert. Gemini erhält keine Autorität, neue aktuelle Brettzüge zu erfinden, sondern formuliert didaktische Sprache über freigegebene Referenzen. `coach_response.v7` verwendet deshalb rhetorische Segmente mit Fact-Referenzen statt einer providerseitigen factual/nonfactual-Klassifikation. Der native Verified-Fact-Renderer löst diese Referenzen in konkrete Darstellung auf; Validierung und sichere Fallbacks bleiben nativ.

Der Provider-Repair-Pass ist kein Standardpfad mehr. Automatische Coach-Turns bleiben bei höchstens einem Provider-Aufruf, und wenn verifizierte native Evidenz einen sicheren Fallback erlaubt, wird kein zweiter LLM-Aufruf für bloße Formulierungsreparatur verbraucht. Provider-sichtbare Kandidaten-/PV-/Fact-Daten dürfen kompakt übertragen werden, während die vollständige native Evidenz für Validierung, Session-State und Fallback unverändert bleibt.

### Update 202 – Evidence-Plan Contract

Der `EvidencePlan` beschreibt kompositionell, welche Quellen und Informationstypen eine freie Schachfrage benötigt. Perspektive, Analyseumfang, Tiefe, Freshness, Interaktionsart und Elo-Ziel sind Planungsmetadaten und keine Schachwahrheit. Dadurch muss KChess nicht für jede mögliche Nutzerformulierung einen neuen festen Intent erfinden.

### Update 203 – Chess Expert Registry

### Update 204 – Existing Analysis Expert
Bereits vorhandene KChess-Analyse wird jetzt als eigener Expert mit expliziter Coverage behandelt. Cache/MultiPV darf neue Engine-Arbeit nur dann ersetzen, wenn die vom Evidence Plan verlangten Engine-Fakten tatsächlich vorhanden sind. Unvollständige Top-N-Analyse gilt ausdrücklich nicht als Beweis für alle legalen Züge oder materielle Konsequenzen.

### Update 205 – Stockfish Expert

Stockfish bleibt objektive Schachwahrheit, wird aber als gezielt budgetierte Expert-Quelle hinter dem EvidencePlan behandelt. Bestehende Analyse hat Vorrang, sofern ihre Coverage die angeforderten Needs wirklich erfüllt; exhaustive oder ausdrücklich frische Anforderungen dürfen weiterhin neue native Analyse auslösen. Engine-Nutzung steuert niemals Gesprächsabsicht oder Formulierung.

### Update 206: Human/Elo Bot Expert
KChess exposes the existing Elo bot policy as a separate `human_model` expert. It projects verified candidate moves into human-likelihood evidence for a requested rating while keeping Stockfish/existing analysis as the only objective source of move quality. The human expert does not launch searches; it consumes already-evaluated candidates so practical prediction stays cheap and cannot replace engine truth.

## Update 207 – Gemeinsames Expert-Evidence-Format

Position-, Taktik-, Opening- und Profilwissen werden nicht neu berechnet, sondern aus den bereits bestehenden KChess-Evidenzströmen in `ExpertEvidence` normalisiert. Jede Einheit trägt ihre `EvidenceSource`, die erfüllbaren `EvidenceNeed`-Anforderungen und die Kennzeichnung, ob sie objektive Brettwahrheit oder heuristische/praktische Evidenz ist. Damit kann der nächste Aggregator mehrere Experten kombinieren, ohne deren ursprüngliche Verantwortlichkeiten zu duplizieren.

## Update 208 – Evidence Aggregator

Der Coach besitzt jetzt eine deterministische Aggregationsgrenze für Ergebnisse mehrerer Schachexperten. Bereits erzeugte Expert-Evidence wird anhand des aktuellen Evidence-Plans nach Informationsbedarf, angeforderter Quelle, Konfidenz und objektiver Wahrheit priorisiert. Identische Evidenz wird dedupliziert; widersprüchliche Payloads mit derselben stabilen Evidence-ID werden nicht gemeinsam an das LLM weitergereicht, sondern deterministisch aufgelöst und diagnostisch gezählt. Nach der finalen Auswahl werden erfüllte und fehlende EvidenceNeeds neu berechnet. Der Aggregator führt selbst keine Engine-, Bot-, Datenbank- oder Providerarbeit aus und erzeugt keine neuen Schachfakten.

## Update 209 – Coach LLM Context v2

Der Provider erhält pro Turn gemeinsam die aktuelle Nutzerfrage, den kanonischen Brettkontext, die relevante Session-Zusammenfassung, den `EvidencePlan` und die tatsächlich ausgewählte native Evidenz. Der `EvidencePlan` beschreibt ausschließlich den Informationsbedarf und ist kein Beweis für eine Schachbehauptung. Dadurch kann das LLM Frage, Gespräch und Schachinformationen als einen zusammenhängenden Coaching-Kontext interpretieren, während konkrete Brettfakten weiterhin ausschließlich aus nativer Evidenz stammen.

### Update 210 – Conversation Understanding

Explizite Folgefragen behalten ihren neu erkannten aktuellen Intent, erhalten aber trotzdem den kompakten vorherigen Nutzerzweck und die letzte akzeptierte Coach-Antwort als Gesprächskontext. Nur echte elliptische Follow-ups erben den vorherigen Intent. Frühere Coach-Texte bleiben ausdrücklich nicht autoritative Schachevidenz; Brettfakten müssen weiterhin aus nativen Evidence-Quellen stammen.

### Update 212 – vereinfachtes Grounding

Mit `coach_response.v8` existiert für konkrete aktuelle Brettwahrheit nur noch ein primärer Grounding-Pfad: native `coach.chess_facts.v1`-Referenzen. Das LLM darf Züge, Schach/Matt, Bewertungen, Figurenfelder und taktische Motive nicht mehr über einen parallelen freien Claim-Vertrag beschreiben. Typisierte Claims bleiben vorübergehend nur für Opening-, Profil- und Move-Contrast-Domänen bestehen, bis auch diese vollständig über native Fact-IDs gerendert werden. Dadurch sinkt die Schema-Komplexität und ein semantisch identischer Schachfakt kann nicht mehr gleichzeitig in zwei konkurrierenden Repräsentationen auftreten.

### Update 213 – Optimierung

Die neue Evidence-Schicht wird unter Kontext- und Latenzbudgets coverage-first verdichtet: zuerst bleibt pro angefordertem Informationsbedarf mindestens die stärkste verfügbare Evidenz erhalten, danach werden Restplätze nach Relevanz gefüllt. Dadurch kann Kontextkompression keine seltene, aber für die aktuelle Frage notwendige Information verdrängen. Die Engine-Tiefe orientiert sich zusätzlich am EvidencePlan; automatische/background Turns werden nicht unnötig auf Deep-Analyse hochgestuft, während explizit tiefe Foreground-Anfragen weiterhin Deep-Budget erhalten können. Bereits vorhandene Analyse bleibt vor neuer Engine-Arbeit priorisiert.

## Update 214 – Learned Chess Evidence Planning abgeschlossen

Die Expert-Schicht vereinheitlicht vorhandene Analyse, Stockfish, Human/Elo-Projektionen, Position/Taktik, Opening, Profil/Historie, Gespräch und Konzepte. Vorhandene Analyse wird nach Coverage wiederverwendet; Freshness kann gezielt frische Engine-Arbeit verlangen oder optionalen Cache-only-Betrieb wählen. Human/Elo-Evidenz beschreibt nur menschliche Wahrscheinlichkeit und ersetzt niemals objektive Engine-Wahrheit. Der Aggregator dedupliziert und priorisiert Evidenz, bevor sie den LLM-Kontext erreicht, während vollständige native Evidenz für Validation und Fallback erhalten bleibt.

Der Coach erhält damit freie Nutzerfrage, kanonischen Brettzustand, bounded Conversation Context, den EvidencePlan und die passende Schachevidenz als getrennte strukturierte Eingaben. Gemini bleibt für Sprachverständnis, Kombination und Coaching-Formulierung zuständig; konkrete aktuelle Brettfakten müssen weiterhin aus nativer Evidenz stammen. Unaufgelöste interne Referenzmarker werden hart blockiert. Diagnosefelder machen Planner-Ausgabe und Evidence-Coverage sichtbar, ohne Entscheidungen zu beeinflussen.

Python bleibt ausschließlich Training/Export des kleinen Modells. Das portable Student-Modell wird später als lokale KChess-Assetdatei eingebunden; normale Source-Update-ZIPs enthalten weder Modellbinary noch `third_party` oder Secrets. Der finale Provider-Kontext dieser Serie verwendet `CoachPrompt v12` mit `coach_response.v8`.
