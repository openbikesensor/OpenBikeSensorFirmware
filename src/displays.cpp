#include "displays.h"

class SSD1306DisplayDevice;

uint8_t DisplayItemType::sNextOrdinal
  = 0;
std::map<String, DisplayItemType&> DisplayItemType::sTypeValues
  = std::map<String, DisplayItemType&>();


const DisplayItemType DisplayItemType::NONE = DisplayItemType("NONE");
const DisplayItemType DisplayItemType::TEXT_8PT = DisplayItemType("TEXT_8PT");
const DisplayItemType DisplayItemType::TEXT_10PT = DisplayItemType("TEXT_10PT");
const DisplayItemType DisplayItemType::TEXT_16PT = DisplayItemType("TEXT_16PT");
const DisplayItemType DisplayItemType::TEXT_20PT = DisplayItemType("TEXT_20PT");
const DisplayItemType DisplayItemType::TEXT_26PT = DisplayItemType("TEXT_26PT");
const DisplayItemType DisplayItemType::TEXT_30PT = DisplayItemType("TEXT_30PT");
const DisplayItemType DisplayItemType::PROGRESS_BAR = DisplayItemType("PROGRESS_BAR");
const DisplayItemType DisplayItemType::WAIT_BAR = DisplayItemType("WAIT_BAR");
const DisplayItemType DisplayItemType::BATTERY_SYMBOLS = DisplayItemType("BATTERY_SYMBOLS");
const DisplayItemType DisplayItemType::TEMPERATURE_SYMBOLS = DisplayItemType("TEMPERATURE_SYMBOLS");

const DisplayItemType& DisplayItemType::fromString(const String& str) {
  return sTypeValues.at(str);
}


void SSD1306DisplayDevice::handle() {
  auto now = millis();
  if (now - mLastHandle < MIN_MILLIS_BETWEEN_DISPLAY_UPDATE) {
    return;
  }
  mLastHandle = now;

  m_display->clear();
  for (DisplayItem &item : mCurrentLayout.items) {
    prepareItem(item);
  }
  m_display->display();

}

void SSD1306DisplayDevice::prepareItem(DisplayItem &displayItem) {
  switch ((int) displayItem.style & 3) {
    case 1:
      m_display->setTextAlignment(TEXT_ALIGN_RIGHT);
      break;
    case 2:
      m_display->setTextAlignment(TEXT_ALIGN_CENTER);
      break;
    case 4:
      m_display->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      break;
    default:
      m_display->setTextAlignment(TEXT_ALIGN_LEFT);
      break;
  }
  m_display->setTextAlignment(
    ((int) displayItem.style) % 2 == 0 ?
    OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT : OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_RIGHT);

  // TODO: DisplayItemType should know how to draw itself.
  if (displayItem.type == &DisplayItemType::TEXT_8PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), Dialog_plain_8);
  } else if (displayItem.type == &DisplayItemType::TEXT_10PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), ArialMT_Plain_10);
  } else if (displayItem.type == &DisplayItemType::TEXT_16PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), ArialMT_Plain_16);
  } else if (displayItem.type == &DisplayItemType::TEXT_20PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), Dialog_plain_20);
  } else if (displayItem.type == &DisplayItemType::TEXT_26PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), Dialog_plain_26);
  } else if (displayItem.type == &DisplayItemType::TEXT_30PT) {
    drawString(displayItem.posX, displayItem.posY, getStringValue(displayItem), Dialog_plain_30);
  } else if (displayItem.type == &DisplayItemType::BATTERY_SYMBOLS) {
    drawBatterySymbols(displayItem.posX, displayItem.posY, displayItem.contentGetter->getValueAsUint32());
  } else {
    log_d("Unexpected display item type %d", (int) displayItem.type);
  }
}

void SSD1306DisplayDevice::drawString(int16_t x, int16_t y, const String& text, const uint8_t *font = DEFAULT_FONT) {
  m_display->setFont(font);
  m_display->drawString(x, y, text);
}


void SSD1306DisplayDevice::showNumConfirmed() {
  String val = String(confirmedMeasurements);
  if (confirmedMeasurements <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(2, 4, val, Dialog_plain_20);
  this->prepareTextOnGrid(3, 5, "conf",DEFAULT_FONT);
}

void SSD1306DisplayDevice::showNumButtonPressed() {
  String val = String(numButtonReleased);
  if (numButtonReleased <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(0, 4, val, Dialog_plain_20);
  this->prepareTextOnGrid(1, 5, "press",DEFAULT_FONT);
}

void SSD1306DisplayDevice::showValues(
  HCSR04SensorInfo sensor1, HCSR04SensorInfo sensor2, uint16_t minDistanceToConfirm,  int16_t BatterieVolt,
  int16_t TemperaturValue, int lastMeasurements, boolean insidePrivacyArea) {
  // Show sensor1, when DisplaySimple or DisplayLeft is configured
  if (config.displayConfig & DisplaySimple || config.displayConfig & DisplayLeft) {
    uint16_t value1 = sensor1.minDistance;
    if (minDistanceToConfirm != MAX_SENSOR_VALUE) {
      value1 = minDistanceToConfirm;
    }

    String loc1 = sensor1.sensorLocation;
    if (insidePrivacyArea) {
      loc1 = "(" + loc1 + ")";
    }

    // Do not show location, when DisplaySimple is configured
    if (!(config.displayConfig & DisplaySimple)) {
      this->prepareTextOnGrid(0, 0, loc1,DEFAULT_FONT);
    }

    if (value1 == MAX_SENSOR_VALUE) {
      this->prepareTextOnGrid(0, 1, "---", Dialog_plain_26);
    } else {
      String val = String(value1);
      if (value1 <= 9) {
        val = "00" + val;
      } else if (value1 >= 10 && value1 <= 99) {
        val = "0" + val;
      }

      this->prepareTextOnGrid(0, 1, val, Dialog_plain_26);
    }

    if (config.displayConfig & DisplaySimple) {
      this->prepareTextOnGrid(2, 2, "cm", Dialog_plain_20,5,0);
    }
  }

  if (!(config.displayConfig & DisplaySimple)) {

    // Show sensor2, when DisplayRight is configured
    if (config.displayConfig & DisplayRight) {
      uint16_t value2 = sensor2.distance;
      String loc2 = sensor2.sensorLocation;

      this->prepareTextOnGrid(3, 0, loc2,DEFAULT_FONT);
      if (value2 == MAX_SENSOR_VALUE) {
        this->prepareTextOnGrid(2, 1, "---", Dialog_plain_26,5,0);
      } else {
        String val = String(value2);
        if (value2 <= 9) {
          val = "00" + val;
        } else if (value2 >= 10 && value2 <= 99) {
          val = "0" + val;
        }
        this->prepareTextOnGrid(2, 1, val, Dialog_plain_26,5,0);
      }
    }
    if (config.displayConfig & DisplayDistanceDetail) {
      const int bufSize = 64;
      char buffer[bufSize];
      snprintf(buffer, bufSize - 1, "%03d|%02d|%03d", sensor1.rawDistance,
        lastMeasurements, sensor2.rawDistance);
//      snprintf(buffer, bufSize - 1, "%03d|%02d|%uk", sensor1.rawDistance,
//               lastMeasurements, ESP.getFreeHeap() / 1024);
//      snprintf(buffer, bufSize - 1, "%03d|%02d|%3.2fV", sensor1.rawDistance,
//               lastMeasurements, voltageMeter->read());

      this->prepareTextOnGrid(0, 4, buffer, Dialog_plain_20);
    } else if (config.displayConfig & DisplayNumConfirmed) {
      showNumButtonPressed();
      showNumConfirmed();
    } else {
      // Show GPS info, when DisplaySatellites is configured
      if (config.displayConfig & DisplaySatellites) {
        showGPS();
      }

      // Show velocity, when DisplayVelocity is configured
      if (config.displayConfig & DisplayVelocity) {
        if (gps.speed.age() < 2000) {
          showVelocity(gps.speed.kmph());
        } else {
          showVelocity(-1);
        }
      }


    }
  }
  // Show Batterie voltage
  #warning not checked if colliding with other stuff

    if(BatterieVolt >=-1)
        showBatterieValue((BatterieVolt));

  if (!(config.displayConfig & DisplaySimple)){
    if(BMP280_active == true)
      showTemperatureValue(TemperaturValue);
  }

  m_display->display();

}

void SSD1306DisplayDevice::showGPS() {
  int sats = gps.satellites.value();
  String val = String(sats);
  if (sats <= 9) {
    val = "0" + val;
  }
  this->prepareTextOnGrid(2, 4, val, Dialog_plain_20);
  this->prepareTextOnGrid(3, 5, "sats",DEFAULT_FONT);
}

void SSD1306DisplayDevice::showBatterieValue(int16_t input_val){

    uint8_t x_offset_batterie_logo = 65;
    uint8_t y_offset_batterie_logo = 2;
    int8_t xlocation = 2;

     if ((config.displayConfig & DisplaySimple)){
       x_offset_batterie_logo += 32;
       xlocation += 1;
     }

    //cleanGridCellcomplete(3,0);

/*     if(input_val == -1){
      cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
      m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo6);
      m_display->setColor(BLACK);
      this->showTextOnGrid(3, 0, " " + String(0) + "%", Dialog_plain_8,3,0);
      m_display->setColor(WHITE);
      m_display->display();
      this->showTextOnGrid(3, 0, "calc", Dialog_plain_8,6,0);
    }else{
      m_display->setColor(BLACK);
      this->showTextOnGrid(3, 0, "calc", Dialog_plain_8,6,0);
      m_display->setColor(WHITE);
      m_display->display();
    } */
		if(input_val >= 0){
			String val = String(input_val);
      //showLogo(true);
			this->showTextOnGrid(xlocation, 0, " " + val + "%", Dialog_plain_8,3,0);
       //m_display[0]->drawXbm(192, 0, 8, 9, BatterieLogo1);

       if(input_val > 90){
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo1);
       }else if (input_val > 70)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo2);
       }else if (input_val> 50)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo3);
       }else if (input_val > 30)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo4);
       }else if (input_val >10)
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo5);
       }else
       {
         cleanBatterie(x_offset_batterie_logo, y_offset_batterie_logo);
         m_display->drawXbm(x_offset_batterie_logo, y_offset_batterie_logo, 8, 9, BatterieLogo6);
       }

		}
    //m_display->display();
	}

void SSD1306DisplayDevice::showTemperatureValue(int16_t input_val){
    uint8_t x_offset_temp_logo = 30;
    uint8_t y_offset_temp_logo = 2;
    cleanTemperatur(x_offset_temp_logo,y_offset_temp_logo);
    m_display->drawXbm(x_offset_temp_logo, y_offset_temp_logo, 8, 9, TempLogo);
    String val = String(input_val);
    this->showTextOnGrid(1, 0, " " + val + "°C", Dialog_plain_8,-3,0);
}

void SSD1306DisplayDevice::showVelocity(double velocity) {
  const int bufSize = 4;
  char buffer[bufSize];
  if (velocity >= 0) {
    snprintf(buffer, bufSize - 1, "%02d", (int) velocity);
  } else {
    snprintf(buffer, bufSize - 1, "--");
  }
  this->prepareTextOnGrid(0, 4, buffer, Dialog_plain_20);
  this->prepareTextOnGrid(1, 5, "km/h",DEFAULT_FONT);
}

void SSD1306DisplayDevice::addDisplayableString(const String& name, std::function<String()> getValue) {
  auto *getter = new DisplayStringContentGetter(getValue);
  mDisplayableValues.insert({name, getter});
}

void SSD1306DisplayDevice::addDisplayableInt(const String& name, std::function<int32_t()> getValue) {
  auto *getter = new DisplayIntegerContentGetter(getValue);
  mDisplayableValues.insert({name, getter});
}

void SSD1306DisplayDevice::addDisplayableDouble(const String& name, std::function<double()> getValue, int defaultDigits) {
  auto *getter = new DisplayDoubleContentGetter(getValue, defaultDigits);
  mDisplayableValues.insert({name, getter});
}

void SSD1306DisplayDevice::addDisplayableString(const String& name, String label) {
  auto *getter = new DisplayStringContentGetter(label);
  mDisplayableValues.insert({name, getter});
}

String SSD1306DisplayDevice::getStringValue(DisplayContentGetter getter, DisplayItemTextStyle style = DisplayItemTextStyle::LEFT_ALIGNED) {
  return apply(getter.getValueAsString(), style);
}

String SSD1306DisplayDevice::getStringValue(DisplayItem& item) {
  return apply(item.contentGetter->getValueAsString(), item.style);
}

int32_t SSD1306DisplayDevice::getInt32Value(DisplayItem& item) {
  return item.contentGetter->getValueAsUint32();
}

double SSD1306DisplayDevice::getDoubleValue(DisplayItem& item) {
  return item.contentGetter->getValueAsDouble();
}

String SSD1306DisplayDevice::apply(String data, DisplayItemTextStyle style) {
  switch(style) {
    case DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED:
    case DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED_RIGHT_ALIGNED:
      data = data.substring(0, 3);
      while(data.length() < 3) {
        data = "0" + data;
      }
      return data;
    case DisplayItemTextStyle::NUMBER_2_DIGITS_0_FILLED:
    case DisplayItemTextStyle::NUMBER_2_DIGITS_0_FILLED_RIGHT_ALIGNED:
      data = data.substring(0, 2);
      while(data.length() < 2) {
        data = "0" + data;
      }
      return data;
    default:
      return data;
  }
}

void SSD1306DisplayDevice::drawBatterySymbols(uint8_t x, uint8_t y, int32_t value) {
  if (value > 90) {
    m_display->drawXbm(x, y, 8, 9, BatterieLogo1);
  } else if (value > 70) {
    m_display->drawXbm(x, y, 8, 9, BatterieLogo2);
  } else if (value > 50) {
    m_display->drawXbm(x, y, 8, 9, BatterieLogo3);
  } else if (value > 30) {
    m_display->drawXbm(x, y, 8, 9, BatterieLogo4);
  } else if (value > 10) {
    m_display->drawXbm(x, y, 8, 9, BatterieLogo5);
  } else if (value >= 0){
    m_display->drawXbm(x, y, 8, 9, BatterieLogo6);
  } else {
    // no image!
  }
}

void SSD1306DisplayDevice::selectComplexLayout() {
/*  mCurrentLayout.items.clear();

  mCurrentLayout.items.emplace_back(0, 0, mDisplayableValues.find("sensor.left.label"));



  mCurrentLayout.items.emplace_back(0, 10, mDisplayableValues.find("sensor.left.confirmDistance"),
                                    DisplayItemType::TEXT_30PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED);
  mCurrentLayout.items.emplace_back(128, 0, DisplayContent::DISTANCE_RIGHT_TOF_SENSOR_LABEL,
                                    DisplayItemType::TEXT_8PT, DisplayItemTextStyle::RIGHT_ALIGNED);
  mCurrentLayout.items.emplace_back(128, 10, DisplayContent::DISTANCE_RIGHT_TOF_SENSOR,
                                    DisplayItemType::TEXT_30PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED_RIGHT_ALIGNED);

  mCurrentLayout.items.emplace_back(0, 40, DisplayContent::RAW_DISTANCE_TOF_LEFT,
                                    DisplayItemType::TEXT_20PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED);
  mCurrentLayout.items.emplace_back(128, 40, DisplayContent::RAW_DISTANCE_TOF_RIGHT,
                                    DisplayItemType::TEXT_20PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED_RIGHT_ALIGNED);
  mCurrentLayout.items.emplace_back(52, 40, DisplayContent::MEASUREMENT_LOOPS_PER_INTERVAL,
                                    DisplayItemType::TEXT_20PT,
                                    DisplayItemTextStyle::NUMBER_2_DIGITS_0_FILLED);
  mCurrentLayout.items.emplace_back(40, 40, DisplayContent::V_BAR_LABEL, DisplayItemType::TEXT_20PT);
  mCurrentLayout.items.emplace_back(80, 40, DisplayContent::V_BAR_LABEL, DisplayItemType::TEXT_20PT);

  mCurrentLayout.items.emplace_back(20, 0, DisplayContent::BATTERY_VOLTAGE);
  mCurrentLayout.items.emplace_back(40, 0, DisplayContent::BATTERY_VOLTAGE_LABEL);

  mCurrentLayout.items.emplace_back(58, 0, DisplayContent::BATTERY_PERCENTAGE, DisplayItemType::TEXT_8PT,
                                    DisplayItemTextStyle::RIGHT_ALIGNED);
  mCurrentLayout.items.emplace_back(58, 0, DisplayContent::BATTERY_PERCENTAGE_LABEL);
  mCurrentLayout.items.emplace_back(68, 0, DisplayContent::BATTERY_PERCENTAGE,
                                    DisplayItemType::BATTERY_SYMBOLS);

  mCurrentLayout.items.emplace_back(92, 0, DisplayContent::FREE_HEAP_KB, DisplayItemType::TEXT_8PT,
                                    DisplayItemTextStyle::RIGHT_ALIGNED);
  mCurrentLayout.items.emplace_back(92, 0, DisplayContent::FREE_HEAP_KB_LABEL); */
}

void SSD1306DisplayDevice::selectSimpleLayout() {
  mCurrentLayout.items.clear();

  mCurrentLayout.items.push_back(DisplayItem(0, 0, mDisplayableValues["sensor.left.label"]));
  mCurrentLayout.items.push_back(DisplayItem(8, 10, mDisplayableValues["sensor.left.confirmDistance"],
                                    DisplayItemType::TEXT_30PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED));
  mCurrentLayout.items.push_back(DisplayItem(68, 18, mDisplayableValues["sensor.unit"], DisplayItemType::TEXT_20PT));


/*

  mCurrentLayout.items.emplace_back(0, 0, mDisplayableValues["sensor.left.label"]);
  mCurrentLayout.items.emplace_back(8, 10, mDisplayableValues["sensor.left.confirmDistance"],
                                    DisplayItemType::TEXT_30PT,
                                    DisplayItemTextStyle::NUMBER_3_DIGITS_0_FILLED);
  mCurrentLayout.items.emplace_back(68, 18, mDisplayableValues["sensor.unit"], DisplayItemType::TEXT_20PT);
  */
}

DisplayItem::DisplayItem(const std::map<String, DisplayContentGetter *> &displayContentGetter, String &item) {
  if (item.isEmpty() || !item.endsWith("}") || !item.startsWith("{")) {
    log_e("Invalid display item format %s", item.c_str());
    return;
  }
  size_t pos = item.indexOf(":");
  String name = item.substring(1, pos);
  contentGetter = displayContentGetter.at(name);

  size_t end = item.indexOf(":", pos);
  String sPosX = item.substring(pos, end);
  if (sPosX.startsWith("C")) {
    posX = 32 * atoi(item.substring(pos + 1, end).c_str());
  } else {
    posX = atoi(item.substring(pos, end).c_str());
  }
  pos = end;
  end = item.indexOf(":", pos);
  if (end == -1) {
    end = item.indexOf("}", pos);
  }
  String sPosY = item.substring(pos, end);
  if (sPosY.startsWith("R")) {
    posY = 10 * atoi(item.substring(pos + 1, end).c_str());
  } else {
    posY = atoi(item.substring(pos, end).c_str());
  }
  pos = end;
  end = item.indexOf(":", pos);
  if (end == -1) {
    end = item.indexOf("}", pos);
  }
  if (end != -1) {
    type = &DisplayItemType::fromString(item.substring(pos, end));
  } else {
    type = &DisplayItemType::TEXT_8PT;
  }
  pos = end;
  end = item.indexOf("}", pos);
  if (end != -1) {
    // TODO Style from string
    log_e("TODO: Parse style from %s", item.substring(pos, end).c_str());
  }
  style = DisplayItemTextStyle::RIGHT_ALIGNED;
}

