Sonic Advance 2 Android v0.2.0 OCS4 — RC2, corrección del arranque ARM64.

Esta es una **prerelease de prueba**. Conserva los siete personajes y todas las
correcciones de RC1; no se publica como versión estable.

Corregido el fallo de inicialización del audio reportado en Galaxy A15 / Android
16. El build ID `327d17e6d670bd37ca654530034a333a52a84977` confirma que el
informe corresponde exactamente al ARM64 publicado en RC1. El código se detiene
en `m4aSoundInit+572`, PC `0x6a4340`, al escribir en `0x20` a través de un puntero
nulo. La tabla de reproductores tenía entradas separadas por 256 bytes, mientras
el código C esperaba 24: `.align 8` significa algo distinto en el ensamblador ARM.
Se usa alineación explícita en bytes para que tablas y estructuras C coincidan.
La misma corrección se aplica a canciones, instrumentos y demás recursos nativos.

La verificación de la APK inspecciona ahora las tablas de audio realmente
empaquetadas en cada ABI: cuatro reproductores, 507 canciones, 807 referencias
de pistas y 2.364 instrumentos. Además, Actions ejecuta las instrucciones ARM64
de inicialización y mezcla en Unicorn, incluyendo una prueba negativa de la
APK RC1 que debe reproducir exactamente el fallo reportado. Esta prueba de CPU
no representa la salida de audio Android ni el rendimiento físico del A15.

Se mantienen las pruebas de personajes, bonus, controles y guardado, así como
la instalación y recorrido real de la APK x86_64 en el emulador Android 16.
Los informes, hashes, símbolos y capturas se adjuntan a la publicación.

Validación aprobada en el commit `3a6024d880c0782a3bc862d4c8512cf3376451e3`:
[Actions 38992392672](https://github.com/YlPorts/sa2/actions/runs/38992392672).
Pasaron compilación, regresiones e instalación Android 16. La prueba ARM64
completó seis inicializaciones y 360 ciclos de mezcla de intro, nivel, bonus y
anillos, con muestras válidas y avance del secuenciador. También pasaron ocho
pruebas unitarias, 132 escenarios del motor y 24 finales de bonus con rutas
Android. Estos finales usan fixtures; sigue pendiente completar los bonus jugando.

La APK x86_64 instalada en Android 16 completó recreación de actividad, ajustes,
arrastre/guardado del control A, suspensión/reanudación, perfil original,
entrada al nivel por controles táctiles y recarga del guardado tras forzar el
cierre. Se revisaron las capturas de arranque, editor y partida.

La APK ARM32/ARM64 publicada conserva exactamente el artefacto verificado:
33.898.548 bytes, versionCode 22, SHA-256
`ebe60c8d1e49bcead4a2feffa9df96fc9dbd2d216517ba23790b83faadac7295`.

Si falta la firma de producción, el paquete es
`com.ylports.sonicadvance2.preview.rc2` y la aplicación aparece como
**Sonic Advance 2 RC2**. Puede instalarse junto a RC1 y Beta 5 sin desinstalarlas
ni modificar sus partidas privadas. No importa automáticamente su progreso.
La clave temporal de RC1 no está disponible para firmar una actualización suya.

Pendiente: confirmar el arranque y audio en el Galaxy A15 físico, comprobar
ARM32/ARM64 durante partidas prolongadas, completar manualmente los bonus y
revisar todos los actos/jefes, mandos físicos y ultrawide. La firma original
sigue siendo necesaria para actualizar el paquete de Beta 5.
