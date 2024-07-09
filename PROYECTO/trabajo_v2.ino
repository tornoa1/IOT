#include <WiFi.h>
#include "SPIFFS.h"
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

//Configuración WiFi
const char* ssid = "iPhone de Francisco";
const char* password = "yasuo123";

//Configuración AWS
const char* mqtt_server = "a2qfuun8g1atyc-ats.iot.us-east-1.amazonaws.com";
const int mqtt_port = 8883;

String Read_rootca;
String Read_cert;
String Read_privatekey;

//Configuración Telegram
const char* botToken = "7192412407:AAH7i9uSYpus3jfMlOSNv--_aICrc-OZ9sM";
const char* chat_id = "1854855888";
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);

// Configuración ThingSpeak
const char* serverName = "http://api.thingspeak.com/update";
String apiKey = "N9IKVYJTG1DLB80Z";

const int trigPin = 12; //trigger para el sensor
const int echoPin = 13; //echo para el sensor
const int buzzerPin = 14; //pin para el buzzzzzzzillaaaaaaaa

bool sistemaActivado = false;
bool alarmaActivada = false;
unsigned long lastTime = 0;
unsigned long timerDelay = 10000; // 10 segundos
long lastMsg = 0;
char msg[256];
byte mac[6];
char mac_Id[18];
int count = 1;

// Cliente MQTT
WiFiClientSecure net;
PubSubClient clientAWS(net);

// Conectar a red Wifi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

//callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// Conectar a broker MQTT
void reconnect() {
  while (!clientAWS.connected()) {
    Serial.print("Conectando a MQTT...");
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    if (clientAWS.connect(clientId.c_str())) {
      Serial.println("conectado");
      clientAWS.publish("ei_out", "hello world");
      clientAWS.subscribe("ei_in");
    } else {
      Serial.print("falló, rc=");
      Serial.print(clientAWS.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  setup_wifi();

  // Inicializar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Se ha producido un error al montar SPIFFS");
    return;
  }
  //**********************
  //Root CA leer archivo.
  File file2 = SPIFFS.open("/AmazonRootCA1.pem", "r");
  if (!file2) {
    Serial.println("No se pudo abrir el archivo para leerlo");
    return;
  }
  Serial.println("Root CA File Content:");
  while (file2.available()) {
    Read_rootca = file2.readString();
    Serial.println(Read_rootca);
  }
  //*****************************
  // Cert leer archivo
  File file4 = SPIFFS.open("/b408278b8def.pem.crt", "r");//modificar
  if (!file4) {
    Serial.println("No se pudo abrir el archivo para leerlo");
    return;
  }
  Serial.println("Cert File Content:");
  while (file4.available()) {
    Read_cert = file4.readString();
    Serial.println(Read_cert);
  }
  //***************************************
  //Privatekey leer archivo
  File file6 = SPIFFS.open("/b408278b8def.pem.key", "r");
  if (!file6) {
    Serial.println("No se pudo abrir el archivo para leerlo");
    return;
  }
  Serial.println("privateKey contenido:");
  while (file6.available()) {
    Read_privatekey = file6.readString();
    Serial.println(Read_privatekey);
  }

  net.setCACert(Read_rootca.c_str());
  net.setCertificate(Read_cert.c_str());
  net.setPrivateKey(Read_privatekey.c_str());

  clientAWS.setServer(mqtt_server, mqtt_port);
  clientAWS.setCallback(callback);

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

void loop() {
  if (!clientAWS.connected()) {
    reconnect();
  }
  clientAWS.loop();

  // Medir la distancia con el sensor ultrasónico
  long duration;
  float distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2; // Distancia en cm

  Serial.print("Distancia: ");
  Serial.println(distance);

  if (distance < 350 && sistemaActivado) { //su max es 450cm
    Serial.println("Movimiento Detectado");
    enviarDatosAWS(distance);
    enviarAlertaTelegram(distance);
    activarBuzzer(distance);
    if ((millis() - lastTime) > timerDelay) {
      registrarThingSpeak(distance);
      lastTime = millis();
    }
  }

  verificarComandosTelegram();
  delay(1000);
}

void enviarDatosAWS(float distancia) {
  String valor = String(distancia);
  snprintf(msg, 256, "{\"Distancia\" : \"%s\"}", valor.c_str());
  Serial.print("Publicando mensaje: ");
  Serial.println(msg);
  clientAWS.publish("proyecto", msg);
}

void enviarAlertaTelegram(float distancia) {
  String mensaje = "Alerta: Movimiento detectado a " + String(distancia) + " cm.";
  bot.sendMessage(chat_id, mensaje, "");
}

void activarBuzzer(float distancia) {
  float distanciaMinima = 0;
  float distanciaMaxima = 350;

  // Calcular la frecuencia y el intervalo en función de la distancia
  int frecuencia = map(distancia, distanciaMaxima, distanciaMinima, 500, 2000);
  int intervalo = map(distancia, distanciaMaxima, distanciaMinima, 1000, 100);

  tone(buzzerPin, frecuencia); // Emitir tono
  delay(intervalo); // Esperar
  noTone(buzzerPin); // Apagar tono
  delay(intervalo); // Esperar
}

void registrarThingSpeak(float distancia) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String httpRequestData = "api_key=" + apiKey + "&field1=" + String(distancia);          
    int httpResponseCode = http.POST(httpRequestData);
    Serial.print("Codigo de Respuesta HTTP: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi Desconectado");
  }
}

void verificarComandosTelegram() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      if (text == "/activar") {
        sistemaActivado = true;
        bot.sendMessage(chat_id, "Sistema activado", "");
      } else if (text == "/desactivar") {
        sistemaActivado = false;
        bot.sendMessage(chat_id, "Sistema desactivado", "");
      } else if (text == "/silenciar") {
        alarmaActivada = false;
        noTone(buzzerPin);
        bot.sendMessage(chat_id, "Alarma silenciada", "");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}
