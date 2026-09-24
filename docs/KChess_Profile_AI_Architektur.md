# KChess – Player Profile, Background Analysis und Knowledge-Graph-Architektur

## 1. Zweck

Dieses Dokument beschreibt den aktuellen produktiven Stand der personalisierten KChess-Architektur nach den Profil-, Analyse- und Knowledge-Graph-Updates bis einschließlich Cleanup Update 124.

Die zentralen Ziele sind:

- ein dauerhaft wachsendes, erklärbares Spielerprofil,
- eine begrenzte und wiederaufnehmbare Hintergrundanalyse,
- genau ein autoritativer Analyse-Cache,
- ein allgemeiner lokaler Knowledge Graph statt eines separaten Profilgraphs,
- token-sparendes, nachvollziehbares Coach-Retrieval,
- klare Ownership zwischen Flutter, C++ und Python.

Historische Zwischenarchitekturen wie der frühere `ProfileGraphRetriever`, der service-seitige `coach_profile_context_resolver` und der separate sparse profile-probe store sind nicht mehr Teil des Systems.

## 2. Verbindliche Ownership

```text
Flutter / Dart
  UI, Navigation, View-State, Darstellung, ARB-Lokalisierung

C++20
  Profilpipeline, Schachlogik, Engine-Orchestrierung, Persistenz,
  Knowledge Graph, Retrieval, Ranking, Confidence/Coverage/Freshness,
  Active Learning, Coach Evidence Packets

Python
  Offline-Evaluierung und reproduzierbare Entwicklerwerkzeuge,
  Test-/Hilfstools; keine zweite App-Runtime
```

Flutter berechnet keine Profil-, Graph-, Ranking-, Confidence-, Coverage- oder Engine-Entscheidungen.

## 3. Autoritative Datenquellen

KChess hält fachliche Wahrheit in den bereits dafür vorgesehenen Stores:

- Partie und Metadaten → Games DB
- Statistik → Statistics Service / bestehende Aggregate
- Engineanalyse → `analysis_runs` / `move_analysis`
- gelerntes Spielerprofil → `ai_chess_profiles`
- Profil-Resume/Scheduling → `ai_profile_queue`
- Evidenz-Locators → `ai_profile_evidence_registry`
- allgemeine Graphbeziehungen → `knowledge_*`-Tabellen
- semantische Chunks → Knowledge Chunk Registry
- Vektoren → Knowledge Vector Index

Es gibt keinen zweiten Profil-Engine-Cache und keinen separaten Profilgraphen mehr.

## 4. Gemeinsamer Analyse-Cache

Normale Analyse und Player Profile Maintenance benutzen denselben gespeicherten Analysebestand.

Grundregeln:

- equal-or-better gespeicherte Analyse wird wiederverwendet,
- schwächere Hintergrundarbeit darf stärkere Analyse nie überschreiben,
- Profilpflege startet keine zweite Analysepersistenz,
- stärkere spätere Analyse invalidiert nur das daraus abgeleitete Profilwissen,
- Nutzerlöschung der Analyse bleibt der autoritative Löschpfad.

Der frühere `ai_profile_probe_evidence`-Store wurde mit Migration 35 entfernt.

## 5. Neunstufige Profilpipeline

Die Profilpipeline läuft von billig nach teuer und wird nativ durch `PlayerProfileService` orchestriert.

### Stage 1 – Initial Fast Sample

Kleine diverse Stichprobe für eine erste schnelle Profilbasis.

### Stage 2 – Global Metadata / Relevance Sweep

Alle bekannten Partien werden metadata-basiert betrachtet und priorisiert. Das ist keine Engineanalyse.

### Stage 3 – Representative Historical Sample

Deterministische repräsentative historische Stichprobe mit standardmäßig maximal 500 Partien. Sie soll die reale Bibliotheksverteilung erhalten und verhindert, dass große Accounts nahezu vollständig in teure Analyse geraten.

### Stage 4 – Interesting Game Filter

Nur relevante Partien aus dem zulässigen Sample werden für tiefere Evidenz ausgewählt.

### Stage 5 – Position Candidates

Nur wenige aussagekräftige Positionen/Züge pro Partie werden als Kandidaten bestimmt.

### Stage 6 – Existing Evidence Reuse

Vor neuer Enginearbeit wird geprüft, ob bestehende KChess-Evidenz die Frage bereits beantwortet.

### Stage 7 – Fast Quality Request

Kleine Analysequalität als Promotionssignal. Produktiv wird weiterhin der gemeinsame Full-Game-Analysis-Pfad benutzt.

### Stage 8 – Verification

Nur weiterhin unsichere/wichtige Evidenz wird auf ein höheres Qualitätsniveau angehoben.

### Stage 9 – Selective Deep Analysis

Nur streng begrenzte Fälle erhalten das höchste Profilbudget.

Die Stufen 7–9 sind Qualitäts-/Promotionsstufen, keine separaten Persistenzspeicher.

## 6. Persistenter Profilfortschritt

`ai_profile_queue` ist der einzige Resume-Punkt der Profilpipeline.

Wichtige Zustände:

- `indexed` → metadata-seitig abgeschlossen,
- `done` → relevantes Profilwork für die aktuelle Source-Version aufgelöst,
- `queued` / `engine_pending` / `processing` → noch offene Arbeit.

Die UI zeigt native Telemetrie wie:

- `totalGames`,
- `indexedGames`,
- `historicalSampleGames`,
- `interestingGames`,
- `enginePromotedGames`,
- `relevantGames`,
- `resolvedRelevantGames`,
- `reusedAnalysisGames`,
- Queue-/Engine-Status und Live-Fortschritt.

Die alten `knownGames`, aktiven `processedGames`-Ausgaben sowie fast/verification/deep probe-position counters wurden in Cleanup 124 entfernt. Beim Einlesen sehr alter Profil-JSONs wird `processedGames` nur noch als Kompatibilitätsfallback für `indexedGames` akzeptiert.

## 7. Learned Chess Profile

`native/ai/profile/` besitzt ausschließlich die Lernlogik des kompakten Spielerprofils:

- Pattern Matching,
- Strengths / Weaknesses,
- Habits / Behavior,
- Trends,
- Hypothesen,
- Coach Priorities,
- Accuracy-/Strength-Schätzungen,
- Evidence Adapter,
- Sampling-/Funnel-Logik.

Dieser Layer liest nicht direkt SQLite und startet nicht selbst Stockfish.

## 8. Evidence Registry

`ai_profile_evidence_registry` ist ein payload-freier Locator-Katalog für autoritative Quellen.

Er kann z. B. auf folgende Quellen zeigen:

- Game Metadata,
- bestehende Statistics-Aggregate,
- vollständige gespeicherte Analysis-Runs,
- das gelernte Profilmodell.

Er speichert keine PGNs, Engine-Lines oder vollständigen Analysepayloads erneut.

## 9. Allgemeiner Knowledge Graph

Der einzige produktive Graph liegt unter:

```text
native/src/knowledge/
```

Er ist ein Property Graph und ausdrücklich kein Baum.

Er modelliert unter anderem:

- Player / Account / Game / Position,
- OpeningFamily / Opening / Variation,
- Transpositionen,
- Pawn-/Middlegame-/Endgame-Strukturen,
- taktische und strategische Motive,
- Resultat- und Verlustursachen,
- Strength / Weakness / Habit / Behavior,
- Trends / Hypothesen,
- Knowledge Gaps,
- Provenance und Dependencies.

Der Graph bleibt Routing-/Beziehungsschicht. Große fachliche Payloads bleiben in ihren autoritativen Stores.

## 10. Inkrementelle Aktualisierung

Neue oder geänderte Partien erzwingen keinen Gesamt-Rebuild.

Der typische Ablauf lautet:

```text
Partie / Quelle ändert sich
  -> Source-Version ändert sich
  -> abhängige Graph-Einträge werden gezielt invalidiert
  -> betroffene Projektionen werden aktualisiert
  -> betroffene Chunks werden erneuert
  -> betroffene Embeddings werden erneuert
  -> Quality / Conflicts / Knowledge Gaps werden aktualisiert
```

Provenance und Dependency Tracking liegen in `native/src/knowledge/dependency_tracker.*`.

## 11. Retrieval

Der Coach-Retrievalpfad lautet:

```text
Nutzerfrage
  -> bestehender AI QueryPlan
  -> Knowledge Query Router
  -> Entity Extraction
  -> Query Planner
  -> Hybrid Retrieval
       - Exact / Statistics
       - Graph Traversal
       - Lexical
       - Text Vector
       - Position Similarity
  -> deterministisches Hybrid Ranking
  -> Answerability Gate
  -> Evidence Packet
  -> Provider
```

Der Graph darf die ursprüngliche Query-Klassifikation nicht eigenständig ersetzen.

## 12. Current Board vs. historisches Profil

Ein geladenes Brett bedeutet nur, dass Positionskontext verfügbar ist.

Es darf eine historische/persönliche Frage nicht dominieren. Current-board-Retrieval wird nur aktiviert, wenn die klassifizierte Anfrage tatsächlich Positionskontext braucht oder ausdrücklich auf eine aktuelle Stellung verweist.

## 13. Confidence, Coverage und Freshness

Diese Größen sind getrennte native Werte:

- Confidence → Belastbarkeit einer konkreten Aussage,
- Coverage → Abdeckung eines Wissensbereichs,
- Freshness → Aktualität der Evidenz.

Eine Aussage kann z. B. hohe Confidence bei geringer Coverage haben.

Historische und aktuelle Evidenz dürfen nebeneinander existieren. Konflikte werden explizit modelliert und nicht blind überschrieben.

## 14. Knowledge Gaps und Active Learning

Knowledge Gaps entstehen z. B. bei:

- geringer Coverage,
- geringer Confidence,
- veralteter Evidenz,
- widersprüchlicher Evidenz.

Sie besitzen keine eigene Queue und starten nie direkt Stockfish.

Stattdessen liefern sie begrenzte Prioritätshinweise an die bestehende neunstufige Profilpipeline. Historische Partien müssen weiterhin durch die Stage-3-Sampling-Grenze.

## 15. Coach Evidence Packets

`KnowledgeRuntime::coach_evidence(...)` ist der einzige persönliche Coach-Retrievalpfad.

Der stabile Provider-/Validator-Vertrag bleibt:

```text
profile.context.v3
```

Evidence Packets trennen:

- Facts,
- Observations,
- Evidence,
- Uncertainties,
- optional Current Position,
- optional Recent Context,
- Source Trace.

Das LLM bekommt nur einen kleinen relevanten Ausschnitt, nicht das komplette Profil oder den gesamten Graphen.

## 16. Query Traces und Graph Inspector

Für Diagnose speichert KChess begrenzte Query-Traces mit Routing-/Retrieval-/Ranking-Metadaten.

Der Flutter Graph Inspector ist read-only und zeigt native Zustände. Er führt selbst keine Suche, Klassifikation oder Graphlogik aus.

## 17. Aktuelle Kern-Dateien

### Profil-Lernen

```text
native/ai/profile/chess_profile.*
native/ai/profile/profile_analysis_funnel.*
native/ai/profile/profile_evidence_adapter.*
native/ai/profile/pattern_matcher.*
native/ai/profile/trend_engine.*
native/ai/profile/hypothesis_manager.*
native/ai/profile/coach_priority_engine.*
```

### Profil-Orchestrierung / Persistenz

```text
native/src/services/player_profile_service.*
native/src/services/analysis_service.*
native/src/services/coach_profile_bridge.*
native/src/persistence/database.*
```

### Knowledge Graph

```text
native/src/knowledge/knowledge_runtime.*
native/src/knowledge/graph_store.*
native/src/knowledge/dependency_tracker.*
native/src/knowledge/chunk_registry.*
native/src/knowledge/*_graph_projector.*
native/src/knowledge/query_router.*
native/src/knowledge/hybrid_retrieval.*
native/src/knowledge/retrieval_ranking.*
native/src/knowledge/evidence_packet_builder.*
native/src/knowledge/knowledge_gap_engine.*
native/src/knowledge/query_trace_store.*
```

### Coach

```text
native/src/services/coach_service.*
native/ai/query_classifier.*
native/ai/query_planner.*
native/ai/evidence_retriever.*
native/ai/validation/response_validator.*
```

### Flutter

```text
flutter_app/lib/features/profile/
flutter_app/lib/features/coach/
flutter_app/lib/ffi/
```

## 18. Cleanup Update 124

Cleanup 124 entfernt nachweislich nicht mehr benötigte Teile der früheren Profilgraph-Architektur:

- `native/ai/profile/profile_graph.*`
- `native/ai/profile/profile_graph_builder.*`
- `native/ai/profile/profile_graph_retriever.*`
- `native/src/services/coach_profile_context_resolver.*`
- SQLite `ai_profile_graph_nodes`
- SQLite `ai_profile_graph_edges`
- SQLite `ai_profile_probe_evidence`
- alte Probe-Position-Telemetrie in Profil-JSON/UI
- `knownGames` und aktive `processedGames`-Ausgabe
- temporäre Repository-Artefakte `codex_changes.patch` und `codex_status.txt`

Historische Migrationen bleiben im Migrationspfad, damit ältere Datenbanken sicher bis Migration 35 aktualisiert werden können.

## 19. Nicht wieder einführen

Nicht wieder einführen:

- zweiten Profilgraphen,
- zweiten Analyse-Cache für Profilpflege,
- separaten sparse probe evidence store,
- Coach-Retrieval direkt aus SQLite in Service-/Flutter-Code,
- Flutter-seitige Confidence-/Coverage-/Routinglogik,
- unbeschränkte historische Deep Analysis,
- vollständige Profil-/Graph-Payloads im LLM-Kontext.

## 20. Zielzustand

KChess besitzt damit eine einzige konsistente Personalisierungsarchitektur:

```text
Games / Statistics / Shared Analysis / Learned Profile
                    |
                    v
              Evidence Registry
                    |
                    v
              Knowledge Runtime
         / Projection / Graph / Chunks /
        Quality / Conflicts / Retrieval /
        Ranking / Gaps / Evidence Packets
                    |
                    v
              profile.context.v3
                    |
                    v
                  Coach
```

Der Graph erklärt und routet Wissen; er ersetzt nicht dessen autoritative Quellen.
