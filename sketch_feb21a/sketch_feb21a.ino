// имя пароль вашей домашней сети
// можно ввести, подключившись к ESP AP c паролем 1234567890
#define WIFI "Vodafone-946c+ "
#define WIFIPASS "19191919"
#define INDIKATOR 8         // на каком пине индикаторный светодиод
 






#include <Arduino.h>
#include "timer.h"
#include "led.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <GyverDBFile.h>
#include <LittleFS.h>
GyverDBFile db(&LittleFS, "/nicelight.db"); // база данных для хранения настроек будет автоматически записываться в файл при изменениях

#include <SettingsGyver.h>
    // период обновлений, мс. 0 чтобы отключить !!!!!!!!!!!!!!!!!!!!
    uint16_t updateTout = 2500;
#include <WiFiConnector.h>
SettingsGyver sett("🪟", &db);// указывается заголовок меню, подключается база данных


#include <GyverNTP.h>

//обявление фкнций для их видимости из вкладок.
void build(sets::Builder& b);
void update(sets::Updater& upd);



// ключи для хранения в базе данных они же идентификаторы полей для веб морды
enum kk : size_t {
    wifi_ssid,
    wifi_pass,
    lbl1,      
    timeOpen,  
    timeClose, 
    motorRunTime,
    btnOpen,    
    btnClose,  
    apply,
    blinds_status,     // Добавлен статус жалюзи
    datetime_display // Добавлен новый ключ для отображения даты и времени
};

struct Data {
  uint32_t secondsNow = 0;
  String dateTimeStr = ""; 
  String blindsStatus = "Закрыты";
  bool isBlindsMoving = false;    // Флаг движения жалюзи
  bool isBlindsOpen = false;      // Флаг текущего состояния
};
Data data;

Timer each60Sec(60000); // таймер раз в минуту

LED indikator(INDIKATOR, 300, 3, 50, 20); //каждые 1000 милисек мигаем 3 раза каждых 50 мс, время горения 20 мсек

int valNum;
uint32_t startSeconds = 0;
uint32_t stopSeconds = 0;
bool notice_f; // флаг на отправку уведомления о подключении к wifi
byte initially = 10; // первых 10 секунд приписываем время в переменную
 


// ===== Переменные управления мотором =====
const int IN1 = 0;  // Пин мотора
const int IN2 = 1;  // Пин мотора
unsigned long motorTime = 5000; // Время работы мотора (по умолчанию)
unsigned long startTime = 0;
bool timerActive = false;

// ===== Пины физических кнопок =====
const int buttonOpenPin = 5;
const int buttonClosePin = 7;

// ===== Дребезг кнопок =====
bool openButtonState = false;
bool closeButtonState = false;
unsigned long lastDebounceTimeOpen = 0;
unsigned long lastDebounceTimeClose = 0;
const unsigned long debounceDelay = 50; // Задержка дребезга

// ===== Переменные времени =====
unsigned long timeToOpen = 0;
unsigned long timeToClose = 0;

// ===== Управление мотором =====
void handleOpenButton() {
  if (timerActive || data.isBlindsMoving || data.isBlindsOpen) return;
  
  data.isBlindsMoving = true;
  data.blindsStatus = "Открываются";
  
  Serial.println("Открытие");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  startTime = millis();
  timerActive = true;
}

void handleCloseButton() {
  if (timerActive || data.isBlindsMoving || !data.isBlindsOpen) return;
  
  data.isBlindsMoving = true;
  data.blindsStatus = "Закрываются";
  
  Serial.println("Закрытие");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  startTime = millis();
  timerActive = true;
}

void checkTimer() {
  if (!timerActive) return;

  if (millis() - startTime >= motorTime) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("Мотор остановлен");
    
    // Обновляем состояние после завершения движения
    data.isBlindsMoving = false;
    if (data.blindsStatus == "Открываются") {
      data.isBlindsOpen = true;
      data.blindsStatus = "Открыты";
    } else {
      data.isBlindsOpen = false;
      data.blindsStatus = "Закрыты";
    }
    
    timerActive = false;
  }
}





// ===== Управление кнопками =====
void handleButtons() {
  unsigned long currentMillis = millis();

  bool readingOpen = !digitalRead(buttonOpenPin);
  if (readingOpen != openButtonState) {
    if (currentMillis - lastDebounceTimeOpen > debounceDelay) {
      lastDebounceTimeOpen = currentMillis;
      openButtonState = readingOpen;
      if (openButtonState) handleOpenButton();
    }
  }

  bool readingClose = !digitalRead(buttonClosePin);
  if (readingClose != closeButtonState) {
    if (currentMillis - lastDebounceTimeClose > debounceDelay) {
      lastDebounceTimeClose = currentMillis;
      closeButtonState = readingClose;
      if (closeButtonState) handleCloseButton();
    }
  }
}













// билдер! Тут строится наше окно настроек



void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(buttonOpenPin, INPUT_PULLUP);
  pinMode(buttonClosePin, INPUT_PULLUP);

  // ======== SETTINGS ========
  WiFi.mode(WIFI_AP_STA);
  

  
  sett.begin();
  sett.onBuild(build);
  sett.onUpdate(update);




  // ======== DATABASE ========
 #ifdef ESP32
  LittleFS.begin(true);
 #else
  LittleFS.begin();
 #endif
  db.begin();
  db.init(kk::wifi_ssid, "Vodafone-946C_plus");
  db.init(kk::wifi_pass, "19191919");
  db.init(kk::datetime_display, "");
  db.init(kk::motorRunTime, 5000);
  db.init(kk::timeOpen, 8*3600);   // 08:00 по умолчанию
  db.init(kk::timeClose, 20*3600); // 20:00 по умолчанию
  db.init(kk::blinds_status, "Закрыты"); // Добавлена инициализация

  // Загрузка сохраненных значений
  motorTime = db[kk::motorRunTime].toInt();
  timeToOpen = db[kk::timeOpen].toInt();
  timeToClose = db[kk::timeClose].toInt();

  
  // ======== WIFI ========
  // подключение и реакция на подключение или ошибку
  WiFiConnector.setPass("1234567890"); // пароль точки доступа
  WiFiConnector.setTimeout(20); // сколько секунд пытаться приконнектиттся
  WiFiConnector.onConnect([]() {
    Serial.print("Connected! ");
    Serial.println(WiFi.localIP());
    indikator.setPeriod(3000, 1, 200, 150); //раз в 000 сек, 0 раз взмигнем - по 00 милисек периоды, гореть будем 0 милисек
  });
  WiFiConnector.onError([]() {
    Serial.print("Error! start AP ");
    Serial.println(WiFi.softAPIP());
    indikator.setPeriod(600, 2, 100, 50); //раз в  секунду два раза взмигнем - по 200 милисек, гореть будем 50 милисек

  });

  WiFiConnector.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);

  NTP.begin(1); // часовой пояс. Обновляться раз в 100 сек
  NTP.setPeriod(100); // обновлять раз в 100 сек


}//setup

void updateDateTime() {
  if (NTP.online()) {
    data.dateTimeStr = NTP.dateToString() + " " + NTP.timeToString();
  } else {
    data.dateTimeStr = "NTP disconnected";
  }
}




void loop() {
  WiFiConnector.tick();
  sett.tick();
  NTP.tick();
  indikator.tick();
  
  handleButtons();
  checkTimer();


  if (each60Sec.ready()) {
    updateDateTime();
  }

  if (NTP.newSecond()) {
    data.secondsNow = NTP.daySeconds();
    checkAutoActions();
  }
}

void checkAutoActions() {

  static uint32_t lastOpen = 0;
  static uint32_t lastClose = 0;
  
  // Для открытия
  if (data.secondsNow >= timeToOpen && 
      data.secondsNow - timeToOpen < 60 && 
      lastOpen != timeToOpen &&
      !data.isBlindsOpen &&      // проверяем что жалюзи закрыты
      !data.isBlindsMoving) {    // проверяем что не в движении
    
    handleOpenButton();
    lastOpen = timeToOpen;
  }
  
  // Для закрытия
  if (data.secondsNow >= timeToClose && 
      data.secondsNow - timeToClose < 60 && 
      lastClose != timeToClose &&
      data.isBlindsOpen &&       // проверяем что жалюзи открыты
      !data.isBlindsMoving) {    // проверяем что не в движении
    
    handleCloseButton();
    lastClose = timeToClose;
  }
}

  

