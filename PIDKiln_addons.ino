/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 ThermocoupleA(MAXCS1);

// If we have defines power meter pins
#ifdef ENERGY_MON_PIN
#include <EmonLib.h>
#define ENERGY_MON_AMPS 30        // how many amps produces 1V on your meter (usualy with voltage output meters it's their max value).
#define EMERGY_MON_VOLTAGE 230    // what is your mains voltage
#define ENERGY_IGNORE_VALUE 0.4   // if measured current is below this - ignore it (it's just noise)
EnergyMonitor emon1;
#endif
uint16_t Energy_Wattage=0;        // keeping present power consumtion in Watts
double Energy_Usage=0;            // total energy used (Watt/time)

// If you have second thermoucouple
#ifdef MAXCS2
Adafruit_MAX31855 ThermocoupleB(MAXCS2);
#endif

boolean SSR_On; // just to narrow down state changes.. I don't know if this is needed/faster

// Simple functions to enable/disable SSR - for clarity, everything is separate
//
void Enable_SSR(){
  if(!SSR_On){
    digitalWrite(SSR1_RELAY_PIN, HIGH);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, HIGH);
#endif
    SSR_On=true;
  }
}

void Disable_SSR(){
  if(SSR_On){
    digitalWrite(SSR1_RELAY_PIN, LOW);
#ifdef SSR2_RELAY_PIN
    digitalWrite(SSR2_RELAY_PIN, LOW);
#endif
    SSR_On=false;
  }
}

void Enable_EMR(){
  digitalWrite(EMR_RELAY_PIN, HIGH);
}

void Disable_EMR(){
  digitalWrite(EMR_RELAY_PIN, LOW);
}



// ThermocoupleA temperature readout
//
void Update_TemperatureA(){
uint32_t raw;
double kiln_tmp1,kiln_tmp2;

  raw = ThermocoupleA.readRaw();
  kiln_tmp1 = ThermocoupleA.decodeInternal(raw); 
  if (isnan(kiln_tmp1)) {
    DBG Serial.println("[ADDONS] !! Something wrong with MAX31855-A! Internal readout failed");
    ABORT_Program(PR_ERR_MAX31A_INT_ERR);
    return;
  }
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = ThermocoupleA.decodeCelsius(raw);
  kiln_tmp2 = ThermocoupleA.linearizeCelcius(int_temp, kiln_tmp1);
  
  if (isnan(kiln_tmp1) || isnan(kiln_tmp2)) {
    DBG Serial.println("[ADDONS] !! Something wrong with thermocoupleA! External readout failed");
    ABORT_Program(PR_ERR_MAX31A_KPROBE);
    return;
  }
  kiln_temp=(kiln_temp*0.9+kiln_tmp2*0.1);    // We try to make bigger hysteresis

//  DBG Serial.printf("Temperature readout: Internal = %.1f \t Kiln raw = %.1f \t Kiln final = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
}


#ifdef MAXCS2
// ThermocoupleB temperature readout
//
void Update_TemperatureB(){
uint32_t raw;
double case_tmp1,case_tmp2;

  raw = ThermocoupleB.readRaw();
  case_tmp1 = ThermocoupleB.decodeInternal(raw); 
  if (isnan(case_tmp1)) {
    DBG Serial.println("[ADDONS] !! Something wrong with MAX31855-B! Internal readout failed");
    ABORT_Program(PR_ERR_MAX31B_INT_ERR);
    return;
  }
  int_temp = (int_temp+case_tmp1)/2;
  
  case_tmp1 = ThermocoupleB.decodeCelsius(raw);
  case_tmp2 = ThermocoupleB.linearizeCelcius(int_temp, case_tmp1);
  
  if (isnan(case_tmp1) || isnan(case_tmp2)) {
    DBG Serial.println("[ADDONS] !! Something wrong with thermocoupleB! External readout failed");
    ABORT_Program(PR_ERR_MAX31B_KPROBE);
    return;
  }
  case_temp=(case_temp*0.8+case_tmp2*0.2);    // We try to make bigger hysteresis

  DBG Serial.printf("[ADDONS] TemperatureB readout: Internal = %.1f \t Case raw = %.1f \t Case final = %.1f\n", int_temp, case_tmp1, case_temp); 
}
#endif


// Measure current power usage - to be expanded
//
void Read_Energy_INPUT(){
double Irms;
static uint8_t cnt=0;
static uint32_t last=0;

#ifdef ENERGY_MON_PIN
  Irms = emon1.calcIrms(512);  // Calculate Irms only; 512 = number of samples (internaly ESP does 8 samples per measurement)
  if(Irms<ENERGY_IGNORE_VALUE){
    Energy_Wattage=0;
    return;   // In my case everything below 0,3A is just noise. Comparing to 10-30A we are going to use we can ignore it. Final readout is correct.  
  }
  Energy_Wattage=(uint16_t)(Energy_Wattage+Irms*EMERGY_MON_VOLTAGE)/2;  // just some small hysteresis
  if(last){
    uint16_t ttime;
    ttime=millis()-last;
    Energy_Usage+=(double)(Energy_Wattage*ttime)/3600000;  // W/h - 60*60*1000 (miliseconds)
  }
  last=millis();

  if(cnt++>20){
    DBG Serial.printf("[ADDONS] VCC is set:%d ; RAW Power: %.1fW, Raw current: %.2fA, Power global:%d W/h:%.6f\n",emon1.readVcc(),Irms*EMERGY_MON_VOLTAGE,Irms,Energy_Wattage,Energy_Usage);
    cnt=0;
  }

#else
  return;
#endif

}


// Power metter loop - read energy consumption
//
void Power_Loop(void * parameter){
  for(;;){
    Read_Energy_INPUT();  // current redout takes around 3-5ms - so we will do it 10 times a second.
    vTaskDelay( 100 / portTICK_PERIOD_MS );
  }
}


// Stops Alarm
//
void STOP_Alarm(){
  ALARM_countdown=0;
  digitalWrite(ENCODER0_PINA, LOW);
}
// Start Alarm
//
void START_Alarm(){
  if(!Prefs[PRF_ALARM_TIMEOUT].value.uint16) return;
  ALARM_countdown=Prefs[PRF_ALARM_TIMEOUT].value.uint16;
  digitalWrite(ENCODER0_PINA, HIGH);
}


void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR1_RELAY_PIN, OUTPUT);
#ifdef SSR2_RELAY_PIN
    pinMode(SSR2_RELAY_PIN, OUTPUT);
#endif

  pinMode(ALARM_PIN, OUTPUT);

  SSR_On=false;
  ThermocoupleA.begin(ESP32_SPI);
#ifdef MAXCS2
  ThermocoupleB.begin(ESP32_SPI);
#endif
#ifdef ENERGY_MON_PIN
  emon1.current(ENERGY_MON_PIN, ENERGY_MON_AMPS);
  xTaskCreatePinnedToCore(
              Power_Loop,    /* Task function. */
              "Power_metter",  /* String with name of task. */
              2048,            /* Stack size in bytes. */
              NULL,            /* Parameter passed as input of the task */
              2,               /* Priority of the task. */
              NULL,1);         /* Task handle. */
              
#endif
}
