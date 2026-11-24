# Datos de las pruebas

## `registro.csv`

Registro descargado de la microSD del dispositivo el 20 de noviembre de 2026.

### Formato vigente, 10 columnas

```
tiempo_ms,hora,distancia_mm,altura_medida_mm,altura_validada_mm,
llenado_pct,volumen_cm3,validacion,material,confianza
```

| Campo | Descripción |
|---|---|
| `tiempo_ms` | Milisegundos desde el arranque. Se reinicia con la placa |
| `hora` | Marca temporal real si hubo sincronización NTP, `sin_hora` si no |
| `distancia_mm` | Lectura cruda del ultrasónico, mediana de 5 |
| `altura_medida_mm` | Altura sobre el cero según el ultrasónico |
| `altura_validada_mm` | Altura que usan los indicadores y la alerta |
| `validacion` | `confirmada`, `objetada` o `sin_confirmar` |

### Sobre el contenido

El archivo conserva registros de **varias versiones del firmware**, incluido el
formato de cuatro columnas del Avance 2. Se dejaron a propósito: documentan la
evolución del proyecto desde el clasificador de materiales hasta el monitor de
acumulación.

El análisis de P6 se hizo sobre la sesión más larga con el firmware definitivo:
**425 filas, 35,6 minutos, sin reinicios**. Los detalles están en
`testing/protocolo_pruebas.md`.
