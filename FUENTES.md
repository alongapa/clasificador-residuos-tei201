# Declaración de fuentes

Este archivo declara todo el código, las librerías y las herramientas externas
usadas en el proyecto, incluido el uso de inteligencia artificial.

> **Para el equipo:** cada sección de la parte 3 incluye un bloque
> *Comprensión del código*. Ahí está, en lenguaje llano, qué hace cada parte y
> por qué se decidió así. El evaluador puede preguntarle a cualquier integrante
> por cualquier función, de modo que esos bloques son la guía de estudio para
> la defensa.
>
> Las decisiones que más se prestan para preguntas son cuatro: por qué la
> medición cuenta por defecto y la cámara solo objeta (3.4), por qué la banda
> de ruido y el umbral de confirmación se mueven juntos (3.3), por qué la ruta
> de calibración levanta una bandera en vez de calibrar directamente (3.6), y
> por qué se agruparon los materiales en una sola clase (3.4).

---

## 1. Librerías utilizadas

| Librería | Versión | Uso en el proyecto | Fuente |
|---|---|---|---|
| `esp32` (core Arduino) | 3.3.12 | Soporte de placa, GPIO, `HardwareSerial` para el enlace UART | https://github.com/espressif/arduino-esp32 |
| `SD` | incluida en el core | Montaje y escritura de la microSD por SPI en el DevKit | https://github.com/espressif/arduino-esp32/tree/master/libraries/SD |
| `SPI` | incluida en el core | Bus SPI hacia el módulo microSD | https://github.com/espressif/arduino-esp32/tree/master/libraries/SPI |
| `WiFi` | incluida en el core | Conexión al hotspot y modo punto de acceso de respaldo | https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi |
| `WebServer` | incluida en el core | Servidor HTTP embebido: sirve el dashboard y las rutas `/datos` y `/calibrar` | https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer |
| `WiFiClientSecure` | incluida en el core | Capa TLS para hablar con la API de Telegram; en el core 3.x es la cabecera de compatibilidad de `NetworkClientSecure` | https://github.com/espressif/arduino-esp32/tree/master/libraries |
| `HTTPClient` | incluida en el core | Peticiones HTTPS `POST` a `api.telegram.org` para enviar los avisos | https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPClient |
| `time.h` + SNTP | incluida en el core | Sincronización horaria por NTP, para la marca temporal real de cada fila del CSV | https://github.com/espressif/arduino-esp32 |
| `esp_camera` | incluida en el core | Inicialización y captura del sensor OV2640 | https://github.com/espressif/esp32-camera |
| `gapa-project-1_inferencing` | exportada 2026-10 | SDK de Edge Impulse con el modelo entrenado por el equipo; ejecuta la inferencia en la placa | Generada en https://edgeimpulse.com |
| `edge-impulse-sdk/dsp/image` | dentro de la anterior | Recorte y reescalado de la imagen a 96×96 antes de la inferencia | https://github.com/edgeimpulse/inferencing-sdk-cpp |

El modelo dentro de `gapa-project-1_inferencing` fue **entrenado por el equipo**
con un dataset propio: fotografías tomadas con la misma ESP32-CAM del prototipo.
La librería es el empaquetado que genera Edge Impulse; los pesos son resultado
de nuestro entrenamiento.

---

## 2. Código externo adaptado

### 2.1 Ejemplo `esp32_camera` de Edge Impulse

**Archivo:** `firmware/clasificador_camara/clasificador_camara.ino`
**Fuente:** ejemplo incluido en la librería exportada por Edge Impulse
(`Archivo → Ejemplos → gapa-project-1_inferencing → esp32 → esp32_camera`).
Código original de Edge Impulse Inc., 2022, licencia MIT.

El archivo conserva la estructura, la configuración de cámara y el flujo de
inferencia del ejemplo original. Se le hicieron cinco modificaciones:

1. **Selección del modelo de cámara.** El ejemplo viene con
   `CAMERA_MODEL_ESP_EYE` activo. Se comentó esa línea y se activó
   `CAMERA_MODEL_AI_THINKER`, que es la placa del prototipo. Sin este cambio la
   cámara se inicializa con los pines equivocados y no entrega imagen.

2. **Declaración de un segundo puerto serial.** Se agregó
   `HardwareSerial SerialDevKit(1)` para usar la UART1 del ESP32 como enlace con
   el DevKit, dejando la UART0 libre para el monitor USB.

3. **Inicialización del enlace.** En `setup()` se agregó
   `SerialDevKit.begin(9600, SERIAL_8N1, -1, 13)`: 9600 baudios, sin pin de
   recepción (`-1`, la cámara solo transmite) y GPIO 13 como transmisión.

4. **Envío del resultado.** Después del bucle que imprime las predicciones se
   agregó un recorrido que busca el índice de mayor confianza y lo transmite por
   el enlace con el formato `clase,confianza\n`.

5. **Eliminación de la espera por monitor serial.** Se comentó
   `while (!Serial);`. Esa línea detiene el arranque hasta que haya un monitor
   conectado; con la placa alimentada desde la protoboard, sin USB, dejaba el
   programa colgado antes de clasificar.

**Elección de GPIO 13:** se descartó el GPIO 12 porque es un pin
de arranque del ESP32 — si está en alto al encender, el chip cambia el voltaje
de la memoria flash y la placa no bootea. El GPIO 13 no cumple esa función y
queda libre cuando no se usa la ranura SD interna de la cámara.

### 2.2 Pinout de la ESP32-CAM AI-Thinker

**Fuente:** `camera_pins.h` de los ejemplos oficiales de arduino-esp32
https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h
**Adaptación:** ninguna. Las definiciones de pines se usan tal como vienen; son
las direcciones físicas del hardware y no admiten modificación.

---

## 3. Uso de Inteligencia Artificial

Se usó **Claude (Anthropic)** durante octubre y noviembre de 2026 como apoyo en
el desarrollo del firmware, la depuración del hardware y la documentación. A
continuación se declara cada uso.

### 3.1 Programa principal del DevKit

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino` (completo)
**Herramienta:** Claude (octubre 2026)
**Prompt, en resumen:** se le describió el hardware disponible, la asignación de
pines decidida por el equipo y la regla de decisión que queríamos —validar una
detección solo si el sensor confirma presencia *y* la confianza del modelo supera
un umbral— y se pidió el programa que lo implementara.

**Qué generó la IA:** la estructura completa del archivo, las funciones de
medición, registro, consulta a la cámara y procesamiento del ciclo.

**Qué aportó el equipo:** la especificación de la lógica de fusión, la asignación
de pines, los valores de umbral, y la depuración completa en hardware real —
incluida la corrección de varios errores del código generado.

**Comprensión del código:**

- `medirDistancia()` toma cinco lecturas del ultrasónico separadas 30 ms,
  descarta las que caen fuera de rango, ordena las válidas y devuelve la del
  medio. Se usa mediana y no promedio porque el HC-SR04 produce lecturas
  sueltas muy desviadas cuando el eco rebota mal; un promedio las incorpora al
  resultado, la mediana las descarta sin suavizar los cambios reales de
  distancia. Entre lectura y lectura llama a `servidor.handleClient()`, para
  que el dashboard no se congele durante los 150 ms que toma la medición.
- `pedirCaracterizacion()` vacía el buffer de entrada antes de esperar, para no
  leer un resultado viejo. La cámara responde una línea `clase,confianza`; si
  llega cortada a la mitad —posible si estaba transmitiendo justo cuando se
  vació el buffer— la clase parcial no coincide con ninguna de las tres
  conocidas y se descarta.
- El ciclo no se dispara en bucle con un objeto quieto porque la cámara solo se
  consulta cuando la altura **sube** más de `CONFIRMA_SUBIDA_MM`, o cuando pasan
  `REVALIDA_MS` desde la última confirmación. Un nivel estable no genera
  consultas nuevas; una bajada no necesita confirmación.
- El pulsador se lee con `INPUT_PULLUP`: el pin queda en alto por la resistencia
  interna del ESP32 y baja a cero al presionar, lo que ahorra una resistencia
  externa. Se distingue toque corto de largo midiendo cuánto tiempo permanece en
  bajo.

### 3.2 Modificaciones al programa de la cámara

**Archivo:** `firmware/clasificador_camara/clasificador_camara.ino`
**Herramienta:** Claude (octubre 2026)
**Uso:** se pidió indicar qué agregar al ejemplo de Edge Impulse para que
transmitiera el resultado de la clasificación por UART. Las cinco
modificaciones están detalladas en la sección 2.1.

**Nota metodológica:** en una primera iteración se intentó reescribir el
programa de la cámara completo en lugar de modificar el ejemplo. Ese programa
compilaba y arrancaba, pero nunca llegaba a clasificar: imprimía `cam_hal:
FB-OVF` en bucle. La causa fue una configuración de cámara distinta a la del
ejemplo (`fb_count = 2` con `CAMERA_GRAB_LATEST` en vez de `fb_count = 1` con
`CAMERA_GRAB_WHEN_EMPTY`), que desbordaba el buffer de cuadros. Se descartó ese
archivo y se volvió al ejemplo oficial, modificándolo lo mínimo.

### 3.3 Medición de acumulación

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** tras la recomendación del profesor en el Avance 2, se
pidió reorientar el sistema desde la clasificación de materiales hacia la
medición de cuánto residuo se acumula, usando el mismo sensor ultrasónico.

**Qué generó la IA:** las funciones de cálculo de altura, porcentaje de llenado,
ritmo de crecimiento y volumen estimado, el muestreo periódico, la calibración
del cero y la lógica que decide cuándo consultar a la cámara.

**Qué aportó el equipo:** la decisión de cambiar el eje del proyecto, los
parámetros de la zona de observación, los umbrales y la validación física de las
mediciones contra una regla.

**Comprensión del código:**

- La altura se obtiene restando la lectura actual a una distancia de referencia
  medida con la zona vacía. Esa referencia no está fija en el código: se calibra
  en terreno, porque depende de a qué altura quedó montado el dispositivo.
- El ritmo de crecimiento usa la diferencia entre la primera y la última muestra
  de la ventana, dividida por el tiempo transcurrido. Se prefirió sobre un
  ajuste lineal porque es más robusta ante una muestra aislada mala y más fácil
  de explicar.
- La cámara se consulta cuando la altura sube más de `CONFIRMA_SUBIDA_MM`
  entre muestras consecutivas, y además cada 2 minutos mientras haya algo
  medido. Si el nivel oscila dentro del ruido no se gastan los 1,2 segundos de
  inferencia, porque no llegó nada nuevo.
- **`RUIDO_MM` vale 20 mm y `CONFIRMA_SUBIDA_MM` vale 25 mm.** Partieron en 8 y
  10, y se subieron el 19 de noviembre tras observar que, con la zona
  despejada, la lectura oscilaba hasta 16 mm respecto al cero calibrado, lo que
  mostraba un 12 % de llenado sobre una mesa vacía.

  **Los dos valores se mueven juntos y esa es la parte que importa entender.**
  Si solo se sube la banda de ruido, el sistema consultaría a la cámara cada
  vez que la lectura oscile 10 mm, es decir por puro ruido, y cada consulta
  bloquea hasta 3 segundos. `CONFIRMA_SUBIDA_MM` tiene que quedar por encima de
  la banda de ruido para que solo una subida real dispare la inferencia.

  La banda de 2 cm está respaldada por P8: la resolución del sensor es de 4 mm
  por paso y 2 cm de residuos no constituyen una acumulación que valga la pena
  reportar.
- **La altura reportada no es la del punto más alto del cúmulo.** El
  ultrasónico informa la primera superficie que le devuelve un eco fuerte
  dentro del cono. P8 midió que, con objetos angostos respecto al cono, el
  sistema subestima la altura en torno a un 26 %. Para el caso de uso esa
  subestimación es conservadora: la alerta se dispara más tarde, nunca antes.
- El volumen multiplica el área de la zona por la altura y por un factor de
  ocupación, porque los residuos no llenan el volumen como un sólido. Los tres
  valores se muestran en el dashboard junto al resultado: la cifra no se
  presenta como una medición sino como una estimación con supuestos visibles.

### 3.4 Validación de la medición con la cámara

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`, funciones
`confirmarConCamara`, `pedirCaracterizacion` e `indiceClase`
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** se pidió que la cámara dejara de ser un accesorio y
pasara a confirmar que lo que el ultrasónico mide corresponde a residuos y no a
cualquier objeto que pase por la zona.

**Qué generó la IA:** la separación de la altura en dos variables —medida y
validada—, la función de confirmación con sus tres resultados, la
reconfirmación periódica, el campo de validación en el CSV y su señalización en
el dashboard.

**Qué aportó el equipo:** la idea completa. El planteamiento fue que si pasa un
auto por la zona y no hay basura, el sistema no debe registrar acumulación.
También la decisión de invertir la regla el 19 de noviembre, descrita abajo,
tomada a partir de las mediciones en terreno.

**Comprensión del código:**

- El sistema maneja **dos alturas**. La medida es lo que entrega el
  ultrasónico, que detecta cualquier superficie. La validada es la que alimenta
  los indicadores, el volumen y la alerta.

- **El modelo tiene tres clases: `residuo`, `otro` y `vacio`.** Hasta el 18 de
  noviembre fueron cuatro —`carton`, `metal`, `plastico`, `vacio`— y se
  agruparon los tres materiales en una sola. El motivo está medido en P4: con
  cinco clases el acierto global era 63,5 %, `carton` acertaba el 20 % y la
  mitad de `otro` se iba a `metal`. Distinguir cartón de metal nunca fue parte
  de ninguna decisión del sistema, y ese esfuerzo consumía la capacidad del
  modelo en la frontera que sí importa. Con tres clases la validación subió a
  91,9 %.

- `confirmarConCamara` devuelve **tres valores**: `1` confirmada, `0` objetada
  porque la cámara ve la zona vacía o un objeto que no es residuo, y `-1` sin
  certeza porque no respondió o su confianza quedó bajo `CONFIANZA_MIN`.

- **LA MEDICIÓN CUENTA POR DEFECTO. La cámara solo puede objetar.** Esta es la
  decisión de diseño más importante del firmware, y se tomó el 19 de noviembre
  después de medir que el modelo alcanzaba apenas 0,63 de confianza en terreno.

  El razonamiento: el ultrasónico es un detector de presencia **físico**. La
  zona vacía devuelve siempre la misma distancia y cualquier objeto que entre
  al cono la acorta; eso no es probabilístico. El modelo de visión sí lo es.

  Si la medición solo contara cuando la cámara la confirma, un modelo dudoso
  dejaría al sistema sin registrar nunca una acumulación real: el componente
  probabilístico bloquearía al confiable. Con la regla invertida el sistema
  **degrada con elegancia** — cuando la cámara duda se comporta como un monitor
  ultrasónico puro, que es el núcleo del proyecto.

  En el código esto es el `if (estadoValidacion == 0)` de `muestrear()`: solo
  el valor `0`, la objeción con certeza, baja la altura validada a cero. Tanto
  `1` como `-1` hacen que la medición cuente.

- **`CONFIANZA_MIN` está en 0,80**, subido desde 0,50. Con el umbral alto, una
  clasificación dudosa devuelve `-1` en vez de actuar, y la medición física
  prevalece. Las clasificaciones correctas vienen con confianza alta: P6 midió
  301 clasificaciones con mediana 0,97 y mínimo 0,92.

- Una **bajada** de nivel no se consulta a la cámara. Si había residuos y ahora
  hay menos, alguien recolectó o el material se movió.

- La **reconfirmación cada 2 minutos** cubre una lectura que quedó pegada en
  algo que ya no está.

- `pedirCaracterizacion` **vacía el buffer antes de esperar**. Eso garantiza
  que la línea que llegue sea una clasificación nueva y no una guardada. El
  byte `'C'` que se escribe hacia la cámara es vestigial: el enlace quedó de
  una sola vía y nadie lo lee.

**Limitación medida, declarada en P9.** La cámara confirma presencia de
material de forma fiable, pero **no distingue un residuo de otro objeto
sólido**: una mano se clasifica como `residuo` con confianza alta. En 425
muestras de operación continua no objetó ninguna medición. Por eso los textos
del dashboard y de los avisos dicen «detecta material» y no «ve residuos»: la
interfaz no puede afirmar lo que el sistema no demuestra.

### 3.5 Notificaciones por Telegram

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`, funciones
`enviarTelegram`, `escaparJson`, `avisarRecoleccion` y
`avisarRecoleccionRealizada`
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** se pidió que el dispositivo avisara por Telegram cuando
el nivel superara el umbral de recolección, siguiendo la recomendación del
profesor en el Avance 2.

**Qué generó la IA:** el cliente HTTPS, la construcción del cuerpo JSON, el
escape de caracteres, la lógica de histéresis y el formato de los mensajes.

**Qué aportó el equipo:** la decisión de qué información lleva el aviso, los
umbrales de disparo y rearme, y la verificación del envío en terreno.

**Comprensión del código:**

- El aviso se envía cuando el nivel **cruza** el umbral, no en cada muestra que
  lo supera. Para rearmar, el nivel tiene que bajar antes del 55 %. Sin esa
  histéresis una oscilación de pocos milímetros justo en el borde del 70 %
  dispararía un mensaje cada cinco segundos.
- `cliente.setInsecure()` desactiva la validación del certificado de Telegram.
  Es una **simplificación consciente**: validar requeriría embeber el
  certificado raíz y mantenerlo actualizado, y el mensaje no transporta datos
  sensibles. En un despliegue real habría que anclar el certificado.
- El mensaje se manda como JSON por POST en vez de como parámetros en la URL,
  porque así no hay que codificar saltos de línea ni acentos.
- La función se desactiva sola si el token o el chat están vacíos, o si la placa
  no tiene salida a internet. El resto del sistema no depende de ella.
- **El ritmo informa su ventana.** `ventanaMinutos()` devuelve cuántos minutos
  abarca la serie usada para calcular la tasa, y el mensaje lo incluye. Sin ese
  dato la cifra engaña: colocar un objeto de golpe sobre una ventana de pocos
  minutos extrapola a miles de mm/h, que es correcto aritméticamente y absurdo
  en contexto. Se agregó tras observarlo en P11.
- El aviso dice **«Presencia de material confirmada por camara»**, no
  «confirmado visualmente». Es lo único que P9 respalda.

**Nota de seguridad declarada:** el token del bot **no se versiona**. Los campos
`TG_TOKEN` y `TG_CHAT` quedan vacíos en el repositorio y se llenan solo en la
copia local que se carga a la placa.

### 3.6 Dashboard web, servidor embebido y calibración remota

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`, secciones
de red, servidor, página HTML y la ruta `/calibrar`
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** se pidió un dashboard servido por el propio DevKit, que
no dependiera de internet ni de la red del lugar, y que mostrara indicadores
conectados al problema de los microbasurales. El 20 de noviembre se agregó la
calibración remota.

**Qué generó la IA:** el servidor, la página, el gráfico SVG, el endpoint JSON
y la ruta de calibración.

**Qué aportó el equipo:** qué indicadores debía mostrar, los umbrales, y **la
necesidad de calibrar desde el dashboard**, detectada al montar la electrónica
dentro del gabinete.

**Comprensión del código:**

- El dispositivo intenta primero conectarse a una red conocida. Si no lo logra
  levanta su propia red en modo AP. El dashboard no puede depender del WiFi de
  la sala. En modo AP no hay salida a internet, así que Telegram queda
  inactivo; la medición y el registro siguen.
- La página se guarda en `PROGMEM` y se envía con `send_P`, para que el HTML
  viva en flash y no consuma RAM.
- `servidor.handleClient()` se llama también **dentro** de las esperas largas:
  la lectura del ultrasónico, los 3 segundos de espera a la cámara y el bucle
  de calibración. La librería `WebServer` atiende las peticiones en el mismo
  hilo del `loop()`, así que sin esas llamadas el dashboard se congelaría cada
  vez que el sistema mide o consulta.
- La serie para el gráfico es un arreglo circular en RAM. El registro completo
  vive en la microSD; la RAM solo guarda lo que el dashboard necesita dibujar.
- El navegador pide `/datos` cada 2 segundos y recibe un JSON; la página se
  redibuja sola sin recargar.

**La calibración remota y por qué está hecha así:**

- Con el equipo montado **el pulsador no es accionable** —la lengüeta impresa
  de `Lid_B` no transmite la pulsación— **y tampoco hay acceso al pin D4**, así
  que la ruta `/calibrar` es la única vía para fijar el cero en terreno. Dejó de
  ser una comodidad y pasó a ser un requisito.
- **La ruta no ejecuta la calibración: levanta una bandera que atiende
  `loop()`.** Esto no es un rodeo, es necesario. `calibrarCero()` llama a
  `servidor.handleClient()` dentro de su bucle de lectura, y ejecutarla desde
  un manejador de ruta significaría reentrar en el servidor web mientras este
  está procesando una petición. La librería no es reentrante.
- El botón **pide dos clics**, con cancelación automática a los 8 segundos.
  Calibrar con material en la zona graba un cero falso y deja todas las
  mediciones corridas — ya ocurrió una vez durante las pruebas de P8. Con un
  botón de un solo toque en un teléfono, ese error es demasiado fácil.

**Decisión de interfaz declarada.** Los textos dicen «la cámara detecta
material» y no «ve residuos». P9 midió que el modelo clasifica una mano como
`residuo`, así que la interfaz no puede afirmar que distingue basura de
cualquier otra cosa.

### 3.7 Sketches de prueba por módulo

**Archivos:** todo `firmware/pruebas/`
**Herramienta:** Claude (octubre 2026)
**Uso:** se pidió un sketch de validación por cada componente —LED y pulsador,
sensor ultrasónico, módulo microSD— para probarlos por separado antes de
integrar.

**Adaptación del equipo:** se ajustaron los pines a la asignación real del
montaje y se corrigieron los umbrales según las mediciones obtenidas.

### 3.8 Apoyo en depuración de hardware

**Herramienta:** Claude (octubre 2026)
**Uso:** diagnóstico guiado de fallas durante el montaje. No generó código; se
usó como apoyo para aislar causas. Los diagnósticos que resultaron correctos:

- Una ESP32-CAM con la memoria flash dañada, identificada tras descartar
  puertos, dos computadores, tres fuentes de alimentación y cuatro
  combinaciones de modo y frecuencia de flash. El síntoma era que la escritura
  reportaba éxito y la lectura devolvía datos corruptos.
- El driver CP210x faltante que impedía que apareciera el puerto COM del DevKit.
- El cable plano de la cámara mal asentado (`SCCB_Read addr phase failed`).
- Caché de compilación corrupta del Arduino IDE (`file truncated`), resuelta
  borrando `%LOCALAPPDATA%\arduino` y excluyendo esas carpetas del antivirus.

### 3.9 Documentación

**Archivos:** `README.md`, `hardware/esquematico.html`, estructura de este
archivo, `testing/protocolo_pruebas.md`
**Herramienta:** Claude (octubre–noviembre 2026)
**Uso:** redacción y maquetación a partir de los datos técnicos del proyecto.
Las medidas, los valores y los resultados son del equipo; la IA los organizó y
dio formato.

---

## 4. Decisiones de diseño tomadas por el equipo

Para dejar claro qué no vino de ninguna fuente externa:

- La **arquitectura de dos placas** con la inferencia separada del control.
- La **validación invertida**: la medición del ultrasónico cuenta por defecto y
  la cámara solo puede *objetar*, y solo por encima de `CONFIANZA_MIN`. Un
  componente probabilístico nunca bloquea al componente físico confiable.
- Las **tres clases** del modelo (`residuo`, `otro`, `vacio`), tras comprobar
  que a 96×96 píxeles los materiales no se distinguen entre sí de forma fiable
  y que el Avance 1 nunca preguntó de qué material era la basura.
- La **alimentación del HC-SR04 a 3,3 V** en lugar de 5 V, lo que permite
  conectar el ECHO directo al GPIO sin divisor de tensión, a costa de reducir el
  alcance de unos 4 m a cerca de 1 m — suficiente para medir a 10–20 cm.
- El **registro local en microSD** como base garantizada, en lugar de depender
  de transmisión por red, para que el dispositivo funcione en terreno sin
  conectividad.
- El **cambio de eje del sistema**: de clasificar materiales a medir acumulación,
  con la clasificación como anotación secundaria. El dato duro lo entrega el
  ultrasónico, que es físico y confiable; la cámara aporta composición, que es
  probabilística.
- El **rol de la cámara como validador**. El ultrasónico mide, pero no sabe qué
  mide: un auto que pasa da la misma lectura que un cúmulo de basura. La cámara
  confirma que hay residuos antes de que la medición cuente como acumulación.
  Esto además aprovecha la parte robusta del modelo —distinguir si hay algo o no
  hay nada— en lugar de la parte frágil, que es acertar el material.
- Los **indicadores del dashboard**: composición del material acumulado y ritmo
  de acumulación, elegidos porque son las dos preguntas que el Avance 1 dejó
  abiertas sobre los microbasurales.
- El **dataset propio**, fotografiado con la misma cámara del prototipo para que
  el modelo entrene en las mismas condiciones ópticas en que va a operar.

---

## Apéndice · Índice de funciones del firmware del DevKit

Las 26 funciones de `clasificador_devkit.ino`, con lo que hace cada una. El
evaluador puede preguntar por cualquiera; las decisiones de diseño detrás de
las más relevantes están explicadas en la parte 3.

### Medición

| Función | Qué hace |
|---|---|
| `medirCruda()` | Un disparo del HC-SR04: pulso en TRIG y medición del ancho del pulso de ECHO, convertido a milímetros |
| `medirDistancia()` | Toma 5 lecturas crudas y devuelve la **mediana**, no el promedio. La mediana descarta un eco aislado malo sin suavizar un cambio real |
| `calibrarCero()` | Promedia 8 mediciones de la zona vacía y las graba como referencia. Si consigue menos de 4 válidas no toca el cero: dejarlo mal calibrado es peor que conservar el anterior |
| `porcentajeLlenado()` | Altura validada respecto a `CAPACIDAD_MM`, recortado a 100 % |
| `volumenEstimado()` | Área de la zona × altura validada × factor de ocupación. Es una estimación con supuestos declarados, no una medición |
| `tasaCrecimiento()` | Diferencia de altura entre la primera y la última muestra de la serie, dividida por el tiempo |
| `ventanaMinutos()` | Cuántos minutos abarca esa serie. Se informa junto al ritmo porque la cifra no significa nada sin su ventana |
| `muestrear()` | El ciclo principal: mide, decide si consulta a la cámara, actualiza la altura validada, registra y evalúa la alerta |

### Cámara

| Función | Qué hace |
|---|---|
| `indiceClase()` | Traduce el nombre de clase recibido a un índice, y de paso valida que sea una de las tres conocidas |
| `pedirCaracterizacion()` | Vacía el buffer de recepción y espera hasta 3 s una clasificación nueva por UART |
| `confirmarConCamara()` | La decisión: devuelve `1` confirmada, `0` objetada o `-1` sin certeza |

### Indicadores

| Función | Qué hace |
|---|---|
| `color()` | Enciende el LED RGB con una combinación dada |
| `actualizarLed()` | Traduce el porcentaje de llenado a color: verde holgado, ámbar a la mitad, rojo sobre el umbral |

### Registro

| Función | Qué hace |
|---|---|
| `horaTexto()` | Marca temporal legible si hubo sincronización NTP, `sin_hora` si no |
| `guardarMuestra()` | Agrega una muestra al arreglo circular en RAM que alimenta el gráfico |
| `registrarEnTarjeta()` | Escribe una fila en `registro.csv` de la microSD, creando el encabezado si el archivo no existe |

### Notificaciones

| Función | Qué hace |
|---|---|
| `escaparJson()` | Escapa comillas y saltos de línea para que el cuerpo JSON sea válido |
| `enviarTelegram()` | POST HTTPS a la API de Telegram |
| `avisarRecoleccion()` | Arma y envía el aviso de umbral superado |
| `avisarRecoleccionRealizada()` | Avisa que el nivel volvió a la normalidad, cerrando el ciclo |

### Red y servidor

| Función | Qué hace |
|---|---|
| `iniciarRed()` | Intenta conectarse a la red conocida; si falla levanta el punto de acceso propio. Sincroniza la hora por NTP si hay internet |
| `rutaRaiz()` | Sirve la página del dashboard desde `PROGMEM` |
| `rutaDatos()` | Devuelve el estado completo como JSON, que el navegador pide cada 2 s |
| `rutaCalibrar()` | **No calibra**: levanta una bandera que atiende `loop()`, para no reentrar en el servidor web |

### Arranque y ciclo

| Función | Qué hace |
|---|---|
| `setup()` | Configura pines, verifica los cuatro módulos, levanta la red y arranca el servidor |
| `loop()` | Atiende el servidor, la bandera de calibración, el pulsador y el muestreo periódico |
