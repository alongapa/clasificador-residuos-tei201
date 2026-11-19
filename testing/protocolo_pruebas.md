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

## P6 — Autonomía y estabilidad *(pendiente)*

**Objetivo.** Verificar que el sistema opera sin intervención durante un periodo
prolongado, y que la serie de datos queda completa y sin huecos.

**Condiciones.** Sistema montado y calibrado, microSD insertada antes de
encender, alimentación estable por USB. El registro es continuo: una fila cada
5 segundos, haya o no material nuevo.

**Procedimiento.**
1. Calibrar el cero con la zona vacía y anotar la hora de inicio
2. Dejar el sistema corriendo **2 horas sin tocarlo**, agregando material cada
   15 o 20 minutos para que la curva tenga forma
3. Al terminar, sacar la microSD y abrir el CSV

**Criterio de éxito.**
- Cero reinicios en las 2 horas.
- El CSV tiene aproximadamente **1440 filas** (2 h ÷ 5 s), con menos del 2 % de
  filas faltantes.
- Los `tiempo_ms` son monótonamente crecientes y los saltos entre filas
  consecutivas están cerca de 5000 ms. Un salto mucho mayor indica un reinicio o
  una pausa por consulta a la cámara.

**Resultados.**

| Métrica | Valor |
|---|---|
| Duración real de la corrida | |
| Filas en el CSV | |
| Filas esperadas (duración ÷ 5 s) | |
| Reinicios detectados | |
| Salto máximo entre filas (ms) | |
| Mediciones validadas / descartadas / sin confirmar | |

**Nota sobre los saltos.** Cada consulta a la cámara bloquea hasta 3 segundos,
así que una fila puede tardar hasta 8 s en lugar de 5. Eso es comportamiento
esperado, no una falla. Un salto de 15 s o más sí es sospechoso.

**Esta prueba alimenta a P7.** El CSV que sale de acá es el insumo del análisis
siguiente, así que conviene correrlas encadenadas.

---

## P7 — Validación contra el problema del Avance 1 *(pendiente)*

**Objetivo.** Demostrar que los datos capturados responden al problema original
y permiten tomar una decisión, que es la prueba de que el artefacto sirve para
algo y no solo funciona.

**Insumo.** El CSV de la corrida de 2 horas de P6.

**Procedimiento.**
1. Abrir el CSV en Excel y graficar `altura_validada_mm` contra `hora`
2. Calcular el ritmo de acumulación promedio en mm/h sobre la corrida completa
3. Con ese ritmo y la capacidad de referencia, estimar **en cuántas horas se
   llenaría la zona desde vacía**
4. Contar cuántas filas tienen `validacion = 0`, o sea mediciones que el
   ultrasónico registró y la cámara descartó

**Lo que hay que escribir, con los números de arriba:**

- **La pregunta del Avance 1 que esto responde.** El Avance 1 preguntaba cuánta
  basura se acumula en los microbasurales y a qué ritmo, porque sin eso no se
  puede dimensionar una frecuencia de recolección.
- **La decisión que permite tomar.** Con el ritmo medido, cada cuántos días
  habría que pasar a recolectar esa zona para que no se desborde.
- **El valor de la columna de validación.** Cuántas mediciones falsas se
  filtraron. Ese número es la justificación cuantitativa de que la cámara esté
  en el prototipo.

**Resultados.**

| Métrica | Valor |
|---|---|
| Ritmo promedio medido (mm/h) | |
| Horas estimadas para llegar al 100 % desde vacío | |
| Frecuencia de recolección que eso implica | |
| Filas con `validacion = 0` (descartadas) | |
| Porcentaje de mediciones descartadas | |

**Advertencia de honestidad.** Dos horas en una maqueta no permiten extrapolar a
un microbasural real: el ritmo de acumulación en la maqueta lo determina quien
va poniendo la basura, no un fenómeno urbano. Lo que la prueba demuestra es que
**el método de medición entrega los datos necesarios** para calcular esa
frecuencia, no cuál es la frecuencia real. Hay que decirlo así en la
presentación, porque es la objeción obvia.

---

## P8 — Exactitud de la medición de altura *(pendiente)*

**Objetivo.** Verificar que la altura calculada corresponde a la altura real del
cúmulo, y acotar el error del método.

**Condiciones.** Dispositivo montado y calibrado sobre la zona vacía. Una regla
o pie de metro como patrón.

**Procedimiento.**
1. Calibrar el cero con la zona despejada
2. Colocar un objeto de altura conocida en el centro de la zona
3. Anotar la altura real medida con la regla y la que reporta el dashboard
4. Repetir con cinco alturas distintas entre 10 y 140 mm
5. Repetir una de las alturas cinco veces seguidas, para medir repetibilidad

**Criterio de éxito.** Error absoluto bajo 10 mm en todo el rango, y dispersión
bajo 5 mm en las repeticiones.

**Resultados.**

| Altura real (mm) | Altura reportada (mm) | Error (mm) |
|---|---|---|
| | | |
| | | |
| | | |
| | | |
| | | |

Repetibilidad a ___ mm: desviación de ___ mm en 5 mediciones.

**Nota sobre el método.** El ultrasónico devuelve la superficie más cercana de
su cono, así que para un cúmulo irregular reporta el punto más alto, no la
altura media. Hay que verificar cuánto se desvía con un cúmulo real frente a un
objeto plano.

---

## P9 — Validación de la medición con la cámara *(pendiente)*

**Objetivo.** Demostrar que el sistema distingue un cúmulo de residuos de
cualquier otro objeto que entre en el cono del sensor, y que una orden de
recolección no se dispara por un falso positivo.

Esta es la prueba central del Avance 3: es la que justifica que el prototipo
tenga una cámara.

**Condiciones.** Sistema completo, calibrado sobre la zona vacía, dashboard
abierto y monitor serial del DevKit a 115200 para seguir la decisión.

### Paso 0 — Verificación del punto de vista *(hacer esto primero)*

El dataset se fotografió de frente; la cámara en operación mira hacia abajo. Hay
que comprobar que la frontera `vacio` / `hay algo` sobrevive ese cambio **antes**
de correr el resto de la prueba, porque si no sobrevive, nada de lo que sigue
significa algo.

Con la cámara ya montada en su posición definitiva, mirando la zona, y su propio
monitor serial abierto a 115200:

| | Qué haces | Qué debe aparecer |
|---|---|---|
| 0a | Zona completamente vacía, mirar 10 clasificaciones seguidas | `vacio` con confianza **> 0,70** en al menos 8 de 10 |
| 0b | Poner una botella en la zona, mirar 10 clasificaciones | **Cualquier clase que no sea `vacio`**. El material da igual |
| 0c | Repetir 0b con una lata y con cartón | Igual: lo único que importa es que no diga `vacio` |

**Criterio de éxito.** La clase `vacio` aparece de forma estable con la zona
vacía, y desaparece de forma estable cuando hay material. El material que
reporte es irrelevante para este criterio.

**Si falla**, la validación no es viable con el modelo actual y hay que levantar
un **modelo binario** `vacio` / `residuo` fotografiado desde esta misma posición:
alrededor de 40 fotos por clase, dos clases en vez de cuatro. Es una tarde de
trabajo y es un problema mucho más fácil que discriminar materiales. Anotar el
resultado acá antes de decidir.

| Caso | Clase reportada (10 muestras) | Confianza típica | ¿Cumple? |
|---|---|---|---|
| 0a zona vacía | | | |
| 0b botella | | | |
| 0c lata | | | |
| 0c cartón | | | |

### Procedimiento principal

**Procedimiento.**

| # | Acción | Resultado esperado |
|---|---|---|
| 1 | Zona vacía, dejar correr 1 min | Altura validada 0 mm · LED verde |
| 2 | Poner un objeto **no residuo** bajo el sensor (una mano, una caja de herramientas, un auto de juguete en la maqueta) | El serial muestra la subida, consulta a la cámara, y reporta `VALIDACION: la camara no ve residuos -> medicion descartada`. El dashboard muestra la altura medida en la línea chica, la validada sigue en 0, y aparece el aviso azul de medición descartada |
| 3 | Retirar el objeto | Altura medida vuelve a 0, sin cambios en la validada |
| 4 | Poner **residuos reales** (botella, lata, cartón) | `VALIDACION: confirmada como residuo`. La altura validada toma el valor medido, el punto del gráfico sale verde, el material queda registrado |
| 5 | Agregar más residuos hasta pasar el 70 % | Se dispara la alerta: LED rojo, aviso en el dashboard y mensaje de Telegram que incluye la línea *Confirmado visualmente por camara* |
| 6 | Retirar todo | La altura validada baja siguiendo a la medida, sin consultar a la cámara. Bajo el 55 % llega el mensaje de recolección realizada |
| 7 | Repetir el paso 2 y **dejar el objeto 2 minutos** | La reconfirmación periódica vuelve a consultar y mantiene la medición descartada |

**Criterio de éxito.**
- Ningún objeto no-residuo produce altura validada mayor a 0 mm.
- Todo residuo real queda confirmado, sin importar el material que la cámara le
  asigne.
- La alerta de recolección solo se dispara en el paso 5.

**Resultados.**

| Paso | Lo que reportó el serial | Altura medida | Altura validada | ¿Correcto? |
|---|---|---|---|---|
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |
| 6 | | | | |
| 7 | | | | |

**Falsos positivos observados:** ___ de ___ intentos
**Falsos negativos observados:** ___ de ___ intentos

**Nota sobre el caso límite.** Si la cámara no responde o su confianza queda
bajo 0,50, el sistema reporta *sin confirmar* y conserva la última altura
validada. Hay que anotar cuántas veces ocurre, porque es el modo de falla del
diseño: el sistema no se equivoca, pero se queda sin saber.

---

## P10 — Dashboard en vivo *(pendiente)*

**Objetivo.** Verificar que el dashboard es accesible y refleja las detecciones
en tiempo real, en las condiciones de la presentación.

**Condiciones.** Sistema completo encendido, sin red de la sala disponible (modo
punto de acceso), un teléfono o notebook conectado a la red del dispositivo.

**Procedimiento.**
1. Encender y anotar la dirección que imprime el monitor serial
2. Conectar un dispositivo a la red `Clasificador-Residuos`
3. Abrir la dirección en el navegador
4. Agregar material y verificar que la curva del gráfico sube, con el punto de
   la muestra en **verde** (confirmada por la cámara)
5. Poner un objeto que no sea residuo y verificar que aparece el **aviso azul de
   medición descartada**, que el punto sale **rojo**, y que la línea chica bajo
   el número grande muestra la lectura cruda mientras el número grande no se
   mueve
6. Seguir agregando residuos hasta superar el umbral y verificar que salte la
   alerta
7. Comprobar que el indicador de **Confirmación visual** refleja el estado
   correcto en los tres casos: confirmada, descartada y sin confirmar
8. Repetir con un segundo dispositivo conectado al mismo tiempo

**Criterio de éxito.** La curva refleja el cambio en menos de 7 segundos (una
muestra), los indicadores cuadran con lo que imprime el monitor serial, y la
alerta se dispara al superar el umbral.

**Resultados.**

| Métrica | Valor |
|---|---|
| Latencia cambio → gráfico | |
| Dispositivos conectados simultáneamente | |
| Alerta disparada correctamente | |
| Aviso de medición descartada visible | |
| Puntos verdes y rojos correctos en el gráfico | |
| Caídas de conexión en 10 minutos | |

---

## P11 — Notificaciones *(pendiente)*

**Objetivo.** Verificar que el aviso de recolección llega, que llega una sola
vez por cruce de umbral, y que la histéresis evita avisos repetidos.

**Condiciones.** Placa conectada a una red con salida a internet (hotspot),
bot y grupo configurados, zona calibrada.

**Procedimiento.**
1. Calibrar con la zona vacía
2. Agregar material hasta superar el 70 % y cronometrar cuánto tarda el aviso
3. Mantener el nivel justo en el borde del umbral 3 minutos, agregando y
   quitando material, y contar cuántos mensajes llegan
4. Retirar material hasta bajar del 55 % y verificar el aviso de normalización
5. Volver a superar el umbral y verificar que avise de nuevo
6. **Prueba del falso positivo:** con la zona vacía y calibrada, poner un objeto
   que no sea residuo y que ocupe más del 70 % de la capacidad. **No debe llegar
   ningún mensaje**, porque la alerta se evalúa sobre la altura validada

**Criterio de éxito.** Un solo mensaje por cruce, el aviso llega en menos de 15
segundos, oscilar en el borde no genera mensajes adicionales, el mensaje incluye
la línea *Confirmado visualmente por camara*, y el paso 6 **no** genera aviso.

**Resultados.**

| Métrica | Valor |
|---|---|
| Latencia cruce → mensaje recibido | |
| Mensajes en 3 min oscilando en el borde | |
| Aviso de normalización recibido | |
| Rearme correcto tras segundo cruce | |
| Mensajes generados por el falso positivo (debe ser 0) | |
