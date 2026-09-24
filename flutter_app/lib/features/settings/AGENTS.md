# Settings UI Agent

## Start hier

`presentation/settings_screen.dart` routet zu General, Design, Analysis, Engine und Data/Storage. Gemeinsame Controls liegen in `setting_controls.dart` / `settings_section.dart`.

## Domain-Grenze

Flutter zeigt/editiert Settings. Effektive Enginewerte, Persistenz, Aliase und Rückwärtskompatibilität bleiben nativ.

## Native bei Bedarf

`native/src/services/settings_service.*`, danach nur die konkret referenzierte Settings-Registry/Core-Funktion.

Alte Settings-Aliase nicht als tot löschen, nur weil aktuelle UI sie nicht setzt.
