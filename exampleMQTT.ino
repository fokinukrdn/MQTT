#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define ONE_WIRE_BUS 22
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire); //Подключаем датчик температуры DS18B20 на 22 цифровой порт
#define BBT "mqtt.beebotte.com" // Домен MQTT брокера
#define TOKEN "token_4g41ZmAfLxxxxxxx" // токен, который можно посмотреть в канале в личном кабинете
#define Channel "Arduino2" // Имя канала, который вы создали в личном кабинете
#define CoResource "temperature" //Название топика
#define Write true

long lastMsg = 0;
long lastReconnectAttempt = 0;
float TempValue; // Тут храним значение температуры
const int powerled=23; // Цыфровой пин для управление светодиодом

//Настройка для интернет соеденения(можно менять как нам угодно)
EthernetClient ethClient;
PubSubClient client(ethClient);
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 50);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

void setup() {
  Serial.begin(9600); 
  sensors.begin();//Начинаем работать с датчиком температуры
  pinMode(powerled, OUTPUT); //настраиваем пин светодиода
  //Начинаем подключение к MQTT брокеру
  client.setServer(BBT, 1883);
  while (!Serial) {
    ; //Необходимо для usb порта
  }
  //Начинаем интернет соединение
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Если не получилось настроить соединение через dhcp, пробуем через ip 
     Ethernet.begin(mac, ip, gateway, subnet);
  }
  // Даем время для инициализации Ethernet порта
  delay(1000);
  Serial.println("connecting...");
  lastReconnectAttempt = 0;
  client.setCallback(onMessage);//слушаем брокера для управления светодиодом
}

void onMessage(char* topic, byte* payload, unsigned int length) {
 //Beebotte отправляет и принимает сообщения через json
  StaticJsonBuffer<128> jsonInBuffer;
  JsonObject& root = jsonInBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
  // булевая переменная для управления светодиодом
  bool data = root["data"];

  // Зажигаем или тушим наш светодиод
  digitalWrite(powerled, data ? HIGH : LOW);

 //выводим информацию для отладки
  Serial.print("data ");
  Serial.print(data);
  Serial.println(); 
  
}

void readSensorData()
{
  //Читаем информацию с датчика температуры и делаем публикацию в топик, отвечающий за температуру
  sensors.requestTemperatures(); 
  TempValue = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.println(TempValue);
      if (!isnan(TempValue )) {
        publish(CoResource, TempValue, Write);
    }
}

// готовим json для отправки броккеру
void publish(const char* resource, float data, bool persist)
{
    StaticJsonBuffer<128> jsonOutBuffer;
    JsonObject& root = jsonOutBuffer.createObject();
    root["channel"] = Channel;
    root["resource"] = resource;
    if (persist) {
        root["write"] = true;
    }
    root["data"] = data;
    char buffer[128];
    root.printTo(buffer, sizeof(buffer));

   //Публикация в топик
    char topic[64];
    sprintf(topic, "%s/%s", Channel, resource);
    client.publish(topic, buffer);
}

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
char id[17];

const char * generateID()
{
  randomSeed(analogRead(0));
  int i = 0;
  for(i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) -1] = '\0';

  return id;
}

// рекконект к MQTT серверу
void reconnect() {
  // цикл пока не соединимся
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // попытка соединится
    if (client.connect(generateID(), TOKEN, "")) {
      Serial.println("connected");
      //Подписываемся на топик, отвечающий за светодиод
      client.subscribe("Arduino2/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
   readSensorData(); // отправляем значение температуры раз в минуту
  }
}
