#include "battery.h"

CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> voltageBuffer;
float BatterieVoltage_value = 0;
float BatterieVoltage_movav = 0;


int8_t get_batterie_percent(uint16_t adc_value){

    if(adc_value == -1) return -1;
    if(adc_value > 3193){
      return 100;
    }else if (adc_value > 2750)
    {
      return  0.113 * adc_value - 260.384;
    }else if (adc_value > 2620)
    {
      return 0.192 * adc_value - 478.846;
    }else if (adc_value > 2370)
    {
      return 0.1 * adc_value - 237;
    }

    return -1;

}

uint16_t movingaverage(CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> *buffer,float *movav,int16_t input){
	int64_t value = 0;
	uint16_t numberofelements = 0;
	numberofelements = (*buffer).capacity - (*buffer).available();

	if ((*buffer).available() > 0)
	{
		(*buffer).push(input);
		*movav = *movav + (float) input;
		value = (int64_t) (*movav / (float)numberofelements);
	}
	else{
		*movav = *movav - (float) (*buffer).first();
		(*buffer).push(input);
		*movav = *movav + (float)input;
		value = (int64_t) (*movav / (float)numberofelements);
	}
	return (uint16_t)value;
}

uint16_t batterie_voltage_read(uint8_t Batterie_PIN){
  float BatterieVoltage_value= 0;
  for (uint8_t i = 0; i<255;i++)
    {
      BatterieVoltage_value += (float)analogRead(Batterie_PIN);
    }
    return (uint16_t) (BatterieVoltage_value / 255);
}
