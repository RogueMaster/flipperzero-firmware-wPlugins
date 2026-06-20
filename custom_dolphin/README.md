# Fondos custom de Miguel (mruwzum)

Animaciones/fondos propios para integrarlos en el firmware RogueMaster vía la pipeline de Jenkins.

- `MRUWZUM/` — animación de fondo propia (frame_0.bm + meta.txt), formato Flipper.
- `MRUWZUM_manifest.txt` — entrada para añadir MRUWZUM al manifest de animaciones del firmware.
- `manifest_Mruwuzum.txt` — manifiesto completo de la SD (referencia).

El pipeline SyncAndBuildRogueMaster inyecta estos assets tras clonar el tag oficial, antes de compilar, para que el firmware salga con el fondo de Miguel.
