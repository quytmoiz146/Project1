#include <RTClib.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#define LED 41
#define BUZZER 39
#define INPUT_PIN 33
#define RS485_EN 34

String ssid = "iPhone_Quyet";
String password = "14062005";

const char* mqtt_server = "1ec955b8f9784f91afb6532b88936962.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "quyet";
const char* mqtt_password = "Quyt1406";

RTC_DS1307 rtc; //Khai báo DS1307
Preferences prefsRTC; //Khai báo thư viện cho DS1307
WiFiClientSecure espClient; //Khai báo đối tượng mạng TCP
PubSubClient client(espClient); //Khai báo đối tượng MQTT
HardwareSerial RS485Serial(1);  //Khai báo RS485 sử dụng UART1

void initWiFi(){
  unsigned long t_start = millis();
  int retryCount = 0;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.print("Connecting to Wifi ");
  Serial.println(ssid);
  delay(500);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Status: ");
    Serial.println(WiFi.status());
    delay(500);
  }
  randomSeed(micros());
  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void changeWiFi(String newSSID, String newPASS){
  Serial.println("Thay đổi kết nối WiFi!");
  ssid = newSSID;
  password = newPASS;
  WiFi.disconnect(true);
  delay(1000);
  initWiFi();
}

void Led_Buzzer(){
  static unsigned long t_start = 0;
  static unsigned long t_current = 0;
  static unsigned long t_lastLed = 0;
  static unsigned long t_lastBuzzer = 0;
  static bool ledState = 1;
  static bool buzzerState = 0;
  static bool currentInputState = 0;
  static bool lastInputState = 0;
  static bool active = false;

  currentInputState = digitalRead(INPUT_PIN);
  if(currentInputState != lastInputState){
    active = true;
    t_start = millis();
  }
  lastInputState = currentInputState;

  if(active){
    t_current = millis();
    if(t_current - t_start >= 30000){
      active = false;
      digitalWrite(LED, HIGH);
      digitalWrite(BUZZER, LOW);
      return;
    }

    if(t_current - t_lastBuzzer >= 1000){
      t_lastBuzzer = t_current;
      buzzerState = !buzzerState;
      digitalWrite(BUZZER, buzzerState);
    }

    if(t_current - t_lastLed >= 500){
      t_lastLed = t_current;
      ledState = !ledState;
      digitalWrite(LED, ledState);
    }
  }
}

void Real_Time(){
  static unsigned long t_lastSend = 0;
  unsigned long nowMillis = millis();
  if(nowMillis - t_lastSend >= 1000){
    t_lastSend = nowMillis;
    DateTime now = rtc.now(); //Lấy thời gian từ RTC
    char data[30];
    sprintf(data, "%02d:%02d:%02d %02d/%02d/%04d", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
    digitalWrite(RS485_EN, HIGH); //Bật chế độ gửi
    RS485Serial.println(data);  //Gửi dữ liệu data đến RS485
    delay(10);
    digitalWrite(RS485_EN, LOW);  //Quay về chế độ nhận
    // Serial.println(data);
  }
}

void Save_Time_To_Flash(){
  static unsigned long t_lastSave = 0;
  unsigned long nowMillis = millis();
  if(nowMillis - t_lastSave >= 10000){
    t_lastSave = nowMillis;
    DateTime now = rtc.now(); //Lấy thời gian từ rtc
    char data[30];
    sprintf(data, "%02d:%02d:%02d %02d/%02d/%04d", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
    prefsRTC.putString("time", data);  //Lưu thời gian vào flash
  }
}

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Nhận dữ liệu từ topic: ");
  Serial.println(topic);

  String msg = "";
  for (int i = 0; i < length; i++){
    msg += (char)payload[i];
  }
  Serial.print("Nội dung: ");
  Serial.println(msg);

  if (msg == "ON"){
    digitalWrite(LED, LOW);
  } else if (msg == "OFF"){
    digitalWrite(LED, HIGH);
  }

  if (msg.startsWith("WIFI:")) {
    int index = msg.indexOf(',');

    if (index > 5) {
      String newSSID = msg.substring(5, index);
      String newPASS = msg.substring(index + 1);

      Serial.println("SSID mới: " + newSSID);
      Serial.println("PASS mới: " + newPASS);

      changeWiFi(newSSID, newPASS);
    }
  }
}

void reconnect(){
  while (!client.connected()){
    Serial.print("Đang kết nối MQTT . . . ");
    String clientID = "ESPClient-";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)){
      Serial.println("Connected!");
      client.subscribe("esp32/control");
    } else {
      Serial.print("falled, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  initWiFi(); //Kết nối WiFi

  pinMode(INPUT_PIN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED, HIGH);
  digitalWrite(BUZZER, LOW);

  Wire.begin(8, 9); //SDA, SCL

  //Khởi tạo RTC
  if(!rtc.begin()){
    Serial.println("Không tìm thấy DS1307");
  }

  //Nếu RTC chưa chạy thì lấy thời gian theo máy tính
  if(!rtc.isrunning()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);  //Chế độ nhận

  RS485Serial.begin(9600, SERIAL_8N1, 18, 17);  //RX=18, TX=17

  prefsRTC.begin("rtc_data", false);

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  Led_Buzzer();
  Real_Time();
  Save_Time_To_Flash();

  // Quét wifi xung quanh
  // int n = WiFi.scanNetworks();
  // if(n == 0){
  //   Serial.println("No Wifi found");
  // }
  // else {
  //   Serial.println("Found " + String(n) + " Wifi:");
  //   for(int i=0;i<n;i++){
  //     Serial.println(String(i+1) + ". " + WiFi.SSID(i) + "(" + WiFi.RSSI(i) + ")");
  //   }
  // }
  // delay(5000);

  // Serial.print("RSSI: ");
  // Serial.println(WiFi.RSSI());
  // delay(1000);

  if(!client.connected()){
    reconnect();
  }
  client.loop();

  static unsigned long t_lastmsg = 0;
  if (millis() - t_lastmsg >= 10000){
    t_lastmsg = millis();
    String time = prefsRTC.getString("time", "");
    client.publish("esp32/data", time.c_str());
    Serial.println("Đã gửi: " + time);
  }
}