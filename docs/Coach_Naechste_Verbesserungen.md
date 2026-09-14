# Nächste Coach-Verbesserungen nach Update 174

**Stand Update 175:** Die vier folgenden Arbeitsrichtungen sind als native Segmente/Cache-Grenze und privater Offline-Review-Workflow umgesetzt. Echte menschlich bewertete Dialoge fehlen noch; ohne sie gibt es keine gemessene Fehlerquote und keine begründete Modell-Tuning-Entscheidung. Die ursprünglichen Ziele und ihre verbleibenden Grenzen stehen unten.

Die Reihenfolge richtet sich nach überprüfbarem Nutzen, nicht nach der Anzahl neuer Modelle. KChess bleibt bei nativer Schach-/Profildomäne und Gemini als Sprachebene. Keine Benutzerpartien oder Gespräche kommen in Repository-Datensätze.

## 1. Vollständige Aussage-zu-Quelle-Prüfung

Der aktuelle `answer_quote`-Vertrag verbindet vorhandene typisierte Behauptungen mit einer exakten Antwortstelle. Er beweist noch nicht, dass **jede** faktische Aussage als Claim ausgegeben wurde oder dass eine grammatische Schlussfolgerung aus einem Skalar folgt. Als nächstes den Provider eine kurze Folge atomarer Antwortsegmente mit referenzierten Claim-IDs ausgeben lassen und den sichtbaren Text ausschließlich aus akzeptierten Segmenten zusammensetzen. Allgemeine Schachregeln und ausdrücklich hypothetische Vorschläge erhalten eigene, klar markierte Segmentarten. Metrik: Anteil unbelegter konkreter Aussagen in manuell geprüften Dialogen, getrennt nach Profil, Brett, Engine und Eröffnung.

## 2. Objektive Zugfolgen von statischen Merkmalen trennen

`move.contrast.v1` vergleicht bereits gespielten Zug und früheren Kandidaten anhand deterministischer Brettmerkmale. Für eine objektive Erklärung von Verlust oder Vorteil muss ein vorhandener, gleichwertiger Analysis-Cache-Eintrag für beide Linien und die relevante gegnerische Antwort vorliegen. Nur dann WDL-/Bewertungsunterschied und kritische Folge erklären; sonst beim statischen Vergleich mit benannter Grenze bleiben. Keine zusätzliche Automatic-Engine-Suche. Metrik: Anteil konkreter Zugurteile mit tatsächlich vergleichbarer Analysequalität und korrekter Ausgangsstellung.

## 3. Dialog-Evaluation über echte Fehlerfälle

Den synthetischen Katalog mit lokal geprüften echten Fällen ergänzen, aber Transkripte und Profile privat halten. Pro Fall Antwort, provider-sichtbare Evidenz, native Validierungsfehler, menschliche Bewertung und eine redigierte Musterantwort erfassen. Fälle nach Partien und Spielern trennen, um Leckage zwischen Entwicklungs- und späterer Bewertungsmenge zu verhindern. Priorität für Fehler mit falschen Fakten oder verfehlter Spielerfrage; reine Tonprobleme separat zählen. Metrik: faktische Fehlerquote, Abdeckungsquote, Feedback-Kontinuität, wahrgenommener Nutzen und Reparatur-/Provider-Aufrufe.

## 4. Lehrstil erst anhand redigierter Antworten verbessern

Aus menschlich redigierten Paaren zuerst die Unterschiede klassifizieren: fehlende Evidenz, falsche Auswahl des Lehrpunkts, zu frühe Lösung, Wiederholung oder bloß holprige Sprache. Retrieval-/Planungsfehler nativ beheben. Erst wenn ein stabiler Rest aus reinen Sprach-/Tonfehlern bleibt, ein dafür geeignetes Sprachmodell mit diesen Paaren feinabstimmen und gegen dieselbe zurückgehaltene Bewertungsmenge vergleichen. Kein Training auf ungeprüften Gemini-Antworten und kein Finetuning als Ersatz für Profil-/Engine-Wahrheit.

## Freigabekriterium für ein mögliches Sprachmodell-Tuning

Ein Tuning lohnt nur, wenn die faktische Stützung und Dialogkontinuität im zurückgehaltenen Katalog zuverlässig sind, menschlich redigierte Beispiele vorliegen und ein Vergleich gegenüber unverändertem Gemini mit gleichem Evidenzpaket klar bessere Lehrqualität zeigt. Die aktuelle Gemini-API-Architektur bleibt bis dahin unverändert.

## Umsetzungsstand und nächste Messung

1. **Segment/Claim-Bindung umgesetzt:** Gemini liefert `coach_response.v4`; C++ setzt Antwort und Frage ausschließlich aus Segmenten zusammen. Faktische Segmente brauchen genau einen belegten Claim mit identischem Textanker. Damit ist die strukturelle Abdeckung prüfbar; semantische Schlussfolgerungen bleiben Gegenstand manueller Prüfung.
2. **Objektive Zugfolge an Cache gebunden:** Für einen nachweislich gespielten Zug wird nur eine vollständige persistierte Analyse mit übereinstimmender Partie, Halbzug, UCI und FEN gelesen. Erwartete Scores/Klassifikation kommen daraus; eine kritische Antwort wird nur genannt, wenn die gespeicherte Linie mit dem gespielten Zug beginnt. Fehlt sie, bleibt `move.contrast.v1` statisch. Keine automatische Zusatzsuche.
3. **Reale Fälle vorbereitet, noch nicht erhoben:** Das Offline-Skript kann private lokale Fälle zusätzlich zum synthetischen Katalog aufnehmen, gruppiert dieselbe Partie/denselben Spieler konsistent in Entwicklung oder Held-out und fasst menschliche Urteile zusammen. Private Transkripte/Profile bleiben außerhalb des Repositorys. Die App behauptet damit noch keine gemessene Qualitätssteigerung.
4. **Stil-Tuning-Entscheidungsfilter umgesetzt, Training offen:** Edit-Kategorien trennen Evidenz-, Lehrpunkt-, Timing-, Wiederholungs- und reine Tonprobleme. Erst nach genügend echten Held-out-Fällen ohne Fakt-/Scope-Fehler und genügend geprüften Tonkorrekturen ist ein kontrollierter Vergleich sinnvoll. Ein Modell wird weder automatisch gewählt noch trainiert.

Nächster Schritt bei erlaubter Evaluation: echte lokale Dialoge mit sichtbarem Evidenzpaket und Native-Trace menschlich prüfen, häufige semantische Fehlbindungen im bestehenden Validator/Retrieval korrigieren und danach die Lehrqualität gegen unverändertes Gemini vergleichen. Dabei Quellabdeckung, reparierte Antworten und Gesprächsnutzen getrennt messen.
