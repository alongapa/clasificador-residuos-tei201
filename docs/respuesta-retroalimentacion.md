# Respuesta a la retroalimentación del Avance 2

Nota obtenida: **6,6 / 7,0**

El Avance 2 dejó dos observaciones de corrección y tres recomendaciones. Este
documento registra qué se hizo con cada una y, donde se decidió no seguir una
recomendación, por qué.

---

## Observaciones de corrección

### "Documentar el circuito"

**Resuelto.** Se produjo documentación completa del circuito: diagrama de
bloques, tablas de conexión por módulo, tabla completa de pines del DevKit con
la justificación de cada asignación, las restricciones de montaje encontradas en
terreno, y el listado de materiales.

Está en `hardware/esquematico.html` y en la sección *Conexiones* del `README.md`.

Entre las restricciones documentadas hay dos que no son obvias y que costaron
horas de depuración:

- **GPIO 12 de la ESP32-CAM es un pin de strapping de arranque** y debe quedar
  libre. El TX del enlace se movió a GPIO 13 por eso.
- **Los rieles de alimentación de una protoboard de 830 puntos están partidos al
  medio**: cada riel son dos segmentos independientes. La cámara alimentada en
  el agujero 25 y la fuente en el 29 están en segmentos distintos y no se
  conectan entre sí.

### "Falta el código"

**Resuelto.** El código completo está versionado en este repositorio, con el
firmware de ambas placas, los sketches de prueba por módulo, el protocolo de
pruebas y la declaración de fuentes (`FUENTES.md`), que detalla por archivo qué
generó una herramienta de IA, qué aportó el equipo y qué entiende el equipo de
cada función.

---

## Recomendaciones del profesor

### 1. "Seguir entrenando la IA"

**Atendida, pero resuelta por diseño en lugar de por más entrenamiento.**

El diagnóstico detrás de la recomendación es correcto y nosotros lo habíamos
medido: la prueba P5 del protocolo documenta que el modelo **no generaliza entre
materiales**. Confundió una botella térmica de acero con plástico y una caja de
cartón con metal. La causa está identificada: a 96×96 píxeles la geometría
domina sobre el material, y cada clase del dataset tenía esencialmente un solo
tipo de objeto. El 98 % que reporta Edge Impulse mide *"¿reconoce estos
objetos?"*, no *"¿reconoce este material?"*.

Frente a eso había dos caminos: mejorar el clasificador, o dejar de depender de
él. **Elegimos el segundo**, y es el cambio central del Avance 3.

El sistema ya no usa la cámara para decidir qué material es cada cosa. La usa
para **confirmar que lo que el ultrasónico midió son residuos**. El dato
cuantitativo lo entrega el sensor ultrasónico, que es físico y confiable; la
cámara solo responde una pregunta binaria: *¿hay material, o la zona está
vacía?*

Tres razones por las que esto es mejor que reentrenar:

1. **Usa la parte confiable del modelo.** La única frontera de la que el sistema
   depende ahora es `vacio` contra el resto, y esa clase obtuvo **100 % de
   precisión y F1 = 1,00** en el *model testing*. La frontera frágil era la que
   separa materiales entre sí, y ya no se usa para decidir nada.

2. **Los errores de P5 no habrían afectado al sistema actual.** Los tres errores
   registrados son errores *entre materiales*. En los cuatro casos el modelo
   acertó que **había un objeto**: ninguno se reportó como `vacio`. Bajo la
   arquitectura actual, esas cuatro mediciones se habrían validado
   correctamente.

3. **Responde la pregunta del Avance 1.** El Avance 1 preguntaba cuánta basura
   se acumula y a qué ritmo, para poder dimensionar una frecuencia de
   recolección. Nunca preguntó de qué material estaba hecha.

**Riesgo identificado sobre esta decisión.** El dataset se fotografió con la
cámara de frente a los objetos; el dispositivo en operación mira hacia abajo. Es
un cambio de punto de vista que el modelo no vio, y afecta justamente la silueta,
que es lo que P5 muestra que aprendió. El argumento de que la frontera `vacio` /
`hay algo` resiste ese cambio mejor que la discriminación de materiales es
razonable —responde a estructura y textura del cuadro, no a contorno— pero es un
argumento, no una medición. Se verifica en **P9 paso 0**, y si no se cumple, el
plan de contingencia es un modelo **binario** fotografiado desde la posición real
de montaje: dos clases, alrededor de 40 fotos cada una, una tarde de trabajo.

La vista cenital, en cambio, no se cambia: es la geometría del concepto
—un lector satelital observa a nadir— y el ultrasónico requiere estar
perpendicular para que la distancia sea la altura.

Reentrenar sigue siendo una mejora real y está especificada en P5 —ampliar a 5–8
objetos físicamente distintos por categoría, con la precisión reportada bajando
probablemente a 88–92 %, que sería un número más honesto porque mediría
generalización y no memorización. Pero dejó de ser un requisito para que el
sistema funcione.

### 2. "Hacer una maqueta pequeña de una calle con casas y basura en miniatura"

**No se implementó, por una razón técnica concreta.**

La maqueta con basura en miniatura obligaría a reentrenar el modelo desde cero.
El dataset actual se fotografió con basura de tamaño real, de cerca y con la
misma cámara del prototipo. Un objeto en miniatura a 96×96 píxeles no se parece
a lo que el modelo vio: el problema es el mismo que documenta P5, que la
geometría domina sobre el material a esa resolución.

Es decir, la maqueta no es un cambio de escenografía: arrastra un dataset nuevo
y un reentrenamiento completo, que es justamente el trabajo que la decisión
anterior concluyó que no era la mejor inversión del tiempo disponible.

La demostración se hace sobre una zona de observación delimitada de 15 × 15 cm
con residuos reales, que es la condición en que el modelo fue entrenado y en que
sus resultados son válidos.

**La maqueta queda identificada como el paso siguiente del proyecto**, con su
costo real declarado: dataset nuevo de 5–8 objetos miniatura por categoría,
reentrenamiento, y revalidación de P4, P5 y P9.

### 3. "Guardar las fotos o los datos en la nube, con notificación"

**Atendida parcialmente, con una sustitución deliberada.**

*Implementado:* **notificación por Telegram** cuando el nivel cruza el umbral de
recolección. El mensaje incluye el nivel, el porcentaje de llenado, el ritmo de
acumulación, el volumen estimado, el material predominante y la confirmación de
que la medición fue validada visualmente. Hay histéresis —avisa al cruzar el
70 %, y para volver a avisar el nivel tiene que bajar del 55 %— para que una
oscilación en el borde no genere una lluvia de mensajes. Se prueba en P11.

*Implementado:* **dashboard web servido por la propia placa**, accesible desde
cualquier dispositivo en la misma red, con la serie temporal completa. Se prueba
en P10.

*No implementado:* **subir las imágenes a la nube.** La ESP32-CAM tiene la
memoria justa para correr la inferencia; mantener un buffer de imagen completo
mientras se abre una conexión TLS y se hace un POST multipart es el camino
directo al desbordamiento de memoria que ya nos costó un día de depuración
(`cam_hal: FB-OVF`). Lo que sí se guarda de forma garantizada es el **registro
de datos en microSD**, continuo, una fila cada 5 segundos, que es el dato con el
que se reconstruye la curva de acumulación. Preferimos un registro que funciona
siempre a una transmisión de imágenes que compromete la estabilidad del
dispositivo.

---

## Resumen de la decisión de fondo

El Avance 2 entregaba un clasificador de materiales que funcionaba en
demostración y era frágil en terreno. El Avance 3 entrega un medidor de
acumulación que usa la visión por computador para validar sus propias
mediciones.

El cambio no vino de una recomendación: vino de la prueba P5, que nosotros
mismos corrimos y documentamos como un resultado negativo. Esa prueba es la
razón por la que el sistema hoy hace lo que hace.
