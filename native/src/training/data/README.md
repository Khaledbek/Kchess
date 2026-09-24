# Eingebettete Trainingsdaten

## Opening-Katalog

Die Dateien `openings_a.inc` bis `openings_e.inc` enthalten die Datenzeilen des im Repository dokumentierten Lichess-`chess-openings`-Snapshots. Grundlage sind die fünf TSV-Dateien unter `tools/opening_names/chess-openings/`.

Die Reihenfolge und daraus abgeleitete IDs sind Teil des persistierten Trainingsvertrags. Eine Änderung der Reihenfolge oder ID-Zuordnung benötigt eine passende Migration vorhandener `opening_<id>`-Fortschritte.

Lizenz-, Commit- und Prüfsummeninformationen stehen in:

- `THIRD_PARTY_NOTICES.md`
- `tools/opening_names/BUILD_METADATA.md`

## Endspielstudien

`studies.inc` enthält den eingebetteten Studienkatalog. Die Quelldaten nennen:

- Titel: `Chess Studies, Or, Endings of Games`
- Autoren: Josef Kling und Bernhard Horwitz
- Jahr: 1851
- Rechte: Public Domain

Sichtbare UI-Titel, Hinweise und Beschreibungen werden über ARB-Schlüssel lokalisiert. Die Stellungen, IDs, Schwierigkeitswerte und Quellenmetadaten bleiben native Inhaltsdaten.

## Runtime-Verantwortung

Die eingebetteten Daten werden von der nativen Trainingspipeline ausgewertet. Lösungsprüfung, Zuglegalität, Verteidiger-Jobs und Fortschritt liegen in C++; Flutter besitzt keine parallele Trainings-Schachlogik.

## Dateischnitt

Die `.inc`-Dateien sind Inhaltsdaten, kein handgeschriebener Algorithmus. Die Opening-Aufteilung folgt den ECO-Gruppen A bis E; Studien bleiben in einem separaten Katalog. Vendorte/erzeugte Inhaltszeilen werden nicht nur für Stilregeln umformatiert.


## Training-Arena role

`openings_a.inc` bis `openings_e.inc` sind seit der KCL-Migration **kein
Fortsetzungs-/Antwortbuch** mehr. Sie bleiben als stabile Katalog-/ID-/Hierarchie-
und Setup-Quelle erhalten, damit bestehende `opening_<id>`-Fortschritte und die
UI-Auswahl kompatibel bleiben.

KCL1 speichert pro sortiertem Terminal-Positionsschluessel eine komplette
Zugfolge von der Standardstellung bis zu dieser Position. Diese gespeicherten
Move-Ranges sind **keine ausgehenden Kanten**. Native `opening_line_graph.*`
replayt die Linien beim Laden und leitet daraus einen positionsbasierten Graphen
ab, in dem gemeinsame Praefixe und Transpositionen zusammenlaufen.

Sobald ein Opening-Drill an seiner benannten Startstellung angekommen ist, ist
dieser abgeleitete KCL-Graph die einzige erlaubte Topologie fuer weitere Zuege.
`opening_book.kcb` gewichtet die abgeleiteten KCL-Kanten statistisch, und
`opening_names.kco` benennt erreichte Positionen. Der fruehere PolyGlot-Fallback
ist fuer die produktive Training-Arena entfernt.
