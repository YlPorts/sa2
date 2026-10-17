# Sonic Advance 2 Android v0.2.0 OCS4 — RC3 de prueba

Esta candidata corrige la selección de personajes y retira el menú permanente de la partida. Conserva el widescreen de 426×240 que ya existía.

- Sonic, Cream, Tails, Knuckles y Amy se pueden seleccionar en el modo de un jugador desde una partida nueva. Sus retratos usan las paletas originales, sin las siluetas de bloqueo. Los desbloqueos de la historia y los niveles completados del guardado se conservan.
- Los siete OC siguen disponibles, incluidos Sans y Spider-Man. Sus atlas y habilidades se conservan.
- Se elimina el botón de tres puntos. «Ajustes» aparece al pausar con Start y desaparece al continuar. Atrás de Android también abre los ajustes.
- Nuevo ajuste «Pantalla completa · encuadre»: llena paneles anchos con escala uniforme y desplaza el HUD dentro del área visible. Recorta parte del margen vertical de la escena de 426×240; no amplía el mundo ni cambia la cámara, los eventos o las físicas. No es un nuevo motor ultrawide.
- «Escena completa · 16:9» y «Píxeles exactos» siguen disponibles. Los menús originales conservan 3:2; los jefes y paneles de proporción incompatible conservan el encuadre completo.
- Se mantiene la corrección de arranque de audio ARM64 de RC2, los bonus 3D originales, los controles editables, el guardado y la ausencia de la introducción adicional del port de PC.

Compatibilidad: Android API 23 o posterior, ARM32 y ARM64; bibliotecas de 64 bits y APK alineadas para páginas de 16 KB. La configuración de pantalla y controles se guarda entre sesiones.

Esta APK utiliza el paquete independiente `com.ylports.sonicadvance2.preview.rc3` cuando no está configurada la firma original. Se instala como «Sonic Advance 2 RC3» junto a Beta5 y las candidatas anteriores. Sus progresos privados no se transfieren automáticamente. Conserva las instalaciones anteriores.

Validación necesaria antes de publicar: pruebas nativas, selección de los cinco originales desde un guardado nuevo, comparación de atlas, audio ARM64 y ejecución de la APK en Android 36 con formato 2340×1080, ajustes desde pausa/Atrás y reanudación sin superposiciones. Los registros y las capturas de la compilación se adjuntan cuando las pruebas finalizan correctamente.

Pendiente en hardware real: Samsung Galaxy A15, GPU Mali, latencia multitáctil y sonido del dispositivo. También siguen pendientes el recorrido completo de todos los actos y jefes y la finalización natural de los siete Special Stages. Las pruebas de bonus existentes incluyen el motor real y finales mediante fixtures; no equivalen a completar los siete bonus jugando desde el principio. Esta candidata no se presenta como la versión estable final.
