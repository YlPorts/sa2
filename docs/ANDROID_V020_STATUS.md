# Android v0.2.0 OCS4 — candidata RC3

Esta es una candidata de prueba, no la versión estable v0.2.0.

## Selección, pantalla y pausa en RC3

Se conserva el widescreen existente de 426×240. «Pantalla completa · encuadre»
llena paneles anchos mediante escala uniforme y recorte vertical limitado: no
amplía el mundo, altera las físicas ni cambia el disparo de eventos. El HUD
se desplaza hacia el área visible. En 2340×1080 el margen seguro es de 22 píxeles
del framebuffer. Los menús conservan 240×160; jefes y proporciones incompatibles
usan el encuadre completo. Siguen disponibles 16:9 completo y píxeles enteros.

Los retratos oscuros de Cream, Tails, Knuckles y Amy correspondían al bloqueo
de personajes de una partida nueva. Android permite ahora seleccionar los
cinco originales sin modificar los bits de desbloqueo ni inventar progreso.
Se conservan sus sprites y paletas originales y los siete personajes OC.

Se retira el botón permanente de tres puntos. «Ajustes» aparece solamente en
la pausa nativa y se oculta al continuar; Atrás de Android también abre los
ajustes. El cambio se comunica desde el juego al hilo de interfaz de Android.
La prueba exige tanto localizar y pulsar el botón visible como comprobar su
visibilidad real después de reanudar. Una SurfaceView de SDL sin controles Java
visibles puede no exponer una raíz de accesibilidad en Android 36; se verifican
entonces el estado real de la vista, la ventana enfocada y el proceso activo.
La ausencia del símbolo de tres puntos se comprueba en el DEX de la APK.

Se reprodujo además una pérdida real de pulsaciones breves en el bucle SDL:
la presentación consumía el contacto pendiente y otra lectura sobrescribía
`REG_KEYINPUT` antes de `GetInput`. El backend Android muestrea ahora una sola
vez, al devolver el control al siguiente fotograma de lógica. El harness usa
eventos SDL DOWN/UP reales: la rutina anterior falla en la primera pulsación
de A (`expected=1 pressed=0 input=0`), mientras la corregida entrega 32 toques
de A/B/Start/derecha, dirección+salto en el mismo tick y liberaciones limpias.
La prueba forma parte del job Android nativo y no se empaqueta en la APK.

La APK sin firma original utiliza `com.ylports.sonicadvance2.preview.rc3`,
versionCode 23, `0.2.0-rc3-OCS4`. Conserva instaladas las versiones anteriores;
sus datos privados no se importan automáticamente.

La publicación exige aprobar compilación, regresiones y recorrido Android 36
del mismo commit. El publicador RC3 verifica el manifiesto, el SHA-256 y los
resultados antes de publicar exactamente el artefacto probado, sin recompilar.

### Verificación de RC3

La ejecución [39072881331](https://github.com/YlPorts/sa2/actions/runs/39072881331)
aprobó `apk`, `regressions` y `android-emulator` sobre el commit
`638d02dd6849933769ca20aeafb3974d0d6877c4`. Pasaron ocho pruebas unitarias,
controles/plataforma, 132 casos nativos, cinco selecciones originales con
guardado nuevo y 24 casos de resultados de bonus mediante fixtures Android.
El bucle SDL real retuvo 32 pulsaciones breves y dirección+salto, comprobando
también sus liberaciones. El arranque nativo alcanzó 1.200 frames de nivel y
el motor se cerró/reinició dos veces en el mismo proceso.

La variante x86_64 instalada en Android 36 completó perfil original, navegación
por los cinco originales, confirmación de Amy, entrada al nivel, movimiento y
salto. Las capturas revisadas muestran los retratos con sus colores, la partida
a 2340×1080 sin barras laterales y con HUD visible, y el botón Ajustes presente
en pausa y ausente al reanudar. También pasaron recreación de actividad, editor,
suspensión, ajustes persistentes y recarga del perfil después de cierre forzado.

Las instrucciones de la APK ARM64 aprobaron seis inicializaciones y cuatro
pruebas de 90 ciclos de mezcla (360 en total), además del control negativo de
RC1 en `0x6a4340`/`0x20`. Ambas ABI conservan los siete atlas y paletas
verificados contra el código. La firma y la alineación ZIP/ELF se verificaron.

APK ARM32/ARM64: 33.901.165 bytes; SHA-256
`ff2e6d2678b62311e36a061ab90544cc6c30158e695a4da555673a0d7a0b50da`.
Biblioteca ARM64: SHA-256
`81a76e4d4adfbf6a4f848189e9138bcd02768868fc2a144ad5297adbd47863d2`.
La jugabilidad ARM, GPU Mali y salida de audio en el A15 físico siguen pendientes.

## Cierre ARM64 de RC1 y corrección RC2

El informe físico del Galaxy A15 identifica el build
`327d17e6d670bd37ca654530034a333a52a84977`, idéntico al ARM64 publicado en RC1.
Se reprodujo con las instrucciones reales de esa APK en Unicorn: escritura nula
en `0x20`, PC `0x6a4340`, `m4aSoundInit+572`, línea 100 de `src/lib/m4a/m4a.c`.
El segundo reproductor se lee a 24 bytes del primero, pero `mAlignWord` usaba
`.align 8`, que en ARM64 añade alineación a 256 bytes. El espacio intermedio
contiene cero. En x86_64 `.align 8` significa ocho bytes y por eso la prueba de
Android x86 no detectaba este fallo. Se sustituye por `.balign 8`/`.balign 4`,
con significado en bytes consistente y el mismo formato GBA/ARM32 anterior.

`android/native_audio_check.py` inspecciona los punteros reubicados de las
bibliotecas realmente empaquetadas. Exige las cuatro entradas de reproductor,
507 canciones, 807 referencias de pistas y 2.364 instrumentos en cada ABI.
El comprobador rechaza el ARM64 original de RC1 y acepta sus ARM32/x86_64.

`android/test-arm64-audio.py` ejecuta las instrucciones NDK reales en Unicorn;
solo proporciona las operaciones de memoria importadas de libc. El workflow
exige reproducir el fallo exacto en la APK RC1 de hash conocido y después
inicializar/mezclar con la nueva APK ARM64. No se usa una recompilación x86
para esta prueba ni se incluye el harness en la APK. Esto no prueba la salida
Android de audio, la GPU Mali ni el rendimiento en el teléfono físico.

La clave temporal CI de RC1 no está disponible. Sin la firma original, RC2 usa
`com.ylports.sonicadvance2.preview.rc2` y la etiqueta "Sonic Advance 2 RC2" para
instalarse junto a RC1 y Beta 5, conservando sus partidas privadas. No requiere
desinstalar ninguna aplicación anterior ni importa automáticamente su progreso.
RC1 sigue conservada como versión anterior con un cierre ARM64 conocido.

### Verificación de RC2

La ejecución [38992392672](https://github.com/YlPorts/sa2/actions/runs/38992392672)
aprobó `apk`, `regressions` y `android-emulator` sobre el commit
`3a6024d880c0782a3bc862d4c8512cf3376451e3`. Se publica el mismo artefacto con
`sa2-publish-verified-rc2.yml`, que exige esos resultados y el hash de la APK,
sin recompilar ni cambiar su firma temporal al publicarla.

La CPU ARM64 ejecutó seis inicializaciones y cuatro casos de 90 ciclos de
mezcla: intro (reloj 78), Leaf Forest (92), Special Stage (92) y anillos (50).
Cada caso verifica la canción seleccionada, avance del secuenciador y muestras
PCM/PSG válidas después de reinicializar. Se inspeccionan los bloques DMA activos
y el buffer PSG. El control negativo reproduce la escritura a `0x20` de RC1.

Pasaron ocho pruebas unitarias, controles/plataforma, conversión de recursos,
132 escenarios nativos y 24 finales de bonus con rutas Android. La APK x86_64
instalada en Android 16 completó el recorrido de ajustes, editor, recreación,
reanudación, perfil original, entrada al nivel y recarga del guardado. Las
capturas de arranque, editor y nivel se revisaron. El test reconoce el aviso
de pantalla completa del sistema antes de tocar controles cubiertos.

APK: `com.ylports.sonicadvance2.preview.rc2`, versionCode 22,
`0.2.0-rc2-OCS4`, 33.898.548 bytes. SHA-256:
`ebe60c8d1e49bcead4a2feffa9df96fc9dbd2d216517ba23790b83faadac7295`.
La biblioteca ARM64 tiene SHA-256
`1d246aa88ac9bbc4c5f1517141f58dd0936f5f56ec5b8d6e959fc11c24c97680`.
La salida Android de audio y el juego ARM en el A15 físico siguen pendientes.

## Continuidad y recursos

La rama `android-v0.2.0-ocs4` conserva `Mis-4-Ocs`. El árbol OCS4
`a7c50c9b3d5ef2b3084f8505eea7820771790e8e`, registrado por el informe de la
APK Beta 5, estaba subido sin un commit accesible. Se recuperó como
`eb2dcc06db618194086a83a40bc0feb7b1ed0c71`.

Los bytes de los siete atlas/paletas/proyectiles recuperados se compararon con
`libmain.so` de `SonicAdvance2-0.1.0-beta5-ocs4-32bit.apk`.
Elizabeth, Jude, Kiro, Yuliana, Kura, Sans y Spider-Man permanecen disponibles.
Los cinco personajes originales conservan sus índices físicos y de guardado.
La identidad OC continúa guardándose por separado con el mismo formato OCS1.

## Cambios

- Android desactiva `ENABLE_DECOMP_CREDITS`: desaparece la presentación del
  port de PC. La presentación original, el perfil, el título y sus transiciones
  siguen siendo las rutinas originales del juego. Se conservan los créditos
  del README y las atribuciones accesibles en Ajustes.
- La selección OC utiliza sus atlas reales para espera, respiración, parpadeo
  y confirmación. Las transferencias de VRAM se producen al cambiar de pose.
- Los resultados del bonus usan las cuatro poses de victoria propias de cada
  OC, con su cadencia y el mismo pivote de pies. Se conserva su arte anterior.
- El bonus muestra el nombre del OC seleccionado junto al mensaje de esmeralda,
  usando el mismo alfabeto de identidad del resto de la interfaz OC. Los cinco
  nombres originales conservan sus gráficos. En Android el panel y sus
  contadores utilizan su encuadre original 240×160; el mundo 3D mantiene el suyo.
- El título, la presentación original y la creación de perfil se muestran en
  240×160, evitando la repetición lateral de gráficos diseñados para GBA. Los
  buffers HBlank conservan espacio para todas las filas del renderer portátil.
  Al entrar a un nivel se restaura el encuadre de juego ampliado.
- La frenada OC alcanza la última pose y la mantiene, sin reiniciar el derrape
  cada doce fotogramas. La fase de carrera trasera del bonus queda acotada.
- La búsqueda de profundidad del bonus conserva la fila de perspectiva en
  el intervalo válido. Una coincidencia exacta ya no confunde la fila con el
  tamaño del paso de búsqueda. No se sustituye el motor de bonus.
- Los controles admiten tamaño y posición por botón, tamaño global, opacidad,
  área táctil, restauración y persistencia. Se conserva la combinación de
  cruceta, salto y habilidad. Los toques breves se retienen hasta el siguiente
  tick; el movimiento de un contacto cancelado no crea un dedo fantasma.
- Ajustes de Android se abren desde la pausa o con Atrás de Android. El juego y
  su audio se suspenden mientras están abiertos. Los cambios llegan al hilo
  del juego mediante una copia en la cola SDL, sin compartir referencias JNI.
- Escalado con proporción correcta y alternativa de píxeles enteros. El filtro
  predeterminado es vecino más cercano. Los menús de 240×160 y la escena
  ampliada de 426×240 conservan sus proporciones.
- Volúmenes general, música y efectos independientes; se mantienen los canales
  PCM y PSG originales. El buffer solicitado al dispositivo es independiente
  de la generación del audio a 60 Hz.
- Presentación de 30 o 60 FPS sin cambiar la frecuencia de la lógica. El
  diagnóstico opcional cuenta presentaciones reales por segundo.
- El port x86 de `CpuFastSet` deja de exigir alineación SIMD de 16 bytes donde
  la API GBA permite cuatro. Una prueba de la rutina anterior reproduce SIGSEGV;
  la rutina corregida verifica copias, rellenos, redondeo BIOS y límites.
- Corregido un cierre real de Android 36 en `GameInit`: el array de tareas
  multijugador tenía una alineación menor que la asumida por el compilador
  nativo. La declaración, definición y biblioteca empaquetada ahora coinciden
  en 16 bytes. La traza nativa identificó la instrucción que fallaba.
- La biblioteca de llamadas GBA termina de generarse antes de enlazar el juego;
  se evita leer su archivo mientras una compilación paralela lo reescribe.
- Reparado el guardado real del motor: la detección portátil devolvía error y
  activaba `FLAGS_NO_FLASH_MEMORY`. Ahora se configura la flash de 512 Kbit en
  memoria, con lectura/escritura/verificación nativas y persistencia atómica.
  Se copian los bytes reales de cada registro sin leer un sector entero desde
  un buffer corto. Los límites se verifican antes de modificar la memoria.
  El checksum usa palabras de 32 bits, igual que el formato GBA/ARM32, también
  cuando se compila ARM64; se mantienen sectores anteriores para recuperación.
- Android devuelve el control a SDL al recibir el cierre de su actividad,
  guarda y libera recursos sin `exit(0)`. Puede reiniciar el motor en el mismo
  proceso al recrearse la actividad. El backend host prueba dos ciclos reales.
  La finalización del hilo SDL se vincula a su actividad original para que no
  cierre otra actividad durante la recreación por cambios de configuración.
- Si AAudio rechaza abrir el dispositivo, se prueban OpenSL ES y AudioTrack
  manteniendo formato, frecuencia y lógica del juego. La integración Android
  comprueba que se haya abierto un backend de audio real.
- Se adapta el registro de receptores USB/Bluetooth de SDL 2.30.3 a API 33+,
  siguiendo la corrección upstream de SDL2. AGP 8.5.2 y Gradle 8.7 permiten
  empaquetar las bibliotecas nativas sin comprimir con alineación de 16 KB.
- Compilación ARM32 y ARM64 para el teléfono, con alineación ELF de 16 KB en ARM64.
  Se compila un paquete x86_64 separado para las pruebas de emulador Android.

## Verificaciones

`bash android/test-ocs.sh` ejecuta los escenarios de selección, memoria, VRAM,
habilidades, daño, pausa, agua, reinicio y victoria de acto. Incluye los cinco
originales, los siete OC, tres poses animadas de selección por OC y las siete
zonas de bonus. Cada proceso debe emitir un marcador explícito de aprobación.
La batería ampliada produce 132 marcadores. La prueba de victoria de acto usa
una ubicación inicial próxima a la meta real y después entradas normales;
no equivale a recorrer todo el acto.

Hay 36 casos adicionales de final de bonus: siete zonas, los cinco personajes
originales y los siete OC, con objetivo de anillos cumplido y sin cumplir. La
fixture fija anillos/tiempo y deja que las rutinas reales desaceleren, terminen,
muestren resultados, concedan la esmeralda cuando corresponda, escriban y
recarguen el guardado y vuelvan al nivel. Verifica las cuatro poses de victoria
por OC. Esto prueba las condiciones de salida; no es una partida completa
recogiendo naturalmente todos los anillos.

Una segunda compilación ejecuta 24 finales con las rutas `__ANDROID__` activas:
los doce personajes, objetivo cumplido/sin cumplir y las siete zonas. Verifica
además el nombre OC en la VRAM real. Conserva capturas del panel original y del
regreso a la escena ampliada. Son pruebas de host con ASan, sin GPU Android.

La prueba de almacenamiento del motor escribe dos generaciones, recarga el
progreso y las esmeraldas, invalida la cabecera más reciente y comprueba la
recuperación de la anterior. También detecta lecturas excesivas desde un buffer
corto y exige que una escritura fuera de rango no modifique ningún sector.

`bash android/test-platform.sh` verifica geometría a cinco resoluciones,
proporción, escalado entero, edición de posiciones, acciones simultáneas,
toques rápidos, cancelación, dispositivos táctiles distintos y configuración
persistida. También comprueba ambas implementaciones de `CpuFastSet`.

`python3 -m unittest discover -s android/tests -v` conserva las ocho pruebas de
importación, guardado, informes de cierre y paletas deliberadamente desalineadas.
Los tres verificadores de `tools/oc_sprites` prueban la conversión nativa y la
preservación de los recursos anteriores.

`SA_RUNTIME_ANDROID_BACKEND=1 bash android/test-runtime.sh` ejecuta arranque,
menús y 1.200 fotogramas de nivel con el backend Android en host, vídeo/audio
dummy y AddressSanitizer. No representa un dispositivo Android físico.
También envía `SDL_QUIT`, exige que el motor devuelva el control y repite la
inicialización/limpieza en el mismo proceso, sin terminarlo.

El workflow `sa2-v020.yml` añade instalación y pruebas de la APK x86_64 en un
emulador Android 36: captura arranque, Ajustes, editor táctil, reanudación,
persistencia y entrada al nivel. Sus resultados y capturas se guardan como
artefactos; el estado aprobado/fallido procede de `report.json` de cada ejecución.

`android/verify-sa2-apk.py` abre cada APK, comprueba la arquitectura, el
identificador esperado y los bytes de cada atlas/paleta/proyectil en la
biblioteca realmente empaquetada. Rechaza la intro del port y símbolos del
harness. La firma se comprueba con `apksigner`.
La alineación ZIP se verifica con `zipalign -P 16`; los segmentos ELF de las
bibliotecas de 64 bits también se inspeccionan en la APK real.

La publicación de RC1 exigía compilación, regresiones y emulador en el
mismo commit, con activación explícita. Se publicó como prerelease y se
rechazaba sobrescribir una RC1 existente. RC3 conserva esas condiciones y
publica mediante `sa2-publish-verified-rc3.yml` y su manifiesto específico.

También puede promoverse el artefacto ya verificado con
`sa2-publish-verified-rc.yml`: el manifiesto registra commit, ejecución, paquete
y SHA-256. El publicador exige los tres jobs aprobados, comprueba que el código
del juego no haya cambiado y publica los mismos bytes sin una nueva compilación
ni una nueva clave de pruebas.

### Evidencia conservada de RC1 (versión anterior)

- Código de la APK: `ba57e076f7ba84b6ab666be346ba001a3fc4cbbb`.
- [Ejecución 38965740681](https://github.com/YlPorts/sa2/actions/runs/38965740681):
  `apk`, `regressions` y `android-emulator` aprobados.
- 132 marcadores del motor y 24 finales con rutas Android, sin errores ASan;
  ocho pruebas unitarias y comprobaciones de controles, recursos y CpuFastSet.
- APK x86_64 instalada en Android 16 / API 36: arranque, recreación de actividad
  en el mismo proceso, ajustes, arrastre/guardado de A, reanudación, creación de
  perfil original, entrada al nivel con toques y recarga del guardado después
  de un cierre forzado. Las capturas de ajustes, editor y nivel se revisaron.
- ARM32 y ARM64 compilados y recursos verificados; no se ejecutó su jugabilidad
  en un dispositivo ARM. El emulador no prueba la GPU Mali del Galaxy A15.
- APK: 35.766.313 bytes, `com.ylports.sonicadvance2.preview`, versionCode 21,
  versionName `0.2.0-rc1-OCS4`, firma de pruebas.
- SHA-256: `fa41cd674c0a0dd04b477638fff9d7ca06119e9938c2df28829f90fb6a05f9f2`.

La Release adjunta el informe de APK, firma y alineación, registros nativos,
informe y capturas Android, símbolos nativos y vínculo al commit exacto.

## Firma e instalación

La firma anterior tiene SHA-256
`15767e621886f4c420af89c6f9767d1857b5fe830782095177a0166400a14e76`.
El repositorio no contiene esa clave privada. Si no está configurada en Actions,
RC3 usa `com.ylports.sonicadvance2.preview.rc3` y una firma de pruebas.
Así puede instalarse junto a la Beta 5 y sus archivos privados permanecen
separados. No debe desinstalarse la Beta 5 para instalar esta candidata.

Para firmar una actualización del paquete original, Actions admite los secretos
`SA_SIGNING_KEYSTORE_BASE64`, `SA_SIGNING_STORE_PASSWORD`, `SA_SIGNING_KEY_ALIAS`
y `SA_SIGNING_KEY_PASSWORD`. El certificado debe coincidir con el anterior.
La clave se prepara fuera del repositorio y se elimina al terminar el job.

## Pendiente antes de una versión estable

- En el A15: instalar RC3 junto a las anteriores, terminar el perfil original,
  elegir Cream/Tails/Knuckles/Amy y entrar al nivel con cada uno; comprobar
  toques breves y carrera+salto+habilidad con varios dedos. Pausar con Start,
  abrir Ajustes y reanudar: la entrada de Ajustes debe desaparecer al jugar.
  Alternar encuadre/16:9/píxeles enteros, suspender, reabrir y comprobar HUD,
  proporciones, controles y persistencia. Registrar vídeo/log si falla.
- Instalación y juego prolongado en Samsung Galaxy A15 SM-A155M / Android 16,
  tanto con ARM32 como con ARM64, incluyendo GPU Mali, audio y multitáctil real.
- Comparación visual completa con GBA en todos los actos y jefes.
- Completar manualmente los bonus, comprobar resultados, esmeraldas y retorno
  al progreso. Los recorridos automatizados de 1.200 frames prueban inicio,
  movimiento, proyección y sprites; no certifican completar los siete bonus.
- Comprobar los mandos Bluetooth/USB físicos y las interrupciones de audio.
- Ampliación real a 19:9/ultrawide, HUD configurable y cámaras en todas las zonas.
  La candidata conserva un encuadre seguro sin estirar la imagen.
- Usar la firma original para una actualización que conserve el mismo paquete.

No se afirma haber dibujado nuevas animaciones para todos los estados ni haber
reescrito las físicas originales. Las correcciones de esta candidata se limitan
a los cambios descritos y a las verificaciones registradas.
