#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <CircularBuffer.h>

#define MOVAV_LENGTH_BATTERY 800

extern CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> voltageBuffer;
extern float BatterieVoltage_value;
extern float BatterieVoltage_movav;

int8_t get_batterie_percent(uint16_t ADC_value);
uint16_t movingaverage(CircularBuffer<uint16_t,MOVAV_LENGTH_BATTERY> *buffer,float *movav,int16_t input);
uint16_t batterie_voltage_read(uint8_t Batterie_PIN);

#endif
