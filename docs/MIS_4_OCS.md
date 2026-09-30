# Mis 4 Ocs y Kura — Sonic Advance 2

Esta rama parte de la beta 5 Android, commit
`f3c2fe6a1113f7768d02feb4b067992f2218dd1f`.

El selector de un jugador tiene diez entradas: los cinco personajes originales
y Elizabeth, Jude, Kiro, Yuliana y Kura. Los cinco OCs están disponibles desde el
principio. Amy conserva su condición de desbloqueo. Arriba/abajo o izquierda/derecha
giran la ruleta; A confirma y B vuelve atrás.

Los OCs conservan sus diseños del ZIP. Los nombres siguen los archivos recibidos:
Jude tiene el pelo blanco/amarillo y sudadera amarilla; Kiro lleva rizos oscuros,
sudadera verde con estrella y pantalones oscuros. Elizabeth conserva pelo plateado,
blusa blanca y shorts negros; Yuliana conserva pelo negro largo, sudadera negra
y pantalones cargo verdes.

Kura se adapta desde la nueva referencia: pelo magenta y turquesa, orejas y cola
negras con lazo, flor clara, alas pequeñas, jersey de rayas violeta/negro, falda
y botas violetas. Sus recursos se crearon sobre esta rama de beta 5.

Hay 64 dibujos por personaje: 32 poses generales, ocho vistas traseras para el
bonus y 24 poses nuevas (12 de carrera, cuatro de salto humano, cuatro de ataque
y cuatro de celebración). La carrera avanza por distancia recorrida, conserva
el apoyo de los pies y se detiene al pausar. La altura en reposo es de 36 píxeles,
o 38 para Yuliana.
Las hojas se convierten a objetos nativos 4bpp con transparencia binaria y una
paleta RGB555 por personaje. Las imágenes fuente y los atlas están en
`graphics/sa2/ocs/`; `node tools/oc_sprites/verify.mjs` verifica la conversión.

## Movimientos y habilidades

Los cinco mantienen una silueta humana al saltar; no usan la bola, el ataque
giratorio ni el spindash de Sonic. A salta, B usa la habilidad y abajo agacha.
El salto conserva el tamaño de colisión compacto del motor para atravesar
rampas y conductos, y recupera el tamaño de pie al aterrizar.
Los ataques tienen anticipación, una ventana de daño, recuperación y tiempo
de espera; mantener B no inicia ataques nuevos. Los proyectiles se detienen
contra enemigos y paredes y desaparecen al terminar su recorrido.

| Personaje | Habilidad | Celebración |
| --- | --- | --- |
| Kiro | Corte de katana con alcance propio y daño real. | Enciende el cigarro una vez, da una calada y sale humo. |
| Yuliana | Disparo de pistola con proyectil y colisión propios. | Levanta el dedo del medio hacia la cámara. |
| Jude | Movimientos de Amy con sartén amarilla; B golpea y un segundo A en el aire usa la sartén una vez por vuelo. | Celebra con la sartén y saluda. |
| Elizabeth | Pulso zombie breve; puede esquivar durante su ventana activa y daña a corta distancia. | Mira su mano asustada y muestra brevemente su lado zombie. |
| Kura | Impulso corto con golpe de palma. | Saluda con movimiento de orejas y cola. |

Jude usa la base de Amy, incluido su movimiento aéreo sin añadir un doble salto.
Los demás usan la locomoción de Sonic con sus ataques sustituidos. El progreso
y los récords usan el perfil de Amy para Jude y el de Sonic para los demás; no
hay cinco perfiles de progreso adicionales. La elección del OC se recuerda en un
archivo privado separado, sin cambiar el formato del guardado de la beta 5.
El multijugador y las escenas canónicas de Super Sonic conservan los originales.

## Correcciones de esta revisión

Durante la cuenta atrás, una animación nativa volvía a cargar la paleta de Sonic
sobre los sprites de los OCs. Ahora se respeta únicamente el fundido inicial
y después se restaura la paleta propia, también durante el parpadeo por daño
y para la cabeza del HUD.

El cierre `Task_PauseMenuUpdate+660`, dirección nula `0xc`, se corresponde con
la lectura de una tarea de agua ya destruida. La creación y destrucción del agua
limpian su estado activo, y la pausa y las interrupciones comprueban que la tarea
existe antes de acceder a su paleta.

## Compilación y pruebas

`bash android/build-apk.sh sa2` genera la versión `0.1.0-beta5-ocs2`, código 18,
optimizada y no depurable. La firma habitual se suministra mediante las variables
privadas documentadas en `android/README.md`; nunca se guarda una clave en Git.
`SA_UNSIGNED_RELEASE=1 bash android/build-apk.sh sa2` prepara un paquete sin
firma para revisar antes de firmarlo con la clave existente.

`bash android/test-ocs.sh` comprueba el selector, cancelación, bloqueo de Amy,
liberación de tareas/VRAM, multijugador, arranque y movimiento de los cinco OCs,
paletas durante la cuenta atrás, habilidades y bonus con AddressSanitizer.
También prueba la pausa con agua activa, el cambio a un acto seco, salir al
menú y volver a jugar, así como la sartén aérea de Jude una vez por salto.
La prueba de celebración coloca inicialmente al jugador cerca de la meta del
mapa y cruza el interruptor real: el motor activa la meta y su secuencia de
fin de acto. No simula el estado de victoria ni afirma completar todo el nivel.
`SA_OC_ARCH=arm bash android/test-ocs.sh` ejecuta el mismo motor ARM32 optimizado
con QEMU y SDL estático. Las capturas provienen del framebuffer del motor.
Estas pruebas no certifican el rendimiento físico del Samsung A15.
