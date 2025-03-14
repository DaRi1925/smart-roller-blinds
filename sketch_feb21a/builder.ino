void build(sets::Builder& b) {
  { 
    sets::Group g(b, "");
    b.Label(kk::datetime_display, "Дата и время", data.dateTimeStr.c_str());
    b.Label(kk::blinds_status, "Статус жалюзи", data.blindsStatus.c_str());

    if (b.beginButtons()) {
      if (b.Button(kk::btnOpen, "⬆Открыть", data.isBlindsOpen ? sets::Colors::Gray : sets::Colors::Default)) {
        if (!data.isBlindsOpen && !data.isBlindsMoving) handleOpenButton();
      }
      if (b.Button(kk::btnClose, "⬇Закрыть", !data.isBlindsOpen ? sets::Colors::Gray : sets::Colors::Red)) {
        if (data.isBlindsOpen && !data.isBlindsMoving) handleCloseButton();
      }
      b.endButtons();
    }
  }

  if (b.beginGroup("")) {
    if (b.beginMenu("Настройки")) {
      b.Time(kk::timeOpen, "Время открытия");
      b.Time(kk::timeClose, "Время закрытия");
      b.Number(kk::motorRunTime, "Время работы (мс)");            

      { 
        sets::Group g(b, "Настройки WiFi🛜");
        b.Input(kk::wifi_ssid, "SSID");
        b.Pass(kk::wifi_pass, "Пароль");
    
        if (b.Button(kk::apply, "Сохранить и перезагрузить🔁")) {
          db.update();
          ESP.restart();
        }
      }    
      b.endMenu();
    }
    b.endGroup();
  }
}

// Функция update должна быть ВНЕ функции build!
void update(sets::Updater& upd) {
    upd.update(kk::datetime_display, data.dateTimeStr.c_str());
    upd.update(kk::blinds_status, data.blindsStatus.c_str());

    // Обновление параметров из БД
    timeToOpen = db[kk::timeOpen].toInt();
    timeToClose = db[kk::timeClose].toInt();
    motorTime = db[kk::motorRunTime].toInt();

    // Валидация значения времени работы
    if (motorTime < 100) motorTime = 100;
    if (motorTime > 30000) motorTime = 30000;
    motorTime = motorTime / 100 * 100;
    
    db[kk::motorRunTime] = motorTime;
}