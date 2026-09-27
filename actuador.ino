// =====================================================
// RECEPTOR VENTO-APP
// ESP32-S3 + LCD ST7789 + 2 SERVOS
// =====================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Arduino_GFX_Library.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "construccion";
const char* password = "aaaaaaaa";

// =====================================================
// SERVIDOR WEB
// =====================================================

WebServer server(80);

// =====================================================
// SERVOMOTORES
// =====================================================

#define SERVO_PIN_1 7
#define SERVO_PIN_2 8

Servo servo1;
Servo servo2;

const int ANGULO_CIERRE   = 0;
const int ANGULO_APERTURA = 45;

const unsigned long TIEMPO_ESPERA = 30000; // 30 segundos

// =====================================================
// LCD ST7789
// =====================================================

#define LCD_MOSI 45
#define LCD_SCLK 40
#define LCD_CS   42
#define LCD_DC   41
#define LCD_RST  39
#define LCD_BL   46

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC,
  LCD_CS,
  LCD_SCLK,
  LCD_MOSI
);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  0,
  true,
  172,
  320,
  34,
  0,
  34,
  0
);

// =====================================================
// ESTADOS
// =====================================================

enum EstadoVentana {
  CERRADA,
  ABRIENDO,
  ESPERANDO,
  CERRANDO
};

EstadoVentana estadoVentana = CERRADA;

unsigned long tiempoInicioAccion = 0;


// =====================================================
// LCD
// =====================================================

void mostrarLCD(
  String estado,
  String detalle = ""
) {

  gfx->fillScreen(RGB565_WHITE);

  // -----------------------------
  // Título
  // -----------------------------

  gfx->setTextColor(RGB565_BLACK);
  gfx->setTextSize(2);

  gfx->setCursor(15, 25);
  gfx->println("VENTO-APP");

  gfx->setCursor(15, 50);
  gfx->println("RECEPTOR");

  // Línea separadora
  gfx->drawLine(
    10,
    75,
    162,
    75,
    RGB565_BLACK
  );

  // -----------------------------
  // Estado
  // -----------------------------

  gfx->setTextSize(2);

  gfx->setCursor(15, 100);
  gfx->println(estado);

  // -----------------------------
  // Detalle
  // -----------------------------

  if (detalle != "") {

    gfx->setTextSize(1);

    gfx->setCursor(15, 135);

    gfx->println(detalle);
  }

  // -----------------------------
  // IP
  // -----------------------------

  if (WiFi.status() == WL_CONNECTED) {

    gfx->setTextSize(1);

    gfx->setCursor(15, 270);

    gfx->println("IP:");

    gfx->setCursor(15, 285);

    gfx->println(WiFi.localIP().toString());
  }
}


// =====================================================
// MOSTRAR ESTADO DE VENTANA
// =====================================================

void actualizarLCD() {

  switch (estadoVentana) {

    case CERRADA:

      mostrarLCD(
        "VENTANA",
        "CERRADA"
      );

      break;


    case ABRIENDO:

      mostrarLCD(
        "ALERTA CO",
        "Abriendo ventana..."
      );

      break;


    case ESPERANDO: {

      unsigned long ahora = millis();

      unsigned long transcurrido =
        ahora - tiempoInicioAccion;

      unsigned long restante =
        (TIEMPO_ESPERA - transcurrido) / 1000;

      String detalle =
        "Ventana abierta\n";

      gfx->fillScreen(RGB565_WHITE);

      gfx->setTextColor(RGB565_BLACK);

      gfx->setTextSize(2);

      gfx->setCursor(15, 25);
      gfx->println("VENTO-APP");

      gfx->setCursor(15, 50);
      gfx->println("ALERTA CO");

      gfx->drawLine(
        10,
        75,
        162,
        75,
        RGB565_BLACK
      );

      gfx->setTextSize(2);

      gfx->setCursor(15, 100);
      gfx->println("VENTANA");

      gfx->setCursor(15, 130);
      gfx->println("ABIERTA");

      gfx->setTextSize(3);

      gfx->setCursor(45, 175);

      gfx->print(restante);

      gfx->println(" s");

      gfx->setTextSize(1);

      gfx->setCursor(15, 270);
      gfx->println("IP:");

      gfx->setCursor(15, 285);
      gfx->println(WiFi.localIP().toString());

      break;
    }


    case CERRANDO:

      mostrarLCD(
        "VENTANA",
        "Cerrando..."
      );

      break;
  }
}


// =====================================================
// MÁQUINA DE ESTADOS
// =====================================================

void gestionarVentana() {

  unsigned long ahora = millis();

  switch (estadoVentana) {

    case CERRADA:

      break;


    case ABRIENDO:

      if (ahora - tiempoInicioAccion >= 500) {

        estadoVentana = ESPERANDO;

        tiempoInicioAccion = ahora;

        Serial.println(
          "✅ Ventana abierta. Esperando 30 segundos..."
        );

        actualizarLCD();
      }

      break;


    case ESPERANDO:

      if (ahora - tiempoInicioAccion >= TIEMPO_ESPERA) {

        Serial.println(
          "⏱️ 30 segundos cumplidos. Cerrando ventana..."
        );

        estadoVentana = CERRANDO;

        actualizarLCD();
      }

      break;


    case CERRANDO:

      servo1.write(ANGULO_CIERRE);
      servo2.write(ANGULO_CIERRE);

      estadoVentana = CERRADA;

      Serial.println(
        "🔒 Ventana cerrada. Listo para nueva alerta."
      );

      actualizarLCD();

      break;
  }
}


// =====================================================
// HANDLER /mensaje
// =====================================================

void handleMensaje() {

  // ---------------------------------------------------
  // Si la ventana está ocupada
  // ---------------------------------------------------

  if (estadoVentana != CERRADA) {

    Serial.println(
      "⚠️ ALERTA IGNORADA: Ventana ocupada"
    );

    server.send(
      409,
      "text/plain",
      "Ventana ocupada - Intenta mas tarde"
    );

    return;
  }


  // ---------------------------------------------------
  // Verificar mensaje
  // ---------------------------------------------------

  if (server.hasArg("mensaje")) {

    String msg = server.arg("mensaje");

    Serial.println();
    Serial.println("================================");
    Serial.print("📩 MENSAJE RECIBIDO: ");
    Serial.println(msg);


    // =================================================
    // ALERTA CO
    // =================================================

    if (msg == "ALERTA CO") {

      Serial.println(
        "🚨 ALERTA CO detectada. Abriendo ventana..."
      );

      // LCD
      mostrarLCD(
        "ALERTA CO",
        "Abriendo ventana..."
      );


      // Servos
      servo1.write(ANGULO_APERTURA);
      servo2.write(ANGULO_APERTURA);


      // Estado
      estadoVentana = ABRIENDO;

      tiempoInicioAccion = millis();


      server.send(
        200,
        "text/plain",
        "OK - Ventana activada"
      );

    }

    else {

      server.send(
        400,
        "text/plain",
        "Mensaje no reconocido"
      );
    }

  }

  else {

    server.send(
      400,
      "text/plain",
      "Falta parametro 'mensaje'"
    );
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);


  // ===================================================
  // LCD
  // ===================================================

  pinMode(LCD_BL, OUTPUT);

  digitalWrite(LCD_BL, HIGH);

  Serial.println(
    "Iniciando pantalla..."
  );

  if (!gfx->begin()) {

    Serial.println(
      "ERROR iniciando LCD"
    );

  } else {

    Serial.println(
      "Pantalla iniciada"
    );

    gfx->fillScreen(
      RGB565_WHITE
    );

    gfx->setTextColor(
      RGB565_BLACK
    );

    gfx->setTextSize(2);

    gfx->setCursor(
      15,
      25
    );

    gfx->println(
      "VENTO-APP"
    );

    gfx->setCursor(
      15,
      55
    );

    gfx->println(
      "Iniciando..."
    );

    delay(1000);
  }


  // ===================================================
  // SERVOS
  // ===================================================

  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);

  servo1.write(ANGULO_CIERRE);
  servo2.write(ANGULO_CIERRE);

  Serial.println(
    "Servomotores inicializados en posición de cierre."
  );


  // ===================================================
  // WIFI
  // ===================================================

  WiFi.begin(
    ssid,
    password
  );

  Serial.print(
    "Conectando WiFi"
  );

  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "WiFi conectado!"
  );

  Serial.print(
    "IP RECEPTOR: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // ===================================================
  // LCD - ESTADO INICIAL
  // ===================================================

  mostrarLCD(
    "VENTANA",
    "CERRADA - LISTO"
  );


  // ===================================================
  // SERVIDOR
  // ===================================================

  server.on(
    "/mensaje",
    HTTP_GET,
    handleMensaje
  );


  server.on(
    "/",
    HTTP_GET,
    []() {

      server.send(
        200,
        "text/plain",
        "ESP32 RECEPTOR - Listo"
      );

    }
  );


  server.begin();

  Serial.println(
    "Servidor HTTP iniciado en puerto 80"
  );

  Serial.println(
    "Esperando alertas del ESP32 emisor..."
  );
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  server.handleClient();

  gestionarVentana();


  // ---------------------------------------------------
  // Actualizar contador del LCD mientras espera
  // ---------------------------------------------------

  static unsigned long ultimaActualizacionLCD = 0;

  if (
    estadoVentana == ESPERANDO &&
    millis() - ultimaActualizacionLCD >= 1000
  ) {

    ultimaActualizacionLCD = millis();

    actualizarLCD();
  }
}