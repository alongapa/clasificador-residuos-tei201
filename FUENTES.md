# Declaración de fuentes

Este archivo declara todo el código, las librerías y las herramientas externas
usadas en el proyecto, incluido el uso de inteligencia artificial.

> **Para el equipo:** las secciones marcadas con *(verificar)* describen código
> que el evaluador puede preguntar en la defensa. Cada integrante debe poder
> explicar qué hace, por qué se tomó esa decisión y cómo se integra al sistema.
> Revisen esas secciones antes de entregar y corrijan lo que no calce con su
> comprensión real.

---

## 1. Librerías utilizadas

| Librería | Versión | Uso en el proyecto | Fuente |
|---|---|---|---|
| `esp32` (core Arduino) | 3.3.12 | Soporte de placa, GPIO, `HardwareSerial` para el enlace UART | https://github.com/espressif/arduino-esp32 |
| `SD` | incluida en el core | Montaje y escritura de la microSD por SPI en el DevKit | https://github.com/espressif/arduino-esp32/tree/master/libraries/SD |
| `SPI` | incluida en el core | Bus SPI hacia el módulo microSD | https://github.com/espressif/arduino-esp32/tree/master/libraries/SPI |
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

**Elección de GPIO 13 *(verificar)*:** se descartó el GPIO 12 porque es un pin
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

**Comprensión del código *(verificar)*:**

- `medir()` toma cinco lecturas del ultrasónico separadas 30 ms, las ordena y
  devuelve la del medio. Se usa mediana y no promedio porque el HC-SR04 produce
  lecturas sueltas muy desviadas cuando el eco rebota mal; un promedio las
  incorpora al resultado, la mediana las descarta sin suavizar los cambios
  reales de distancia.
- `pedirClasificacion()` vacía el buffer de entrada antes de esperar, para no
  leer un resultado viejo de una detección anterior. Valida que la clase
  recibida sea una de las cuatro conocidas: si llega una línea cortada a la
  mitad —posible si la cámara estaba transmitiendo justo cuando se vació el
  buffer— el texto parcial no coincide con ninguna clase y se descarta.
- La variable `armado` evita que un objeto quieto dispare el ciclo en bucle.
  Después de una detección el sistema queda desarmado y solo vuelve a armarse
  cuando la distancia supera `REARME_MM`, es decir cuando el objeto se retiró.
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

**Comprensión del código *(verificar)*:**

- La altura se obtiene restando la lectura actual a una distancia de referencia
  medida con la zona vacía. Esa referencia no está fija en el código: se calibra
  en terreno, porque depende de a qué altura quedó montado el dispositivo.
- El ritmo de crecimiento usa la diferencia entre la primera y la última muestra
  de la ventana, dividida por el tiempo transcurrido. Se prefirió sobre un
  ajuste lineal porque es más robusta ante una muestra aislada mala y más fácil
  de explicar.
- La cámara se consulta cuando la altura sube más de 10 mm entre muestras
  consecutivas, y además cada 2 minutos mientras haya algo medido. Si el nivel
  oscila dentro del ruido del sensor no se gastan los 1,2 segundos de
  inferencia, porque no llegó nada nuevo.
- El volumen multiplica el área de la zona por la altura y por un factor de
  ocupación, porque los residuos no llenan el volumen como un sólido. Los tres
  valores se muestran en el dashboard junto al resultado: la cifra no se
  presenta como una medición sino como una estimación con supuestos visibles.

### 3.4 Validación de la medición con la cámara

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`, funciones
`confirmarConCamara`, `pedirCaracterizacion` e `indiceClase`
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** se pidió que la cámara dejara de ser un accesorio y
pasara a cumplir una función concreta: confirmar que lo que el ultrasónico mide
son realmente residuos y no cualquier objeto que pase por debajo del sensor.

**Qué generó la IA:** la separación de la altura en dos variables —medida y
validada—, la función de confirmación con sus tres resultados posibles, la
reconfirmación periódica, el campo de validación en el registro CSV y la
señalización del estado en el dashboard.

**Qué aportó el equipo:** la idea completa. El planteamiento fue que si un día
pasa un auto por la zona y no hay basura, el sistema no debe registrar
acumulación: la cámara tiene que confirmar que hay residuos de cualquier
material y que la medición del ultrasónico correspondía a basura real.

**Comprensión del código *(verificar)*:**

- El sistema maneja **dos alturas distintas**. La medida es lo que entrega el
  ultrasónico, que detecta cualquier superficie. La validada es la que la cámara
  confirmó como material. Los indicadores, el volumen y la alerta de recolección
  usan la validada; la medida se muestra aparte como contraste.
- `confirmarConCamara` devuelve **tres valores, no dos**: `1` confirmada, `0`
  rechazada porque la cámara ve la zona vacía, y `-1` sin confirmar porque la
  cámara no respondió o su confianza quedó bajo 0,50. El caso `-1` conserva la
  última altura validada en lugar de inventar un dato: no saber no es lo mismo
  que saber que no hay nada.
- Para la validación **solo importa la frontera `vacio` contra el resto**. Que
  el modelo confunda cartón con metal no afecta la decisión; es la frontera
  entre materiales la que resultó frágil en el Avance 2, no la frontera entre
  haber algo y no haber nada.
- Una **bajada** de nivel no se consulta a la cámara. Si había residuos y ahora
  hay menos, alguien recolectó o el material se movió; la altura validada sigue
  a la medida hacia abajo sin preguntar.
- La **reconfirmación cada 2 minutos** cubre el caso de una lectura que quedó
  pegada en un objeto que ya no está: si el auto se fue, la cámara ve la zona
  vacía y la altura validada baja a cero.
- `pedirCaracterizacion` **vacía el buffer de recepción antes de esperar**. Eso
  es lo que garantiza que la línea que llegue sea una clasificación nueva y no
  una que estaba guardada de antes. El byte `'C'` que se escribe hacia la cámara
  es vestigial: el enlace quedó de una sola vía y nadie lo lee, lo que está
  declarado como comentario en el código y en el README.

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

**Comprensión del código *(verificar)*:**

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

**Nota de seguridad declarada:** el token del bot **no se versiona**. Los campos
`TG_TOKEN` y `TG_CHAT` quedan vacíos en el repositorio y se llenan solo en la
copia local que se carga a la placa.

### 3.6 Dashboard web y servidor embebido

**Archivo:** `firmware/clasificador_devkit/clasificador_devkit.ino`, secciones
de red, servidor y página HTML
**Herramienta:** Claude (noviembre 2026)
**Prompt, en resumen:** se pidió un dashboard servido por el propio DevKit, que
no dependiera de internet ni de la red del lugar, y que mostrara indicadores
conectados al problema de los microbasurales.

**Qué generó la IA:** la lógica de conexión con respaldo en modo punto de
acceso, los dos endpoints del servidor, la construcción del JSON y la página
HTML con sus estilos y su script.

**Qué aportó el equipo:** la definición de qué indicadores importan —material
predominante y ritmo de acumulación, que son las dos preguntas que el Avance 1
dejó planteadas— y los umbrales de la alerta.

**Comprensión del código *(verificar)*:**

- El dispositivo intenta primero conectarse a una red conocida. Si no lo logra
  en 12 segundos levanta su propia red en modo AP. Esto es una decisión de
  robustez para la demostración: el dashboard no puede depender del WiFi de la
  sala.
- La página se guarda en `PROGMEM` y se envía con `send_P`, para que el texto
  HTML viva en la memoria flash y no consuma RAM.
- `servidor.handleClient()` se llama también dentro de `medir()` y de las
  esperas de `procesar()`. Sin eso el dashboard se congela durante los
  segundos que el sistema está midiendo o esperando a la cámara, porque la
  librería `WebServer` atiende las peticiones en el mismo hilo del `loop()`.
- El historial es un arreglo circular de 40 posiciones en RAM. Cuando se llena,
  corre todo una posición y escribe al final. El registro completo vive en la
  microSD; la RAM solo guarda lo que el dashboard necesita mostrar.
- La alerta compara cuántas detecciones válidas ocurrieron dentro de una ventana
  móvil de tiempo, recorriendo el historial y descartando las más antiguas que
  la ventana.

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
- La **regla de fusión**: exigir presencia confirmada *y* confianza sobre umbral,
  y reportar *no concluyente* en vez de adivinar cuando solo se cumple una.
- Las **cuatro clases** del modelo, y la decisión de **eliminar la clase vidrio**
  tras comprobar que a 96×96 píxeles no se distingue del plástico transparente.
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
