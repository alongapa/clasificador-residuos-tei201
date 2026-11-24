# Diseño 3D

Gabinete paramétrico modelado en Fusion 360. El ensamble no es solo la carcasa:
incluye los modelos CAD reales de la ESP32-CAM, el ESP32 DevKit V1 y la
protoboard, más bloques dimensionados del HC-SR04, el módulo microSD y el
pulsador. Eso permite verificar que el hardware real cabe físicamente en el
encapsulado, que es lo que distingue un gemelo 3D de una caja vacía.

## Archivos

| Archivo | Qué es |
|---|---|
| `encapsulado.f3d` | Ensamble completo. 324 items en el timeline, sin errores |
| `planos.pdf` | 10 láminas A3 con vistas ortogonales, cotas, tolerancias y escala |
| `renders/exterior.png` | Gabinete cerrado, 1920×1080 |
| `renders/interior.png` | Tapas ocultas: ESP32-CAM, DevKit, protoboard, HC-SR04 y microSD |
| `renders/explosionado.png` | Las piezas separadas |
| `renders/detalle-pared-frontal.png` | Cámara y HC-SR04 a la misma altura, apuntando al mismo punto |
| `stl/` | Las piezas listas para imprimir |

La cartela de los planos cita el archivo fuente como
`Gabinete_ESP32_Entrega2_ensamble.f3d`, que es el nombre con que se exportó
desde Fusion. En este repositorio ese mismo archivo está como
`encapsulado.f3d`: es el mismo ensamble, renombrado para la estructura de
carpetas.

## Verificación

- **Timeline:** 324 items, sin errores ni advertencias
- **Interferencias:** 0 entre las 10 piezas imprimibles y 0 contra la
  electrónica real
- **Reapertura:** el `.f3d` se abrió en un documento nuevo y cargó las 10
  piezas con la electrónica

La vista explosionada se generó desplazando las piezas solo para el render; los
desplazamientos se deshicieron y se comprobó que todo volviera a su posición
antes de guardar.

## Dos notas honestas

**Las líneas de montaje del explosionado.** La API de Fusion no permite
generarlas por código, así que el render explosionado no las tiene. La **lámina
1 del PDF** sí las muestra, como ejes de los tornillos, y cumple la misma
función documental.

**El render exterior es anterior a las platinas puente.** Se ve igual de todos
modos: las platinas van por dentro de la caja y sus agujeros quedan en la cara
inferior.

## Reparación: el diseño y lo que se pudo construir

Esta sección separa dos cosas que no coinciden, y conviene leerla entera antes de
juzgar el prototipo fotografiado.

### Lo que está diseñado

El gabinete se divide en tres segmentos porque excede el volumen de impresión de
10 × 10 × 10 cm. La primera versión unía los segmentos con adhesivo, lo que
habría sellado el encapsulado de forma permanente.

Se reemplazó por una **platina puente atornillada** (`Platina_Puente.stl`, 2
copias, 100 % de relleno) que cruza cada costura por el interior, fijada con
tornillos M3 pasantes. En el modelo, el equipo se desarma completo sin destruir
ninguna pieza, y las tapas se retiran sin herramientas gracias a un labio de
registro de 2 mm con 0,3 mm de holgura.

### Lo que se construyó

El prototipo **no se abre así**. Dos cosas lo impidieron:

1. **En la impresión se perdieron las piezas chicas de retención.** Quien
   imprimió descartó varias piezas pequeñas tomándolas por soporte o descarte.
   Eran los elementos que sostenían las tapas en su sitio.
2. **No se consiguió la tornillería M3** a tiempo para las platinas puente.

Sin retención ni pernos, el conjunto no se sostenía con la electrónica dentro.
Las tres bases quedaron **pegadas entre sí**, y una de las tapas quedó fija a su
base con cola fría. Las otras dos se levantan y dan acceso visual al interior.

### La tapa fija no es solo consecuencia del pegamento

La tapa que quedó fija es la que **sostiene la cámara y el sensor ultrasónico**.
Esos dos tienen que mantener una geometría rígida entre sí y respecto de la zona
observada: el cero calibrado es una distancia medida desde ese montaje, y si el
par se mueve, la calibración deja de valer y las alturas quedan mal.

Es decir, ese segmento iba a quedar fijo de todos modos en cualquier versión
operativa. Lo que la impresión costó fue la parte de abajo, que sí estaba pensada
para desarmarse.

### Estado honesto

| | Diseñado | Construido |
|---|---|---|
| Unión entre segmentos | Platina atornillada M3 | Pegada |
| Acceso a la electrónica | Las tres tapas se retiran | Dos tapas se levantan |
| Segmento cámara + sensor | Retirable | Fijo, y debe serlo |

Reimprimir las piezas de retención y comprar los pernos devuelve el prototipo al
comportamiento diseñado. Nada del modelo cambia.

## Piezas

| Pieza | Contenido |
|---|---|
| Base_A | Pared frontal con ventanas de cámara y HC-SR04, rieles de retención |
| Base_B | Zona central, protoboard, canal de cable |
| Base_C | DevKit elevado sobre postes, ranura USB-C, módulo microSD |
| Lid_A | Ventana difusora del LED |
| Lid_B | Botón de membrana con lengüeta en voladizo — impresa, la lengüeta resultó demasiado rígida y no acciona el táctil |
| Lid_C | Lisa, conserva la mitad superior de la ranura USB-C |
| Cuna_Pulsador_B | Se pega al interior de Lid_B |
| Cuna_LED_A | Se pega al interior de Lid_A |
| Platina_Puente ×2 | Unión atornillada entre segmentos |
