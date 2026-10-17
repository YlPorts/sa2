# Sonic Advance 2 Android v0.2.0 OCS4 — RC3 de prueba

Esta candidata corrige la selección de personajes y retira el menú permanente de la partida. Conserva el widescreen de 426×240 que ya existía.

- Sonic, Cream, Tails, Knuckles y Amy se pueden seleccionar en el modo de un jugador desde una partida nueva. Sus retratos usan las paletas originales, sin las siluetas de bloqueo. Los desbloqueos de la historia y los niveles completados del guardado se conservan.
- Los siete OC siguen disponibles, incluidos Sans y Spider-Man. Sus atlas y habilidades se conservan.
- Se elimina el botón de tres puntos. «Ajustes» aparece al pausar con Start y desaparece al continuar. Atrás de Android también abre los ajustes.
- Corregida la pérdida de pulsaciones táctiles breves: el renderizado podía consumir un toque antes de que lo leyera la lógica del juego. La entrada se entrega una sola vez por fotograma de lógica, sin cambiar los 60 Hz del juego.
- Nuevo ajuste «Pantalla completa · encuadre»: llena paneles anchos con escala uniforme y desplaza el HUD dentro del área visible. Recorta parte del margen vertical de la escena de 426×240; no amplía el mundo ni cambia la cámara, los eventos o las físicas. No es un nuevo motor ultrawide.
- «Escena completa · 16:9» y «Píxeles exactos» siguen disponibles. Los menús originales conservan 3:2; los jefes y paneles de proporción incompatible conservan el encuadre completo.
- Se mantiene la corrección de arranque de audio ARM64 de RC2, los bonus 3D originales, los controles editables, el guardado y la ausencia de la introducción adicional del port de PC.

Compatibilidad: Android API 23 o posterior, ARM32 y ARM64; bibliotecas de 64 bits y APK alineadas para páginas de 16 KB. La configuración de pantalla y controles se guarda entre sesiones.

Esta APK utiliza el paquete independiente `com.ylports.sonicadvance2.preview.rc3` cuando no está configurada la firma original. Se instala como «Sonic Advance 2 RC3» junto a Beta5 y las candidatas anteriores. Sus progresos privados no se transfieren automáticamente. Conserva las instalaciones anteriores.

Validación realizada en la ejecución 39072881331: ocho pruebas unitarias, controles/plataforma, 132 casos nativos, selección de los cinco originales desde un guardado nuevo y 24 casos de resultados de bonus con rutas Android. El bucle SDL real pasó 32 pulsaciones breves y dirección+salto sin perder entradas. Se verificaron los siete atlas/paletas en ARM32 y ARM64 y la ausencia de la intro del port de PC. Las instrucciones de la APK ARM64 completaron seis inicializaciones y cuatro pruebas de audio de 90 ciclos; el control negativo reprodujo el cierre exacto de RC1.

La variante x86_64 del mismo código se instaló y ejecutó en Android 36 a 2340×1080: creación de perfil original mediante controles táctiles, navegación por los cinco originales, selección de Amy, nivel, salto, ajustes desde pausa/Atrás, ocultación del botón al reanudar, recreación, suspensión y persistencia tras cierre forzado. Se revisaron las capturas de los originales, el nivel a pantalla completa y la pausa. Se adjuntan los registros, informes y capturas. Esto no certifica la jugabilidad ARM ni la salida de audio en el A15 físico.

Pendiente en hardware real: Samsung Galaxy A15, GPU Mali, latencia multitáctil y sonido del dispositivo. También siguen pendientes el recorrido completo de todos los actos y jefes y la finalización natural de los siete Special Stages. Las pruebas de bonus existentes incluyen el motor real y finales mediante fixtures; no equivalen a completar los siete bonus jugando desde el principio. Esta candidata no se presenta como la versión estable final.
