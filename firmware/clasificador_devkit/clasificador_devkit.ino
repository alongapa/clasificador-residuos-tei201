/* ==========================================================================
 *  MONITOR DE ACUMULACION DE RESIDUOS  -  ESP32 DevKit V1
 *  TEI201 - Taller de Diseno en Ingenieria - UAI
 *
 *  Mide de forma continua cuanto residuo se ha acumulado en una zona de
 *  observacion, calcula a que ritmo crece, estima el volumen y avisa
 *  cuando conviene recolectar. Cuando llega material nuevo, le pide a la
 *  ESP32-CAM que caracterice de que esta compuesto.
 *
 *  ---------------------------------------------------------------------
 *  PRINCIPIO DE MEDICION
 *
 *  El dispositivo se monta mirando hacia abajo, perpendicular al suelo de
 *  la zona de observacion. El ultrasonico mide la distancia a la
 *  superficie mas cercana dentro de su cono:
 *
 *      altura del cumulo  =  distancia a zona vacia  -  distancia actual
 *
 *  La "distancia a zona vacia" se fija calibrando con la zona limpia
 *  (toque largo del pulsador). Es el mismo principio de los sensores
 *  comerciales de llenado de contenedores.
 *
 *  LIMITACION DECLARADA: un ultrasonico entrega una sola distancia, la de
 *  la superficie mas cercana del cono. No escanea la zona. La altura
 *  medida corresponde al punto mas alto del cumulo, no a su altura media.
 *  El volumen es por lo tanto una ESTIMACION bajo supuestos explicitos
 *  (area de la zona y factor de ocupacion, declarados abajo).
 *
 *  ---------------------------------------------------------------------
 *  CONEXIONES
 *    HC-SR04:  VCC -> riel 3.3V    TRIG -> D21    ECHO -> D22
 *    LED RGB:  R -> 220ohm -> D32  G -> 220ohm -> D33
 *              B -> 220ohm -> D25  pata larga -> GND
 *    Pulsador: D4 -> GND           (pull-up interno)
 *    microSD:  VCC -> riel 5V      CS -> D5    SCK -> D18
 *              MOSI -> D23         MISO -> D19
 *    Camara:   D26 <- ESP32-CAM GPIO13   (UART 9600, tierras unidas)
 *
 *  ---------------------------------------------------------------------
 *  DASHBOARD
 *    Intenta conectarse a la red configurada. Si falla, levanta la suya:
 *       Red: Clasificador-Residuos   Clave: residuos2026
 *       http://192.168.4.1
 *
 *  ---------------------------------------------------------------------
 *  PULSADOR
 *    Toque corto  -> caracteriza con la camara lo que hay ahora
 *    Toque largo  -> calibra el cero con la zona vacia e inicia sesion
 * ========================================================================== */

#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

// ===================== CONFIGURACION =====================

const char* WIFI_SSID  = "";          // si queda vacio, levanta red propia
const char* WIFI_CLAVE = "";
const char* AP_SSID    = "Clasificador-Residuos";
const char* AP_CLAVE   = "residuos2026";

// --------- Notificaciones por Telegram ---------
// Requiere que la placa tenga salida a internet, o sea que se conecte a
// una red real (el hotspot de un telefono sirve). En modo punto de acceso
// el dashboard funciona igual, pero no hay notificaciones.
//
// Para obtener estos dos valores, ver la seccion "Notificaciones" del
// README. Si quedan vacios, la funcion se desactiva sola.
const char* TG_TOKEN = "";     // token que entrega @BotFather
const char* TG_CHAT  = "";     // id del chat o grupo a notificar

// --------- Pines ---------
#define TRIG     21
#define ECHO     22
#define LED_R    32
#define LED_G    33
#define LED_B    25
#define BOTON     4
#define CS_SD     5
#define RX_CAM   26
#define TX_CAM   27

// --------- Zona de observacion ---------
// Distancia del sensor al suelo de la zona vacia, en mm. Es el cero de la
// medicion. Este valor se sobrescribe al calibrar con el pulsador.
#define CERO_POR_DEFECTO   250

// Altura de cumulo que se considera 100% de llenado, en mm.
#define CAPACIDAD_MM       150

// Supuestos para estimar volumen. Van declarados porque la estimacion
// depende de ellos y el dashboard los muestra junto al resultado.
#define AREA_ZONA_CM2      225.0    // zona de 15 x 15 cm
#define FACTOR_OCUPACION   0.60     // los residuos no llenan el volumen solido

// --------- Parametros de medicion ---------
// Un microbasural cambia en horas o dias, asi que 5 segundos es mas que
// suficiente para seguir el fenomeno. Se eligio tan corto para que durante
// una demostracion de pocos minutos la curva sea visible.
#define MUESTREO_MS        5000

#define MAX_MM             4000
// RUIDO_MM y CONFIRMA_SUBIDA_MM se ajustan JUNTOS. Partieron en 8 y 10 mm.
// P8 midio el ruido real del HC-SR04 a 3,3 V: 6 mm en el peor caso sobre
// blanco plano, asi que 8 mm dejaba pasar ruido como si fuera material. Al
// subir RUIDO_MM a 20 hubo que subir tambien el umbral de confirmacion: si
// el disparo de camara queda por debajo del ruido tolerado, el sistema
// consulta a la camara por puro ruido y cada consulta bloquea 3 segundos.
#define RUIDO_MM             20     // cambios menores se consideran ruido
#define CONFIRMA_SUBIDA_MM   25     // subida que obliga a confirmar con la camara
#define REVALIDA_MS      120000     // cada cuanto se reconfirma un nivel estable
// Umbral de confianza para que la camara decida algo.
//
// Subido de 0,50 a 0,80 tras el reentrenamiento del 18-nov. El modelo de
// 3 clases acierta la clase "otro" el 71 % de las veces, y el 21 % que
// falla lo llama "residuo". Con el umbral alto, esas clasificaciones
// dudosas caen bajo 0,80 y el sistema responde "sin confirmar" en vez de
// confirmar basura que no existe: deja el nivel como estaba, que es el
// resultado seguro. Las clasificaciones correctas vienen con confianza
// alta (residuo acierta 95 %, vacio 100 %), asi que no se pierden.
#define CONFIANZA_MIN      0.80

// --------- Alerta ---------
#define UMBRAL_ALERTA_PCT    70     // sobre esto, conviene recolectar
#define REARME_ALERTA_PCT    55     // bajo esto se habilita una nueva alerta

// --------- Memoria ---------
#define SERIE               120     // muestras guardadas (10 min a 5 s)
#define CARACTERIZACIONES    12

#define ESPERA_CAM         3000

// =========================================================

HardwareSerial enlace(1);
WebServer servidor(80);

struct Muestra {
  unsigned long ms;
  time_t        epoca;
  int           altura;      // mm sobre el cero, medida por el ultrasonico
  int8_t        validacion;  // -1 no evaluada / 0 rechazada / 1 confirmada
};

struct Caracterizacion {
  unsigned long ms;
  time_t        epoca;
  int           altura;
  char          clase[14];
  float         confianza;
};

Muestra          serie[SERIE];
Caracterizacion  carac[CARACTERIZACIONES];
int nSerie = 0, nCarac = 0;

// El sistema maneja dos alturas. La MEDIDA es lo que entrega el
// ultrasonico. La VALIDADA es la que la camara confirmo que corresponde a
// residuos. Los indicadores y la alerta usan la validada: asi un auto que
// pasa o un eco falso no disparan una orden de recoleccion.
int  ceroMM         = CERO_POR_DEFECTO;
int  alturaMedida   = 0;
int  alturaValidada = 0;
int  alturaMaxima   = 0;
int  ultimaAltura   = 0;
unsigned long ultimaRevalidacion = 0;

// -1 sin confirmar aun / 0 rechazada por la camara / 1 confirmada
int  estadoValidacion = -1;
bool hayTarjeta    = false;
bool horaOk        = false;
bool calibrado     = false;
bool hayInternet   = false;
bool alertaEnviada = false;
int  avisosEnviados = 0;
unsigned long inicioSesion = 0;
unsigned long ultimoMuestreo = 0;
String direccionWeb = "";

// Clases del modelo, reentrenado el 18-nov-2026. Antes eran cuatro
// (carton, metal, plastico, vacio). Se agruparon los tres materiales en
// "residuo" y se agrego "otro" para objetos solidos que NO son residuos:
// manos, animales, un vehiculo. Distinguir carton de metal nunca fue parte
// de ninguna decision del sistema, y ese esfuerzo se comia la capacidad
// del modelo en la frontera que si importa.
const int N_CLASES = 3;
const char* NOMBRES[N_CLASES] = { "residuo", "otro", "vacio" };


// ============================================================
//  LED
//
//  El color refleja el nivel de llenado, que es la informacion
//  accionable: verde se puede dejar, rojo hay que recolectar.
// ============================================================
void color(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

int porcentajeLlenado() {
  int p = (int)((alturaValidada * 100.0) / CAPACIDAD_MM);
  if (p < 0)   p = 0;
  if (p > 100) p = 100;
  return p;
}

void actualizarLed() {
  int p = porcentajeLlenado();
  if      (!calibrado)             color(0, 0, 1);   // azul: falta calibrar
  else if (p >= UMBRAL_ALERTA_PCT) color(1, 0, 0);  // rojo: recolectar
  else if (p >= 40)                color(1, 1, 0);  // ambar: a la mitad
  else                             color(0, 1, 0);  // verde: holgado
}


// ============================================================
//  Sensor ultrasonico
//
//  Una medicion cruda puede salir muy desviada cuando el eco rebota mal
//  en un borde. Se toman cinco y se devuelve la mediana: descarta los
//  valores sueltos sin suavizar los cambios reales, que es lo que haria
//  un promedio.
// ============================================================
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

long medirDistancia() {
  long v[5];
  int  n = 0;

  for (int i = 0; i < 5; i++) {
    long m = medirCruda();
    if (m > 0 && m < MAX_MM) v[n++] = m;
    delay(30);
    servidor.handleClient();
  }
  if (n == 0) return -1;

  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (v[j] < v[i]) { long t = v[i]; v[i] = v[j]; v[j] = t; }

  return v[n / 2];
}


// ============================================================
//  Calibracion del cero
//
//  Todas las alturas se calculan como (cero - distancia actual). El cero
//  es la distancia desde el sensor hasta la zona VACIA, y depende de como
//  quede montado el soporte, asi que no se puede fijar en el codigo: hay
//  que medirlo en terreno.
//
//  Se mide con la zona despejada y se promedian varias mediciones, cada
//  una ya mediana de cinco lecturas crudas.
// ============================================================
void calibrarCero() {
  Serial.println();
  Serial.println("== CALIBRACION DEL CERO ==");
  Serial.println("La zona de observacion debe estar vacia.");

  color(0, 0, 1);               // azul fijo mientras mide

  long suma = 0;
  int  n    = 0;

  for (int i = 0; i < 8; i++) {
    long d = medirDistancia();
    if (d > 0) { suma += d; n++; Serial.printf("  lectura %d: %ld mm\n", i + 1, d); }
    servidor.handleClient();
  }

  if (n < 4) {
    // Sin lecturas suficientes no se toca el cero: dejarlo mal calibrado
    // es peor que conservar el valor anterior.
    Serial.println("  Lecturas insuficientes, el cero no se modifico.");
    for (int i = 0; i < 6; i++) { color(1, 0, 0); delay(120); color(0,0,0); delay(120); }
    actualizarLed();
    return;
  }

  ceroMM    = (int)(suma / n);
  calibrado = true;

  // Al fijar un cero nuevo, todo lo acumulado antes deja de tener sentido.
  alturaMedida       = 0;
  alturaValidada     = 0;
  alturaMaxima       = 0;
  ultimaAltura       = 0;
  estadoValidacion   = -1;
  alertaEnviada      = false;
  ultimaRevalidacion = 0;
  nSerie             = 0;
  nCarac             = 0;

  Serial.printf("  Cero fijado en %d mm (%d lecturas validas)\n", ceroMM, n);
  Serial.printf("  Capacidad de referencia: %d mm -> 100%% de llenado\n", CAPACIDAD_MM);
  Serial.println();

  for (int i = 0; i < 3; i++) { color(0, 1, 0); delay(150); color(0,0,0); delay(150); }
  actualizarLed();
}


// ============================================================
//  Indicadores
// ============================================================
float volumenEstimado() {
  // area (cm2) x altura (cm) x factor de ocupacion  ->  cm3
  return AREA_ZONA_CM2 * (alturaValidada / 10.0) * FACTOR_OCUPACION;
}

// Tasa de crecimiento en mm/hora, calculada sobre la ventana disponible.
// Se usa la diferencia entre extremos y no un ajuste lineal: es mas
// robusta ante una muestra aislada mala y mas facil de explicar.
// Minutos que abarca la serie usada para calcular el ritmo. Se informa junto
// al ritmo porque la cifra no significa nada sin su ventana: una colocacion
// instantanea sobre una ventana corta extrapola a miles de mm/h.
float ventanaMinutos() {
  if (nSerie < 2) return 0;
  return (serie[nSerie - 1].ms - serie[0].ms) / 60000.0;
}

float tasaCrecimiento() {
  if (nSerie < 2) return 0;

  Muestra &a = serie[0];
  Muestra &b = serie[nSerie - 1];

  float horas = (b.ms - a.ms) / 3600000.0;
  if (horas < 0.002) return 0;              // menos de 7 segundos

  return (b.altura - a.altura) / horas;
}


// ============================================================
//  Registro
// ============================================================
void horaTexto(time_t e, char *buf, size_t n, const char *fmt) {
  if (e > 0) {
    struct tm *t = localtime(&e);
    strftime(buf, n, fmt, t);
  } else {
    snprintf(buf, n, "sin_hora");
  }
}

void guardarMuestra(int altura, int8_t validacion) {
  if (nSerie == SERIE) {
    for (int i = 0; i < SERIE - 1; i++) serie[i] = serie[i + 1];
    nSerie = SERIE - 1;
  }
  Muestra &m = serie[nSerie++];
  m.ms         = millis();
  m.epoca      = horaOk ? time(nullptr) : 0;
  m.altura     = altura;
  m.validacion = validacion;
}

void registrarEnTarjeta(long distancia, const String &clase, float conf) {
  if (!hayTarjeta) return;

  File f = SD.open("/registro.csv", FILE_APPEND);
  if (!f) return;

  char hora[24];
  horaTexto(horaOk ? time(nullptr) : 0, hora, sizeof(hora), "%Y-%m-%d %H:%M:%S");

  const char *val = estadoValidacion == 1 ? "confirmada"
                  : estadoValidacion == 0 ? "objetada"
                                          : "sin_confirmar";

  f.printf("%lu,%s,%ld,%d,%d,%d,%.1f,%s,%s,%.3f\n",
           millis(), hora, distancia, alturaMedida, alturaValidada,
           porcentajeLlenado(), volumenEstimado(), val, clase.c_str(), conf);
  f.close();
}


// ============================================================
//  Confirmacion con la camara
//
//  El ultrasonico mide, pero no sabe QUE mide: un auto que pasa, una
//  persona o un eco falso de un borde dan la misma lectura que un
//  residuo. La camara es la que confirma si lo que hay en la zona es
//  material acumulado.
// ============================================================
int indiceClase(const String &c) {
  for (int i = 0; i < N_CLASES; i++) if (c == NOMBRES[i]) return i;
  return -1;
}

// Pide una caracterizacion a la camara.
//
// NOTA HONESTA SOBRE EL ENLACE: el enlace con la ESP32-CAM quedo de una
// sola via (camara -> DevKit). La camara clasifica de forma continua, cada
// 1,2 s aproximadamente, y envia cada resultado por UART; no escucha
// peticiones. El byte 'C' se escribe por compatibilidad con el diseno
// bidireccional original, que no alcanzamos a hacer funcionar, pero nadie
// lo lee.
//
// Lo que hace que esto funcione es la linea anterior: se vacia el buffer y
// se descarta todo lo acumulado, de modo que la siguiente linea que llegue
// es necesariamente una clasificacion NUEVA, posterior a la medicion del
// ultrasonico. Como la camara emite cada 1,2 s y la ventana de espera es
// de 3 s, alcanza a llegar.
bool pedirCaracterizacion(String &clase, float &confianza) {
  while (enlace.available()) enlace.read();   // descarta resultados viejos
  enlace.write('C');                          // vestigial, ver nota arriba

  unsigned long t0 = millis();
  while (millis() - t0 < ESPERA_CAM) {
    color(0, 0, (millis() / 200) % 2);        // azul parpadeando
    servidor.handleClient();                  // el dashboard sigue vivo

    if (enlace.available()) {
      String linea = enlace.readStringUntil('\n');
      linea.trim();
      if (linea.length() == 0) continue;

      int coma = linea.indexOf(',');
      if (coma < 0) continue;

      clase     = linea.substring(0, coma);
      confianza = linea.substring(coma + 1).toFloat();

      // Una linea puede llegar cortada si el buffer se vacio a mitad de
      // una transmision. Solo se acepta si la clase es conocida.
      if (indiceClase(clase) >= 0) return true;
    }
  }
  return false;
}

// Devuelve: -1 la camara no respondio o no esta segura
//            0 la camara ve la zona vacia  -> la medicion NO es residuo
//            1 la camara ve material       -> la medicion queda confirmada
int confirmarConCamara(const char *motivo) {
  Serial.printf("  Camara    : confirmando (%s)\n", motivo);

  String clase = "sin_dato";
  float  conf  = 0.0;

  if (!pedirCaracterizacion(clase, conf)) {
    Serial.println("  Camara    : sin respuesta -> la medicion del ultrasonico cuenta igual");
    actualizarLed();
    return -1;
  }

  Serial.printf("  Camara    : %s (%.2f)\n", clase.c_str(), conf);

  if (conf < CONFIANZA_MIN) {
    Serial.println("  Camara    : sin certeza -> no objeta, la medicion cuenta");
    actualizarLed();
    return -1;
  }

  // Dos clases rechazan la medicion, por motivos distintos:
  //   "vacio" -> no hay nada en la zona, el ultrasonico midio un eco falso
  //   "otro"  -> hay un objeto solido, pero no es un residuo (una mano, un
  //              animal, un vehiculo). Es el caso que motivo todo el
  //              rediseno: un auto estacionado da la misma lectura de
  //              distancia que un cumulo de basura.
  if (clase == "vacio" || clase == "otro") {
    if (clase == "vacio")
      Serial.println("  OBJECION  : la zona esta vacia -> medicion descartada");
    else
      Serial.println("  OBJECION  : hay un objeto, pero no es residuo -> medicion descartada");
    actualizarLed();
    return 0;
  }

  // Hay material. Se guarda la caracterizacion y se confirma la medicion.
  if (nCarac == CARACTERIZACIONES) {
    for (int i = 0; i < CARACTERIZACIONES - 1; i++) carac[i] = carac[i + 1];
    nCarac = CARACTERIZACIONES - 1;
  }
  Caracterizacion &c = carac[nCarac++];
  c.ms        = millis();
  c.epoca     = horaOk ? time(nullptr) : 0;
  c.altura    = alturaMedida;
  c.confianza = conf;
  clase.toCharArray(c.clase, sizeof(c.clase));

  Serial.println("  VALIDACION: confirmada como residuo");
  actualizarLed();
  return 1;
}


// ============================================================
//  Notificaciones por Telegram
//
//  Se envia un aviso cuando el nivel CRUZA el umbral, no en cada
//  muestra que lo supera. Para volver a avisar, el nivel tiene que
//  bajar antes de REARME_ALERTA_PCT. Esa histeresis evita que una
//  oscilacion justo en el borde dispare una lluvia de mensajes.
// ============================================================
String escaparJson(const String &t) {
  String r;
  for (unsigned int i = 0; i < t.length(); i++) {
    char c = t[i];
    if      (c == '"')  r += "\\\"";
    else if (c == '\\') r += "\\\\";
    else if (c == '\n') r += "\\n";
    else                r += c;
  }
  return r;
}

bool enviarTelegram(const String &mensaje) {
  if (!hayInternet || strlen(TG_TOKEN) == 0 || strlen(TG_CHAT) == 0) return false;

  WiFiClientSecure cliente;
  // El certificado de Telegram no se valida. Es una simplificacion
  // consciente para un prototipo academico: el mensaje no lleva datos
  // sensibles. En produccion habria que anclar el certificado raiz.
  cliente.setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TG_TOKEN) + "/sendMessage";

  if (!http.begin(cliente, url)) return false;
  http.addHeader("Content-Type", "application/json");

  String cuerpo = "{\"chat_id\":\"" + String(TG_CHAT) +
                  "\",\"text\":\"" + escaparJson(mensaje) + "\"}";

  int codigo = http.POST(cuerpo);
  http.end();

  if (codigo == 200) {
    avisosEnviados++;
    Serial.println("  Telegram  : aviso enviado");
    return true;
  }

  Serial.printf("  Telegram  : fallo el envio (codigo %d)\n", codigo);
  return false;
}

void avisarRecoleccion() {
  char hora[24];
  horaTexto(horaOk ? time(nullptr) : 0, hora, sizeof(hora), "%d-%m-%Y %H:%M");

  String m = "RECOLECCION REQUERIDA\n\n";
  m += "Zona de observacion 1\n";
  m += "Nivel: " + String(alturaValidada) + " mm (" + String(porcentajeLlenado()) + "% de capacidad)\n";
  m += "Ritmo: " + String(tasaCrecimiento(), 0) + " mm/h";
  m += " (ventana " + String(ventanaMinutos(), 0) + " min)\n";
  m += "Volumen estimado: " + String(volumenEstimado(), 0) + " cm3\n";

  if (nCarac > 0) {
    m += "Material predominante: " + String(carac[nCarac - 1].clase);
    m += " (" + String(carac[nCarac - 1].confianza, 2) + ")\n";
  }

  // Acotado a lo que P9 demostro: la camara distingue zona ocupada de zona
  // vacia con 0,92-0,97 de confianza, pero NO distingue residuos de otros
  // objetos solidos. El aviso no puede afirmar mas que eso.
  m += "Presencia de material confirmada por camara\n";

  m += "\n" + String(hora);

  enviarTelegram(m);
}

void avisarRecoleccionRealizada() {
  char hora[24];
  horaTexto(horaOk ? time(nullptr) : 0, hora, sizeof(hora), "%d-%m-%Y %H:%M");

  String m = "Nivel normalizado\n\n";
  m += "Zona de observacion 1\n";
  m += "Nivel actual: " + String(alturaValidada) + " mm (" + String(porcentajeLlenado()) + "%)\n";
  m += "\n" + String(hora);

  enviarTelegram(m);
}


// ============================================================
//  Ciclo de medicion
// ============================================================
void muestrear() {
  long d = medirDistancia();
  if (d < 0) return;

  int altura = ceroMM - (int)d;
  if (altura < 0) altura = 0;

  alturaMedida = altura;
  int delta = altura - ultimaAltura;

  Serial.printf("[%6lus] distancia %4ld mm | medida %3d mm | validada %3d mm | llenado %3d%%\n",
                millis() / 1000, d, alturaMedida, alturaValidada, porcentajeLlenado());

  // ---------- Decidir si hay que consultar a la camara ----------
  //
  // El ultrasonico mide, pero no sabe QUE mide: un auto que pasa, una
  // persona o un eco falso de un borde dan la misma lectura que un
  // residuo. La camara es la que confirma.
  //
  // Se consulta en dos casos:
  //   - el nivel subio de golpe: pudo llegar material, o pudo pasar algo
  //   - el nivel lleva rato estable y toca reconfirmar, por si la lectura
  //     quedo pegada en algo que ya no esta
  bool subio     = (delta >= CONFIRMA_SUBIDA_MM);
  bool tocaRevalidar = (alturaMedida > RUIDO_MM) &&
                       (millis() - ultimaRevalidacion >= REVALIDA_MS);

  if (calibrado && (subio || tocaRevalidar)) {
    if (subio) Serial.printf("  Cambio    : +%d mm respecto a la muestra anterior\n", delta);

    estadoValidacion    = confirmarConCamara(subio ? "subida de nivel" : "reconfirmacion periodica");
    ultimaRevalidacion  = millis();

    // LA MEDICION CUENTA POR DEFECTO. La camara solo puede OBJETAR, y solo
    // cuando esta seguir (confianza sobre CONFIANZA_MIN).
    //
    // El razonamiento: el ultrasonico es un detector de presencia fisico y
    // confiable. La zona vacia devuelve siempre la misma distancia; cualquier
    // objeto que entre al cono la acorta. Eso no es probabilistico. El modelo
    // de vision si lo es, y en terreno se equivoca con objetos que no vio en
    // el entrenamiento.
    //
    // Si la medicion solo contara cuando la camara la confirma, un modelo
    // dudoso dejaria al sistema sin registrar nunca una acumulacion real: el
    // componente probabilistico bloquearia al componente confiable. Invertido,
    // el sistema degrada con elegancia -- cuando la camara duda se comporta
    // como un monitor ultrasonico puro, que es el nucleo del proyecto -- y la
    // vision aporta solo cuando aporta de verdad.
    if (estadoValidacion == 0) {
      // Objecion con certeza: la camara ve la zona vacia, o ve un objeto que
      // no es residuo (una mano, un animal, un vehiculo). La medicion se
      // descarta aunque el ultrasonico siga reportando altura.
      alturaValidada = 0;
    } else {
      // Confirmada (1) o sin objecion (-1): la medicion del ultrasonico cuenta.
      alturaValidada = alturaMedida;
      if (alturaValidada > alturaMaxima) alturaMaxima = alturaValidada;
    }
  }
  else if (altura < alturaValidada - RUIDO_MM) {
    // El nivel BAJO. Una bajada no necesita confirmacion de la camara: si
    // habia residuos y ahora hay menos, alguien recolecto o el material se
    // movio. La altura validada sigue a la medida hacia abajo.
    alturaValidada   = altura;
    estadoValidacion = 1;
    Serial.println("  Cambio    : el nivel bajo, altura validada actualizada");
  }

  guardarMuestra(alturaMedida, (int8_t)estadoValidacion);
  actualizarLed();

  String clase = "-";
  float  conf  = 0;
  if (estadoValidacion == 1 && nCarac > 0) {
    clase = String(carac[nCarac - 1].clase);
    conf  = carac[nCarac - 1].confianza;
  }

  registrarEnTarjeta(d, clase, conf);

  // ---------- Alerta de recoleccion ----------
  // Se evalua sobre la altura VALIDADA, no la medida. Un auto que pasa no
  // puede disparar una orden de recoleccion.
  int pct = porcentajeLlenado();

  if (!alertaEnviada && calibrado && pct >= UMBRAL_ALERTA_PCT) {
    alertaEnviada = true;
    Serial.printf("  ALERTA    : llenado %d%% sobre el umbral de %d%%\n",
                  pct, UMBRAL_ALERTA_PCT);
    avisarRecoleccion();
  }
  else if (alertaEnviada && pct <= REARME_ALERTA_PCT) {
    alertaEnviada = false;
    Serial.printf("  NORMAL    : llenado %d%%, alerta rearmada\n", pct);
    avisarRecoleccionRealizada();
  }

  ultimaAltura = altura;
}


// ============================================================
//  Dashboard
// ============================================================
const char PAGINA[] PROGMEM = R"HTML(<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monitor de acumulación</title><style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#f4f2ec;color:#14231d;font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;padding:20px 16px 48px}
.w{max-width:980px;margin:0 auto}
header{border-bottom:2px solid #14231d;padding-bottom:14px;margin-bottom:22px;display:flex;flex-wrap:wrap;gap:10px;justify-content:space-between;align-items:flex-end}
h1{font-size:clamp(20px,4vw,28px);font-weight:600;letter-spacing:-.01em}
.sub{font-size:12px;color:#5e6b64;font-family:ui-monospace,monospace;letter-spacing:.08em;text-transform:uppercase}
.vivo{display:inline-flex;align-items:center;gap:7px;font-family:ui-monospace,monospace;font-size:12px;color:#2e7d4f}
.punto{width:8px;height:8px;border-radius:50%;background:#2e7d4f;animation:lat 2s infinite}
@keyframes lat{0%,100%{opacity:1}50%{opacity:.25}}
.aviso{border-radius:6px;padding:14px 18px;margin-bottom:20px;display:flex;gap:14px;align-items:flex-start}
.aviso b{display:block;font-size:14px;margin-bottom:3px}
.aviso span{font-size:13px;opacity:.93}
.rojo{background:#b5481a;color:#fff}.azul{background:#1f6f9c;color:#fff}
.hero{background:#fffefb;border:1px solid #d9d4c6;border-radius:6px;padding:22px 24px;margin-bottom:14px;display:flex;flex-wrap:wrap;gap:24px;align-items:center}
.hero .big{font-size:60px;font-weight:600;line-height:1;font-variant-numeric:tabular-nums}
.hero .big em{font-size:22px;font-style:normal;color:#5e6b64;margin-left:4px}
.hero .k{font-family:ui-monospace,monospace;font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:#5e6b64;margin-bottom:8px}
.gauge{flex:1;min-width:220px}
.gbar{height:26px;background:#ece8dd;border-radius:4px;overflow:hidden;position:relative}
.gbar i{display:block;height:100%;transition:width .5s}
.gbar u{position:absolute;top:0;bottom:0;width:2px;background:#14231d;opacity:.45}
.gl{display:flex;justify-content:space-between;font-family:ui-monospace,monospace;font-size:11px;color:#5e6b64;margin-top:6px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:18px}
.t{background:#fffefb;border:1px solid #d9d4c6;border-radius:6px;padding:16px}
.t .k{font-family:ui-monospace,monospace;font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:#5e6b64;margin-bottom:8px}
.t .v{font-size:28px;font-weight:600;line-height:1;font-variant-numeric:tabular-nums}
.t .u{font-size:12px;color:#5e6b64;margin-top:5px}
.cal{background:#fffefb;border:1px solid #d9d4c6;border-radius:6px;padding:16px 18px;margin-bottom:20px;display:flex;gap:14px;align-items:center;flex-wrap:wrap}
.cal .txt{flex:1 1 220px;min-width:0}
.cal .k{font-family:ui-monospace,monospace;font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:#5e6b64;margin-bottom:5px}
.cal .d{font-size:12.5px;color:#5e6b64;line-height:1.5}
.cal button{font:600 14px/1 inherit;color:#fffefb;background:#1d6b5f;border:0;border-radius:6px;padding:13px 20px;cursor:pointer;flex:none}
.cal button:hover{background:#17564c}
.cal button:disabled{background:#9aa8a2;cursor:default}
.cal button.arm{background:#a33028}
.cal button.arm:hover{background:#8a2820}
.panel{background:#fffefb;border:1px solid #d9d4c6;border-radius:6px;padding:18px 20px;margin-bottom:18px}
h2{font-family:ui-monospace,monospace;font-size:11px;letter-spacing:.11em;text-transform:uppercase;color:#5e6b64;font-weight:600;margin-bottom:14px}
svg{display:block;width:100%;height:auto}
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;font-family:ui-monospace,monospace;font-size:10px;letter-spacing:.09em;text-transform:uppercase;color:#5e6b64;font-weight:500;padding-bottom:7px;border-bottom:1px solid #d9d4c6}
td{padding:7px 0;border-bottom:1px solid #ece8dd;font-variant-numeric:tabular-nums}
td:last-child,th:last-child{text-align:right}
.vacio{color:#5e6b64;font-size:13px;padding:22px 0;text-align:center}
.ok{color:#2e7d4f}.no{color:#b5481a}.sc{color:#c47a18}
.sup{font-size:12px;color:#5e6b64;margin-top:10px;line-height:1.5}
footer{margin-top:26px;font-family:ui-monospace,monospace;font-size:11px;color:#5e6b64;display:flex;flex-wrap:wrap;gap:6px 20px}
</style></head><body><div class="w">
<header>
  <div><h1>Monitor de acumulación</h1><p class="sub">TEI201 · Zona de observación</p></div>
  <span class="vivo"><i class="punto"></i><span id="lat">conectando…</span></span>
</header>
<div id="av"></div>
<div class="hero">
  <div><div class="k">Residuo confirmado</div><div class="big" id="alt">—<em>mm</em></div><div class="u" id="med" style="font-size:12px;color:#5e6b64;margin-top:5px">&nbsp;</div></div>
  <div class="gauge">
    <div class="k">Nivel de llenado</div>
    <div class="gbar"><i id="gb" style="width:0%;background:#2e7d4f"></i><u id="gu" style="left:70%"></u></div>
    <div class="gl"><span>0%</span><span id="gt">umbral 70%</span><span>100%</span></div>
  </div>
</div>
<div class="cal">
  <div class="txt">
    <div class="k">Calibración del cero</div>
    <div class="d" id="cald">Fija la distancia de referencia a la zona vacía. <b>Despeja la zona antes de calibrar.</b></div>
  </div>
  <button id="calb" onclick="cal()">Calibrar</button>
</div>
<div class="tiles">
  <div class="t"><div class="k">Ritmo de acumulación</div><div class="v" id="tasa">—</div><div class="u">mm / hora</div></div>
  <div class="t"><div class="k">Volumen estimado</div><div class="v" id="vol">—</div><div class="u">cm³</div></div>
  <div class="t"><div class="k">Confirmación visual</div><div class="v" id="val" style="font-size:20px">—</div><div class="u" id="valu">&nbsp;</div></div>
  <div class="t"><div class="k">Último material</div><div class="v" id="mat" style="font-size:20px">—</div><div class="u" id="matc">&nbsp;</div></div>
</div>
<div class="panel"><h2>Evolución del nivel</h2><div id="graf"></div>
  <p class="sup">Los puntos verdes son mediciones en que la cámara detectó material en la zona. Los rojos, mediciones objetadas: el ultrasónico midió algo pero la cámara vio la zona despejada.</p></div>
<div class="panel"><h2>Caracterizaciones recientes</h2><div id="tab"></div>
  <p class="sup" id="sup"></p></div>
<footer><span>Actualiza cada 2 s</span><span id="err"></span></footer>
</div><script>
var calArmado=false,calT=null;
function cal(){
  var b=document.getElementById("calb"),d=document.getElementById("cald");
  if(!calArmado){
    // Primer clic: arma. Calibrar con material en la zona arruina el cero,
    // asi que se exige una confirmacion explicita.
    calArmado=true;b.textContent="Confirmar";b.className="arm";
    d.innerHTML="<b>¿La zona está despejada?</b> Pulsa otra vez para fijar el cero. Se cancela sola en 8 s.";
    calT=setTimeout(function(){calArmado=false;b.textContent="Calibrar";b.className="";
      d.innerHTML="Fija la distancia de referencia a la zona vacía. <b>Despeja la zona antes de calibrar.</b>";},8000);
    return;
  }
  clearTimeout(calT);calArmado=false;
  b.disabled=true;b.textContent="Calibrando…";b.className="";
  d.textContent="Tomando 8 lecturas de la zona vacía…";
  fetch("/calibrar").then(function(){
    setTimeout(function(){
      b.disabled=false;b.textContent="Calibrar";
      d.textContent="Cero actualizado. Verifica abajo que el nivel marque 0 mm.";
    },6000);
  }).catch(function(){
    b.disabled=false;b.textContent="Calibrar";
    d.textContent="No se pudo contactar al dispositivo. Reintenta.";
  });
}
function dur(s){var h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+" h "+m+" min":m+" min"}
function grafico(p,cap,umb){
  var W=900,H=240,ml=46,mr=12,mt=12,mb=26;
  if(!p.length)return'<p class="vacio">Sin muestras todavía.</p>';
  var ymax=Math.max(cap,Math.max.apply(null,p.map(function(d){return d.a})))*1.1;
  var n=p.length,iw=W-ml-mr,ih=H-mt-mb;
  function X(i){return ml+(n<2?iw:i/(n-1)*iw)}
  function Y(v){return mt+ih-(v/ymax*ih)}
  var s='<svg viewBox="0 0 '+W+' '+H+'" preserveAspectRatio="none" role="img" aria-label="Evolución de la altura acumulada en el tiempo">';
  for(var g=0;g<=4;g++){var v=ymax*g/4,y=Y(v);
    s+='<line x1="'+ml+'" y1="'+y+'" x2="'+(W-mr)+'" y2="'+y+'" stroke="#ece8dd" stroke-width="1"/>';
    s+='<text x="'+(ml-8)+'" y="'+(y+4)+'" text-anchor="end" font-family="ui-monospace,monospace" font-size="11" fill="#5e6b64">'+Math.round(v)+'</text>';}
  var yu=Y(cap*umb/100);
  s+='<line x1="'+ml+'" y1="'+yu+'" x2="'+(W-mr)+'" y2="'+yu+'" stroke="#b5481a" stroke-width="1.5" stroke-dasharray="5 4"/>';
  s+='<text x="'+(W-mr)+'" y="'+(yu-6)+'" text-anchor="end" font-family="ui-monospace,monospace" font-size="11" fill="#b5481a">umbral de recolección</text>';
  var d="",a="";
  for(var i=0;i<n;i++){d+=(i?" L":"M")+X(i)+" "+Y(p[i].a);}
  a=d+" L"+X(n-1)+" "+Y(0)+" L"+X(0)+" "+Y(0)+" Z";
  s+='<path d="'+a+'" fill="#2e7d4f" fill-opacity="0.10"/>';
  s+='<path d="'+d+'" fill="none" stroke="#2e7d4f" stroke-width="2.5" stroke-linejoin="round"/>';
  for(var i=0;i<n;i++){if(p[i].v===0)s+='<circle cx="'+X(i)+'" cy="'+Y(p[i].a)+'" r="3.5" fill="#b5481a"/>';else if(p[i].v===1)s+='<circle cx="'+X(i)+'" cy="'+Y(p[i].a)+'" r="3" fill="#2e7d4f"/>';}
  s+='<circle cx="'+X(n-1)+'" cy="'+Y(p[n-1].a)+'" r="4.5" fill="#14231d"/>';
  s+='<text x="'+ml+'" y="'+(H-6)+'" font-family="ui-monospace,monospace" font-size="11" fill="#5e6b64">'+p[0].h+'</text>';
  s+='<text x="'+(W-mr)+'" y="'+(H-6)+'" text-anchor="end" font-family="ui-monospace,monospace" font-size="11" fill="#5e6b64">'+p[n-1].h+'</text>';
  return s+'</svg>';
}
function pinta(d){
  document.getElementById("lat").textContent="en vivo";
  document.getElementById("err").textContent="";
  document.getElementById("alt").innerHTML=d.altura+'<em>mm</em>';
  document.getElementById("med").textContent=(d.medida!=d.altura)?("ultrasónico mide "+d.medida+" mm · máx. sesión "+d.maxima+" mm"):("máx. sesión "+d.maxima+" mm");
  document.getElementById("tasa").textContent=(d.tasa>=0?"+":"")+d.tasa.toFixed(0);
  document.getElementById("vol").textContent=Math.round(d.volumen);

  var ve=document.getElementById("val"),vu=document.getElementById("valu");
  // El texto afirma solo lo que el modelo demostro poder distinguir en P9:
  // presencia de material contra zona vacia. NO distingue residuos de otros
  // objetos solidos -- se midio que una mano se clasifica como residuo.
  if(d.altura<=20){ve.textContent="Sin material";ve.className="v";vu.textContent="la zona está despejada";}
  else if(d.validacion==1){ve.textContent="Confirmado";ve.className="v ok";vu.textContent="la cámara detecta material";}
  else if(d.validacion===0){ve.textContent="Objetado";ve.className="v no";vu.textContent="la cámara no detecta material";}
  else{ve.textContent="Sin confirmar";ve.className="v sc";vu.textContent="esperando a la cámara";}
  document.getElementById("mat").textContent=d.material||"—";
  document.getElementById("matc").textContent=d.materialConf?"confianza "+d.materialConf.toFixed(2):" ";

  var c=d.llenado>=d.umbral?"#b5481a":(d.llenado>=40?"#c47a18":"#2e7d4f");
  document.getElementById("gb").style.width=d.llenado+"%";
  document.getElementById("gb").style.background=c;
  document.getElementById("gu").style.left=d.umbral+"%";
  document.getElementById("gt").textContent="umbral "+d.umbral+"%";

  var a="";
  if(!d.calibrado)a='<div class="aviso azul"><b>i</b><div><b>Falta calibrar el cero</b><span>Despeje la zona de observación y mantenga presionado el pulsador para fijar la referencia. Hasta entonces las alturas no son confiables.</span></div></div>';
  else if(d.validacion===0&&d.medida>20)a='<div class="aviso azul"><b>i</b><div><b>Medición descartada</b><span>El ultrasónico detecta '+d.medida+' mm, pero la cámara no detecta material en la zona. Probablemente un eco falso: no se cuenta como acumulación.</span></div></div>';
  else if(d.llenado>=d.umbral)a='<div class="aviso rojo"><b>▲</b><div><b>Nivel sobre el umbral de recolección</b><span>El cúmulo alcanzó '+d.altura+' mm ('+d.llenado+'% de la capacidad definida). Al ritmo actual de '+d.tasa.toFixed(0)+' mm/h, conviene priorizar la recolección en este punto.</span></div></div>';
  document.getElementById("av").innerHTML=a;

  document.getElementById("graf").innerHTML=grafico(d.serie,d.capacidad,d.umbral);

  if(!d.carac.length){document.getElementById("tab").innerHTML='<p class="vacio">Sin caracterizaciones. Se solicitan cuando el nivel sube de golpe.</p>';}
  else{var t='<table><tr><th>Hora</th><th>Material</th><th>Confianza</th><th>Altura</th></tr>';
    for(var i=d.carac.length-1;i>=0;i--){var r=d.carac[i];
      t+='<tr><td>'+r.h+'</td><td>'+r.c+'</td><td>'+r.k.toFixed(2)+'</td><td>'+r.a+' mm</td></tr>';}
    document.getElementById("tab").innerHTML=t+'</table>';}

  document.getElementById("sup").textContent="Volumen estimado bajo supuestos declarados: zona de "+d.area+" cm², factor de ocupación "+d.factor+". Cero calibrado a "+d.cero+" mm. Capacidad de referencia "+d.capacidad+" mm. Muestreo cada "+d.muestreo+" s. Operando hace "+dur(d.uptime)+(d.sd?" · registrando en microSD":" · sin tarjeta")+(d.notifica?" · notificaciones activas ("+d.avisos+" enviadas)":" · notificaciones inactivas, sin salida a internet")+". La cámara reconfirma el nivel cada 2 minutos.";
}
function pide(){
  fetch("/datos").then(function(r){return r.json()}).then(pinta).catch(function(){
    document.getElementById("lat").textContent="sin conexión";
    document.getElementById("err").textContent="No se pudo contactar al dispositivo";});
}
pide();setInterval(pide,2000);
</script></body></html>)HTML";


void rutaRaiz() {
  servidor.send_P(200, "text/html; charset=utf-8", PAGINA);
}

// Calibracion remota.
//
// Con el equipo montado dentro del gabinete no hay acceso fisico ni al
// pulsador ni al pin D4, asi que esta es la unica via para fijar el cero en
// terreno. La ruta NO llama a calibrarCero() directamente: ese procedimiento
// invoca servidor.handleClient() en su bucle de lectura, y llamarlo desde
// dentro de un manejador de ruta seria reentrar en el servidor web. En vez de
// eso levanta una bandera y el trabajo real ocurre en loop().
volatile bool pedidoCalibrar = false;

void rutaCalibrar() {
  pedidoCalibrar = true;
  servidor.send(200, "application/json", "{\"ok\":true}");
}

void rutaDatos() {
  String j = "{";
  j += "\"altura\":"     + String(alturaValidada);
  j += ",\"medida\":"     + String(alturaMedida);
  j += ",\"validacion\":" + String(estadoValidacion);
  j += ",\"maxima\":"    + String(alturaMaxima);
  j += ",\"llenado\":"   + String(porcentajeLlenado());
  j += ",\"capacidad\":" + String(CAPACIDAD_MM);
  j += ",\"umbral\":"    + String(UMBRAL_ALERTA_PCT);
  j += ",\"tasa\":"      + String(tasaCrecimiento(), 1);
  j += ",\"volumen\":"   + String(volumenEstimado(), 1);
  j += ",\"area\":"      + String(AREA_ZONA_CM2, 0);
  j += ",\"factor\":"    + String(FACTOR_OCUPACION, 2);
  j += ",\"cero\":"      + String(ceroMM);
  j += ",\"muestreo\":"  + String(MUESTREO_MS / 1000);
  j += ",\"calibrado\":" + String(calibrado ? "true" : "false");
  j += ",\"sd\":"        + String(hayTarjeta ? "true" : "false");
  j += ",\"avisos\":"    + String(avisosEnviados);
  j += ",\"notifica\":"  + String((hayInternet && strlen(TG_TOKEN) > 0 && strlen(TG_CHAT) > 0) ? "true" : "false");
  j += ",\"uptime\":"    + String((millis() - inicioSesion) / 1000);

  if (nCarac > 0) {
    j += ",\"material\":\"" + String(carac[nCarac - 1].clase) + "\"";
    j += ",\"materialConf\":" + String(carac[nCarac - 1].confianza, 3);
  } else {
    j += ",\"material\":null,\"materialConf\":0";
  }

  char hora[12];

  j += ",\"serie\":[";
  for (int i = 0; i < nSerie; i++) {
    if (serie[i].epoca > 0) horaTexto(serie[i].epoca, hora, sizeof(hora), "%H:%M:%S");
    else snprintf(hora, sizeof(hora), "+%lus", (serie[i].ms - inicioSesion) / 1000);

    j += "{\"a\":" + String(serie[i].altura);
    j += ",\"v\":" + String(serie[i].validacion);
    j += ",\"h\":\"" + String(hora) + "\"}";
    if (i < nSerie - 1) j += ",";
  }
  j += "]";

  j += ",\"carac\":[";
  for (int i = 0; i < nCarac; i++) {
    if (carac[i].epoca > 0) horaTexto(carac[i].epoca, hora, sizeof(hora), "%H:%M:%S");
    else snprintf(hora, sizeof(hora), "+%lus", (carac[i].ms - inicioSesion) / 1000);

    j += "{\"h\":\"" + String(hora) + "\"";
    j += ",\"c\":\"" + String(carac[i].clase) + "\"";
    j += ",\"k\":"   + String(carac[i].confianza, 3);
    j += ",\"a\":"   + String(carac[i].altura) + "}";
    if (i < nCarac - 1) j += ",";
  }
  j += "]}";

  servidor.send(200, "application/json", j);
}


// ============================================================
//  Red
// ============================================================
void iniciarRed() {
  if (strlen(WIFI_SSID) > 0) {
    Serial.printf("  WiFi .............. conectando a \"%s\"", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_CLAVE);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
      delay(400);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      direccionWeb = "http://" + WiFi.localIP().toString();
      Serial.println(" OK");

      configTime(-3 * 3600, 0, "pool.ntp.org", "time.cloudflare.com");
      unsigned long t1 = millis();
      while (time(nullptr) < 100000 && millis() - t1 < 6000) delay(200);
      horaOk      = time(nullptr) > 100000;
      hayInternet = true;
      return;
    }
    Serial.println(" sin exito");
  }

  // Red propia: el dashboard no puede depender de la red del lugar donde
  // se haga la demostracion.
  Serial.println("  WiFi .............. levantando red propia");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_CLAVE);
  direccionWeb = "http://" + WiFi.softAPIP().toString();
}


// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BOTON, INPUT_PULLUP);
  digitalWrite(TRIG, LOW);
  color(0, 0, 0);

  enlace.begin(9600, SERIAL_8N1, RX_CAM, TX_CAM);
  inicioSesion = millis();

  Serial.println("\n\n==========================================");
  Serial.println("   MONITOR DE ACUMULACION DE RESIDUOS");
  Serial.println("==========================================\n");

  if (SD.begin(CS_SD) && SD.cardType() != CARD_NONE) {
    hayTarjeta = true;
    Serial.println("  microSD ........... OK");
    if (!SD.exists("/registro.csv")) {
      File f = SD.open("/registro.csv", FILE_WRITE);
      if (f) {
        f.println("tiempo_ms,hora,distancia_mm,altura_medida_mm,altura_validada_mm,llenado_pct,volumen_cm3,validacion,material,confianza");
        f.close();
      }
    }
  } else {
    Serial.println("  microSD ........... NO DISPONIBLE");
  }

  long p = medirCruda();
  Serial.printf("  Sensor HC-SR04 .... %s\n", p > 0 ? "OK" : "sin eco");
  Serial.println("  LED RGB ........... OK");
  Serial.println("  Pulsador .......... OK");

  iniciarRed();

  servidor.on("/", rutaRaiz);
  servidor.on("/datos", rutaDatos);
  servidor.on("/calibrar", rutaCalibrar);
  servidor.begin();

  Serial.println("\n  ------------------------------------");
  if (WiFi.getMode() == WIFI_AP) {
    Serial.printf("  Red del dispositivo : %s\n", AP_SSID);
    Serial.printf("  Clave               : %s\n", AP_CLAVE);
  }
  Serial.printf("  Dashboard           : %s\n", direccionWeb.c_str());
  Serial.printf("  Hora sincronizada   : %s\n", horaOk ? "si" : "no (tiempo relativo)");
  if (strlen(TG_TOKEN) == 0 || strlen(TG_CHAT) == 0)
    Serial.println("  Notificaciones      : sin configurar (ver README)");
  else if (!hayInternet)
    Serial.println("  Notificaciones      : configuradas, pero sin salida a internet");
  else
    Serial.println("  Notificaciones      : activas por Telegram");
  Serial.println("  ------------------------------------\n");

  Serial.printf("  Cero por defecto    : %d mm  (sin calibrar)\n", CERO_POR_DEFECTO);
  Serial.printf("  Capacidad           : %d mm = 100%% de llenado\n", CAPACIDAD_MM);
  Serial.printf("  Zona de observacion : %.0f cm2, factor %.2f\n", AREA_ZONA_CM2, FACTOR_OCUPACION);
  Serial.printf("  Muestreo            : cada %d s\n", MUESTREO_MS / 1000);
  Serial.printf("  Alerta              : sobre %d%% de llenado\n", UMBRAL_ALERTA_PCT);
  Serial.println("\n  Toque corto del pulsador = confirmar con la camara ahora");
  Serial.println("  Toque largo del pulsador = calibrar cero e iniciar sesion");
  Serial.println("\n  >>> Calibre antes de medir: despeje la zona y mantenga");
  Serial.println("      presionado el pulsador.\n");

  color(1,0,0); delay(250);
  color(0,1,0); delay(250);
  color(0,0,1); delay(250);
  actualizarLed();
}


// ============================================================
//  LOOP
// ============================================================
void loop() {
  servidor.handleClient();

  // ---------- Calibracion pedida desde el dashboard ----------
  if (pedidoCalibrar) {
    pedidoCalibrar = false;
    Serial.println("  Calibracion solicitada desde el dashboard");
    calibrarCero();
    return;
  }

  // ---------- Pulsador ----------
  if (digitalRead(BOTON) == LOW) {
    delay(50);
    if (digitalRead(BOTON) == LOW) {
      unsigned long t0 = millis();
      while (digitalRead(BOTON) == LOW) {
        delay(10);
        servidor.handleClient();
        if (millis() - t0 > 4000) break;
      }
      if (millis() - t0 >= 1200) calibrarCero();
      else {
        estadoValidacion   = confirmarConCamara("solicitud manual");
        ultimaRevalidacion = millis();
        // Misma regla que en muestrear(): cuenta salvo objecion con certeza.
        if (estadoValidacion == 0) alturaValidada = 0;
        else                       alturaValidada = alturaMedida;
      }
      return;
    }
  }

  // ---------- Muestreo periodico ----------
  if (millis() - ultimoMuestreo >= MUESTREO_MS) {
    ultimoMuestreo = millis();
    muestrear();
  }

  delay(20);
}
