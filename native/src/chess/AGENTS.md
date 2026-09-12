# Chess Core Agent

## Scope

Schachmodell, Legalität, Züge, PGN, SAN und FEN.

## Kontext sparen

Bei Parser-/Legalitätsproblemen nur betroffene Parser-/Position-/Move-Dateien und deren direkte Call-Sites öffnen. Keine Analyse-/Engine-Dateien laden, wenn das Problem rein notationell ist.

## Regeln

Dieser Bereich ist autoritative Schachwahrheit. Flutter darf Ergebnisse nur konsumieren. Änderungen an SAN/FEN/PGN auf Import, Export, Training und Analyse-Positionen auf Seiteneffekte prüfen.
