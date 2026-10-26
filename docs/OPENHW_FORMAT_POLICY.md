# OpenHW authoring format policy

Each public asset type has one canonical authoring grammar. Editors, importers,
validators and runtime readers must agree on that grammar. Release documentation
describes that single grammar, not a collection of historical alternatives.

When development changes a format, update its producer, reader, fixtures and
documentation together and regenerate or explicitly migrate affected data.
Reject obsolete generated representations with an actionable error; do not
silently try older readers, alternate field layouts or filename-based guesses.
Migration is a setup/update operation, not a second runtime interpretation.

Original RM files are import inputs. Supporting their source syntax at the
import boundary does not authorize exposing multiple OpenHW authoring syntaxes.
Generated caches are internal implementation details, not alternate authoring
formats. Do not document them as another way users can configure an asset.

Existing development compatibility paths are technical debt, not precedent.
Remove them as their replacement is completed and validated. Do not claim that
this policy means every existing loader has already been migrated.
