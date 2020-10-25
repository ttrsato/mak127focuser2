// ATOM_StepMotorCtrl.ino
// MAK127SP focuser stepper motor driver
//
// MIT License
// 
// Copyright (c) 2020 Tatsuro Sato
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// ATOM_StepMotorCtrl.ino uses I2C slave library WireSlave Receiver
//
// WireSlave Receiver
// by Gutierrez PS <https://github.com/gutierrezps>
// ESP32 I2C slave library: <https://github.com/gutierrezps/ESP32_I2C_Slave>
// based on the example by Nicholas Zambetti <http://www.zambetti.com>

#include "M5Atom.h"
#include <Ticker.h>
#include <WireSlave.h>

// #define DEBUG_PRINT

const int I2C_SLAVE_ADDR  = 0x04;
const int SDA_PIN         = 26;
const int SCL_PIN         = 32;
const int EN_PIN          = 22;
const int DIR_PIN         = 23;
const int STEP_PIN        = 19;
const int FAULT_PIN       = 25;
const int VIN_PIN         = 33;
const int PULSE_WH        = 500;
const int PULSE_WL        = 500;
int pulse_wh = PULSE_WH;
int pulse_wl = PULSE_WL;

const int ATOM_CMD = 0x01;

int step = 0;
int speed = 0;

volatile int move_step = 0;

enum {
  LED_ENABLE  = 0x300000,
  LED_DISABLE = 0x003000,
  LED_FAULT   = 0x707070,
  LED_OFF     = 0x000000,
};

enum {BLK_STOP, BLK_H, BLK_L};
Ticker blink_tick;
volatile int blink_stat = BLK_STOP;
int blink_color = LED_FAULT;

Ticker uart_timeout;
uint8_t buff[4];
uint8_t buff_cnt = 0;

void receiveEvent(int howMany);

void debugPrint(char *s)
{
#ifdef DEBUG_PRINT
  Serial.println(s);
#endif
}

void debugPrint(int d)
{
#ifdef DEBUG_PRINT
  Serial.println(d);
#endif
}

int isFault()
{
  int fault = digitalRead(FAULT_PIN) ? 0 : 1;
  int vin = digitalRead(VIN_PIN) ? 0 : 1;
  if (fault) {
    blink_color = LED_FAULT;
  } else {
    blink_color = LED_DISABLE;
  }
  return fault || vin;
}

void setEnable(bool en)
{
  if (en) {
    digitalWrite(EN_PIN, LOW);
    M5.dis.drawpix(0, LED_ENABLE);
    debugPrint("Enabled");
    ets_delay_us(2);
  } else {
    digitalWrite(EN_PIN, HIGH);
    ets_delay_us(2);
    M5.dis.drawpix(0, LED_DISABLE);
    debugPrint("Disabled");
  }
}

void stepPulse()
{
  digitalWrite(STEP_PIN, HIGH);
  ets_delay_us(pulse_wh);
  digitalWrite(STEP_PIN, LOW);
  ets_delay_us(pulse_wl);
}

enum {ST_ENABLE, ST_DISABLE, ST_FAULT};

int state = ST_FAULT;

void blink_toggle()
{
  if (blink_stat == BLK_H) {
    M5.dis.drawpix(0, LED_OFF);  
    blink_stat = BLK_L;
  } else {
    M5.dis.drawpix(0, blink_color);  
    blink_stat = BLK_H;
  }
}

void updateStatus()
{
  int is_fault = isFault();
  int btn_was_pressed = M5.Btn.wasPressed();
  if (is_fault) {
    if (blink_stat == BLK_STOP) {
      debugPrint("FAULT");
      state = ST_FAULT;
      blink_stat = BLK_H;
      M5.dis.drawpix(0, blink_color);
      blink_tick.attach(0.5, blink_toggle);
      return;
    }
  } else {
    switch (state) {
      case ST_ENABLE:
        if (btn_was_pressed) {
          state = ST_DISABLE;
          setEnable(false);
        }
        break;
      case ST_DISABLE:
        if (btn_was_pressed) {
          state = ST_ENABLE;
          setEnable(true);
        }
        break;
      case ST_FAULT:
        debugPrint("Exit fault");
        state = ST_ENABLE;
        blink_stat = BLK_STOP;
        blink_tick.detach();
        setEnable(true);
        break;
      default:
        state = ST_ENABLE;
        setEnable(true);
        break;
    }
  }
}

void setup()
{
  M5.begin(true, false, true);
  Serial.begin(115200);

  bool success = WireSlave.begin(SDA_PIN, SCL_PIN, I2C_SLAVE_ADDR);
  if (!success) {
    debugPrint("I2C slave init failed");
    while (1) delay(100);
  }

  WireSlave.onReceive(receiveEvent);
  
  // setup the pins on the microcontroller:
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(FAULT_PIN, INPUT);
  pinMode(VIN_PIN, INPUT);
  setEnable(true);
  debugPrint("Start ATOM Stepper!");
}

void uart_timeout_cback()
{
  buff_cnt = 0;
  Serial.println("Timeout");
}

void cmd_decode()
{
  uint8_t sum = buff[0] + buff[1] + buff[2];
  if (buff[4] == sum && buff[0] == 0x01) {
    pulse_wh = PULSE_WH / buff[3];
    pulse_wl = PULSE_WL / buff[3];
    int16_t data = (buff[2] << 8) | buff[1];
    move_step -= data;
  }
}

void loop()
{
  updateStatus();
  WireSlave.update();
  if (move_step > 0) {
    move_step--;
    digitalWrite(DIR_PIN, LOW);
    stepPulse();
    if (move_step == 0) {
      Serial.write('D');
    }
  } else if (move_step < 0) {
    move_step++;
    digitalWrite(DIR_PIN, HIGH);
    stepPulse();
    if (move_step == 0) {
      Serial.write('D');
    }
  }
  M5.update();
  if (Serial.available() > 0) {
    if (buff_cnt == 0) {
      uart_timeout.once_ms(500, uart_timeout_cback);
    }
    buff[buff_cnt] = Serial.read();
    buff_cnt++;
    if (buff_cnt == 5) {
      buff_cnt = 0;
      uart_timeout.detach();
      cmd_decode();
    }
  }
  delay(1);
}

void receiveEvent(int howMany)
{
  int16_t cnt;
  uint8_t c = WireSlave.read();      // receive cmd
  uint8_t l = WireSlave.read();      // receive signed byte parameter
  uint8_t h = WireSlave.read();      // receive signed byte parameter
  uint8_t s = WireSlave.read();      // receive signed byte parameter
  if (c == ATOM_CMD) {
    cnt = (h << 8) | l;
    move_step -= cnt;
    pulse_wh = PULSE_WH / s;
    pulse_wl = PULSE_WL / s;
  }
  while (1 < WireSlave.available());
  debugPrint(cnt);               // print the integer
}
