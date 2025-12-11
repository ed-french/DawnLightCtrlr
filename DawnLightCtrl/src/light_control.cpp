#include <Arduino.h>

#include "light_control.h"



float duty_cycle_steps[] = {0.11f,
    0.13f,
    0.16f,
    0.20f,
    0.26f,
    0.32f,
    0.44f,
    0.54f,
    0.65f,
    0.82f,
    1.0f};


LightControl::LightControl(uint8_t bit_width,uint32_t frequency,uint8_t pin0, uint8_t pin1, uint8_t pin2, uint8_t pin3)
{
  _bit_width=bit_width;
  _frequency=frequency;
  _pin0=pin0;
  _pin1=pin1;
  _pin2=pin2;
  _pin3=pin3;
}

void LightControl::begin()
{

    pinMode(_pin0,OUTPUT);
    pinMode(_pin1,OUTPUT);  
    pinMode(_pin2,OUTPUT);
    pinMode(_pin3,OUTPUT);
  ledcSetup(0,_frequency,_bit_width);
  ledcAttachPin(_pin0,0);
  ledcSetup(1,_frequency,_bit_width);
  ledcAttachPin(_pin1,1);
  ledcSetup(2,_frequency,_bit_width);
  ledcAttachPin(_pin2,2);
  ledcSetup(3,_frequency,_bit_width);
  ledcAttachPin(_pin3,3);
  setLightLevel(0.0f);

}

void LightControl::setLightLevel(float level)
{
  if (level<0.0f) level=0.0f;
  if (level>1.0f) level=1.0f;
//   while (_level_locked)
//     {
//         delay(1);
//     }
//   _level_locked=true;
//   float scaled_level=(uint32_t)(level*1023.0f);
uint32_t duty_cycle=_calc_duty_from_level(level);
Serial.printf("LightControl::setLightLevel: level=%f -> duty_cycle=%d\n",level,duty_cycle);
  _current_raw_outputs[0]=duty_cycle;
    _current_raw_outputs[1]=duty_cycle;
    _current_raw_outputs[2]=duty_cycle;
    _current_raw_outputs[3]=duty_cycle;
//   _level_locked=false;

  ledcWrite(0,_current_raw_outputs[0]);
  ledcWrite(1,_current_raw_outputs[1]);
  ledcWrite(2,_current_raw_outputs[2]);
  ledcWrite(3,_current_raw_outputs[3]);
}

uint32_t LightControl::_calc_duty_from_level(float level)
{
    // Calculate the duty cycle from the level using the calibration data
    // level is from 0.0 to 1.0
    if (level<=0.0f) return 0;
    if (level>=1.0f) return (1<<_bit_width)-1;
    // Find the appropriate step
    size_t num_steps = sizeof(duty_cycle_steps)/sizeof(duty_cycle_steps[0]);
    Serial.printf("Number of steps, should be 11: %d\n",num_steps);
    // Use the calibration points and interpolate between them
    // such that level 0.0 -> duty_cycle_steps[0], level 1.0 -> duty_cycle_steps[num_steps-1]
    float scaled_level = level * (num_steps - 1);
    Serial.printf("Scaled level: %f\n",scaled_level);
    size_t index = (size_t)scaled_level;
    Serial.printf("Index: %d\n",index);
    if (index >= num_steps - 1) return (1<<_bit_width)-1; // This should not happen due to earlier check
    float fraction = ((float)scaled_level - (float)index)*(1.0f/(num_steps - 1));
    float duty_cycle_below=duty_cycle_steps[index];
    float duty_cycle_above=duty_cycle_steps[index+1];
    Serial.printf("Duty cycle below: %f, above: %f, fraction: %f\n",duty_cycle_below,duty_cycle_above,fraction);
    float duty_cycle= duty_cycle_below + fraction * (duty_cycle_above - duty_cycle_below);
    Serial.printf("Float version of duty cycle calculated: index=%d, fraction=%f, duty_cycle=%f\n",index,fraction,duty_cycle);
    return (uint32_t)(duty_cycle*((1<<_bit_width)-1));
}