# KChess Coach – interaktiver Trainer

## Ziel

Der Coach führt ein Schachtraining: Er geht auf die Idee des Spielers ein, erklärt einen überschaubaren Punkt und gibt bei Bedarf eine konkrete Denkaufgabe. Er darf freundlich und natürlich sprechen, gibt sich aber nicht als Mensch aus. Eine direkte Frage erhält zuerst eine direkte Antwort. Nicht jede Antwort muss mit einer Gegenfrage enden.

Die vorhandenen Systeme bleiben maßgeblich: C++ und Stockfish liefern Schachfakten; Gemini formuliert; Flutter präsentiert und sammelt Eingaben. Python wird nicht als App-Runtime benötigt. API-Schlüssel bleiben ausschließlich in der ignorierten Secret-Datei.

## Gespräch und Lernschritte

Ein sinnvoller Ablauf ist **beobachten → vermuten → ausprobieren → Rückmeldung → vergleichen**. Der Coach fragt beispielsweise nach einer ungedeckten Figur, einer gegnerischen Drohung oder dem Ziel eines Plans. Er wartet auf den Versuch und soll anschließend darauf eingehen, statt sofort eine neue Aufgabe zu stellen.

- Kurze Antworten behandeln standardmäßig eine Idee in zwei bis vier Sätzen.
- Eine optionale Trainerfrage ist ein eigenes strukturiertes Feld und wird im Chat hervorgehoben.
- „Stell mir eine Aufgabe“ startet eine Frage zur Stellung. Der Quiz-Modus gibt keine Empfehlungs-Pfeile aus.
- „Züge vergleichen“ verwendet vorhandene native Kandidaten oder eine begrenzte Engine-Analyse. Fehlende Kandidaten oder Bewertungen bleiben unbekannt.
- Hinweise geben zunächst die Figur, dann Zielfeld und Pfeil und schließlich eine Erklärung preis. Der erste Schritt zeigt keinen verräterischen Bestzugpfeil.
- Fehler werden konkret und respektvoll besprochen. Lob bezieht sich auf belegte Ideen und vorhandene Zugklassifikationen.

## Chat und Brett als gemeinsame Oberfläche

Normale Fragen, Antworten und Hinweise erscheinen als offene Textabschnitte ohne Nachrichtenblase oder Rahmen. Nur relevante Zugereignisse bekommen eine farbige Ereigniskarte: Blunder rot, Bestzug grün, Brilliant blau. Beschriftung und Symbol ergänzen die Farbe.

Diese Farben beruhen ausschließlich auf der nativen Klassifikation eines Ereignisses. Die Oberfläche sucht niemals im KI-Text nach Wörtern wie „brilliant“. Ein normaler Bestzug löst nicht automatisch einen zusätzlichen API-Aufruf aus; Bestzug-Karten erscheinen, wenn der native Trainer diesen Moment aus anderen Gründen bereits aufgreift.

Geprüfte Figurenfelder können leuchten. Geprüfte Empfehlungen und bewertete Vergleichszüge können als Pfeile dargestellt werden. „Auf dem Brett zeigen“ stellt diese Markierungen erneut dar, solange die zugehörige FEN sichtbar ist. Die Aktion führt keinen Zug aus. Frühere Stellungsmarkierungen verschwinden beim Navigieren; manuelle Varianten bleiben flüchtig und überschreiben keine Partie.

## Selbstständige Trainerimpulse

Die bestehende native Ereignisprüfung bleibt der einzige Auslöser. Sie berücksichtigt Fehler, verpasste Taktik, relevante Motiv-/Phasenwechsel, persönliche Wiederholungen sowie jetzt auch Brilliant-Züge. Alltägliche Züge erzeugen weiterhin keinen automatischen Gemini-Aufruf.

Das gilt für gespeicherte Partien und für freie Brettvarianten mit vorhandener nativer Analyse. Bei einer Variante werden Job-ID, ausgeführter Zug und die Beziehung zwischen vorheriger und aktueller FEN nativ geprüft. Flutter übermittelt keine selbst berechnete Klassifikation.

Vor einem automatischen Impuls bleibt die Stellung mindestens drei Sekunden sichtbar. Manuelle Fragen und laufende Hinweise haben Vorrang. Veraltete Antworten werden nicht an die neue Stellung angehängt. Ein automatischer Coach-Aufruf startet keine frische Stockfish-Suche.

## Gesprächscache und Analysewiederverwendung

Das native Gedächtnis speichert keinen vollständigen Chatverlauf. Es hält begrenzte Felder für Thema, letztes Anliegen, letzte Antwort, Empfehlung und offene Trainerfrage. Maximal 64 Sitzungen bleiben im Speicher; die am längsten nicht aktualisierte Sitzung wird bei Bedarf ersetzt. Nur erfolgreiche, validierte Antworten werden übernommen.

Bei einem Brettwechsel verfallen die alten stellungsbezogenen Aussagen und Empfehlungen. Eine offene Trainerfrage behält die ursprüngliche FEN als Gesprächsbezug, damit der Coach auf einen ausgespielten Versuch eingehen kann. Dieser Gesprächsbezug ist ausdrücklich kein Beleg für die neue Stellung.

Engine-Kandidaten werden zunächst aus dem letzten nativen Hint-Cache beziehungsweise vorhandener Partie-/Variantenanalyse übernommen. Der Hint-Cache ist auf eine Stellung begrenzt und an FEN sowie Engine-/Hint-Einstellungen gebunden. Er versorgt auch die Coach-Erklärung und den Vergleich. Erst wenn nötig darf eine manuelle Frage den vorhandenen begrenzten Hint-Analysepfad nutzen. Kein zusätzliches Engine-Backend und kein kostenpflichtiger Gemini-Cache werden eingeführt.

## Halluzinationen begrenzen

Die bisherige Gemini-Anbindung lieferte nur `answer`; dadurch blieben die vorhandenen Prüfungen für strukturierte Aussagen weitgehend ungenutzt. Sie erhält jetzt Antwort, Trainerfrage, typisierte Aussagen, Empfehlungen und Evidenzreferenzen über das strukturierte JSON-Schema.

Die native Prüfung kontrolliert unter anderem:

- existierende Evidenz-IDs aus der aktuellen Anfrage;
- Legalität, Schach, Matt, Material und Figurenfelder über vorhandene Schachfunktionen;
- die konkrete Kandidaten-UCI statt eines beliebigen Engine-Verweises;
- den exakten numerischen Kandidatenwert bei Engine-Bewertungsbehauptungen;
- konkrete ECO-/Namenswerte bei strukturierten Eröffnungsbehauptungen;
- die Qualität einer empfohlenen Alternative: ein beliebiger legaler oder schlechter MultiPV-Zug genügt nicht.

Bei Ablehnung ist höchstens ein Korrekturversuch möglich. Er enthält die beanstandete strukturierte Antwort und die Prüfhinweise. Nicht akzeptierte Inhalte werden nicht als gültige Coach-Antwort angezeigt oder im Gesprächsgedächtnis gespeichert. Kandidatendaten haben beim Kürzen des Provider-Kontexts Vorrang.

**Grenze:** Freie natürliche Sprache und allgemeine Schachprinzipien sind dadurch nicht mathematisch bewiesen. Das Modell kann Sachverhalte unvollständig strukturieren oder irreführend formulieren. Halluzinationen werden eingegrenzt; vollständige Halluzinationsfreiheit wird nicht versprochen. Heuristische Motive bleiben Möglichkeiten, solange ihre Folgen nicht belegt sind.

## Bewusste Grenzen dieser Änderung

Keine neue dauerhafte Lernprofil-Datenbank, kein zweiter Router und keine Python-Runtime. Bestehende Profil-/Practicality-Systeme bleiben erhalten. Keine automatischen Lösungszüge auf dem Brett, keine erfundenen Engine-Werte, kein Lob allein aufgrund einer KI-Behauptung. Die aktuelle Oberfläche vergleicht verfügbare Engine-Kandidaten; ein eigener Editor zur Auswahl beliebiger zweier Züge ist nicht enthalten.

Deutsch, Englisch und Arabisch verwenden gemeinsame ARB-Schlüssel. Die Dart-Sprachklassen werden mit dem vorhandenen Flutter-Lokalisierungsgenerator erzeugt.

Validierung dieser Änderung: C++-Syntaxprüfung mit `/Zs`, Dart-Parser/Formatierung und ARB-Vertragsprüfung. Kein App-Build, kein Testlauf, kein App-Start, kein Commit und kein Push. Das Verhalten mit Gemini und die visuelle Darstellung müssen beim späteren App-Start geprüft werden.
