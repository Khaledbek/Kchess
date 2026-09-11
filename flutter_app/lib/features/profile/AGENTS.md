# Profile UI Agent

## Start hier

- Screen: `presentation/profile_screen.dart`
- UI-Helfer: `presentation/profile_support.dart`

## Domain-Grenze

Provider-Sync, Merge, lokale Profile, Löschung und persistente Zuordnung bleiben nativ.

## Native bei Bedarf

- Profile → `native/src/services/profile_service.*`
- Sync → `native/src/services/provider_service.*`
- Provider → `native/src/providers/`

Provider-DTO-Felder nur nach projektweiter Nutzersuche entfernen; Native-JSON nicht unnötig brechen.
