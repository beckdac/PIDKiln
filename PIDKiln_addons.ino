/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <Adafruit_MAX31855.h>

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
Adafruit_MAX31855 ThermocoupleA(MAXCS1);

#ifdef MAXCS2
Adafruit_MAX31855 ThermocoupleB(MAXCS2);
#endif

boolean SSR_On; // just to narrow down state changes.. I don't know if this is needed/faster

// Simple functions to enable/disable SSR - for clarity, everything is separate
//
void Enable_SSR(){
  if(!SSR_On){
//    DBG Serial.println("[ADDONS] Enable SSR");
    digitalWrite(SSR_RELAY_PIN, HIGH);
    SSR_On=true;
  }
}

void Disable_SSR(){
  if(SSR_On){
//    DBG Serial.println("[ADDONS] Disable SSR");
    digitalWrite(SSR_RELAY_PIN, LOW);
    SSR_On=false;
  }
}

void Enable_EMR(){
  //digitalWrite(EMR_RELAY_PIN, HIGH);
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
    ABORT_Program(PR_ERR_MAX31_INT_ERR);
    return;
  }
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = ThermocoupleA.decodeCelsius(raw);
  kiln_tmp2 = ThermocoupleA.linearizeCelcius(int_temp, kiln_tmp1);
  
  if (isnan(kiln_tmp1) || isnan(kiln_tmp2)) {
    DBG Serial.println("[ADDONS] !! Something wrong with thermocoupleA! External readout failed");
    ABORT_Program(PR_ERR_MAX31_KPROBE);
    return;
  }
  kiln_temp=(kiln_temp*0.9+kiln_tmp2*0.1);    // We try to make bigger hysteresis

  //DBG Serial.printf("Temperature readout: Internal = %.1f \t Kiln raw = %.1f \t Kiln final = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
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
    ABORT_Program(PR_ERR_MAX31_INT_ERR);
    return;
  }
  int_temp = (int_temp+case_tmp1)/2;
  
  case_tmp1 = ThermocoupleB.decodeCelsius(raw);
  case_tmp2 = ThermocoupleB.linearizeCelcius(int_temp, case_tmp1);
  
  if (isnan(case_tmp1) || isnan(case_tmp2)) {
    DBG Serial.println("[ADDONS] !! Something wrong with thermocoupleB! External readout failed");
    ABORT_Program(PR_ERR_MAX31_KPROBE);
    return;
  }
  case_temp=(case_temp*0.8+case_tmp2*0.2);    // We try to make bigger hysteresis

  DBG Serial.printf("[ADDONS] TemperatureB readout: Internal = %.1f \t Case raw = %.1f \t Case final = %.1f\n", int_temp, case_tmp1, case_temp); 
}
#endif


void Setup_Addons(){
  pinMode(EMR_RELAY_PIN, OUTPUT);
  pinMode(SSR_RELAY_PIN, OUTPUT);

  SSR_On=false;
  ThermocoupleA.begin(ESP32_SPI);
#ifdef MAXCS2
  ThermocoupleB.begin(ESP32_SPI);
#endif  
}
