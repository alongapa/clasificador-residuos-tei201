# Fotografías del prototipo ensamblado

Tres ángulos del equipo armado y energizado, con la electrónica ya cerrada
dentro del gabinete impreso.

| Archivo | Qué muestra |
|---|---|
| `01-cara-frontal-sensores.jpg` | Pared frontal (`Base_A`). Arriba la ventana de la ESP32-CAM; abajo los dos transductores del HC-SR04. Es la cara que se orienta hacia la zona monitoreada. |
| `02-conjunto-ensamblado-led.jpg` | Vista de conjunto con el LED RGB encendido a través de la ventana difusora de `Lid_A`. Se aprecian las tres costuras entre segmentos. |
| `03-vista-superior-segmentos.jpg` | Vista superior: los tres segmentos (`Lid_A`, `Lid_B`, `Lid_C`), el LED encendido y, en el canto frontal, los sensores junto a la ranura de alimentación. |

## Dos observaciones sobre lo que se ve en las fotos

**El prototipo está pegado, no atornillado.** El diseño une los tres segmentos
con `Platina_Puente` atornillada y deja las tapas retirables; el prototipo
fotografiado no llegó ahí. En la impresión se perdieron las piezas chicas de
retención y no se consiguió la tornillería M3, así que las bases quedaron
pegadas entre sí y la tapa que sostiene la cámara y el sensor quedó fija con
cola fría. **Las otras dos tapas se levantan** y dan acceso visual al interior.

Esa tapa iba a quedar fija igual: la cámara y el ultrasónico tienen que mantener
una geometría rígida, porque el cero calibrado se mide desde ese montaje.
`diseño-3d/README.md` lo detalla y compara lo diseñado con lo construido.

**El pulsador físico no quedó operativo.** La lengüeta en voladizo de `Lid_B`
no transmite la pulsación al tactil una vez cerrado el gabinete. Las dos
funciones que dependían de él —fijar el cero y leer la IP— se trasladaron al
dashboard web, donde la calibración es un botón (`/calibrar`). El reinicio, si
hiciera falta, se obtiene cortando la alimentación.
