/*
** Function for relays (SSR, EMR) and temperature sensors
**
*/
#include <MAX31855.h>
#include <MAX6675.h>

// Initialize SPI and MAX31855
SPIClass *ESP32_SPI = new SPIClass(HSPI);
MAX31855 ThermocoupleA(MAXCS1);
MAX6675 ThermocoupleB(MAXCS2);

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

void print_bits(uint32_t raw){
    for (int i = 31; i >= 0; i--)
    {
        bool b = bitRead(raw, i);
        Serial.print(b);
    }

Serial.println();
}


// ThermocoupleA temperature readout
//
void Update_TemperatureA(){
uint32_t raw;
double kiln_tmp1;

  raw = ThermocoupleA.readRawData();
//Serial.print("A");
//print_bits(raw);

  if(!raw){ // probably MAX31855 not connected
    DBG dbgLog(LOG_ERR,"[ADDONS] MAX31855 for ThermocoupleA did not respond\n");
    ABORT_Program(PR_ERR_MAX31A_NC);
    return;
  }
  if(ThermocoupleA.detectThermocouple(raw) != MAX31855_THERMOCOUPLE_OK){
    switch (ThermocoupleA.detectThermocouple())
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to VCC\n");
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA short to GND\n");
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA not connected\n");
        break;

      default:
        DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA unknown error, check spi cable\n");
        break;
    }
    if(TempA_errors<Prefs[PRF_ERROR_GRACE_COUNT].value.uint8){
      TempA_errors++;
      DBG dbgLog(LOG_ERR,"[ADDONS] ThermocoupleA had an error but we are still below grace threshold - continue. Error %d of %d\n",TempA_errors,Prefs[PRF_ERROR_GRACE_COUNT].value.uint8);
    }else{
      ABORT_Program(PR_ERR_MAX31A_INT_ERR);
    }
    return;
  }

  kiln_tmp1 = ThermocoupleA.getColdJunctionTemperature(raw); 
  int_temp = (int_temp+kiln_tmp1)/2;
  
  kiln_tmp1 = ThermocoupleA.getTemperature(raw);
  kiln_temp=(kiln_temp*0.9+kiln_tmp1*0.1);    // We try to make bigger hysteresis

  if(TempA_errors>0) TempA_errors--;  // Lower errors count after proper readout
  
  DBG dbgLog(LOG_DEBUG, "[ADDONS] Temperature sensor A readout: Internal temp = %.1f \t Last temp = %.1f \t Average kiln temp = %.1f\n", int_temp, kiln_tmp1, kiln_temp); 
}


#ifdef MAXCS2
// ThermocoupleB temperature readout
//
void Update_TemperatureB(){
float raw;
double case_tmp1;

  raw = ThermocoupleB.readTempC();

  if (raw == -1) {
    DBG dbgLog(LOG_ERR, "[ADDONS] MAX6675 for ThermocoupleB failure\n");
    ABORT_Program(PR_ERR_MAX66B_NC);
    return;
  }

  case_tmp1 = raw;
  case_temp=(case_temp*0.8+case_tmp1*0.2);    // We try to make bigger hysteresis
  
  DBG dbgLog(LOG_DEBUG,"[ADDONS] Temperature sensor B readout: Internal temp = %.1f \t Last temp = %.1f \t Average case temp = %.1f\n", int_temp, case_tmp1, case_temp); 
}
#endif



void Setup_Addons(){
  pinMode(SSR1_RELAY_PIN, OUTPUT);

  pinMode(ALARM_PIN, OUTPUT);

  SSR_On=false;
  ThermocoupleA.begin(ESP32_SPI);
  ThermocoupleB = MAX6675(MAXCS2);
}
