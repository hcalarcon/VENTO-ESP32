// EMISOR

#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_GFX_Library.h>

// ===== LCD =====
#define LCD_MOSI 45
#define LCD_SCLK 40
#define LCD_CS   42
#define LCD_DC   41
#define LCD_RST  39
#define LCD_BL   46

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus, LCD_RST, 0, true, 172, 320, 34, 0, 34, 0
);

// ===== WIFI =====
const char* ssid = "construccion";
const char* password = "aaaaaaaa";

// ===== ESP32 RECEPTOR =====
const char* receptor = "http://192.168.137.214"; // ⚠️ IP del RECEPTOR

// ===== SUPABASE =====
const char* supabaseUrl = "https://yjfljufancywoifambbh.supabase.co/functions/v1/ingest-reading";
const char* deviceToken = "ffffa83f-1725-43b1-823d-b8ff5f7d17ec";

// ===== TIEMPO =====
unsigned long ultimaLectura = 0;
const unsigned long intervaloLectura = 30000; // 🔥 1 minuto (60 segundos)
int contadorLectura = 0;


// =====================================================
// MOSTRAR EN LCD
// =====================================================
void mostrarPantalla(String titulo, String mensaje) {
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 30);
  gfx->println("VENTO");
  gfx->println();
  gfx->println(titulo);
  gfx->println();
  gfx->println(mensaje);
}


// =====================================================
// ENVIAR ALERTA AL ESP32 RECEPTOR
// =====================================================
void enviarAlerta() {
  HTTPClient http;
  String url = String(receptor) + "/mensaje?mensaje=ALERTA%20CO";

  Serial.println();
  Serial.println("================================");
  Serial.println("ENVIANDO ALERTA AL RECEPTOR");
  Serial.println(url);

  http.begin(url);
  int codigo = http.GET();

  Serial.print("Codigo HTTP receptor: ");
  Serial.println(codigo);

  if (codigo > 0) {
    String respuesta = http.getString();
    Serial.println("Respuesta del receptor:");
    Serial.println(respuesta);
    
    if (codigo == 200) {
      Serial.println("✅ ALERTA ENVIADA CORRECTAMENTE");
      mostrarPantalla("ALERTA", "ENVIADA!");
    } else if (codigo == 409) {
      Serial.println("⚠️ RECEPTOR OCUPADO: Ventana ya en ciclo");
      mostrarPantalla("RECEPTOR", "OCUPADO");
    } else {
      Serial.println("ERROR ENVIANDO ALERTA");
      mostrarPantalla("ERROR", "ALERTA");
    }
  } else {
    Serial.println("ERROR DE CONEXION");
    mostrarPantalla("ERROR", "CONEXION");
  }
  http.end();
}


// =====================================================
// ENVIAR LECTURA A SUPABASE
// =====================================================
void enviarLectura(float co_ppm, bool alerta) {
  HTTPClient http;

  Serial.println();
  Serial.println("================================");
  Serial.println("ENVIANDO LECTURA A SUPABASE");

  String json = "{";
  json += "\"device_token\":\"" + String(deviceToken) + "\",";
  json += "\"co_ppm\":" + String(co_ppm, 1) + ",";
  json += "\"alert\":" + String(alerta ? "true" : "false");
  json += "}";

  Serial.println("JSON:");
  Serial.println(json);

  http.begin(supabaseUrl);
  http.addHeader("Content-Type", "application/json");

  int codigo = http.POST(json);
  Serial.print("Codigo HTTP Supabase: ");
  Serial.println(codigo);

  if (codigo > 0) {
    String respuesta = http.getString();
    Serial.println("Respuesta:");
    Serial.println(respuesta);

    if (codigo >= 200 && codigo < 300) {
      Serial.println("Lectura enviada correctamente");
      mostrarPantalla(alerta ? "ALERTA CO" : "CO NORMAL", String(co_ppm, 1) + " ppm");
    } else {
      Serial.println("Supabase devolvio un error");
    }
  } else {
    Serial.println("Error conectando con Supabase");
    mostrarPantalla("ERROR", "Supabase");
  }
  http.end();
}


// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // LCD
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 30);
  gfx->println("PANCHO");
  gfx->println();
  gfx->println("Conectando...");

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("IP EMISOR: ");
  Serial.println(WiFi.localIP());

  mostrarPantalla("WiFi OK", WiFi.localIP().toString());
  delay(1500);
  mostrarPantalla("SISTEMA", "Iniciado");
}


// =====================================================
// LOOP
// =====================================================
void loop() {
  unsigned long ahora = millis();

  if (ahora - ultimaLectura >= intervaloLectura) {
    ultimaLectura = ahora;
    contadorLectura++;

    // 🔥 LECTURA SIMULADA: Patrón 2 bajos, 1 alto
    float co_ppm;
    bool alerta = false;

    if (contadorLectura % 3 == 0) {
      // Cada tercera lectura es ALTA
      co_ppm = 115.4;
      alerta = true;
    } else {
      // Las otras dos son BAJAS
      co_ppm = 35.0 + (contadorLectura % 5) * 2.5;
      alerta = false;
    }

    Serial.println();
    Serial.println("********************************");
    Serial.print("Lectura #");
    Serial.println(contadorLectura);
    Serial.print("CO: ");
    Serial.print(co_ppm);
    Serial.println(" ppm");
    Serial.print("Alerta: ");
    Serial.println(alerta ? "SI" : "NO");

    // Enviar a Supabase
    enviarLectura(co_ppm, alerta);

    // Si hay alerta, avisar al receptor
    if (alerta) {
      enviarAlerta();
    }
  }
}