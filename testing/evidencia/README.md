# Evidencia de las pruebas

Capturas tomadas durante la ejecución del protocolo. Cada archivo respalda una
prueba concreta de `testing/protocolo_pruebas.md`.

| Archivo | Respalda |
|---|---|
| `P4-matriz-confusion-3-clases.png` | Modelo vigente: 91,9 % en validación, `vacio` al 100 % |
| `P4-matriz-confusion-5-clases-descartada.png` | Modelo anterior: 63,5 %, descartado por confusión entre materiales |
| `P5-feature-explorer.png` | Separación de clases en el espacio de características |
| `P10-dashboard-indicadores.png` | Dashboard en vivo: nivel, ritmo, volumen y confirmación visual |
| `P10-dashboard-grafico-y-caracterizaciones.png` | Curva de evolución con umbral, y tabla de caracterizaciones con hora y confianza |
| `P11-telegram-ciclo-completo.png` | Avisos de recolección y de normalización |
| `P11-telegram-histeresis.png` | Tres cruces del umbral, tres avisos, sin mensajes repetidos |

## Sobre las matrices de confusión

Las dos se conservan a propósito. La de cinco clases documenta **por qué** se
agruparon los materiales: con `carton` al 20 % de acierto y `otro` perdiendo el
50 % contra `metal`, el modelo no servía para decidir nada. La de tres clases
es la del modelo que se entrega.

Ambas miden la validación de Edge Impulse, cuya división automática reparte
imágenes casi idénticas entre entrenamiento y prueba. El comportamiento en
terreno, que es el dato válido, está en P9.
