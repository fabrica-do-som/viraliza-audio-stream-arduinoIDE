#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h"
#include "secrets.h"

// Credenciais e URLs
static const char *WIFI_SSID = SECRET_WIFI_SSID;
static const char *WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
static const char *RADIO_URL = SECRET_RADIO_URL;

// Configurações de Áudio
static const uint8_t AUDIO_VOLUME = 17;  // 0 a 21

// Pinagem I2S (PCM5100)
static const uint8_t I2S_BCLK_PIN = 4;  // Bit Clock (BCK)
static const uint8_t I2S_WS_PIN = 6;    // Word Select / LCK
static const uint8_t I2S_DOUT_PIN = 5;  // Data Out (DIN)

static const uint8_t RESET_BUTTON_PIN = 0;
static const uint32_t DEBOUNCE_MS = 50;  // tempo mínimo estável pra considerar o clique válido

Audio audio;

volatile bool connect = false;

// Estado do debounce
int lastRawState = HIGH;      // último estado lido (cru, sem filtro)
int stableState = HIGH;       // estado já considerado "estável"
uint32_t lastChangeTime = 0;  // quando o estado cru mudou pela última vez

// Protótipos (necessário em .cpp, diferente do .ino)
void fadeOutAndStopAudio();
void connectToWiFi();

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("\n[WIFI] Conectado! IP: %s - RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      Serial.print("MAC STA:");
      Serial.println(WiFi.macAddress());
      connect = true;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI] Conexão perdida. Tentando reconectar automaticamente...");
      break;

    default:
      break;
  }
}

void fadeOutAndStopAudio() {
  // Reduz o volume gradualmente pra evitar pop no DAC
  for (int v = audio.getVolume(); v >= 0; v -= 2) {
    audio.setVolume(v);
    audio.loop();  // mantém o decoder respirando durante o fade
    delay(15);
  }

  audio.stopSong();  // encerra o stream/decoder de forma limpa
}

void checkResetButton() {
  int raw = digitalRead(RESET_BUTTON_PIN);  // pull-up: LOW = pressionado

  // Toda vez que o valor cru mudar, reinicia a contagem do debounce
  if (raw != lastRawState) {
    lastChangeTime = millis();
    lastRawState = raw;
  }

  // Só aceita o novo estado se ele ficou estável por DEBOUNCE_MS
  if ((millis() - lastChangeTime) > DEBOUNCE_MS) {
    if (raw != stableState) {
      stableState = raw;

      if (stableState == LOW) {
        Serial.println("[BOTAO] Clique detectado. Reiniciando o dispositivo...");

        fadeOutAndStopAudio();
        delay(200);
        ESP.restart();
      }
    }
  }
}

void connectToWiFi() {
  Serial.printf("[WIFI] Conectando em \"%s\"...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  setCpuFrequencyMhz(240);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Configura hardware de áudio
  audio.setPinout(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
  audio.setVolume(AUDIO_VOLUME);
  audio.setConnectionTimeout(3000, 5000);

  // Inicializa eventos e dispara o Wi-Fi
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  connectToWiFi();

  Serial.println("[MAIN] Aguardando conexão Wi-Fi...");
}

void loop() {
  checkResetButton();

  if (connect) {
    connect = false;
    Serial.printf("[AUDIO] Conectando ao stream: %s\n", RADIO_URL);
    audio.connecttohost(RADIO_URL);
  }

  audio.loop();
}

// Callbacks da biblioteca audio
void audio_info(const char *info) {
  Serial.printf("[INFO] %s\n", info);
}

void audio_showstation(const char *info) {
  Serial.printf("[ESTACAO] %s\n", info);
}

void audio_showstreamtitle(const char *info) {
  Serial.printf("[FAIXA] %s\n", info);
}

void audio_bitrate(const char *info) {
  Serial.printf("[BITRATE] %s bps\n", info);
}

void audio_eof_stream(const char *info) {
  Serial.printf("[FIM STREAM] %s. Reconectando...\n", info);
  connect = true;
}