# KChess Runtime Config

## Scope

`config/` contains non-secret runtime configuration that may be committed.

## Rules

- Never place API keys, passwords or tokens in this folder.
- `coach_provider.json` selects the external coach provider (`"provider"`) and holds one settings block per provider under `"providers"`: model, output/timeout bounds and conservative local quota guards. The selected id also names its key file, `secrets/<provider>_api_key.txt`.
- Keep provider configuration data-only; chess or AI domain logic belongs in native C++.
- Free-tier safety is conservative: no paid tools, no automatic retry loops, and bounded request/output limits.
- `coach_provider.json` quota fields (`rpmSoftLimit`, `rpmHardLimit`, `tpmSoftLimit`, `tpmHardLimit`, `rpdSoftLimit`, `rpdHardLimit`) reserve headroom below Google AI Studio limits for Gemini and act as local spend budgets for paid providers; Automatic Coach yields at soft limits while manual/repair calls may use the hard-limit reserve.
