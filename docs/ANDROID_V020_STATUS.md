# Android v0.2.0 OCS4 — candidata RC1

Esta es una candidata de prueba, no la versión estable v0.2.0.

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
- Ajustes de Android se abren con el botón discreto de tres puntos. El juego y
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

La publicación RC1 exige que pasen compilación, regresiones y emulador en el
mismo commit. Se activa explícitamente con la opción `publish_candidate` al
ejecutar el workflow o con `[publish-rc1]` en el mensaje del commit. Siempre
se publica como prerelease y se rechaza sobrescribir una RC1 existente.

También puede promoverse el artefacto ya verificado con
`sa2-publish-verified-rc.yml`: el manifiesto registra commit, ejecución, paquete
y SHA-256. El publicador exige los tres jobs aprobados, comprueba que el código
del juego no haya cambiado y publica los mismos bytes sin una nueva compilación
ni una nueva clave de pruebas.

### Evidencia de la candidata publicada

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
la candidata usa `com.ylports.sonicadvance2.preview` y una firma de pruebas.
Así puede instalarse junto a la Beta 5 y sus archivos privados permanecen
separados. No debe desinstalarse la Beta 5 para instalar esta candidata.

Para firmar una actualización del paquete original, Actions admite los secretos
`SA_SIGNING_KEYSTORE_BASE64`, `SA_SIGNING_STORE_PASSWORD`, `SA_SIGNING_KEY_ALIAS`
y `SA_SIGNING_KEY_PASSWORD`. El certificado debe coincidir con el anterior.
La clave se prepara fuera del repositorio y se elimina al terminar el job.

## Pendiente antes de una versión estable

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
