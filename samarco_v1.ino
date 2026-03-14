/*
 * ============================================================================
 * SISTEMA SAMARCO — PIEZÔMETRO v2.0
 * ============================================================================
 * Hardware:  ESP32 DevKit V1 + BMP180 + OLED SSD1306 128x64
 * Broker:    InfluxDB Cloud v2 (AWS us-east-1)
 * Revisão:   Mar 2026
 *
 * MELHORIAS v2.0:
 *  - Reconexão Wi-Fi automática sem travar o loop
 *  - Watchdog timer (reinicia se travar por 30s)
 *  - NTP para timestamp real nas leituras
 *  - Fila offline (até 10 leituras) — não perde dados se Wi-Fi cair
 *  - Tags de identificação do dispositivo no InfluxDB
 *  - Altitude calculada com pressão local real (912 hPa)
 *  - Log serial estruturado com timestamp
 *  - Intervalo configurável por #define
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <esp_task_wdt.h>   // Watchdog
#include <time.h>           // NTP


// ============================================================
//  CONFIGURAÇÕES — todas as credenciais ficam em config.h
//  Copie config.example.h → config.h e preencha com seus dados
// ============================================================
#include "config.h"

// Pinos
#define LED_VERDE    32
#define LED_AMARELO  33
#define LED_VERMELHO 25
#define BUZZER       26
#define SDA_PIN      21
#define SCL_PIN      22

// Display OLED
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

// Thresholds de pressão (hPa) — calibrados para ~800m altitude
#define PRESSAO_NORMAL   912.0f
#define PRESSAO_ATENCAO  907.0f
#define PRESSAO_MIN_VALIDA 500.0f
#define PRESSAO_MAX_VALIDA 1200.0f

// Pressão local ao nível do mar equivalente para cálculo de altitude
// Calculado para ~800m: 101325 * pow(1 - 800/44330, 5.255) ≈ 91200 Pa
#define PRESSAO_ALTITUDE_PA 91200

// Intervalos (milissegundos)
#define INTERVALO_SENSOR   1000   // Leitura do BMP180: 1s
#define INTERVALO_DISPLAY   250   // Atualização OLED: 250ms
#define INTERVALO_DB      60000   // Envio ao InfluxDB: 60s (produção)
// #define INTERVALO_DB   10000   // 10s para testes — descomente se necessário
#define INTERVALO_WIFI    30000   // Tenta reconectar Wi-Fi a cada 30s
#define WATCHDOG_TIMEOUT     30   // Reinicia se travar por 30s

// Fila offline
#define FILA_MAX 10

// ============================================================
//  OBJETOS
// ============================================================
Adafruit_BMP085 bmp;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("telemetria_samarco");

// ============================================================
//  ESTRUTURAS
// ============================================================
struct Leitura {
  float pressao;
  float temperatura;
  float altitude;
  int   alerta;
  time_t timestamp;
};

// ============================================================
//  VARIÁVEIS GLOBAIS
// ============================================================
float    temperatura  = 0;
float    pressao      = 0;
float    altitude     = 0;
int      corAtual     = 0;   // 0=Normal 1=Atenção 2=Crítico 3=Erro
String   nivelAlerta  = "NORMAL";
bool     wifiConectado = false;
bool     ntpSincronizado = false;

Leitura  fila[FILA_MAX];
int      filaCount = 0;

unsigned long tSensor  = 0;
unsigned long tDisplay = 0;
unsigned long tDB      = 0;
unsigned long tWifi    = 0;
unsigned long tPiscaVermelho = 0;
unsigned long tBuzzer  = 0;
bool estadoBuzzer = false;

// ============================================================
//  LOG SERIAL ESTRUTURADO
// ============================================================
void logInfo(String msg) {
  Serial.printf("[%6lus][INFO ] %s\n", millis() / 1000, msg.c_str());
}
void logWarn(String msg) {
  Serial.printf("[%6lus][WARN ] %s\n", millis() / 1000, msg.c_str());
}
void logErro(String msg) {
  Serial.printf("[%6lus][ERRO ] %s\n", millis() / 1000, msg.c_str());
}

// ============================================================
//  WI-FI — RECONEXÃO SEM BLOQUEAR O LOOP
// ============================================================
void tentarReconectarWiFi() {
  if (wifiMulti.run() == WL_CONNECTED) {
    if (!wifiConectado) {
      wifiConectado = true;
      logInfo("Wi-Fi conectado — IP: " + WiFi.localIP().toString());
    }
    return;
  }

  wifiConectado = false;

  // Tenta reconectar apenas a cada INTERVALO_WIFI
  unsigned long agora = millis();
  if (agora - tWifi < INTERVALO_WIFI) return;
  tWifi = agora;

  logWarn("Wi-Fi perdido — tentando reconectar...");
  // wifiMulti.run() já gerencia a reconexão internamente
  if (wifiMulti.run() == WL_CONNECTED) {
    wifiConectado = true;
    logInfo("Wi-Fi reconectado — IP: " + WiFi.localIP().toString());
    sincronizarNTP();
  }
}

// ============================================================
//  NTP — SINCRONIZAÇÃO DE HORÁRIO
// ============================================================
void sincronizarNTP() {
  if (!wifiConectado) return;

  logInfo("Sincronizando NTP...");
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

  unsigned long inicio = millis();
  while (time(nullptr) < 1000000000UL) {
    if (millis() - inicio > 10000) {
      logWarn("NTP timeout — usando tempo do millis()");
      return;
    }
    delay(200);
  }
  ntpSincronizado = true;

  struct tm ti;
  getLocalTime(&ti);
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &ti);
  logInfo(String("Horário sincronizado: ") + buf);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n============================================");
  Serial.println("  SAMARCO — Piezômetro v2.0");
  Serial.println("============================================");

  // Pinos
  pinMode(LED_VERDE,    OUTPUT);
  pinMode(LED_AMARELO,  OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER,       OUTPUT);

  // Watchdog — reinicia se travar
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  logInfo("Watchdog configurado (" + String(WATCHDOG_TIMEOUT) + "s)");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // BMP180
  if (!bmp.begin()) {
    logErro("BMP180 não encontrado! Verifique a ligação SDA/SCL.");
    while (1) { delay(1000); } // Para aqui — sem sensor não tem sistema
  }
  logInfo("BMP180 inicializado");

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    logWarn("OLED não encontrado — continuando sem display");
  } else {
    display.setTextColor(SSD1306_WHITE);
    mostrarSplash();
    logInfo("OLED inicializado");
  }

  // Wi-Fi
  wifiMulti.addAP(WIFI_SSID_1, WIFI_PASS_1);
  // wifiMulti.addAP(WIFI_SSID_2, WIFI_PASS_2); // Descomente se usar segunda rede

  logInfo("Conectando Wi-Fi...");
  unsigned long tInicio = millis();
  while (wifiMulti.run() != WL_CONNECTED) {
    if (millis() - tInicio > 20000) {
      logWarn("Wi-Fi não disponível no boot — continuando offline");
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (wifiMulti.run() == WL_CONNECTED) {
    wifiConectado = true;
    logInfo("Wi-Fi conectado — IP: " + WiFi.localIP().toString());
    sincronizarNTP();
  }

  // InfluxDB — Tags fixas de identificação
  sensor.addTag("dispositivo", DISPOSITIVO_ID);
  sensor.addTag("local",       LOCAL_ID);
  sensor.addTag("setor",       SETOR_ID);

  // Valida conexão InfluxDB
  if (wifiConectado) {
    if (client.validateConnection()) {
      logInfo("InfluxDB conectado: " + client.getServerUrl());
    } else {
      logWarn("InfluxDB falhou: " + client.getLastErrorMessage());
    }
  }

  digitalWrite(LED_VERDE, HIGH);
  logInfo("Sistema iniciado! Dispositivo: " + String(DISPOSITIVO_ID));
  Serial.println("============================================\n");
}

// ============================================================
//  LOOP PRINCIPAL
// ============================================================
void loop() {
  esp_task_wdt_reset(); // Alimenta o watchdog

  unsigned long agora = millis();

  // 1. Gerencia Wi-Fi
  tentarReconectarWiFi();

  // 2. Lê sensor
  if (agora - tSensor >= INTERVALO_SENSOR) {
    lerSensor();
    tSensor = agora;
  }

  // 3. Atualiza display
  if (agora - tDisplay >= INTERVALO_DISPLAY) {
    mostrarDisplay(agora);
    tDisplay = agora;
  }

  // 4. Envia ao InfluxDB
  if (agora - tDB >= INTERVALO_DB) {
    enviarParaBaseDeDados();
    tDB = agora;
  }

  // 5. Hardware
  atualizarLEDs(agora);
  atualizarBuzzer(agora);
}

// ============================================================
//  LEITURA DO SENSOR
// ============================================================
void lerSensor() {
  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0f;
  float a = bmp.readAltitude(PRESSAO_ALTITUDE_PA); // Corrigido para altitude local

  // Fail-safe — ignora leituras claramente erradas
  if (p < PRESSAO_MIN_VALIDA || p > PRESSAO_MAX_VALIDA) {
    corAtual   = 3;
    nivelAlerta = "ERRO SENSOR";
    logErro("Leitura inválida do BMP180: " + String(p) + " hPa");
    return;
  }

  temperatura = t;
  pressao     = p;
  altitude    = a;

  // Determina nível de alerta
  if (pressao >= PRESSAO_NORMAL) {
    corAtual    = 0;
    nivelAlerta = "NORMAL";
  } else if (pressao >= PRESSAO_ATENCAO) {
    corAtual    = 1;
    nivelAlerta = "ATENCAO";
    logWarn("Pressão em zona de atenção: " + String(pressao, 2) + " hPa");
  } else {
    corAtual    = 2;
    nivelAlerta = "CRITICO";
    logErro("ALERTA CRÍTICO — Pressão: " + String(pressao, 2) + " hPa");
  }
}

// ============================================================
//  ENVIO AO INFLUXDB COM FILA OFFLINE
// ============================================================
void enviarParaBaseDeDados() {
  // Sempre enfileira a leitura atual
  if (filaCount < FILA_MAX) {
    fila[filaCount].pressao     = pressao;
    fila[filaCount].temperatura = temperatura;
    fila[filaCount].altitude    = altitude;
    fila[filaCount].alerta      = corAtual;
    fila[filaCount].timestamp   = ntpSincronizado ? time(nullptr) : 0;
    filaCount++;
  } else {
    logWarn("Fila offline cheia (" + String(FILA_MAX) + ") — descartando leitura mais antiga");
    // Remove a mais antiga (FIFO)
    for (int i = 0; i < FILA_MAX - 1; i++) fila[i] = fila[i + 1];
    fila[FILA_MAX - 1] = { pressao, temperatura, altitude, corAtual,
                           ntpSincronizado ? time(nullptr) : 0 };
  }

  // Sem Wi-Fi — guarda para depois
  if (!wifiConectado) {
    logWarn("Offline — " + String(filaCount) + " leitura(s) na fila");
    return;
  }

  logInfo("Enviando " + String(filaCount) + " leitura(s) ao InfluxDB...");

  int enviados = 0;
  for (int i = 0; i < filaCount; i++) {
    sensor.clearFields();
    sensor.addField("pressao",     fila[i].pressao);
    sensor.addField("temperatura", fila[i].temperatura);
    sensor.addField("altitude",    fila[i].altitude);
    sensor.addField("alerta",      fila[i].alerta);

    // Usa timestamp real se disponível
    if (fila[i].timestamp > 0) {
      sensor.setTime(fila[i].timestamp);
    }

    if (client.writePoint(sensor)) {
      enviados++;
    } else {
      logErro("Falha ao enviar ponto " + String(i + 1) + ": " + client.getLastErrorMessage());
      // Remove os enviados com sucesso e para
      if (enviados > 0) {
        for (int j = 0; j < filaCount - enviados; j++) {
          fila[j] = fila[j + enviados];
        }
        filaCount -= enviados;
      }
      return;
    }
  }

  logInfo("✓ " + String(enviados) + " ponto(s) gravados com sucesso");
  filaCount = 0; // Limpa fila — tudo enviado
}

// ============================================================
//  DISPLAY OLED
// ============================================================
void mostrarSplash() {
  display.clearDisplay();
  display.setTextSize(1); display.setCursor(10, 10);
  display.println("  SAMARCO");
  display.setCursor(5, 25);
  display.println(" Piezometro v2.0");
  display.setCursor(15, 42);
  display.println(DISPOSITIVO_ID);
  display.display();
  delay(2000);
}

void mostrarDisplay(unsigned long agora) {
  display.clearDisplay();

  // Cabeçalho
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SAMARCO PIEZOMETRO");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if (corAtual == 3) {
    // Erro piscante
    display.setTextSize(2);
    display.setCursor(10, 25);
    if ((agora / 500) % 2 == 0) display.print("FALHA NO\n SENSOR!");
  } else {
    // Dados normais
    display.setTextSize(1);
    display.setCursor(0, 14); display.print("Temp:  "); display.print(temperatura, 1); display.print(" C");
    display.setCursor(0, 24); display.print("Press: "); display.print(pressao, 1);     display.print(" hPa");
    display.setCursor(0, 34); display.print("Alt:   "); display.print(altitude, 0);    display.print(" m");
    display.drawLine(0, 44, 128, 44, SSD1306_WHITE);

    // Nível de alerta
    display.setTextSize(2);
    display.setCursor(0, 50);
    if (nivelAlerta == "NORMAL") {
      display.print("NORMAL");
    } else if (nivelAlerta == "ATENCAO" && (agora / 500) % 2 == 0) {
      display.print("ATENCAO");
    } else if (nivelAlerta == "CRITICO" && (agora / 250) % 2 == 0) {
      display.print("CRITICO!");
    }
  }

  // Ícones de status (canto superior direito)
  display.setTextSize(1);
  // Wi-Fi
  display.setCursor(110, 0);
  display.print(wifiConectado ? "W" : "!");
  // Fila offline
  if (filaCount > 0) {
    display.setCursor(120, 0);
    display.print(String(filaCount));
  }

  display.display();
}

// ============================================================
//  LEDS
// ============================================================
void atualizarLEDs(unsigned long agora) {
  switch (corAtual) {
    case 0: // Normal — verde fixo
      digitalWrite(LED_VERDE,    HIGH);
      digitalWrite(LED_AMARELO,  LOW);
      digitalWrite(LED_VERMELHO, LOW);
      break;
    case 1: // Atenção — amarelo fixo
      digitalWrite(LED_VERDE,    LOW);
      digitalWrite(LED_AMARELO,  HIGH);
      digitalWrite(LED_VERMELHO, LOW);
      break;
    case 2: // Crítico — vermelho piscante
    case 3: // Erro sensor — vermelho piscante
      digitalWrite(LED_VERDE,   LOW);
      digitalWrite(LED_AMARELO, LOW);
      if (agora - tPiscaVermelho >= 500) {
        static bool est = false;
        est = !est;
        digitalWrite(LED_VERMELHO, est);
        tPiscaVermelho = agora;
      }
      break;
  }
}

// ============================================================
//  BUZZER
// ============================================================
void atualizarBuzzer(unsigned long agora) {
  switch (corAtual) {
    case 0: // Normal — silêncio
    case 3: // Erro — silêncio (já tem LED piscando)
      digitalWrite(BUZZER, LOW);
      estadoBuzzer = false;
      break;

    case 1: // Atenção — bip curto a cada 2s
      if (!estadoBuzzer && (agora - tBuzzer >= 2000)) {
        digitalWrite(BUZZER, HIGH);
        estadoBuzzer = true;
        tBuzzer = agora;
      } else if (estadoBuzzer && (agora - tBuzzer >= 100)) {
        digitalWrite(BUZZER, LOW);
        estadoBuzzer = false;
      }
      break;

    case 2: // Crítico — bipe rápido contínuo
      if (agora - tBuzzer >= 300) {
        estadoBuzzer = !estadoBuzzer;
        digitalWrite(BUZZER, estadoBuzzer);
        tBuzzer = agora;
      }
      break;
  }
}
