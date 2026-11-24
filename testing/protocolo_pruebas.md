# Protocolo de pruebas

Documenta cómo se validó el sistema, con qué criterio de éxito y qué resultado
se obtuvo. Las pruebas marcadas **pendiente** hay que ejecutarlas y completar.

---

## P1 — Validación de módulos por separado

**Objetivo.** Confirmar que cada componente responde individualmente antes de
integrar, para que una falla en el sistema completo tenga un solo sospechoso.

**Condiciones.** Cada módulo conectado solo, alimentado por el USB del DevKit,
con su sketch de prueba correspondiente en `firmware/pruebas/`.

**Procedimiento.**
1. Cargar el sketch de prueba del módulo
2. Abrir el monitor serial a 115200
3. Estimular el módulo y observar la respuesta

**Criterio de éxito.** Cada módulo responde al estímulo de forma consistente en
al menos cinco repeticiones.

**Resultados.**

| Módulo | Estímulo | Resultado esperado | Resultado | Estado |
|---|---|---|---|---|
| LED RGB | Pulsación del botón | Secuencia azul → verde → rojo → apagado | Correcto en las cuatro transiciones | ✅ |
| Pulsador | Toque corto y largo | Corto dispara lectura, largo recorre colores | Correcto | ✅ |
| HC-SR04 | Objeto a 10 cm | Lectura cercana a 100 mm | Correcto | ✅ |
| Módulo microSD | Escritura de CSV | Archivo creado y leído de vuelta | `TODO CORRECTO` | ✅ |
| ESP32-CAM | Objeto frente al lente | Clasificación con confianza | Correcto | ✅ |

**Falla encontrada y resuelta.** El DevKit quedaba flojo en la protoboard y los
pines no hacían contacto, lo que producía fallos intermitentes sin patrón. Se
resolvió asentando la placa a fondo sobre los headers. Es la causa del 100 % de
los fallos aparentemente aleatorios durante esta etapa.

---

## P2 — Enlace UART entre placas

**Objetivo.** Verificar que la ESP32-CAM transmite y el DevKit recibe.

**Condiciones.** Ambas placas alimentadas, GPIO 13 de la cámara a D26 del
DevKit, tierras unidas.

**Procedimiento.**
1. Cargar `firmware/pruebas/` en modo emisor/receptor con un dato fijo
2. Observar el monitor del DevKit

**Criterio de éxito.** El receptor imprime la línea enviada, sin caracteres
perdidos, durante al menos 30 transmisiones seguidas.

**Resultado.** ✅ Recepción correcta.

**Falla encontrada y resuelta.** Un primer intento de enlace bidireccional
—DevKit pidiendo la clasificación por GPIO 14— no llegó a funcionar en el tiempo
disponible. Se optó por el enlace unidireccional, que ya estaba validado: la
cámara clasifica de forma continua y el DevKit consume el resultado cuando el
sensor confirma presencia. La lógica de fusión no cambia; cambia quién inicia la
captura.

---

## P3 — Ciclo completo integrado *(arquitectura del Avance 2 — superada)*

> **Esta prueba validó el sistema anterior, no el actual.** Se conserva porque
> documenta que la cadena de hardware completa funcionó de punta a punta, y
> porque explica por qué el sistema cambió. El ciclo integrado de la
> arquitectura vigente se prueba en **P9**.

**Objetivo (original).** Verificar la cadena completa del sistema de
clasificación: detección → clasificación → fusión → indicación → registro.

**Condiciones.** Sistema completo montado, DevKit alimentado por USB, cámara
alimentada desde el riel de 5 V, microSD insertada antes de encender.

**Resultado.** ✅ Ciclo completo verificado el 27 de octubre de 2026. Salida real:

```
========== CICLO 1 ==========
  Sensor    : objeto a 152 mm
  Vision    : plastico (0.87)
  Fusion    : distancia OK | confianza OK
  RESULTADO : plastico  [DETECCION VALIDA]
  Registro  : guardado en microSD
```

**Qué se demostró, y sigue vigente.** El ultrasónico mide, la cámara clasifica
sobre la placa, las dos placas se comunican, el LED responde y la microSD
registra. Ese encadenamiento de hardware es el mismo del sistema actual y no
hubo que volver a validarlo.

**Por qué se abandonó esta arquitectura.** La regla de fusión de esta prueba
—presencia confirmada *y* confianza sobre umbral— entrega como salida **el
material del objeto**. P5 mostró que esa salida no es confiable con objetos
fuera del dataset, y el Avance 1 había preguntado *cuánta* basura se acumula, no
de qué está hecha. La arquitectura actual invierte los roles: el ultrasónico
pasó a ser el sensor cuantitativo primario y la cámara pasó a validar que lo
medido sea residuo. Ver P9.

---

## P4 — Precisión del modelo sobre objetos conocidos

**Objetivo.** Medir la precisión del clasificador sobre objetos del mismo tipo
que los del dataset.

**Resultado en Edge Impulse (model testing, fotos nunca vistas por el modelo):**

| Clase | Precisión | F1 |
|---|---|---|
| carton | 100 % | 0,95 |
| metal | 92,9 % | 0,96 |
| plastico | 100 % | 1,00 |
| vacio | 100 % | 1,00 |

Precisión global: **98 %** · Área bajo la curva ROC: **1,00**

**Qué significa esta cifra en la arquitectura actual.** El 98 % mide la
capacidad de distinguir *entre materiales*, que es lo que el sistema dejó de
necesitar. La decisión que el sistema sí toma hoy depende de una sola frontera:
**`vacio` contra todo el resto**.

Esa frontera es la que mejor puntuó de las cuatro: `vacio` obtuvo **100 % de
precisión y F1 = 1,00**. O sea, la clase de la que ahora depende la validación
es precisamente la más confiable del modelo, y no la que P5 muestra frágil.

Es un resultado afortunado, pero no accidental: distinguir *hay un objeto* de
*no hay nada* es una tarea visual mucho más gruesa que distinguir acero de
plástico a 96×96 píxeles.

---

## P5 — Generalización a objetos fuera del dataset

**Objetivo.** Medir qué pasa con objetos que el modelo nunca vio.

**Procedimiento.** Presentar objetos de cada material distintos a los
fotografiados para el dataset, y registrar la clase reportada.

**Resultados.**

| Objeto | Clase real | Clase reportada | Confianza | Correcto |
|---|---|---|---|---|
| Botella plástica transparente | plastico | plastico | 0,87 | ✅ |
| Botella térmica de acero | metal | plastico | 0,80 | ❌ |
| Caja de cartón distinta | carton | metal | — | ❌ |
| Shaker de vidrio | (fuera de clases) | plastico | — | esperado |

**Análisis.** El modelo aprendió los objetos específicos del dataset, no las
categorías de material. A 96×96 píxeles la **geometría domina sobre el
material**: una botella térmica de acero comparte silueta con las botellas
plásticas del entrenamiento, y el brillo especular del acero a esa resolución se
parece al del plástico brillante.

La causa raíz es la composición del dataset: cada clase tenía esencialmente un
tipo de objeto. El 98 % de P4 mide *"¿reconoce estos objetos?"*, no *"¿reconoce
este material?"*.

El caso del vidrio es distinto: esa clase fue **eliminada deliberadamente** del
dataset tras comprobar que a esa resolución no se distingue del plástico
transparente. Que un objeto de vidrio se reporte como plástico es el
comportamiento esperado de un modelo de cuatro clases, no un error.

**Segundo hallazgo: el dataset no comparte punto de vista con la operación.**
Las fotos se tomaron con la cámara apuntando a los objetos sobre una mesa,
aproximadamente de frente. El dispositivo en operación mira hacia abajo. Como
este mismo análisis concluye que el modelo aprendió la silueta, el cambio de
punto de vista afecta justamente lo que aprendió: un objeto visto desde arriba
tiene una silueta distinta a la del entrenamiento.

Esto agrava el resultado de P5 para la clasificación de materiales, y es otra
razón por la que el sistema dejó de depender de ella. El efecto sobre la
frontera `vacio` / `hay algo` se mide en **P9, paso 0**.

**Mejora propuesta.** Ampliar el dataset a 5–8 objetos físicamente distintos por
categoría, fotografiados con la misma cámara **y desde la posición real de
montaje**, mirando hacia abajo. La precisión reportada probablemente baje de 98 % a 88–92 %, y ese
número más bajo será más honesto: medirá generalización y no memorización.

**Segunda medición, 19-nov-2026, con el modelo de 3 clases.** Tras reentrenar
agrupando los materiales en `residuo` y agregando la clase `otro`, el
comportamiento en terreno sigue siendo limitado: la confianza con objetos
reales se mueve en torno a **0,63-0,64**, y objetos no vistos en entrenamiento
—un teléfono— no se reconocen como `otro`. Ver el paso 0 de P9 para el detalle.

La causa está identificada: el split automático de Edge Impulse reparte fotos
casi idénticas entre entrenamiento y prueba, así que la métrica reportada mide
memorización. Un dataset honesto exige separar los conjuntos **por sesión de
captura**, no al azar.

**Consecuencia de diseño: esta prueba es la que cambió el sistema.** Un
clasificador que no generaliza entre materiales no puede ser la base de ninguna
decisión. En lugar de insistir en mejorarlo, se le cambió la función: la cámara
ya no responde *de qué material es esto*, sino *hay algo o no hay nada*, que es
la frontera que P4 muestra con F1 = 1,00.

Los tres errores de la tabla de arriba son todos errores *entre materiales*. En
los cuatro casos el modelo acertó que **había un objeto** — ninguno se reportó
como `vacio`. Bajo la arquitectura actual, esas cuatro mediciones se habrían
validado correctamente como residuo.

Esto baja la urgencia de reentrenar: el material pasó a ser un dato
complementario que se registra, no una base de decisión. Reentrenar sigue siendo
una mejora real, pero ya no es un requisito para que el sistema funcione.

---

## P6 — Autonomía y estabilidad ✅

**Fecha:** 20 de noviembre de 2026
**Archivo analizado:** `testing/datos/registro.csv`

**Objetivo.** Verificar que el sistema opera sin intervención y que la serie de
datos queda completa, sin huecos ni reinicios.

### Sesión evaluada

La tarjeta contiene 13 sesiones, separadas por los reinicios de placa que
impone cada carga de firmware durante el desarrollo. Se evalúa la **más larga y
la única con el firmware definitivo**:

| | |
|---|---|
| Inicio | 2026-11-20 15:50:10 |
| Fin | 2026-11-20 16:25:44 |
| Duración | **35,6 minutos** |
| Reinicios durante la sesión | **0** |

### Completitud de la serie

| Métrica | Valor |
|---|---|
| Filas esperadas (35,6 min ÷ 5 s) | 427 |
| Filas registradas | **425** |
| Completitud | **99,5 %** |

Dos muestras ausentes sobre 427. El criterio era menos del 2 % de pérdida; se
obtuvo 0,5 %.

### Regularidad del muestreo

| Métrica | Valor |
|---|---|
| Intervalo nominal | 5000 ms |
| Intervalo mediano | **5017 ms** |
| Intervalo mínimo | 3768 ms |
| Intervalo máximo | 12 848 ms |
| Saltos sobre 8 s | 1 de 424 |
| Saltos sobre 15 s | **0** |

La mediana se desvía 17 ms del nominal sobre media hora de operación: el lazo
de muestreo es estable.

**El salto único de 12,8 s tiene explicación conocida.** Las operaciones
bloqueantes del ciclo son la consulta a la cámara, que espera hasta 3 s, y el
envío de un aviso por Telegram, que abre una conexión TLS. Cuando ambas
coinciden en la misma muestra, el intervalo se estira. No es una falla: es el
costo declarado de esas dos funciones, y por eso el criterio de la prueba
admitía saltos de hasta 8 s como normales y reservaba la sospecha para los de
15 s o más, de los cuales no hubo ninguno.

### Registro en microSD

El archivo se escribió de forma continua durante toda la sesión, sin pérdida de
tarjeta ni corrupción. **Esto es también la evidencia del criterio de
repositorio de datos**: un histórico accesible en CSV estructurado, con marca
temporal real sincronizada por NTP.

### Hallazgo: la capa de visión no objetó ninguna medición

De las 425 muestras:

| Estado | Muestras |
|---|---|
| Confirmada | 301 |
| Sin confirmar | 124 |
| **Objetada** | **0** |

En media hora de operación la cámara **nunca** rechazó una medición. Cuando
reportó clase lo hizo con confianza alta y consistente —301 clasificaciones,
mediana 0,97, mínimo 0,92— pero siempre como `residuo`.

Esto confirma cuantitativamente lo que P9 midió de forma puntual: la capa de
visión confirma presencia de material de manera fiable, y en la práctica no
ejerce su capacidad de veto. El sistema funcionó, durante toda la sesión, con
la medición del ultrasónico como única fuente efectiva.

### Nota sobre el archivo

El CSV conserva además registros de versiones anteriores del firmware, incluido
el formato de cuatro columnas del Avance 2. Se dejaron deliberadamente: son el
registro de la evolución del proyecto. El análisis se hizo solo sobre las 846
filas del formato vigente de diez columnas.

---

## P7 — Validación contra el problema del Avance 1 *(no realizada)*

**Objetivo.** Demostrar que los datos capturados permiten calcular una
frecuencia de recolección, que es la pregunta que planteaba el Avance 1.

### Por qué no se pudo con los datos existentes

La sesión de 35,6 minutos de P6 contiene **cero tramos de acumulación
sostenida**: el análisis del CSV no encontró ninguna secuencia de tres o más
muestras consecutivas con nivel creciente.

La razón es el propósito de esa sesión. Fue una batería de pruebas de
instrumentación —P8, P9, P10, P11— en la que los objetos se colocaron y
retiraron de golpe para medir exactitud, validación y notificaciones. La curva
resultante son escalones verticales, no una acumulación.

Calcular un ritmo en mm/h sobre esa curva daría cifras como las que el propio
dashboard reportó durante las pruebas, del orden de 2000 mm/h, que son
correctas aritméticamente y carentes de sentido físico.

### Qué haría falta

Una corrida de **al menos 30 minutos** agregando pequeñas cantidades cada 5
minutos, de modo que el nivel ascienda por escalones de 20 a 30 mm. Con seis
puntos de una curva monótona creciente se obtiene un ritmo defendible, y de ahí
las horas hasta el 100 % y la frecuencia de recolección implicada.

### Lo que sí se puede afirmar

El sistema **registra todo lo necesario** para ese cálculo: nivel validado con
marca temporal real cada 5 segundos, con 99,5 % de completitud comprobada en
P6. La limitación no está en el instrumento sino en que no se ejecutó un
escenario de acumulación gradual.

---

## P8 — Exactitud de la medición de altura ✅

**Fecha:** 20 de noviembre de 2026
**Montaje:** HC-SR04 perpendicular, cero calibrado en **548 mm** sobre la zona
despejada. Alimentación a 3,3 V.

**Objetivo.** Caracterizar el error del método de medición de altura, para poder
declarar con qué incertidumbre se reporta una acumulación.

---

### Resultado 1 — Exactitud con blanco plano

Se colocó una cartulina rígida de unos 15 × 15 cm sobre cada objeto, para
presentar al sensor una superficie plana, horizontal y más ancha que el cono.

| Altura real | Altura reportada | Error |
|---|---|---|
| 67 mm | 61 mm | −6 mm |
| 102 mm | 104 mm | +2 mm |
| 140 mm | 142 mm | +2 mm |

- **Error medio:** −0,7 mm
- **Error absoluto medio:** 3,3 mm
- **Peor caso:** 6 mm en la serie estable, y 10 mm en una colocación aislada del
  blanco de 67 mm, atribuida a basculación de la cartulina sobre un objeto
  angosto

**Conclusión.** Con un blanco adecuado, el error cabe dentro de **±6 mm**, del
orden de un paso de resolución del sensor. En las muestras consecutivas de una
misma colocación la lectura no varió, o varió 4 mm — exactamente un paso de
cuantización.

---

### Resultado 2 — Comportamiento con objetos angostos o irregulares

Los mismos objetos, **sin** la cartulina encima:

| Altura real | Altura reportada | Error | Proporción |
|---|---|---|---|
| 57 mm | 43 mm | −14 mm | 0,75 |
| 130 mm | 94 mm | −36 mm | 0,72 |

**El sensor subestima la altura en aproximadamente un 26 %**, y el error crece
en proporción a la altura, no como una constante.

**Explicación física.** El HC-SR04 no reporta el punto más alto del objeto:
reporta la **primera superficie que le devuelve un eco suficientemente fuerte**
dentro de su cono. Si la cara superior es más pequeña que el cono —a 450 mm el
cono mide unos 12 cm de diámetro— esa cara dispersa el pulso, y el eco que
vuelve con energía suficiente proviene de una parte más ancha y más baja del
objeto. El sensor informa esa superficie.

---

### Por qué esto importa para el proyecto

Un cúmulo de residuos reales es irregular por definición: no es un bloque con
tapa plana. **Esta prueba mide, por lo tanto, la condición real de operación, no
un caso de laboratorio.**

Corrige además un supuesto que el proyecto tenía mal enunciado. La altura
reportada **no es la del punto más alto del cúmulo**, como se afirmaba antes:
es la de la superficie reflectante dominante dentro del cono, que en un montón
irregular queda por debajo del pico.

La consecuencia es que el sistema **subestima sistemáticamente la acumulación
real**. Para el caso de uso eso es conservador en la dirección correcta: la
alerta de recolección se dispara más tarde, no antes, y nunca avisa por un
cúmulo que no existe.

### Justificación de la banda muerta

Este resultado respalda con dato propio el valor `RUIDO_MM = 20` fijado en el
firmware: con la zona despejada se observaron oscilaciones de hasta 16 mm
respecto al cero calibrado, y la resolución del sensor es de 4 mm por paso. Una
banda muerta de 2 cm deja fuera el ruido sin sacrificar nada útil, porque 2 cm
de residuos no constituyen una acumulación que valga la pena reportar.

### Mejora propuesta

Un segundo sensor ultrasónico, o uno montado sobre un eje que barra la zona,
permitiría construir un perfil de alturas en vez de un solo punto, y con eso
estimar el volumen sin depender del factor de ocupación supuesto. Es la
limitación de fondo del diseño actual y está declarada como tal en el
`README.md`.

---

## P9 — Validación de la medición con la cámara ✅

**Fecha:** 20 de noviembre de 2026
**Montaje:** geometría fija, cero calibrado en 548 mm, `CONFIANZA_MIN = 0,80`.

**Objetivo.** Determinar qué puede y qué no puede validar la cámara sobre la
medición del ultrasónico.

---

### Resultado 1 — Confirmación de material presente: funciona

Nueve consultas consecutivas durante la sesión de pruebas, registradas por el
dashboard con su hora:

| Hora | Clase | Confianza | Altura |
|---|---|---|---|
| 16:00:47 | residuo | 0,97 | 43 mm |
| 16:01:42 | residuo | 0,93 | 43 mm |
| 16:03:26 | residuo | 0,96 | 121 mm |
| 16:05:26 | residuo | 0,95 | 102 mm |
| 16:06:02 | residuo | 0,96 | 177 mm |
| 16:06:27 | residuo | 0,97 | 57 mm |
| 16:07:28 | residuo | 0,94 | 130 mm |
| 16:08:27 | residuo | 0,92 | 61 mm |
| 16:09:32 | residuo | 0,97 | 98 mm |

**9 de 9 correctas, con confianza entre 0,92 y 0,97**, todas sobre el umbral de
0,80. La cámara confirma de forma fiable que hay material en la zona.

Esto contrasta con la medición preliminar del 19 de noviembre, donde la
confianza se movía entre 0,63 y 0,64 con el montaje suelto. **La diferencia es
la geometría fija:** con la cámara en posición estable y el objeto encuadrado
igual que en el entrenamiento, el modelo alcanza certeza.

---

### Resultado 2 — Distinción entre residuo y no-residuo: falla

**Procedimiento.** Se colocó una mano en la zona de observación, dos veces.

**Resultado.** En ambas la cámara reportó `residuo`. Ninguna reportó `otro`.

Lo mismo había ocurrido el 19 de noviembre con un teléfono.

**El paso del protocolo que buscaba demostrar el rechazo de un objeto ajeno
—la pregunta "¿y si pasa un auto?"— no se cumple.**

**Diagnóstico.** La clase `otro` se entrenó con 81 fotografías, pero de muy
pocos objetos distintos —una mano, un perro, una sandalia, un vehículo de
juguete— y todas capturadas en una sola sesión. A 96 × 96 píxeles el modelo
memorizó esas instancias concretas en lugar de aprender la categoría «objeto
sólido que no es un residuo». Es el mismo mecanismo que P5 documentó para los
materiales.

**Mejora fundamentada.** Un dataset con muchos más objetos no-residuo
físicamente distintos, capturados en **sesiones separadas** y con el conjunto
de prueba dividido por sesión y no al azar. La división automática de Edge
Impulse reparte imágenes casi idénticas entre entrenamiento y validación, lo
que produjo la métrica optimista de 91,9 % que el comportamiento en terreno no
respalda.

---

### Qué valida realmente el sistema

| Capacidad | Estado | Evidencia |
|---|---|---|
| Distinguir zona ocupada de zona vacía | **Funciona** | 9/9, confianza 0,92–0,97 |
| Distinguir residuo de otro objeto sólido | **Falla** | Mano clasificada como residuo, 2/2 |

La capa de visión filtra **ecos falsos del ultrasónico**, que es un aporte real
y medible. No filtra objetos ajenos que ocupen físicamente la zona.

### Consecuencia de diseño, y por qué el sistema aguanta

Esta limitación motivó el cambio de lógica del 19 de noviembre: **la medición
del ultrasónico cuenta por defecto y la cámara solo puede objetar cuando
alcanza certeza.** Bajo esa regla, que la cámara no reconozca un objeto ajeno
produce el mismo resultado que si no hubiera cámara: se registra la medición
física, que es honesta.

Si la regla fuera la inversa —contar solo lo confirmado— un modelo con esta
limitación habría dejado al sistema sin registrar acumulaciones reales.

**Se corrigió también el texto del dashboard.** Antes afirmaba «la cámara ve
residuos»; ahora dice «la cámara detecta material», que es lo único que la
evidencia respalda. Una interfaz no debe afirmar lo que el sistema no puede
demostrar.

---

## P10 — Dashboard en vivo ✅

**Fecha:** 20 de noviembre de 2026
**Evidencia:** `testing/evidencia/P10-dashboard-indicadores.png` y
`P10-dashboard-grafico-y-caracterizaciones.png`

**Objetivo.** Verificar que el dashboard servido por el propio dispositivo es
accesible desde un teléfono y refleja el estado en tiempo real.

### Resultados

| Verificación | Resultado |
|---|---|
| Acceso desde el teléfono por la red del dispositivo | ✅ `http://172.20.10.2` |
| Actualización sin recargar la página | ✅ cada 2 s |
| Nivel validado y porcentaje de llenado | ✅ |
| Barra con el umbral de recolección marcado | ✅ en 70 % |
| Ritmo de acumulación y volumen estimado | ✅ |
| Indicador de confirmación visual | ✅ con la clase y su confianza |
| Curva de evolución con la línea del umbral | ✅ 10 min de serie visibles |
| Tabla de caracterizaciones con hora, clase, confianza y altura | ✅ 9 entradas |
| Supuestos declarados al pie | ✅ área, factor de ocupación y cero calibrado |

### Lo que el dashboard deja a la vista

La captura de los indicadores muestra el principio del diseño: junto al número
grande aparece **de dónde sale**. El volumen estimado no se presenta como una
medición, sino acompañado de los dos supuestos que lo producen —área de la zona
y factor de ocupación— y del cero calibrado. Quien lee la cifra puede juzgarla.

El pie reporta además el estado operativo: tiempo encendido, si está
registrando en microSD, y cuántas notificaciones se han enviado.

### Corrección derivada de esta prueba

Durante la revisión se observó que el indicador de confirmación visual mostraba
«Confirmado — la cámara ve residuos» con el nivel en 4 mm, porque conservaba el
último estado aunque la zona ya estuviera despejada. Se corrigió: ahora muestra
**«Sin material»** cuando la altura validada cae bajo la banda de ruido, y el
texto pasó de «ve residuos» a «detecta material», acotándolo a lo que P9
demostró.

### No verificado

Dos puntos del procedimiento original quedaron sin medir: el comportamiento con
**dos dispositivos conectados simultáneamente**, y una sesión de **10 minutos
sin caídas de conexión**. El dashboard operó sin interrupciones durante las
sesiones de prueba, pero no se cronometró como criterio.

---

## P11 — Notificaciones por Telegram ✅

**Fecha:** 20 de noviembre de 2026
**Configuración:** bot propio, placa conectada por hotspot con salida a
internet, umbral de alerta 70 %, rearme bajo 55 %.

**Objetivo.** Verificar que el aviso llega, que llega una sola vez por cruce, y
que el ciclo se cierra cuando el nivel se normaliza.

### Resultados

| Hora | Mensaje | Nivel |
|---|---|---|
| 16:03 | RECOLECCION REQUERIDA | 121 mm · 80 % |
| 16:05 | Nivel normalizado | 0 mm · 0 % |
| 16:06 | RECOLECCION REQUERIDA | 177 mm · 100 % |
| 16:06 | Nivel normalizado | 0 mm · 0 % |
| 16:07 | RECOLECCION REQUERIDA | 130 mm · 86 % |
| 16:08 | Nivel normalizado | 4 mm · 2 % |

**Criterio de éxito: cumplido.** Tres cruces del umbral, tres avisos, tres
normalizaciones. **Ningún mensaje repetido dentro de un mismo cruce**, lo que
confirma que la histéresis funciona: una vez disparada la alerta, el nivel tiene
que bajar del 55 % para que el sistema vuelva a armarse.

La latencia entre el cruce y la llegada del mensaje fue inferior a la ventana de
muestreo de 5 segundos en los tres casos.

### Dos correcciones derivadas de esta prueba

**El ritmo ahora informa su ventana.** Los avisos reportaban cifras como
2224 mm/h, que son matemáticamente correctas pero engañosas: provienen de
colocar un objeto de golpe sobre una ventana de pocos minutos, y la
extrapolación a una hora produce un número absurdo. El mensaje ahora incluye
los minutos que abarca la serie, para que la cifra se lea en contexto. En
operación real, con acumulación gradual sobre horas, el valor es
representativo — ver P7.

**La afirmación sobre la cámara se acotó.** El mensaje decía «Confirmado
visualmente por camara». Tras el resultado 2 de P9, que mostró que la cámara no
distingue un residuo de una mano, el texto pasó a «Presencia de material
confirmada por camara», que es lo único que la evidencia respalda.

### Contenido del aviso

```
RECOLECCION REQUERIDA

Zona de observacion 1
Nivel: 130 mm (86% de capacidad)
Ritmo: 1070 mm/h (ventana 8 min)
Volumen estimado: 1378 cm3
Material predominante: residuo (0.94)
Presencia de material confirmada por camara

20-11-2026 16:07
```

### Limitación declarada

La conexión depende de un hotspot de teléfono. Si el teléfono se apaga o se
queda sin batería, la placa pierde la salida a internet y los avisos no se
envían; el registro en microSD y la medición continúan sin verse afectados. En
una instalación real el dispositivo usaría una red fija o un módulo celular
propio.


---

## Síntesis: qué responden estos resultados al problema del Avance 1

El Avance 1 planteó que los microbasurales crecen sin que exista información
sobre **cuánto** material se acumula ni **dónde**, y que sin esa información no
se puede priorizar la recolección ni medir si una intervención sirvió. Esta
sección recorre esa formulación punto por punto contra lo que las once pruebas
midieron.

### 1. Medir cuánto hay, sin que nadie vaya a mirar

**Respondido, con margen de error conocido.**

P8 midió el error del instrumento contra objetos de altura conocida: **−0,7 mm
de media y 3,3 mm en valor absoluto** sobre blanco plano, con 6 mm en el peor
caso. Frente a una capacidad de 150 mm, eso es un error bajo el 5 %: suficiente
para decidir si conviene mandar un camión.

El mismo P8 encontró el límite: sobre objetos de tope angosto la lectura cae un
**26 % sistemático**, porque el HC-SR04 informa la primera superficie que
devuelve eco fuerte dentro de un cono de 15°, no el punto más alto del cúmulo.
Un cúmulo real es irregular, así que **el sistema subestima**. Para la decisión
que importa —¿hay que recolectar?— errar por abajo es el lado seguro: avisa
tarde, no en falso.

### 2. Que el dato sea continuo y sobreviva a la sesión

**Respondido.**

P6 corrió 35,6 minutos sin intervención: **425 de 427 muestras esperadas, 99,5 %
de completitud**, intervalo mediano de 5017 ms sobre 5000 nominales y **cero
reinicios**. Cada fila va a la microSD con marca temporal real sincronizada por
NTP, así que el histórico no depende de que haya conexión. La red se usa para el
dashboard y el aviso, nunca para medir ni para guardar.

Esto es exactamente lo que el Avance 1 echaba de menos: un registro consultable
en vez de reportes esporádicos.

### 3. Saber que lo medido es basura y no cualquier cosa

**Respondido a medias, y el límite está medido.**

P9 puso nueve objetos distintos frente al equipo: la cámara confirmó material en
**9 de 9, con confianzas entre 0,92 y 0,97**. Pero en las dos pruebas con una
mano dentro de la zona, la clasificó como residuo **las dos veces**.

La conclusión honesta es que la capa de visión **confirma presencia de material,
no lo distingue de un objeto ajeno**. P6 lo respalda desde el otro lado: en 425
muestras la cámara **objetó cero veces**. La causa está identificada —la clase
`otro` se entrenó con pocos objetos distintos en una sola sesión— y la
corrección es dataset, no arquitectura.

Por eso el sistema está construido al revés de lo que parecería natural: la
medición del ultrasónico cuenta por defecto y la cámara solo puede **objetar**.
Si la visión falla, el equipo degrada a un monitor ultrasónico que sigue
sirviendo, en vez de dejar de medir.

### 4. Priorizar la recolección

**Parcialmente respondido.**

P11 demostró el mecanismo completo: **tres cruces de umbral, tres avisos, tres
normalizaciones, sin duplicados**. El dispositivo decide solo cuándo avisar y no
satura a quien recibe el mensaje. P10 mostró el dashboard con el histórico y los
indicadores en vivo.

Lo que **no** está demostrado es el paso siguiente: convertir esa curva en una
*frecuencia* de recolección. P7 explica por qué —las sesiones existentes son
escalones de instrumentación, no acumulación gradual— y qué haría falta: una
corrida de 30 minutos agregando material por tramos. El instrumento registra
todo lo necesario para ese cálculo; lo que falta es el escenario, no la
capacidad.

### 5. Medir si una intervención funcionó

**No demostrado.** Requiere lo mismo que el punto anterior más una línea base de
varios días, fuera del alcance de este avance.

### Resumen

| Pregunta del Avance 1 | Estado | Evidencia |
|---|---|---|
| ¿Cuánto material hay? | Respondido | P8: ±3,3 mm; −26 % en cúmulos irregulares |
| ¿El dato es continuo y persistente? | Respondido | P6: 99,5 %, 0 reinicios |
| ¿Es basura o es otra cosa? | A medias | P9: confirma presencia; no distingue una mano |
| ¿Cuándo conviene recolectar? | Mecanismo sí, ritmo no | P10, P11 ✅ · P7 pendiente |
| ¿Sirvió la intervención? | No demostrado | Requiere línea base de días |

Tres de las cinco preguntas tienen respuesta con dato propio. Las dos que no,
están acotadas: se sabe qué falta, por qué falta y cuánto costaría cerrarlas.
