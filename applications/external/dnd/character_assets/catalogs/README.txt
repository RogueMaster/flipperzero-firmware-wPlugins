Dungeons & Dolphins character catalogs

These files install under /ext/apps_assets/dndolphins/catalogs/.
Blank lines and # comments are ignored. Most catalogs use one name per line.
Subclasses use Subclass|Parent Class. Proficiencies use Type|Name|Classes, where
Type is Armor, Weapon or Tool and Classes is comma-separated, All or None.
The app streams 256-byte reads into one bounded 24-name catalog page; later
pages remain selectable. Owned Languages/Proficiencies live in scalable
languages_{id}.txt / proficiencies_{id}.txt sidecars under DNDolphins app data.

New Language opens languages.txt. New Proficiency opens proficiencies.txt in
Allowed mode; Hold OK toggles All. Open an owned entry to replace or delete it.
Class, Subclass and Feature name fields retain their custom-text Hold controls.

Inventory and Spellbook catalogs belong to their own FAP asset roots, not this
folder. See CATALOG_POLICY.md for their row schemas and filter behavior.

Catalog scope:
- Settings -> Catalog defaults to SRD and streams the selected installed catalog on demand.
- Homebrew visibility is controlled independently by Settings -> Homebrew.
- Grant metadata and starting-equipment support follow the same selected catalog scope.
- Catalog settings affect discovery and newly staged grants only; owned character data and already-applied grants are not deleted.
