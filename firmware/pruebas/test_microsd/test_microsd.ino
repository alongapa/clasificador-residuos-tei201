/*
 * PRUEBA MODULO microSD SPI  -  ESP32 DevKit V1
 * ESTADO: PROBADO Y FUNCIONANDO
 *
 * VCC -> riel 5V      CS   -> D5
 * GND -> riel -       SCK  -> D18
 *                     MOSI -> D23
 *                     MISO -> D19
 *
 * La tarjeta debe estar en FAT32.
 * Si no monta, lo PRIMERO es intercambiar MISO y MOSI.
 */

#include <SPI.h>
#include <SD.h>

#define CS_PIN 5

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== PRUEBA microSD SPI ===\n");

  if (!SD.begin(CS_PIN)) {
    Serial.println("FALLO: no se pudo montar la tarjeta.");
    Serial.println("  - MISO y MOSI cambiados");
    Serial.println("  - VCC no esta en 5V");
    Serial.println("  - la tarjeta no esta bien insertada");
    return;
  }

  uint8_t tipo = SD.cardType();
  if (tipo == CARD_NONE) { Serial.println("FALLO: no hay tarjeta."); return; }

  Serial.print("Tipo de tarjeta: ");
  if      (tipo == CARD_MMC)  Serial.println("MMC");
  else if (tipo == CARD_SD)   Serial.println("SDSC");
  else if (tipo == CARD_SDHC) Serial.println("SDHC");
  else                        Serial.println("desconocido");

  Serial.printf("Tamano: %llu MB\n", SD.cardSize() / (1024 * 1024));

  Serial.println("\nContenido de la tarjeta:");
  File raiz = SD.open("/");
  File e = raiz.openNextFile();
  while (e) {
    Serial.printf("  %s%s\n", e.name(), e.isDirectory() ? "/" : "");
    e.close();
    e = raiz.openNextFile();
  }
  raiz.close();

  File f = SD.open("/registro.csv", FILE_WRITE);
  if (!f) { Serial.println("\nFALLO: no deja escribir."); return; }
  f.println("timestamp,distancia_mm,clase,confianza");
  f.println("0,152,plastico,0.87");
  f.close();
  Serial.println("\nESCRITURA OK");

  File r = SD.open("/registro.csv");
  if (!r) { Serial.println("FALLO al leer."); return; }
  Serial.println("\nContenido leido de vuelta:");
  while (r.available()) Serial.write(r.read());
  r.close();

  Serial.println("\n\nTODO CORRECTO.");
}

void loop() {}
