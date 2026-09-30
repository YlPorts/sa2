# Mis 4 Ocs — Sonic Advance 2

Esta rama parte de la beta 5 Android, commit
`f3c2fe6a1113f7768d02feb4b067992f2218dd1f`.

El selector de un jugador tiene nueve entradas: los cinco personajes originales
y Elizabeth, Jude, Kiro y Yuliana. Los cuatro OCs están disponibles desde el
principio. Amy conserva su condición de desbloqueo. Arriba/abajo o izquierda/derecha
giran la ruleta; A confirma y B vuelve atrás.

Los OCs conservan sus diseños del ZIP. Los nombres siguen los archivos recibidos:
Jude tiene el pelo blanco/amarillo y sudadera amarilla; Kiro lleva rizos oscuros,
sudadera verde con estrella y pantalones oscuros. Elizabeth conserva pelo plateado,
blusa blanca y shorts negros; Yuliana conserva pelo negro largo, sudadera negra
y pantalones cargo verdes.

Hay 40 dibujos por personaje: 32 poses para las fases normales y ocho vistas
traseras para el bonus. La altura en reposo es de 36 píxeles, o 38 para Yuliana.
Las hojas se convierten a objetos nativos 4bpp con transparencia binaria y una
paleta RGB555 por personaje. Las imágenes fuente y los atlas están en
`graphics/sa2/ocs/`; `node tools/oc_sprites/verify.mjs` verifica la conversión.

Los cuatro usan los movimientos y colisiones de Sonic: carrera, salto, ataque
giratorio, spindash, trucos y railes. Sus entradas y dibujos son propios.
El progreso y los récords usan el perfil de Sonic del guardado existente; no hay
cuatro perfiles de progreso adicionales. La elección del OC se recuerda en un
archivo privado separado, sin cambiar el formato del guardado de la beta 5.
El multijugador y las escenas canónicas de Super Sonic conservan los originales.

## Compilación y pruebas

`bash android/build-apk.sh sa2` genera la versión `0.1.0-beta5-ocs1`, código 17,
optimizada y no depurable. La firma habitual se suministra mediante las variables
privadas documentadas en `android/README.md`; nunca se guarda una clave en Git.
`SA_UNSIGNED_RELEASE=1 bash android/build-apk.sh sa2` prepara un paquete sin
firma para revisar antes de firmarlo con la clave existente.

`bash android/test-ocs.sh` comprueba el selector, cancelación, bloqueo de Amy,
liberación de tareas/VRAM, multijugador, arranque y movimiento de los cuatro OCs,
y el bonus con AddressSanitizer. Las capturas provienen del framebuffer del motor.
Estas pruebas de escritorio no certifican el rendimiento del Samsung A15.
