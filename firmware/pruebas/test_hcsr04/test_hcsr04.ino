/*
 * PRUEBA SENSOR ULTRASONICO HC-SR04  -  ESP32 DevKit V1
 * ESTADO: PROBADO Y FUNCIONANDO
 *
 * Sensor:  VCC -> riel 3.3V      TRIG -> D21
 *          GND -> riel -         ECHO -> D22  (directo, sin divisor)
 *
 * A 3.3V el ECHO sale a 3.3V y se conecta directo al ESP32.
 * Pierde alcance (~1m en vez de 4m) pero sobra para medir a 10-20cm.
 *
 * LED:     R -> 220ohm -> D32   G -> 220ohm -> D33
 *          B -> 220ohm -> D25   pata larga -> riel -
 */

#define TRIG    21
#define ECHO    22
#define LED_R   32
#define LED_G   33
#define LED_B   25

#define UMBRAL_MM   200
#define MAX_MM     4000

void color(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

long medirCruda() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long dur = pulseIn(ECHO, HIGH, 30000);
  if (dur == 0) return -1;
  return (long)(dur * 0.343 / 2.0);
}

// Mediana de 5 lecturas: el ultrasonico da saltos sueltos y la
// mediana los descarta sin suavizar los cambios reales.
long medir() {
  long v[5];
  int n = 0;

  for (int i = 0; i < 5; i++) {
    long m = medirCruda();
    if (m > 0 && m < MAX_MM) v[n++] = m;
    delay(30);
  }
  if (n == 0) return -1;

  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (v[j] < v[i]) { long t = v[i]; v[i] = v[j]; v[j] = t; }

  return v[n / 2];
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== PRUEBA HC-SR04 ===");

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(TRIG, LOW);
  color(0, 0, 0);

  Serial.printf("Umbral actual: %d mm\n\n", UMBRAL_MM);
}

void loop() {
  long d = medir();

  if (d < 0) {
    Serial.println("    ---  sin eco");
    color(0, 0, 0);
  } else {
    bool hayObjeto = (d < UMBRAL_MM);
    Serial.printf("%5ld mm  (%5.1f cm)   %s\n",
                  d, d / 10.0, hayObjeto ? "<-- OBJETO" : "");
    color(0, hayObjeto, 0);
  }
  delay(200);
}
