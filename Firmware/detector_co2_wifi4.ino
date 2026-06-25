
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SoftwareSerial.h>
#include <WiFiS3.h>


const char WIFI_SSID[] = "iPhone de Franka (3)";       // <-- cambia esto
const char WIFI_PASS[]  = "ju06062017";  // <-- cambia esto

// --- Pines TFT ---
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

// --- Pines sensor CO2 ---
#define CO2_RX    2   // Arduino D2 ← sensor TXD
#define CO2_TX    3   // Arduino D3 → sensor RXD

// --- Objetos ---
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
SoftwareSerial co2Serial(CO2_RX, CO2_TX);
WiFiServer servidorWeb(80);

// --- Colores personalizados (RGB565) ---
#define COLOR_BG        0x0841   // Gris oscuro
#define COLOR_BLANCO    0xFFFF
#define COLOR_VERDE     0x07E0
#define COLOR_AMARILLO  0xFFE0
#define COLOR_NARANJA   0xFD20
#define COLOR_ROJO      0xF800
#define COLOR_AZUL      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_GRIS      0x8410

// --- Variables ---
int co2_ppm = 0;
unsigned long ultimaMedicion = 0;
unsigned long INTERVALO_MS = 30000;  // 30 segundos por defecto. Modificable via /config?intervalo=NN
const unsigned long INTERVALO_MIN_MS = 5000;   // no permitir menos de 5s (limite real del sensor MH-Z19)
const unsigned long INTERVALO_MAX_MS = 600000; // tope de 10 minutos
bool primeraVez = true;
bool calentando = true;
unsigned long tiempoInicio = 0;
const unsigned long CALENTAMIENTO_MS = 60000; // 60 seg calentamiento

// Estado para el servidor web
String ipTexto = "Conectando...";
unsigned long ultimaLecturaMillis = 0; // momento (millis) de la última lectura válida
bool sensorError = false;

// Comando UART para leer CO2 del MH-Z19/1911A
byte cmdLeerCO2[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

// ============================================================
void setup() {
  Serial.begin(9600);
  unsigned long esperaSerial = millis();
  while (!Serial && millis() - esperaSerial < 3000) {
    ; // espera hasta 3s a que se abra el Monitor Serial (USB nativo del R4)
  }
  Serial.println("=== INICIO setup() ===");

  co2Serial.begin(9600);
  tiempoInicio = millis();

  // Iniciar pantalla TFT
  tft.initR(INITR_BLACKTAB);  // Para la mayoría de módulos 1.8"
  // Si los colores se ven invertidos, probar: INITR_GREENTAB
  tft.setRotation(1);          // Horizontal (160x128)
  tft.fillScreen(COLOR_BG);

  pantallaBienvenida();
  conectarWiFi();

  Serial.println("Detector de CO2 iniciado.");
}

// ============================================================
void loop() {
  unsigned long ahora = millis();

  // Atender peticiones web en cada vuelta del loop (no bloqueante si no hay cliente)
  manejarClienteWeb();

  // Fase de calentamiento (primer minuto)
  if (calentando) {
    unsigned long transcurrido = ahora - tiempoInicio;
    if (transcurrido < CALENTAMIENTO_MS) {
      pantallaCalentando(transcurrido);
      delay(200);
      return;
    } else {
      calentando = false;
      tft.fillScreen(COLOR_BG);
      primeraVez = true;
    }
  }

  // Leer cada 30 segundos (o al inicio)
  if (primeraVez || (ahora - ultimaMedicion >= INTERVALO_MS)) {
    ultimaMedicion = ahora;
    primeraVez = false;

    co2_ppm = leerCO2();

    if (co2_ppm > 0) {
      Serial.print("CO2: ");
      Serial.print(co2_ppm);
      Serial.println(" ppm");
      ultimaLecturaMillis = millis();
      sensorError = false;
      mostrarResultado(co2_ppm);
    } else {
      Serial.println("Error leyendo sensor");
      sensorError = true;
      pantallaError();
    }
  }

  // Mostrar cuenta regresiva para la próxima lectura
  if (!primeraVez && co2_ppm > 0) {
    long segsRestantes = (INTERVALO_MS - (millis() - ultimaMedicion)) / 1000;
    if (segsRestantes >= 0) {
      actualizarContador(segsRestantes);
    }
  }

  delay(200);
}

// ============================================================
// Conecta el Arduino a la red WiFi configurada
void conectarWiFi() {
  Serial.println(">>> Entrando a conectarWiFi()");
  Serial.print("Version firmware modulo WiFi: ");
  Serial.println(WiFi.firmwareVersion());
  Serial.print("Estado inicial WiFi.status(): ");
  Serial.println(WiFi.status());

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(15, 20);
  tft.print("Conectando WiFi...");
  tft.setTextColor(COLOR_GRIS);
  tft.setCursor(15, 35);
  tft.print(WIFI_SSID);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("ERROR: modulo WiFi no detectado (WL_NO_MODULE).");
    tft.setTextColor(COLOR_ROJO);
    tft.setCursor(15, 55);
    tft.print("Modulo WiFi no");
    tft.setCursor(15, 67);
    tft.print("detectado.");
    return;
  }

  int intentos = 0;
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED && intentos < 20) {
    Serial.print("Intento ");
    Serial.print(intentos + 1);
    Serial.print("/20 -> SSID: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("   Resultado WiFi.begin() = ");
    Serial.println(status);
    delay(2000);
    intentos++;
  }

  if (status == WL_CONNECTED) {
    // IMPORTANTE: justo después de WL_CONNECTED, el DHCP puede no haber
    // terminado de asignar la IP todavía (se ve como 0.0.0.0). Esperamos
    // activamente hasta que la IP sea válida, con un tope de seguridad.
    IPAddress ip = WiFi.localIP();
    int esperaIp = 0;
    while (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0 && esperaIp < 20) {
      delay(500);
      ip = WiFi.localIP();
      esperaIp++;
    }

    servidorWeb.begin();
    ipTexto = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);

    if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
      Serial.println("ADVERTENCIA: el router no asigno una IP valida (sigue en 0.0.0.0).");
      Serial.println("Revisa el DHCP del router o intenta reiniciar el Arduino.");
    }

    Serial.println("WiFi conectado.");
    Serial.print("IP del Arduino: ");
    Serial.println(ipTexto);
    Serial.println("Usa esa IP en el dashboard del PC.");

    tft.fillScreen(COLOR_BG);

    bool ipInvalida = (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

    if (ipInvalida) {
      tft.setTextColor(COLOR_ROJO);
      tft.setTextSize(1);
      tft.setCursor(10, 25);
      tft.print("WiFi OK pero sin IP");
      tft.setCursor(10, 40);
      tft.print("valida (0.0.0.0)");
      tft.setTextColor(COLOR_GRIS);
      tft.setCursor(10, 60);
      tft.print("Revisa el DHCP del");
      tft.setCursor(10, 72);
      tft.print("router o reinicia");
      tft.setCursor(10, 84);
      tft.print("el Arduino.");
      delay(4000);
      return;
    }

    tft.setTextColor(COLOR_VERDE);
    tft.setTextSize(1);
    tft.setCursor(15, 25);
    tft.print("WiFi conectado!");

    tft.setTextColor(COLOR_BLANCO);
    tft.setCursor(15, 45);
    tft.print("IP del sensor:");
    tft.setTextSize(2);
    tft.setTextColor(COLOR_CYAN);
    tft.setCursor(10, 60);
    tft.print(ipTexto);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRIS);
    tft.setCursor(5, 90);
    tft.print("Usa esta IP en tu");
    tft.setCursor(5, 102);
    tft.print("dashboard del PC.");

    delay(4000);
  } else {
    Serial.println("No se pudo conectar al WiFi. Revisa SSID/clave.");
    ipTexto = "Sin conexion";
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ROJO);
    tft.setCursor(10, 40);
    tft.print("No se pudo conectar");
    tft.setCursor(10, 55);
    tft.print("al WiFi.");
    tft.setTextColor(COLOR_GRIS);
    tft.setCursor(10, 75);
    tft.print("Revisa SSID y clave");
    tft.setCursor(10, 87);
    tft.print("en el sketch.");
    delay(4000);
  }
}

// ============================================================
// Atiende una petición HTTP si hay un cliente esperando.
// Rutas soportadas:
//   GET /data                          -> estado actual en JSON
//   GET /config?intervalo_ms=NNNN      -> cambia el intervalo de muestreo del sensor
// CORS abierto para que el dashboard pueda llamar sin bloqueos del navegador.
void manejarClienteWeb() {
  WiFiClient cliente = servidorWeb.available();
  if (!cliente) return;

  String peticion = "";
  unsigned long inicio = millis();
  while (cliente.connected() && millis() - inicio < 1000) {
    if (cliente.available()) {
      char c = cliente.read();
      peticion += c;
      if (peticion.endsWith("\r\n\r\n")) break;
    }
  }

  // --- Ruta /config: cambia el intervalo de muestreo en caliente ---
  if (peticion.indexOf("GET /config") >= 0) {
    int idx = peticion.indexOf("intervalo_ms=");
    String mensajeConfig = "sin_cambios";

    if (idx >= 0) {
      int inicioValor = idx + String("intervalo_ms=").length();
      int finValor = inicioValor;
      while (finValor < (int)peticion.length() && isDigit(peticion[finValor])) finValor++;
      String valorStr = peticion.substring(inicioValor, finValor);

      if (valorStr.length() > 0) {
        long nuevoIntervalo = valorStr.toInt();
        if (nuevoIntervalo >= (long)INTERVALO_MIN_MS && nuevoIntervalo <= (long)INTERVALO_MAX_MS) {
          INTERVALO_MS = (unsigned long)nuevoIntervalo;
          primeraVez = true; // fuerza una lectura inmediata con el nuevo intervalo
          mensajeConfig = "ok";
          Serial.print("Intervalo de muestreo actualizado a ");
          Serial.print(INTERVALO_MS);
          Serial.println(" ms (via dashboard).");
        } else {
          mensajeConfig = "fuera_de_rango";
        }
      }
    }

    String jsonConfig = "{\"intervalo_ms\":" + String(INTERVALO_MS) + ",\"estado\":\"" + mensajeConfig + "\"}";
    enviarRespuestaJson(cliente, jsonConfig);
    cliente.stop();
    return;
  }

  // --- Ruta /data (y cualquier otra por defecto): estado actual del sensor ---
  String json = "{";
  json += "\"ppm\":" + String(co2_ppm) + ",";
  json += "\"calentando\":" + String(calentando ? "true" : "false") + ",";
  json += "\"error\":" + String(sensorError ? "true" : "false") + ",";
  json += "\"segundos_desde_lectura\":" + String((millis() - ultimaLecturaMillis) / 1000) + ",";
  json += "\"intervalo_ms\":" + String(INTERVALO_MS) + ",";
  json += "\"uptime_ms\":" + String(millis());
  json += "}";

  enviarRespuestaJson(cliente, json);
  cliente.stop();
}

// ============================================================
// Envía una respuesta HTTP 200 con cuerpo JSON y CORS abierto
void enviarRespuestaJson(WiFiClient &cliente, const String &json) {
  cliente.println("HTTP/1.1 200 OK");
  cliente.println("Content-Type: application/json");
  cliente.println("Access-Control-Allow-Origin: *");
  cliente.println("Connection: close");
  cliente.print("Content-Length: ");
  cliente.println(json.length());
  cliente.println();
  cliente.println(json);
  delay(1);
}

// ============================================================
// Lee el CO2 del sensor MH-Z1911A por UART
// Retorna ppm o -1 si hay error
int leerCO2() {
  // Limpiar buffer
  while (co2Serial.available()) co2Serial.read();

  // Enviar comando de lectura
  co2Serial.write(cmdLeerCO2, 9);
  delay(150);

  // Esperar respuesta
  if (co2Serial.available() < 9) {
    delay(200);
  }

  if (co2Serial.available() >= 9) {
    byte respuesta[9];
    for (int i = 0; i < 9; i++) {
      respuesta[i] = co2Serial.read();
    }

    // Validar respuesta
    if (respuesta[0] == 0xFF && respuesta[1] == 0x86) {
      // Verificar checksum
      byte checksum = 0;
      for (int i = 1; i < 8; i++) checksum += respuesta[i];
      checksum = 0xFF - checksum + 1;

      if (checksum == respuesta[8]) {
        int ppm = (respuesta[2] << 8) | respuesta[3];
        return ppm;
      }
    }
  }
  return -1;
}

// ============================================================
// Pantalla de bienvenida
void pantallaBienvenida() {
  tft.fillScreen(COLOR_BG);

  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(18, 15);
  tft.print("DETECTOR");
  tft.setCursor(40, 37);
  tft.print("DE CO2");

  tft.drawLine(10, 58, 150, 58, COLOR_GRIS);

  tft.setTextColor(COLOR_GRIS);
  tft.setTextSize(1);
  tft.setCursor(20, 68);
  tft.print("MH-Z1911A + TFT");

  tft.setCursor(12, 85);
  tft.print("Iniciando sensor...");

  delay(2000);
}

// ============================================================
// Pantalla de calentamiento con barra de progreso
void pantallaCalentando(unsigned long transcurrido) {
  tft.fillScreen(COLOR_BG);

  tft.setTextColor(COLOR_AMARILLO);
  tft.setTextSize(1);
  tft.setCursor(30, 12);
  tft.print("CALENTANDO...");

  // Ícono termómetro simplificado
  tft.fillRect(75, 25, 10, 35, COLOR_GRIS);
  tft.fillCircle(80, 65, 8, COLOR_NARANJA);
  int nivelCalor = map(transcurrido, 0, CALENTAMIENTO_MS, 60, 26);
  tft.fillRect(76, nivelCalor, 8, 60 - nivelCalor, COLOR_NARANJA);

  // Barra de progreso
  int progreso = map(transcurrido, 0, CALENTAMIENTO_MS, 0, 120);
  tft.drawRect(20, 82, 120, 12, COLOR_GRIS);
  tft.fillRect(21, 83, progreso, 10, COLOR_NARANJA);

  int segsRestantes = (CALENTAMIENTO_MS - transcurrido) / 1000;
  tft.setTextColor(COLOR_BLANCO);
  tft.setCursor(40, 100);
  tft.print("Listo en ");
  tft.print(segsRestantes);
  tft.print("s");

  // IP pequeña abajo para tenerla siempre visible
  tft.setTextColor(COLOR_GRIS);
  tft.setCursor(5, 118);
  tft.print("IP: ");
  tft.print(ipTexto);
}

// ============================================================
// Muestra el resultado de CO2 con color según calidad
void mostrarResultado(int ppm) {
  tft.fillScreen(COLOR_BG);

  // Determinar nivel y color
  uint16_t colorNivel;
  String etiqueta;
  String mensaje;

  if (ppm < 800) {
    colorNivel = COLOR_VERDE;
    etiqueta = "EXCELENTE";
    mensaje = "Aire muy fresco";
  } else if (ppm < 1000) {
    colorNivel = 0x07E0; // verde
    etiqueta = "BUENO";
    mensaje = "Aire aceptable";
  } else if (ppm < 1500) {
    colorNivel = COLOR_AMARILLO;
    etiqueta = "REGULAR";
    mensaje = "Ventila pronto";
  } else if (ppm < 2000) {
    colorNivel = COLOR_NARANJA;
    etiqueta = "MALO";
    mensaje = "VENTILAR YA";
  } else {
    colorNivel = COLOR_ROJO;
    etiqueta = "PELIGROSO";
    mensaje = "VENTILAR AHORA";
  }

  // Barra de fondo de color para el nivel
  tft.fillRect(0, 0, 160, 38, colorNivel);

  // Etiqueta de calidad
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(2);
  int xEtiqueta = (160 - etiqueta.length() * 12) / 2;
  if (xEtiqueta < 2) xEtiqueta = 2;
  tft.setCursor(xEtiqueta, 10);
  tft.print(etiqueta);

  // Valor PPM grande
  tft.setTextColor(COLOR_BLANCO);
  tft.setTextSize(3);
  String ppmStr = String(ppm);
  int xPPM = (160 - ppmStr.length() * 18) / 2;
  tft.setCursor(xPPM, 45);
  tft.print(ppmStr);

  // Unidad
  tft.setTextColor(COLOR_GRIS);
  tft.setTextSize(1);
  tft.setCursor(115, 72);
  tft.print("ppm");

  // Línea separadora
  tft.drawLine(10, 82, 150, 82, COLOR_GRIS);

  // Mensaje descriptivo
  tft.setTextColor(colorNivel == COLOR_ROJO ? COLOR_ROJO : colorNivel == COLOR_NARANJA ? COLOR_NARANJA : COLOR_BLANCO);
  tft.setTextSize(1);
  int xMsg = (160 - mensaje.length() * 6) / 2;
  if (xMsg < 2) xMsg = 2;
  tft.setCursor(xMsg, 90);
  tft.print(mensaje);

  // Escala visual de CO2 (mini barra)
  tft.setTextColor(COLOR_GRIS);
  tft.setCursor(10, 106);
  tft.print("CO2: 400");
  tft.setCursor(118, 106);
  tft.print("5000");

  tft.drawRect(10, 114, 140, 8, COLOR_GRIS);
  int barraAncho = map(min(ppm, 5000), 400, 5000, 0, 138);
  tft.fillRect(11, 115, barraAncho, 6, colorNivel);

  // Contador - se actualizará cada segundo
  tft.setTextColor(COLOR_GRIS);
  tft.setTextSize(1);
  tft.setCursor(10, 116);  // posición reservada para el contador (se dibuja abajo)
}

// ============================================================
// Actualiza solo el contador de próxima lectura
void actualizarContador(long segs) {
  // Limpiar área del contador
  tft.fillRect(0, 115, 160, 13, COLOR_BG);

  tft.setTextColor(COLOR_GRIS);
  tft.setTextSize(1);

  String txt = "Prox: " + String(segs) + "s";
  int x = (160 - txt.length() * 6) / 2;
  tft.setCursor(x, 118);
  tft.print(txt);
}

// ============================================================
// Pantalla de error de lectura
void pantallaError() {
  tft.fillScreen(COLOR_BG);

  tft.fillRect(0, 0, 160, 30, COLOR_ROJO);
  tft.setTextColor(COLOR_BLANCO);
  tft.setTextSize(1);
  tft.setCursor(40, 11);
  tft.print("ERROR");

  tft.setTextColor(COLOR_ROJO);
  tft.setTextSize(1);
  tft.setCursor(10, 42);
  tft.print("Sin respuesta del");
  tft.setCursor(10, 55);
  tft.print("sensor CO2.");
  tft.setCursor(10, 72);
  tft.print("Verifica cables:");
  tft.setTextColor(COLOR_GRIS);
  tft.setCursor(10, 85);
  tft.print("TXD sensor -> D2");
  tft.setCursor(10, 97);
  tft.print("RXD sensor -> D3");
  tft.setCursor(10, 109);
  tft.print("VIN -> 5V");

  delay(3000);
}
