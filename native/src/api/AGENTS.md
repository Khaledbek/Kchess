# C-ABI Agent

## Scope

`core_api.cpp` ist die Flutter/Native-Grenze.

## Token-Regel

Datei nicht komplett lesen. Exportsymbol mit `rg` suchen und nur Funktion + direkte Helper öffnen.

## Regeln

- C++-Exceptions abfangen
- C-kompatible Typen/UTF-8/opaque handles verwenden
- Speicherfreigabe eindeutig halten
- Langläufer über Job/Status/Cancel
- bestehende Exports nicht ohne Migrationsgrund entfernen
- C-ABI möglichst dünn halten; Domainlogik gehört in Services/Core
