# Coach, Spielerwissen und Knowledge Graph – Update 135

Stand: 13. September 2026. Dieser Bericht beschreibt die Änderungen dieser Überarbeitung, nicht sämtliche bereits vorher vorhandenen Änderungen im Arbeitsverzeichnis.

## Ausgangspunkt

Der allgemeine Knowledge Graph und die adaptive Profilvorbereitung waren bereits vorhanden. Beim Nachverfolgen des Datenflusses zeigten sich vor allem Integrationsprobleme: Statistik und gemeinsames Spielerprofil konnten verschiedene Account-Umfänge verwenden; ein noch nicht fertig aufgebauter Graph konnte vorhandene Metadaten verdecken; dieselbe Aussage konnte als Knoten und Chunk mehrfach im Kontext landen. Außerdem entsprach das bisherige Textbudget nicht dem tatsächlich weitergereichten JSON-Kontext.

## Was geändert wurde und warum

### Gemeinsame, sofort nutzbare Spielerstatistik

`StatisticsService::player_knowledge_json` liefert ein natives, lesendes Modell aus den vorhandenen Games-Metadaten und dem gemeinsamen Analyse-Cache. Es verwendet den bestehenden gemeinsamen Spielerumfang der Online-Accounts. Lokale Profile behalten ihren eigenen Umfang.

Verfügbar sind Ergebniszahlen, Zeitkontrollen und Farben, Eröffnungen mit ECO und Familie, Partiebeendigungen, gespeicherte Analysekennzahlen nach Spielphase sowie zuletzt gespeicherte Partie-Ratings je Account und Zeitkontrolle. Nenner, Zeitbezug und Analyseabdeckung bleiben sichtbar. Dazu werden keine PGNs geladen und keine Engine-Analysen gestartet.

Der Coach kann diese Fakten auch während der Profilvorbereitung direkt abrufen. Wenn der Graph gerade beschäftigt ist, bleibt dieser Statistikpfad verfügbar. Eine laufende Vorbereitung bedeutet damit nicht pauschal, dass kein Spielerwissen vorliegt. Noch nicht importierte Partien bleiben selbstverständlich außerhalb des bekannten Datenbestands.

Ratings sind ausdrücklich zuletzt gespeicherte Partie-Ratings: keine live abgefragten Kontostände, keine FIDE-Elo und kein historischer Durchschnitt, der als aktuelle Elo ausgegeben wird.

### Verbindungen im bestehenden Graphen

Die Statistikprojektion verwendet dasselbe Lesemodell wie der Coach. Statistik-Knoten werden mit Spieler, Eröffnung, Eröffnungsfamilie, Zeitkontrolle, Farbe und Partiebeendigung verbunden. Rating-Fakten erhalten Account- und Provider-Bezüge. Bereits vorhandene gemeinsame Eröffnungs- und Kontextknoten werden wiederverwendet, ohne ihre reicheren Eigenschaften zu überschreiben.

Identitäten und Quellenversionen bleiben deterministisch. Der Graph bleibt eine Schicht für Beziehungen und Quellenverweise; vollständige Partien und Analysen werden nicht zusätzlich darin gespeichert. Der gelöschte alte Profilgraph und der alte Context Resolver wurden nicht wieder eingeführt.

### Passende Fakten statt pauschalem Profiltext

Ein gemeinsamer nativer Filter begrenzt Kandidaten aus Statistik, Graph, lexikalischer und semantischer Suche auf den angefragten Umfang. Zeitkontrolle, Farbe, Spielphase und Eröffnungsbezug werden zusammen berücksichtigt. Mehrere Werte derselben Achse sind Alternativen; unterschiedliche Achsen müssen gemeinsam passen. Eine ausdrücklich gespeicherte Spielphase hat Vorrang vor Worttreffern.

Das Ranking berücksichtigt die gesuchte Statistik: beispielsweise Häufigkeit oder niedrige Ergebnisquote. Kleine Stichproben werden bei solchen Rangfolgen zurückgestellt. Eine niedrige Ergebnisquote bleibt eine beobachtete Assoziation, kein automatischer Nachweis ihrer Ursache.

### Kompakter, belegbarer LLM-Kontext

Knoten und zugehörige Chunks werden anhand ihrer Graph-Identität dedupliziert. Strukturierte Fakten bleiben vollständig; doppelte Textzusammenfassungen und unnötige Metadaten entfallen. Als ungültig markierte Quellen werden vor der Paketbildung ausgespart.

Das Budget berücksichtigt Eigenschaften und JSON-Hülle. Es wächst bei Vergleichen und Rating-Listen bedarfsabhängig bis zur vorgesehenen Obergrenze von 4.000 geschätzten Tokens für das persönliche Faktenpaket. Das ist weder ein Füllziel noch eine Garantie für die Tokenzahl des gesamten Provider-Requests: Systemanweisungen, Gespräch und gegebenenfalls Brettkontext kommen hinzu.

Pflichtbeleggruppen reservieren den gefragten Themen- und Vergleichsumfang. Die letzte Eingabekürzung prüft erneut, ob diese Belege erhalten geblieben sind. Interne Gruppen-IDs werden anschließend entfernt und nicht an das LLM geschickt. Fehlt eine erforderliche Gruppe, wird der Kontext als unvollständig gekennzeichnet.

Beziehungsbelege benötigen tatsächliche, quellengebundene Graphkanten und mitgelieferte Endpunkte. Ein bloßer Traversierungstreffer genügt nicht. Auch eine vorhandene Kante ist für sich kein Kausalitätsnachweis.

### Weniger Nebenarbeit bei einer Coach-Frage

Eine Coach-Frage startet keine umfangreiche persistierende Embedding-Aufbereitung mehr. Vorhandene Vektoren bleiben nutzbar; höchstens acht bereits eingegrenzte aktuelle Chunks können ergänzend im Speicher eingebettet werden. Das baut nicht den gesamten Vektorindex auf.

Query-Traces sind optionale Diagnoseinformationen. Ihr Schreibzugriff wartet nicht hinter priorisierter Vordergrundanalyse, und ein Fehler beim Trace-Schreiben darf die bereits ermittelte Antwortgrundlage nicht verwerfen. Autoritative Analyse- und Profildaten behalten ihre bestehenden Schreibverträge.

## Welche Fragen der Datenfluss abdeckt

Diese Beispiele wurden anhand der Quellen und Verträge nachvollzogen; sie sind keine ausgeführten Tests:

| Frage | Vorgesehene Grundlage und Grenze |
| --- | --- |
| Welche Elo habe ich? | Zuletzt gespeichertes Partie-Rating getrennt nach Account und Zeitkontrolle, mit Datum. |
| Welche Eröffnungen spiele ich am häufigsten? | Bekannte Eröffnungsklassifikationen und Partieanzahlen mit Bezugsgröße. |
| Mit welchen Eröffnungen schneide ich schlecht ab? | Ergebnisquote, Stichprobengröße und vorhandene Analyseabdeckung; kein unbelegter Ursachenbefund. |
| Wie unterscheiden sich Blitz und Rapid mit Schwarz? | Explizit gefilterte Vergleichszellen; fehlende Zellen werden als Lücke behandelt. |
| Wo liegen meine Schwächen oder typischen Muster? | Vorhandene gelernte Beobachtungen und Analysebelege, ergänzt um passende Statistik. |
| Bin ich in Turmendspielen schwach? | Benötigt echte Materialtyp-Evidenz. Eine späte Spielphase allein reicht ausdrücklich nicht. |
| Was hat meine aktuelle Stellung mit meinem Spielstil zu tun? | Gemischte Anfrage benötigt Brett- und passende persönliche Evidenz. Ein geöffnetes Brett allein dominiert keine historische Frage. |

Allgemeines Schachwissen bleibt Aufgabe des bestehenden Coach-Pfads; persönliche Aussagen benötigen die mitgelieferten Daten. Es wurden keine fertigen Antworten für einzelne Frageformulierungen eingebaut.

## Grenzen und Prüfung

Die vorhandenen Phasenzähler sind nach Zugbereichen definiert: Eröffnung 1–12, Mittelspiel 13–30, späte Phase ab 31. Diese Definition wird mitgeschickt und darf nicht als Materialklassifikation ausgelegt werden. Zeitliche Verbesserung erfordert Trend-Evidenz; aktuelle Daten oder hohe Abdeckung allein beweisen keine Verbesserung. Größere, komplexe Vergleiche können die begrenzte Kontextkapazität überschreiten und müssen dann als unvollständig behandelt werden.

Flutter wurde in dieser Überarbeitung nicht um Fachlogik erweitert. Es kamen keine neuen sichtbaren UI-Texte hinzu, daher waren keine ARB-Änderungen erforderlich. Frühere bereits vorhandene UI-Änderungen bleiben bestehen. Die relevanten `AGENTS.md`-Dateien dokumentieren Update 135 und die Zuständigkeiten.

Durchgeführt wurden Quelltextdurchsicht, Prüfung der Aufrufer und Datenverträge sowie `git diff --check` für die betroffenen Änderungen. Auf ausdrücklichen Wunsch wurden keine Tests, Builds, App-Runs, `flutter analyze`, Commits oder Pushes ausgeführt. Kompilierbarkeit, tatsächliche Provider-Antworten, Laufzeitverhalten und Tokenverbrauch sind daher noch nicht praktisch verifiziert.
