#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>

#include <string.h>

// pines (ajustar según el módulo/placa)
// NOTA: usa números GPIO (no símbolos D1/D2) para compatibilidad con ESP-01
// Cambia estos valores según tu placa (ej. NodeMCU: D6=12, D7=13)
#define TRIG_PIN 12
#define ECHO_PIN 13
#define MAX_DISTANCE 200 // cm

// pantalla I2C 16x2 (direccion 0x27 suele ser común)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// servidor web
ESP8266WebServer server(80);

struct WiFiCred {
  char ssid[32];
  char pass[64];
};
WiFiCred wifiCred;

// sensor ultrasónico
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

bool hasSavedCredentials() {
  return strlen(wifiCred.ssid) > 0;
}

void saveCredentials() {
  EEPROM.put(0, wifiCred);
  EEPROM.commit();
}

void loadCredentials() {
  EEPROM.get(0, wifiCred);
}

void clearCredentials() {
  memset(&wifiCred, 0, sizeof(wifiCred));
  saveCredentials();
}

long getDistance() {
  unsigned int uS = sonar.ping();
  // convertir microsegundos a distancia en cm
  return (long)(uS / US_ROUNDTRIP_CM);
}

void handleRoot() {
  if (WiFi.getMode() == WIFI_AP) {
    // página de configuración
    String html = "<html><head><meta charset='utf-8'><title>Config WiFi</title></head><body>";
    html += "<h3>Configuración de red</h3>";
    html += "<form action='/save' method='get'>";
    html += "SSID: <input name='ssid'><br>";
    html += "Password: <input name='pass' type='password'><br>";
    html += "<input type='submit' value='Guardar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  } else {
    // devolver distancia medida
    long dist = getDistance();
    server.send(200, "text/plain", String(dist));
  }
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.toCharArray(wifiCred.ssid, sizeof(wifiCred.ssid));
    pass.toCharArray(wifiCred.pass, sizeof(wifiCred.pass));
    saveCredentials();
    String resp = "<html><body><h3>Guardado. Reiniciando...</h3></body></html>";
    server.send(200, "text/html", resp);
    delay(1500);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Faltan parámetros");
  }
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SensorNivelCfg");
  IPAddress IP = WiFi.softAPIP();
  lcd.clear();
  lcd.print("AP config");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.softAPIP().toString().c_str());
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiCred.ssid, wifiCred.pass);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
    lcd.clear();
    lcd.print("Conectando");
    lcd.setCursor(0, 1);
    lcd.print(retries);
  }
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.print("IP:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString().c_str());
    server.on("/", handleRoot);
    server.begin();
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  // reservar EEPROM (usar un tamaño fijo razonable para ESP8266)
  EEPROM.begin(512);
  loadCredentials();

  if (hasSavedCredentials()) {
    if (!connectWiFi()) {
      // no pudo conectar, borrar credenciales y arrancar AP
      clearCredentials();
      startAP();
    }
  } else {
    startAP();
  }
}

void loop() {
  server.handleClient();
  // actualizar distancia y mostrar en lcd si está en STA
  static unsigned long last=0;
  if (WiFi.getMode() == WIFI_STA && millis() - last > 1000) {
    last = millis();
    long d = getDistance();
    lcd.clear();
    lcd.print("Dist:");
    lcd.print(d);
    lcd.print("cm");
  }
}
