Sonic Advance 2 Android v0.2.0 OCS4 — RC1 de prueba.

Esta publicación es una **prerelease**. No es la v0.2.0 estable solicitada.

Conserva los siete personajes: Elizabeth, Jude, Kiro, Yuliana, Kura, Sans y
Spider-Man, junto con Sonic, Cream, Tails, Knuckles y Amy.

Cambios de esta candidata:

- Eliminada la introducción de créditos del port de PC en Android. Se conservan
  la presentación original y las atribuciones del proyecto.
- Selección OC con respiración, parpadeo y animación de confirmación reales.
- Victoria animada de los siete OC en resultados del bonus, con sus propios
  fotogramas existentes. Título y presentación original en encuadre 240×160.
- Frenada OC sin reinicios bruscos y corrección de la búsqueda de perspectiva
  de los Special Stages. Se conserva el motor 3D del juego.
- Controles editables y persistentes; corrección de toques rápidos y contactos
  cancelados; cruceta, salto y habilidad simultáneos.
- Ajustes Android accesibles desde el juego: escala, filtro, controles, audio,
  presentación y diagnóstico opcional.
- Proporción correcta, píxeles enteros y ausencia de estiramiento de sprites.
- APK para ARM32/ARM64. Código y compilación reproducible en la rama
  `android-v0.2.0-ocs4`.
- Guardado original del motor habilitado y corregido; checksum compatible con
  palabras de 32 bits también en ARM64, sectores de respaldo y límites seguros.
- Cierres nativos por alineación corregidos, limpieza y reinicio de SDL al
  recrear la actividad y empaquetado de bibliotecas de 64 bits para páginas de 16 KB.

La compilación verifica los siete atlas y paletas en los archivos nativos
empaquetados, la ausencia de la intro adicional y la firma. Actions conserva los
registros de pruebas nativas y las capturas del emulador Android 36.
La batería nativa contiene 132 marcadores, incluidos 36 casos con fixtures de
final de bonus, esmeralda/resultado, recarga del guardado y regreso al nivel.
Las fixtures no sustituyen la prueba pendiente de completar los bonus jugando.

Si el paquete termina en `.preview`, se instala **junto a la Beta 5** con una
firma de pruebas. No actualiza ni importa automáticamente su progreso. Conserva
la Beta 5 instalada; la firma de producción original sigue siendo necesaria
para actualizarla con el mismo identificador.

Pendiente: pruebas físicas en Galaxy A15, revisión completa de todos los actos
y jefes, completar los siete bonus con sus esmeraldas/progreso, mandos físicos
y ampliación real ultrawide. No se garantizan 60 FPS en el teléfono sin medirlo.

El estado detallado, los límites de cada prueba y la configuración de firma
figuran en `docs/ANDROID_V020_STATUS.md`.
