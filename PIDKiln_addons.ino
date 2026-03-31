/*
** PIDKiln - Addons (Relays + Temperature sensors)
** Software-SPI für enjoyneering MAX31855 + ESP32-WROOM
*/

#include <MAX31855.h>

// ==================== PIN DEFINITIONEN ====================
#define THERMO_SCK   18     // Software-SPI Clock
#define THERMO_MISO  19     // Software-SPI MISO
#define THERMO_CS1   27     // CS für Thermocouple A

#ifdef MAXCS2
  #define THERMO_CS2   32
#endif

// MAX31855 Sensoren
MAX31855 ThermocoupleA(THERMO_CS1);
#ifdef MAXCS2
  MAX31855 ThermocoupleB(THERMO_CS2);
#endif

// ==================== ENERGY MONITOR ====================
#ifdef ENERGY_MON_PIN
#include <EmonLib.h>
#define ENERGY_MON_AMPS 30        
#define EMERGY_MON_VOLTAGE 230    
#define ENERGY_IGNORE_VALUE 0.4   

EnergyMonitor emon1;
#endif

uint16_t Energy_Wattage = 0;
double   Energy_Usage   = 0;

boolean SSR_On = false;

// ==============================================
// Relay Funktionen
// ==============================================
void Enable_SSR(){
  if(!SSR_On){
    digitalWrite(SSR1_RELAY_PIN, HIGH);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, HIGH);
#endif
    SSR_On = true;
  }
}

void Disable_SSR(){
  if(SSR_On){
    digitalWrite(SSR1_RELAY_PIN, LOW);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, LOW);
#endif
    SSR_On = false;
  }
}

void Enable_EMR(){ digitalWrite(EMR_RELAY_PIN, HIGH); }
void Disable_EMR(){ digitalWrite(EMR_RELAY_PIN, LOW); }

// ==============================================
// Energie-Monitor
// ==============================================
void Read_Energy_INPUT(){
#ifdef ENERGY_MON_PIN
  double Irms = emon1.calcIrms(148);
  static uint8_t cnt = 0;
  static uint32_t last = 0;

  if(Irms < ENERGY_IGNORE_VALUE){
    Energy_Wattage = 0;
    return;
  }

  Energy_Wattage = (uint16_t)(Energy_Wattage + Irms * EMERGY_MON_VOLTAGE) / 2;

  if(last){
    uint32_t ttime = millis() - last;
    Energy_Usage += (double)(Energy_Wattage * ttime) / 3600000.0;
  }
  last = millis();

  if(cnt++ > 20){
    DBG dbgLog(LOG_DEBUG,"[ADDONS] VCC:%d ; RAW Power: %.1fW, Raw current: %.2fA, Power:%d W/h:%.6f\n",
               emon1.readVcc(), Irms*EMERGY_MON_VOLTAGE, Irms, Energy_Wattage, Energy_Usage);
    cnt = 0;
  }
#endif
}

void Power_Loop(void * parameter){
  for(;;){
#ifdef ENERGY_MON_PIN
    Read_Energy_INPUT();
#endif
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// ==============================================
// Software-SPI Funktion
// ==============================================
uint32_t readMAX31855_SOFT(uint8_t csPin){
  uint32_t value = 0;

  digitalWrite(csPin, LOW);
  delayMicroseconds(5);

  for(int i = 0; i < 32; i++){
    digitalWrite(THERMO_SCK, HIGH);
    delayMicroseconds(2);

    value <<= 1;
    if(digitalRead(THERMO_MISO)) value |= 1;

    digitalWrite(THERMO_SCK, LOW);
    delayMicroseconds(2);
  }

  digitalWrite(csPin, HIGH);
  return value;
}

// ==============================================
// Temperatur Auslesung
// ==============================================
void Update_TemperatureA() {
  uint32_t raw = readMAX31855_SOFT(THERMO_CS1);

  if(raw == 0 || raw == 0xFFFFFFFF){
    DBG dbgLog(LOG_ERR, "[ADDONS] MAX31855 A no valid data (SPI issue)\n");
    return;
  }

  uint8_t status = ThermocoupleA.detectThermocouple(raw);
  if(status != MAX31855_THERMOCOUPLE_OK){
    switch(status){
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC: DBG dbgLog(LOG_ERR, "[ADDONS] TC A short to VCC\n"); break;
      case MAX31855_THERMOCOUPLE_SHORT_TO_GND: DBG dbgLog(LOG_ERR, "[ADDONS] TC A short to GND\n"); break;
      case MAX31855_THERMOCOUPLE_NOT_CONNECTED: DBG dbgLog(LOG_ERR, "[ADDONS] TC A not connected\n"); break;
      default: DBG dbgLog(LOG_ERR, "[ADDONS] TC A unknown error\n"); break;
    }

    if(TempA_errors < Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
      TempA_errors++;
      return;
    } else {
      ABORT_Program(PR_ERR_MAX31A_INT_ERR);
      return;
    }
  }

  double tmp = ThermocoupleA.getColdJunctionTemperature(raw);
  int_temp = int_temp * 0.8 + tmp * 0.2;

  tmp = ThermocoupleA.getTemperature(raw);
  kiln_temp = kiln_temp * 0.9 + tmp * 0.1;

  if(TempA_errors > 0) TempA_errors--;

  DBG dbgLog(LOG_DEBUG, "[ADDONS] Temp A → Internal=%.1f°C  Kiln=%.1f°C\n", int_temp, kiln_temp);

  delay(10); // Sensor-Pause
}

#ifdef MAXCS2
void Update_TemperatureB() {
  uint32_t raw = readMAX31855_SOFT(THERMO_CS2);

  if(raw == 0 || raw == 0xFFFFFFFF){
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 B no valid data (SPI issue)\n");
    return;
  }

  uint8_t status = ThermocoupleB.detectThermocouple(raw);
  if(status != MAX31855_THERMOCOUPLE_OK){
    if(TempB_errors < Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
      TempB_errors++;
    } else {
      ABORT_Program(PR_ERR_MAX31B_INT_ERR);
    }
    return;
  }

  double tmp = ThermocoupleB.getColdJunctionTemperature(raw);
  int_temp = int_temp * 0.8 + tmp * 0.2;

  tmp = ThermocoupleB.getTemperature(raw);
  case_temp = case_temp * 0.8 + tmp * 0.2;

  if(TempB_errors > 0) TempB_errors--;

  DBG dbgLog(LOG_DEBUG,"[ADDONS] Temp B → Internal=%.1f°C  Case=%.1f°C\n", int_temp, case_temp);
}
#endif

// ==============================================
// Alarm Funktionen
// ==============================================
void STOP_Alarm(){
  ALARM_countdown = 0;
  digitalWrite(ALARM_PIN, LOW);
}

void START_Alarm(){
  if(!Prefs[PRF_ALARM_TIMEOUT].value.uint16) return;
  ALARM_countdown = Prefs[PRF_ALARM_TIMEOUT].value.uint16;
  digitalWrite(ALARM_PIN, HIGH);
}

// ==============================================
// Setup_Addons
// ==============================================
void Setup_Addons() {
  // Relais-Pins
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR1_RELAY_PIN, OUTPUT);
#ifdef SSR2_RELAY_PIN
  pinMode(SSR2_RELAY_PIN, OUTPUT);
#endif
  pinMode(ALARM_PIN, OUTPUT);
  SSR_On = false;

  // Software-SPI Pins
  pinMode(THERMO_SCK, OUTPUT);
  pinMode(THERMO_CS1, OUTPUT);
  pinMode(THERMO_MISO, INPUT_PULLUP);

  digitalWrite(THERMO_CS1, HIGH);
  digitalWrite(THERMO_SCK, LOW);

#ifdef MAXCS2
  pinMode(THERMO_CS2, OUTPUT);
  digitalWrite(THERMO_CS2, HIGH);
#endif

  // Energie-Monitor
#ifdef ENERGY_MON_PIN
  emon1.current(ENERGY_MON_PIN, ENERGY_MON_AMPS);
  xTaskCreatePinnedToCore(Power_Loop, "Power_metter", 8192, NULL, 1, NULL, 0);
#endif

  Serial.println("[ADDONS] MAX31855 initialisiert (Software-SPI: SCK=18, MISO=19, CS1=27)");
}
