/*
 * PRUEBA LED RGB + PULSADOR  -  ESP32 DevKit V1
 * ESTADO: PROBADO Y FUNCIONANDO
 *
 * LED catodo comun:  R -> 220ohm -> D32
 *                    G -> 220ohm -> D33
 *                    B -> 220ohm -> D25
 *                    pata larga -> GND
 * Pulsador:          una pata -> D4,  otra pata -> GND
 *
 * Cada toque del boton avanza al siguiente estado.
 */

#define LED_R   32
#define LED_G   33
#define LED_B   25
#define BOTON    4

int estado = 0;

void color(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== PRUEBA LED + PULSADOR ===");
  Serial.println("Aprieta el boton para cambiar de estado.\n");

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BOTON, INPUT_PULLUP);
  color(0, 0, 0);
}

void loop() {
  if (digitalRead(BOTON) == LOW) {
    delay(50);
    if (digitalRead(BOTON) == LOW) {
      while (digitalRead(BOTON) == LOW) delay(10);

      estado = (estado + 1) % 4;
      switch (estado) {
        case 0: Serial.println("APAGADO");                   break;
        case 1: Serial.println("-> Procesando (azul)");      break;
        case 2: Serial.println("-> Clasificado (verde)");    break;
        case 3: Serial.println("-> No concluyente (rojo)");  break;
      }
      delay(150);
    }
  }

  switch (estado) {
    case 0: color(0, 0, 0); break;
    case 1: color(0, 0, 1); delay(150); color(0, 0, 0); delay(150); break;
    case 2: color(0, 1, 0); break;
    case 3: color(1, 0, 0); break;
  }
}
