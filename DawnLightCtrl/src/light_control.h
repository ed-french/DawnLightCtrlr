#include <Arduino.h>



#ifndef LIGHT_CONTROL_H
#define LIGHT_CONTROL_H
class LightControl {
  public:
    LightControl(uint8_t bit_width,uint32_t frequency,uint8_t pin0, uint8_t pin1, uint8_t pin2, uint8_t pin3);
    void begin();
    void setLightLevel(float level); // level from 0.0 to 1.0
    void update();
  private:
    uint8_t _pin0;
    uint8_t _pin3;
    uint8_t _pin1;
    uint8_t _pin2;
    uint8_t _bit_width=10;
    uint32_t _frequency=5000;
    uint32_t _current_raw_outputs[4]={0,0,0,0};
    bool _level_locked=false;
    uint32_t _calc_duty_from_level(float level);
    
};
#endif