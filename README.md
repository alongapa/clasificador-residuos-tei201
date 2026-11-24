# Monitor de acumulación de residuos

Dispositivo IoT que mide de forma continua cuánto residuo se ha acumulado en una
zona de observación, calcula a qué ritmo crece, estima su volumen y avisa por
Telegram cuándo conviene recolectar.

La medición la hace un sensor ultrasónico. Una cámara con un modelo de visión
corriendo en la propia placa **confirma que lo que el sensor midió son
realmente residuos** y no un objeto cualquiera que pasó por debajo, y de paso
registra de qué material se trata. Solo las mediciones confirmadas cuentan como
acumulación.

La inferencia corre en el dispositivo, sin conexión a internet; la red se usa
únicamente para el dashboard y las notificaciones.

**TEI201 — Taller de Diseño en Ingeniería · Universidad Adolfo Ibáñez**

---

## Integrantes

| Nombre | Rol en el proyecto |
|---|---|
| Alonso García | Firmware, entrenamiento del modelo, integración |
| Tomás Quiroz | Contexto del problema y presentación |
| Nicolás Martín | Modelado 3D y encapsulado |
| Raimundo Pérez | Montaje del hardware y ensamblado de las piezas |

---

## El problema

Los microbasurales urbanos crecen sin que exista información sobre qué tipo de
material se acumula ni dónde. Caracterizar manualmente esos residuos es lento y
caro, y sin esa caracterización no se puede priorizar la recolección ni medir si
una intervención funcionó.

Este prototipo apunta a esa brecha: un dispositivo autónomo que identifica el
material de los residuos que tiene delante y deja un registro consultable, para
que quien gestiona la limpieza pueda decidir con datos en vez de con reportes
esporádicos.

Alineación ODS: **ODS 11, Meta 11.6, indicador 11.6.1** — reducir el impacto
ambiental per cápita de las ciudades, con foco en la gestión de desechos.

---

## Cómo funciona

### Principio de medición

El dispositivo se monta mirando hacia abajo, perpendicular al suelo de la zona
de observación. El sensor ultrasónico mide la distancia a la superficie más
cercana dentro de su cono:

```
altura del cúmulo  =  distancia a la zona vacía  −  distancia actual
```

La distancia a la zona vacía se fija calibrando con la zona limpia, desde el
botón **Calibrar** del dashboard. Es el mismo principio de los sensores
comerciales de llenado de contenedores.

### Las dos alturas

El sistema maneja dos alturas, y la distinción es el centro del diseño:

| | Qué es |
|---|---|
| **Altura medida** | Lo que entrega el ultrasónico. Mide cualquier superficie que haya bajo el sensor: residuos, un auto estacionado, una persona, un eco falso en un borde. |
| **Altura validada** | La altura que la cámara confirmó que corresponde a material acumulado. |

**Los indicadores, el volumen y la alerta de recolección usan la altura
validada.** La medida se muestra aparte, como dato de contraste.

El ultrasónico mide, pero no sabe *qué* mide. Si pasa un vehículo bajo el
sensor, registra 80 mm igual que un cúmulo de basura de 80 mm. Sin validación,
eso dispararía una orden de recolección hacia un lugar vacío.

### El rol de la cámara

La cámara no clasifica para decidir en qué contenedor va cada cosa. Su función
es **confirmar que la medición del ultrasónico corresponde a residuos**, de
cualquier material.

El modelo distingue cuatro clases: `carton`, `metal`, `plastico` y `vacio`. Para
la validación solo importa la frontera:

**La medición cuenta por defecto. La cámara solo puede objetar, y solo cuando
está segura.**

| Lo que ve la cámara | Decisión |
|---|---|
| `vacio` con confianza ≥ 0,70 | **Objetada.** La zona está despejada: el ultrasónico midió un eco falso. La altura validada baja a cero. |
| `otro` con confianza ≥ 0,70 | **Objetada.** Hay un objeto sólido, pero no es un residuo: una mano, un animal, un vehículo. |
| `residuo` con confianza ≥ 0,70 | **Confirmada.** La altura medida pasa a ser validada. |
| Confianza < 0,70, o sin respuesta en 3 s | **Sin objeción.** La medición del ultrasónico cuenta igual. |

### Por qué la medición cuenta por defecto

El ultrasónico es un detector de presencia **físico**: la zona vacía devuelve
siempre la misma distancia, y cualquier objeto que entre en el cono la acorta.
Eso no es probabilístico. El modelo de visión sí lo es, y en terreno se
equivoca con objetos que no vio durante el entrenamiento.

Si la medición solo contara cuando la cámara la confirma, un modelo dudoso
dejaría al sistema sin registrar nunca una acumulación real: el componente
probabilístico bloquearía al componente confiable. Con la regla invertida, el
sistema **degrada con elegancia**: cuando la cámara duda se comporta como un
monitor ultrasónico puro, que es el núcleo del proyecto, y la visión aporta
solo cuando aporta de verdad.

Dos efectos físicos distintos, que conviene no confundir: lo que **acorta la
distancia** es la altura del objeto, no su tamaño en general; lo que aporta el
tamaño horizontal es la **probabilidad de que el cono lo alcance**, que a 57 cm
de altura cubre unos 15 cm de diámetro.

Esto resuelve además la debilidad que detectamos en el Avance 2: el modelo
confunde materiales entre sí con objetos que no vio en el entrenamiento —
confundió una caja de cartón con metal. Pero la frontera *hay algo* / *no hay
nada* es mucho más robusta que la frontera entre materiales, y es la única que
el sistema necesita para funcionar.

### Cuándo se consulta a la cámara

La inferencia toma 1,2 s, así que no se corre en cada muestra. Se consulta en
tres situaciones:

1. **El nivel subió 25 mm o más de golpe.** Pudo llegar material, o pudo pasar
   algo por debajo. Hay que distinguir cuál de las dos.
2. **Reconfirmación cada 2 minutos**, mientras haya algo medido. Cubre el caso
   de una lectura que quedó pegada en un objeto que ya no está: si el auto se
   fue, la cámara ve la zona vacía y la altura validada baja a cero.
3. **Toque corto del pulsador**, para verificar a demanda. Solo con el
   gabinete abierto; cerrado no hay acceso al pulsador.

Una **bajada** de nivel no necesita confirmación. Si había residuos y ahora hay
menos, alguien recolectó o el material se movió; la altura validada sigue a la
medida hacia abajo sin preguntar.

### Qué mide y qué estima

| Indicador | Cómo se obtiene |
|---|---|
| **Altura de acumulación** (mm) | Medición directa del ultrasónico, confirmada por la cámara |
| **Porcentaje de llenado** | Altura validada respecto a una capacidad de referencia |
| **Ritmo de acumulación** (mm/h) | Diferencia de altura sobre la ventana de muestreo |
| **Volumen estimado** (cm³) | Área de la zona × altura validada × factor de ocupación |
| **Composición** | Clase que informó la cámara en la última confirmación |

### Limitación declarada

Un ultrasónico entrega **una sola distancia**: la de la primera superficie que
le devuelve un eco suficientemente fuerte dentro de su cono. No escanea la zona.

La altura medida **no es la del punto más alto del cúmulo**. Si la parte más
alta es angosta respecto al cono —que a 450 mm mide unos 12 cm de diámetro—
dispersa el pulso, y el eco que vuelve con energía proviene de una zona más
ancha y más baja. P8 lo midió: con objetos angostos el sistema subestima la
altura en torno a un **26 %**.

Para el caso de uso, esa subestimación es conservadora en la dirección correcta:
la alerta de recolección se dispara más tarde, nunca antes, y el sistema no
avisa por acumulaciones que no existen.

El volumen es por lo tanto una **estimación bajo supuestos explícitos**: un área
de zona de 225 cm² y un factor de ocupación de 0,60, ambos declarados en el
firmware y mostrados en el dashboard junto al resultado. Declarar el supuesto es
lo que hace defendible la cifra.

### Limitación del punto de vista

El dataset se fotografió con la cámara apuntando a los objetos sobre una mesa,
aproximadamente de frente. En operación la cámara mira **hacia abajo**, desde la
posición de montaje. Es un cambio de punto de vista que el modelo nunca vio en
entrenamiento, y ataca precisamente la característica en que P5 mostró que el
modelo se apoya: la silueta. Una botella de perfil es un rectángulo alto con
cuello; desde arriba es un círculo.

**La vista cenital no es un error de diseño, es la geometría correcta.** El
concepto del proyecto es un lector satelital, y un satélite observa a nadir,
perpendicular al suelo. Una toma oblicua sería menos representativa del caso
real. Además el ultrasónico debe estar perpendicular para que la distancia medida
corresponda a la altura. Lo que está desalineado con el montaje es el dataset,
no el montaje.

**Por qué la validación resiste este cambio mejor que la clasificación.** La
pregunta que el sistema le hace hoy a la cámara es *¿la zona está vacía o hay
algo?*. Eso se responde mirando si el cuadro tiene estructura, bordes y textura,
o si es una superficie uniforme — y eso cambia poco al rotar el punto de vista.
La pregunta anterior, *¿este objeto es acero o plástico?*, dependía de la
silueta, que cambia por completo. El cambio de arquitectura del Avance 3 redujo
la exposición a este problema antes de que lo detectáramos, pero **eso es un
argumento, no una medición**: se verifica en P9.

Si la verificación falla, el plan de contingencia está acotado y es barato: un
modelo **binario** `vacio` / `residuo`, fotografiado desde la posición real de
montaje. Son dos clases en vez de cuatro, y es el único discriminante que el
sistema necesita.

### Otras limitaciones

La validación hereda la limitación del modelo: la cámara mira la zona completa,
no el punto exacto donde rebotó el ultrasónico. Si hubiera residuos en un
extremo de la zona y un objeto ajeno en el otro, la validación podría confirmar
una medición que no corresponde al material. Con el tamaño de la maqueta esto no
se da, pero es una limitación real del diseño a escala mayor.

### Flujo del sistema

```
Cada 5 s: medir distancia (mediana de 5 lecturas) → altura medida
        ↓
¿Subió ≥ 25 mm, o pasaron 2 min desde la última confirmación?
        │
        ├── No ──→ ¿bajó el nivel? → la altura validada lo sigue
        │
        └── Sí ──→ consultar a la cámara
                        │
                        ├── ve "vacio"      → altura validada = 0
                        ├── ve material     → altura validada = altura medida
                        └── no responde     → se conserva la anterior
        ↓
Registrar en la serie temporal, la microSD y el dashboard
        ↓
¿La altura VALIDADA superó el umbral de llenado? → alerta por Telegram
```

### Estados del LED

El color refleja el nivel de llenado, que es la información accionable.

| Color | Significado |
|---|---|
| Azul fijo | Falta calibrar el cero |
| Azul parpadeando | Consultando a la cámara |
| Verde | Nivel holgado, bajo el 40 % |
| Ámbar | Entre 40 % y el umbral |
| Rojo | Sobre el umbral: conviene recolectar |

---

---

## Componentes

| Componente | Modelo | Cant. |
|---|---|---|
| Microcontrolador principal | ESP32 DevKit V1 (DOIT, 30 pines) | 1 |
| Cámara con inferencia | ESP32-CAM AI-Thinker + OV2640 | 1 |
| Sensor de distancia | HC-SR04 ultrasónico | 1 |
| Actuador visual | LED RGB 5 mm cátodo común | 1 |
| Resistencias | 220 Ω · 1/4 W | 3 |
| Entrada manual | Pulsador táctil de 2 patas | 1 |
| Almacenamiento | Módulo microSD SPI (HW-125) | 1 |
| Tarjeta de memoria | microSD FAT32 | 1 |
| Soporte | Protoboard 830 puntos (165 × 55 mm) | 1 |

La lista completa con especificaciones y costos está en
[`hardware/BOM.csv`](hardware/BOM.csv).

---

## Conexiones

El esquemático completo, con las restricciones de montaje, está en
[`hardware/esquematico.html`](hardware/esquematico.html).

### Pines del ESP32 DevKit V1

| Pin | Señal | Conectado a |
|---|---|---|
| D4 | Pulsador (pull-up interno) | Pulsador → GND |
| D5 | SPI CS | Módulo microSD |
| D18 | SPI SCK | Módulo microSD |
| D19 | SPI MISO | Módulo microSD |
| D21 | TRIG | HC-SR04 |
| D22 | ECHO | HC-SR04 |
| D23 | SPI MOSI | Módulo microSD |
| D25 | LED azul (220 Ω) | LED RGB |
| D26 | UART RX (9600 bd) | ESP32-CAM GPIO 13 |
| D32 | LED rojo (220 Ω) | LED RGB |
| D33 | LED verde (220 Ω) | LED RGB |
| VIN | 5 V | Riel de 5 V |
| 3V3 | 3,3 V | Riel de 3,3 V |
| GND | Tierra | Riel negativo |

### Enlace entre placas

```
ESP32-CAM GPIO 13 (TX)  →  DevKit D26 (RX)      UART, 9600 baudios
ESP32-CAM 5V            →  riel de 5 V
ESP32-CAM GND           →  riel negativo         ← obligatorio
```

Sin la tierra compartida el UART no transmite nada, aunque la línea de datos
esté bien conectada.

**El enlace es de una sola vía.** El diseño original era bidireccional: el
DevKit pediría una clasificación y la cámara respondería. No logramos hacerlo
funcionar y lo abandonamos por plazo. El esquema final es más simple: la cámara
clasifica de forma continua, cada 1,2 s, y emite cada resultado por UART. El
DevKit vacía su buffer de recepción y espera la siguiente línea, que por
construcción es una clasificación nueva, posterior a la medición del
ultrasónico. El firmware escribe un byte `'C'` hacia la cámara por
compatibilidad con el diseño original, pero nadie lo lee; está comentado como
tal en el código.

---

## Cómo cargar el firmware

### Requisitos

- Arduino IDE 2.x
- Paquete de placas **esp32 by Espressif Systems** (probado con 3.3.12)
- Librería del modelo: `gapa-project-1_inferencing` (exportada desde Edge
  Impulse como Arduino library e instalada con
  `Sketch → Incluir librería → Añadir biblioteca .ZIP`)

### ESP32 DevKit V1

1. Abrir `firmware/clasificador_devkit/clasificador_devkit.ino`
2. `Herramientas → Placa → DOIT ESP32 DEVKIT V1`
3. `Herramientas → Upload Speed → 115200`
4. Seleccionar el puerto y subir

> A 921600 baudios la carga falla con `Checksum error`. Usar 115200.

### ESP32-CAM

1. Montar la placa en el shield programador y conectarla por USB
2. Abrir `firmware/clasificador_camara/clasificador_camara.ino`
3. `Herramientas → Placa → AI Thinker ESP32-CAM`
4. `Herramientas → Partition Scheme → Huge APP (3MB No OTA)`
5. Subir, y después retirar la placa del shield para cablearla

> La primera compilación demora 15–25 minutos: reconstruye el SDK de Edge
> Impulse completo. Las siguientes toman un par de minutos.

> Subir con **una sola placa conectada al USB** a la vez, o se confunden los
> puertos COM.

### Calibración antes de medir

Despeje la zona de observación y pulse **Calibrar** en el dashboard. El sistema
mide la zona vacía, fija ese valor como cero y reinicia la sesión. Hasta que se
calibre, el LED queda azul fijo y el dashboard avisa que las alturas no son
confiables.

Con el gabinete abierto existe la misma función por hardware: un toque largo del
pulsador calibra y un toque corto pide una caracterización manual a la cámara.
Cerrado el equipo no hay acceso físico al pulsador, y por eso la vía del
dashboard es la que vale en terreno (ver *Calibración remota*).

### Verificación

Con la cámara conectada por USB, su monitor serial a 115200 debe mostrar, cada
segundo y medio:

```
Predictions (DSP: 9 ms., Classification: 1206 ms., Anomaly: 0 ms.):
Predictions:
  carton: 0.01172
  metal: 0.05859
  plastico: 0.71875
  vacio: 0.21094
-> DevKit: plastico,0.719
```

---

## El modelo de clasificación

| | |
|---|---|
| Plataforma | Edge Impulse |
| Arquitectura | MobileNetV2 96×96, α 0,35 |
| Método | Aprendizaje por transferencia |
| Cuantización | int8, compilado con EON Compiler |
| Runtime | TensorFlow Lite Micro, sin internet |
| Clases | `residuo`, `otro`, `vacio` |
| Precisión (model testing) | 98 % |
| Latencia en la placa | DSP 9 ms · clasificación 1206 ms |

Esos 1,2 segundos justifican la arquitectura: el sistema no actúa sobre cada
cuadro, sino que consulta a la cámara solo cuando el ultrasónico detecta un
cambio que vale la pena confirmar.

**La clase que importa es `vacio`.** El modelo se entrenó para distinguir
materiales, y en eso es frágil con objetos que no vio: en el Avance 2 confundió
una caja de cartón con metal. Pero el sistema no necesita acertar el material.
Necesita saber si hay algo o no hay nada, y esa frontera es mucho más robusta
que la frontera entre materiales. El material se registra como dato
complementario, no como base de ninguna decisión.

### Para actualizar el modelo

1. Reentrenar en Edge Impulse
2. `Deployment → Arduino library → Quantized (int8) → Build`
3. Borrar la carpeta de la librería anterior en `Documentos/Arduino/libraries/`
4. Instalar el `.zip` nuevo y volver a subir el mismo sketch, sin cambios

---

## Formato del registro

Cada muestra agrega una fila a `/registro.csv` en la microSD:

```csv
tiempo_ms,hora,distancia_mm,altura_medida_mm,altura_validada_mm,llenado_pct,volumen_cm3,validacion,material,confianza
45200,2026-11-03 14:32:05,238,12,0,0,0.0,-1,-,0.000
50210,2026-11-03 14:32:10,196,54,54,36,72.9,1,carton,0.812
55220,2026-11-03 14:32:15,120,130,54,36,72.9,0,-,0.000
```

| Campo | Descripción |
|---|---|
| `tiempo_ms` | Milisegundos desde el arranque de la placa |
| `hora` | Marca temporal real si hubo sincronización NTP, `sin_hora` si no |
| `distancia_mm` | Lectura cruda del ultrasónico (mediana de 5) |
| `altura_medida_mm` | Altura sobre el cero según el ultrasónico, sin validar |
| `altura_validada_mm` | Altura confirmada como residuos. Es la que usan los indicadores |
| `llenado_pct` | Porcentaje de la altura validada respecto a la capacidad |
| `volumen_cm3` | Estimación bajo los supuestos declarados |
| `validacion` | `1` confirmada · `0` rechazada por la cámara · `-1` sin evaluar |
| `material` | Clase de la última confirmación, o `-` |
| `confianza` | Confianza de esa clase, 0 a 1 |

La tercera fila del ejemplo es el caso que motivó todo el diseño: el ultrasónico
midió 130 mm, la cámara miró y vio la zona vacía, la medición se descartó y la
altura validada se mantuvo en el valor anterior. Sin esa columna, el dato
quedaría registrado como un cúmulo que nunca existió.

El registro es **continuo**: una fila cada 5 segundos, haya o no material nuevo.
Eso permite reconstruir la curva de acumulación completa después, y auditar
cuántas mediciones fueron descartadas.

---

## Dashboard

El propio DevKit sirve el dashboard. No requiere internet, servidor externo ni
cuenta en ningún servicio.

Al arrancar intenta conectarse a la red WiFi configurada en el firmware. Si no
hay credenciales o la conexión falla en 12 segundos, **levanta su propia red**:

| | |
|---|---|
| Red | `Clasificador-Residuos` |
| Clave | `residuos2026` |
| Dirección | `http://192.168.4.1` |

Esa doble ruta es deliberada: el dashboard tiene que funcionar durante la
evaluación sin depender de la red del lugar. La dirección exacta se imprime por
el monitor serial al arrancar.

### Calibración remota

El dashboard incluye un botón **Calibrar** que fija el cero sin tocar el
dispositivo. No es una comodidad: con el equipo montado dentro del gabinete
cerrado no hay acceso físico ni al pulsador ni al pin D4, así que es la única
vía para calibrar en terreno.

Funciona en dos pasos — el primer clic arma el botón y el segundo confirma, con
cancelación automática a los 8 segundos — porque calibrar con material en la
zona graba un cero falso y deja todas las mediciones corridas.

Internamente la ruta `/calibrar` no ejecuta la calibración: levanta una bandera
que `loop()` atiende. El procedimiento de calibración llama a
`servidor.handleClient()` dentro de su bucle de lectura, y ejecutarlo desde un
manejador de ruta sería reentrar en el servidor web.

### Qué muestra

**Residuo confirmado**, arriba: la altura validada y el porcentaje de llenado,
con una barra que marca el umbral de recolección. Debajo, en letra chica, la
lectura cruda del ultrasónico, para poder contrastar las dos.

**Cuatro indicadores:** ritmo de acumulación en mm/hora, volumen estimado,
estado de la confirmación visual y último material caracterizado.

**Gráfico de evolución del nivel** en el tiempo, con la línea del umbral
marcada. Cada muestra lleva un punto según su validación: verde si la cámara
confirmó residuos, rojo si la descartó. Es el elemento central: muestra la
tendencia, que es lo que permite decidir.

**Aviso de medición descartada** cuando la última confirmación resultó negativa,
explicando que el ultrasónico midió algo que la cámara no reconoció como
residuo.

**Alerta de recolección** cuando el llenado supera el umbral, con el ritmo
actual para dimensionar la urgencia.

**Confirmaciones recientes** con hora, material, confianza y a qué altura
ocurrieron.

Al pie, los supuestos de la estimación de volumen y los parámetros de
calibración, visibles junto al resultado.

---

## Notificaciones

Cuando el nivel cruza el umbral de recolección, el dispositivo envía un mensaje
por Telegram:

```
RECOLECCION REQUERIDA

Zona de observacion 1
Nivel: 112 mm (75% de capacidad)
Ritmo: 48 mm/h
Volumen estimado: 151 cm3
Material predominante: carton (0.81)
Confirmado visualmente por camara

04-11-2026 14:32
```

Y otro cuando el nivel vuelve a la normalidad, para cerrar el ciclo.

El nivel que aparece en el aviso es la **altura validada**, nunca la lectura
cruda. Una orden de recolección solo se emite sobre material que la cámara
confirmó, y el mensaje lo dice explícitamente para que quien lo recibe sepa que
no es un eco falso.

**El aviso se manda al cruzar el umbral, no en cada muestra que lo supera.** Para
volver a avisar, el nivel tiene que bajar antes del 55 %. Esa histéresis evita
que una oscilación justo en el borde dispare una lluvia de mensajes.

### Requisito

Las notificaciones necesitan que la placa tenga **salida a internet**, o sea que
se conecte a una red real en lugar de levantar la suya. El hotspot de un
teléfono sirve, y es lo recomendado para la demostración: el evaluador se
conecta al mismo hotspot y accede al dashboard por la IP de la placa.

Sin internet el dashboard funciona igual, solo que sin notificaciones. El
sistema indica en cuál de los dos modos está, tanto en el monitor serial como
al pie del dashboard.

### Cómo configurarlo

**1. Crear el bot**

En Telegram, buscar **@BotFather** y enviarle `/newbot`. Pide un nombre y un
usuario, y devuelve un token con esta forma:

```
7891234567:AAEhBOweik6ad9r_RrTSaFDpbkPH3SmAbCd
```

**2. Crear el grupo de avisos**

Crear un grupo de Telegram con los integrantes del equipo y agregar el bot. Un
grupo es mejor que un chat directo: todos reciben el aviso y queda registro
compartido.

**3. Obtener el `chat_id`**

Escribir cualquier mensaje en el grupo y después abrir en el navegador:

```
https://api.telegram.org/bot<TOKEN>/getUpdates
```

Reemplazando `<TOKEN>` por el del paso 1. En la respuesta buscar `"chat"` y
dentro de él `"id"`. Para un grupo es un número negativo, tipo `-1001234567890`.

**4. Ponerlos en el firmware**

Arriba de `clasificador_devkit.ino`:

```cpp
const char* TG_TOKEN = "7891234567:AAEhBOweik6ad9r_RrTSaFDpbkPH3SmAbCd";
const char* TG_CHAT  = "-1001234567890";
```

Y también configurar la red con salida a internet:

```cpp
const char* WIFI_SSID  = "nombre-del-hotspot";
const char* WIFI_CLAVE = "clave-del-hotspot";
```

Si cualquiera de los cuatro queda vacío, la función se desactiva sola y el resto
del sistema sigue funcionando.

### Advertencia de seguridad

El token del bot da control total sobre él. **No lo suban a GitHub.** Antes de
hacer el commit, dejen los dos campos vacíos en el archivo del repositorio y
llenen los valores solo en la copia local que cargan a la placa.

---

### Hora real

Cuando el dispositivo logra conectarse a una red con salida a internet,
sincroniza la hora por NTP y el registro queda con marca temporal real. Sin
conexión usa tiempo relativo desde el arranque, y lo indica.

---

## Estructura del repositorio

```
clasificador-residuos-tei201/
├── README.md
├── FUENTES.md              ← declaración de librerías, código externo y uso de IA
├── firmware/
│   ├── clasificador_devkit/    programa principal del DevKit
│   ├── clasificador_camara/    programa de la ESP32-CAM
│   └── pruebas/                sketches de validación por módulo
├── hardware/
│   ├── esquematico.html        diagrama de conexiones y restricciones
│   ├── BOM.csv                 lista de materiales con costos
│   └── fotos/                  tres ángulos del prototipo ensamblado
├── diseño-3d/
│   ├── encapsulado.f3d         ensamble paramétrico de Fusion 360
│   ├── planos.pdf              10 láminas A3 con vistas ortogonales y cotas
│   ├── renders/                exterior, interior, explosionado y detalle
│   └── stl/                    9 archivos para las 10 piezas imprimibles
├── testing/
│   ├── protocolo_pruebas.md    protocolo y resultados de las 11 pruebas
│   ├── datos/                  registros CSV de las pruebas
│   └── evidencia/              capturas del dashboard, Telegram y Edge Impulse
└── docs/
    └── respuesta-retroalimentacion.md   qué se hizo con la retroalimentación del Avance 2
```

---

## Licencia

Proyecto académico. El código base de la ESP32-CAM proviene de los ejemplos de
Edge Impulse bajo licencia MIT; ver [`FUENTES.md`](FUENTES.md).
