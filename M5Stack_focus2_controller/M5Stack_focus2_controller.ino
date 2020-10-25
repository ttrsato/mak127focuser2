#include <M5Stack.h>
#include <M5StackUpdater.h>
#include <Ticker.h>
#include <WirePacker.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>

static LGFX lcd;

const int I2C_ADDR_FACES   = 0x5E;
const int I2C_ADDR_PLUS    = 0x62;
const int I2C_ADDR_STEPPER = 0x04;

int EncoderI2CAddr = I2C_ADDR_FACES;

const int ATOM_CMD = 0x01;

const int DISP_W = 320;
const int DISP_H = 240;
const int CIR_R = 195 / 2;
const int V_STEP = 28;
const int H_OFST = 115;
const int H_OFS = 70;
const int V_OFS = 72;
const int V_BASE = 70;

char sw_txt[6][5] = {
  "LLLX",
  "LLHX",
  "LHLX",
  "LHHX",
  "HLLX",
  "HHHX"
};

enum {DIR_NONE, DIR_CW, DIR_CCW};

int encoder_increment;
int encoder_value = 0;
int direction;
uint8_t last_button, cur_button, changed = 0;

Ticker seq_tick;
const float toggle_period_sec = 0.1;
volatile bool step_run_flg = false;
volatile int step_run_cnt = 0;
int step_run_dir = DIR_CW;
volatile int tgl_c = 1;

int mark1 = 0;
int mark2 = 0;
int cur_pos = 0;
int last_pos = 0;
int step_round_org = 200;
int step_ustep = 32;
int step_round = step_round_org * step_ustep;
uint16_t step_times = step_ustep;
int step_mag = 1;
float step_angle = 360.0 / step_round;
int sround = 200;
int sustep = 32;
int brightness = 16 * 3 - 1;

bool fine_mode = false;

int color_fg = TFT_RED;
int color_bg = TFT_BLACK;

uint8_t step_dir = DIR_CW;
uint8_t last_step_dir = DIR_CW;
uint8_t ec, last_ec;

uint8_t wasReleased()
{
  return cur_button && changed;
}

uint8_t wasPressed()
{
  return !cur_button && changed;
}

int getValue() {
  //int tmp_encoder_inc;
  Wire.requestFrom(EncoderI2CAddr, 3);
  if (Wire.available()) {
    // Encoder
    int8_t tmp_encoder_inc = Wire.read();
    if (EncoderI2CAddr == I2C_ADDR_PLUS) {
      if (-3 < tmp_encoder_inc && tmp_encoder_inc < 3) {
        tmp_encoder_inc = 0;
      }
    }
    if (tmp_encoder_inc == 0) {
      direction = DIR_NONE;
    } else if (tmp_encoder_inc < 0) {
      direction = DIR_CW;
    } else {
      direction = DIR_CCW;
    }

    // Button
    uint8_t _btn = Wire.read();
    if (EncoderI2CAddr == I2C_ADDR_PLUS) {
      _btn = (_btn == 0xFF) ? 1 : 0;
    }
    if (_btn != cur_button) {
      changed = 1;
    } else {
      changed = 0;
    }
    last_button = cur_button;
    cur_button = _btn;
  }
  if (EncoderI2CAddr == I2C_ADDR_PLUS) {
    vTaskDelay(100);
  }
  return direction;
}

void setLed(int i, int r, int g, int b)
{
  Wire.beginTransmission(EncoderI2CAddr);
  Wire.write(i);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}

int i2c_scanner()
{
  int err;
  Wire.beginTransmission(I2C_ADDR_FACES);
  err = Wire.endTransmission();
  if (err == 0) {
    EncoderI2CAddr = I2C_ADDR_FACES;
  } else {
    Wire.beginTransmission(I2C_ADDR_PLUS);
    err = Wire.endTransmission();
    if (err == 0) {
      EncoderI2CAddr = I2C_ADDR_PLUS;
    } else {
      lcd.println("Encoder module not found.");
      lcd.println("Please connect faces encoder module or plus module.");
      while (1) delay(100);
    }
  }
}

void setNextPhase(uint8_t dir)
{
  step_dir = dir;
  last_pos = cur_pos;
  if (dir == DIR_CW) {
    cur_pos -= step_times;
  } else {
    cur_pos += step_times;
  }
  if (cur_pos < 0) {
    cur_pos = step_round - 1;
  }
  int mod = cur_pos % step_times;
  cur_pos -= mod;
  cur_pos = cur_pos % step_round;
}

void drawVal(int ofs, float val, bool rev)
{
  int cf = color_fg;
  int cb = color_bg;
  if (rev) {
    cf = color_bg;
    cb = color_fg;
  }
  lcd.fillRect(DISP_W - H_OFS - 7, V_OFS + ofs - 4, 72, 24, cb);
  lcd.setTextSize(2);
  lcd.setTextColor(cf);
  lcd.setCursor(DISP_W - H_OFS - 5, V_OFS + ofs);
  lcd.printf("%.02f", val);
}

void drawPos(int pos, int color, int fill)
{
  int center_x = CIR_R;
  int center_y = CIR_R - 2;
  float angle = pos * step_angle / step_times;
  float sx0 = cos((angle - 90) * 0.0174532925);
  float sy0 = sin((angle - 90) * 0.0174532925);
  float sx1 = cos((angle + 175 - 90) * 0.0174532925);
  float sy1 = sin((angle + 175 - 90) * 0.0174532925);
  float sx2 = cos((angle + 185 - 90) * 0.0174532925);
  float sy2 = sin((angle + 185 - 90) * 0.0174532925);
  int x0 = sx0 * (CIR_R - 25) + center_x;
  int y0 = sy0 * (CIR_R - 25) + center_y;
  int x1 = sx1 * (CIR_R - 25) + center_x;
  int y1 = sy1 * (CIR_R - 25) + center_y;
  int x2 = sx2 * (CIR_R - 25) + center_x;
  int y2 = sy2 * (CIR_R - 25) + center_y;
  if (fill) {
    lcd.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  } else {
    lcd.drawTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  drawVal(V_STEP, step_angle / step_times * cur_pos, false);
}

void drawCircle(int clr)
{
  int center_x = CIR_R;
  int center_y = CIR_R - 2;
  if (clr == 0) {
    lcd.fillCircle(center_x, center_y, CIR_R - 1, color_fg);
  }
  lcd.fillCircle(center_x, center_y, CIR_R - 9, color_bg);

  for (int i = 0; i < 360; i += 30) {
    float sx  = cos((i - 90) * 0.0174532925);
    float sy  = sin((i - 90) * 0.0174532925);
    int x0 = sx * (CIR_R - 6)  + center_x;
    int y0 = sy * (CIR_R - 6)  + center_y;
    int x1 = sx * (CIR_R - 20) + center_x;
    int y1 = sy * (CIR_R - 20) + center_y;
    lcd.drawLine(x0, y0, x1, y1, color_fg);
  }
  for (int i = 0; i < step_round / step_times; i++) {
    float deg = i * step_angle;
    float sx  = cos((deg - 90) * 0.0174532925);
    float sy  = sin((deg - 90) * 0.0174532925);
    int x0 = sx * (CIR_R - 18) + center_x;
    int y0 = sy * (CIR_R - 18) + center_y;
    // Draw minute markers
    lcd.drawPixel(x0, y0, color_fg);
  }
  float deg = mark1 * step_angle / step_times;
  float sx  = cos((deg - 90) * 0.0174532925);
  float sy  = sin((deg - 90) * 0.0174532925);
  int x0 = sx * (CIR_R - 13) + center_x;
  int y0 = sy * (CIR_R - 13) + center_y;
  lcd.fillCircle(x0, y0, 3, color_fg);
  deg = mark2 * step_angle / step_times;
  sx  = cos((deg - 90) * 0.0174532925);
  sy  = sin((deg - 90) * 0.0174532925);
  x0 = sx * (CIR_R - 13) + center_x;
  y0 = sy * (CIR_R - 13) + center_y;
  lcd.fillCircle(x0, y0, 3, color_fg);
  drawPos(cur_pos, color_fg, 1);
}

void sendCommand(int dir)
{
  WirePacker packer;
  packer.write(ATOM_CMD);
  int16_t steps;
  if (dir == DIR_CW) {
    steps = -1 * step_times;
  } else {
    steps = step_times;
  }
  Serial.println(steps);
  uint8_t b;
  b = steps & 0xFF;
  packer.write(b);
  b = (steps >> 8) & 0xFF;
  packer.write(b);
  packer.write(step_ustep);
  packer.end();
  Wire.beginTransmission(I2C_ADDR_STEPPER);
  while (packer.available()) {    // write every packet byte
    Wire.write(packer.read());
  }
  Wire.endTransmission();         // stop transmitting
}

void countDownSeq()
{
  if (step_run_cnt <= 0) {
    return;
  }
  sendCommand(step_run_dir);
  step_run_cnt--;
  step_run_flg = true;
  if (step_run_cnt == 0) {
    seq_tick.detach();
  }
}

enum {STEP_TIMES_FINE, STEP_TIMES_KEEP, STEP_TIMES_NEXT};

void updateStepTimes(int mode)
{
  if (mode == STEP_TIMES_FINE) {
    if (fine_mode) {
      step_times = 1;
    } else {
      step_times = step_mag * step_ustep;
    }
  } else if (mode == STEP_TIMES_KEEP) {
    step_times = step_mag * step_ustep;
  } else {
    if (step_mag == 1) {
      step_mag = 5;
    } else if (step_mag == 5) {
      step_mag = 10;
    } else {
      step_mag = 1;
    }
    step_times = step_mag * step_ustep;
  }
  step_angle = 360.0 / step_round * step_times;
  drawVal(0, step_angle, fine_mode);
  lcd.fillRect(80 * 2 + 67, DISP_H - 30, 18 * 4, 16, color_bg);
  lcd.setTextSize(2);
  lcd.setTextColor(color_fg);
  lcd.setCursor(80 * 2 + 80, DISP_H - 30);
  lcd.print("x");
  lcd.print(step_mag);
  drawCircle(1);
  delay(100);
}

enum {FINE_DISABLE, FINE_ENABLE, FINE_TOGGLE};

void setFineMode(int set)
{
  if (set == FINE_DISABLE) {
    if (fine_mode) {
      fine_mode = false;
      updateStepTimes(STEP_TIMES_KEEP);
    } else {
      updateStepTimes(STEP_TIMES_NEXT);
    }
  } else if (set == FINE_ENABLE) {
    fine_mode = true;
    updateStepTimes(STEP_TIMES_FINE);
  } else if (set == FINE_TOGGLE) {
    fine_mode = (fine_mode) ? false : true;
    updateStepTimes(STEP_TIMES_FINE);
  }
}

int blinkCenter(int flg)
{
  int next_flg;
  lcd.setTextSize(2);
  lcd.setCursor(80 * 1 + 42, DISP_H - 30);
  if (flg == 0) {
    lcd.fillRect(80 * 1 + 38, DISP_H - 33, 77, 20, color_bg);
    lcd.setTextColor(color_fg);
    next_flg = 1;
  } else {
    lcd.fillRect(80 * 1 + 38, DISP_H - 33, 77, 20, color_fg);
    lcd.setTextColor(color_bg);
    next_flg = 0;
  }
  lcd.print("Center");
  return next_flg;
}

void initDisplay()
{
  lcd.setCursor(0, 0);
  lcd.setTextColor(color_fg);
  lcd.setTextSize(1);
  if (EncoderI2CAddr == I2C_ADDR_FACES) {
    lcd.print("Faces");
  } else {
    lcd.print("Plus");
  }
  lcd.setTextColor(color_fg);
  lcd.setTextSize(1);
  lcd.setCursor(DISP_W - H_OFST, V_BASE);
  lcd.print("Step");
  lcd.setCursor(DISP_W - H_OFST, V_BASE + 12);
  lcd.print("Angle");
  drawCircle(0);
  lcd.fillRect(DISP_W - H_OFST - 8, 2, 124, 35, color_fg);
  lcd.setTextColor(color_bg);
  lcd.setTextSize(2);
  lcd.setCursor(DISP_W - H_OFST - 5, 5);
  lcd.print("Focus2");
  lcd.setCursor(DISP_W - H_OFST - 5, 20);
  lcd.print("controller");
  lcd.setCursor(DISP_W - H_OFST - 4, 5);
  lcd.print("Focus2");
  lcd.setCursor(DISP_W - H_OFST - 4, 20);
  lcd.print("controller");
  lcd.setTextColor(color_fg);
  lcd.setTextSize(1);
  lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP);
  lcd.print("Cur");
  lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP + 12);
  lcd.print("Pos");
  lcd.setTextSize(1);
  lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP * 2 + 6);
  lcd.setTextSize(1);
  lcd.print("Mark1");
  drawVal(V_STEP * 2, step_angle / step_times * mark1, false);
  lcd.setCursor(DISP_W - H_OFST, V_BASE + V_STEP * 3 + 6);
  lcd.setTextSize(1);
  lcd.print("Mark2");
  drawVal(V_STEP * 3, step_angle / step_times * mark2, false);
  lcd.setTextSize(2);
  lcd.setTextColor(color_fg);
  lcd.setCursor(80 * 0 + 42, DISP_H - 30);
  lcd.print("Mark");
  lcd.setCursor(80 * 2 + 80, DISP_H - 30);
  lcd.print("x");
  lcd.print(step_mag);
  drawVal(0, step_angle, fine_mode);
  blinkCenter(0);
}

Preferences prefs;

void setMotor()
{
  int sel = 1;
  lcd.setTextColor(color_fg);
  lcd.setTextSize(2);
  lcd.setCursor(0, 0);
  lcd.print("STEPPER MOTOR SETTING");
  lcd.setCursor(0, 0 + 18);
  lcd.print(" Step/Round: ");
  lcd.setCursor(0, 18 + 18);
  lcd.print(" Ustep: ");
  lcd.setCursor(0, 18 + 18 * 2);
  lcd.print(" Brightness: ");
  lcd.setCursor(12 * 12, 0 + 18);
  lcd.print(step_round_org);
  lcd.setCursor(12 * 12, 18 + 18);
  if (step_ustep == 1) {
    lcd.print("Full ");
  } else if (step_ustep == 2) {
    lcd.print("Half ");
  } else {
    lcd.print("1/ ");
    lcd.printf("%2d", step_ustep);
  }
  if (step_ustep == 1) {
    lcd.print(sw_txt[0]);
  }
  else if (step_ustep == 2) {
    lcd.print(sw_txt[1]);
  }
  else if (step_ustep == 4) {
    lcd.print(sw_txt[2]);
  }
  else if (step_ustep == 8) {
    lcd.print(sw_txt[3]);
  }
  else if (step_ustep == 16) {
    lcd.print(sw_txt[4]);
  }
  else {
    lcd.print(sw_txt[5]);
  }
  lcd.setCursor(12 * 12, 18 + 18 * 2);
  lcd.print(brightness);
  lcd.setCursor(0, 18 * sel + 18);
  lcd.print("*");
  lcd.setCursor(80 * 0 + 42, DISP_H - 30);
  lcd.print("Save");
  lcd.setCursor(80 * 1 + 60, DISP_H - 30);
  lcd.print("Sel");
  lcd.setCursor(80 * 2 + 67, DISP_H - 30);
  lcd.print("Quit");
  while (1) {
    int ec_dir = getValue();
    if (ec_dir != DIR_NONE) {
      if (ec_dir == DIR_CW) {
        if (sel == 0) {
          step_round_org--;
          if (step_round_org < 0) {
            step_round_org = 0;
          }
        } else if (sel == 1) {
          step_ustep /= 2;
          if (step_ustep <= 0) {
            step_ustep = 1;
          }
        } else {
          brightness -= 16;
          if (brightness < 0) {
            brightness = 0;
          }
          lcd.setBrightness(brightness);
        }
      } else {
        if (sel == 0) {
          step_round_org++;
          if (step_round_org > 1000) {
            step_round_org = 1000;
          }
        } else if (sel == 1) {
          step_ustep *= 2;
          if (step_ustep > 32) {
            step_ustep = 32;
          }
        } else {
          brightness += 16;
          if (brightness > 255) {
            brightness = 255;
          }
          lcd.setBrightness(brightness);
        }
      }
      lcd.fillRect(12 * 12, 0 + 18, 12 * 10, 18 * 3, TFT_BLACK);
      lcd.setCursor(12 * 12, 0 + 18);
      lcd.print(step_round_org);
      lcd.setCursor(12 * 12, 18 + 18);
      if (step_ustep == 1) {
        lcd.print("Full ");
      } else if (step_ustep == 2) {
        lcd.print("Half ");
      } else {
        lcd.print("1/");
        lcd.printf("%2d ", step_ustep);
      }
      if (step_ustep == 1) {
        lcd.print(sw_txt[0]);
      }
      else if (step_ustep == 2) {
        lcd.print(sw_txt[1]);
      }
      else if (step_ustep == 4) {
        lcd.print(sw_txt[2]);
      }
      else if (step_ustep == 8) {
        lcd.print(sw_txt[3]);
      }
      else if (step_ustep == 16) {
        lcd.print(sw_txt[4]);
      }
      else {
        lcd.print(sw_txt[5]);
      }
      lcd.setCursor(12 * 12, 18 + 18 * 2);
      lcd.print(brightness);
    }
    if (M5.BtnA.wasPressed()) {
      lcd.fillScreen(0);
      prefs.putUInt("round", step_round_org);
      prefs.putUInt("ustep", step_ustep);
      prefs.putUInt("bright", brightness);
      Serial.println("Saved");
      step_round = step_round_org * step_ustep;
      step_angle = 360.0 / step_round;
      return;
    }
    if (M5.BtnB.wasPressed()) {
      sel++;
      if (sel > 2) {
        sel = 0;
      }
      lcd.fillRect(0, 0 + 18, 10, 18 * 3, TFT_BLACK);
      lcd.setCursor(0, 18 * sel + 18);
      lcd.print("*");
    }
    if (M5.BtnC.wasPressed()) {
      lcd.fillScreen(0);
      Serial.println("Quit");
      step_round = step_round_org * step_ustep;
      step_angle = 360.0 / step_round;
      return;
    }
    delay(10);
    M5.update();
  }
}

void setup() {
  M5.begin();
  if (digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }
  dacWrite(25, 0); // STOP SP
  Wire.begin();
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(brightness);
  lcd.setColorDepth(16);
  delay(500);
  i2c_scanner();
  M5.update();
  prefs.begin("Focus2", false);
  sround = prefs.getUInt("round", 0);
  if (sround != 0) {
    step_round_org = sround;
    Serial.println("Set round:" + sround);
  }
  sustep = prefs.getUInt("ustep", 0);
  if (sustep != 0) {
    step_ustep = sustep;
    step_times = sustep;
    Serial.println("Set ustep:" + sustep);
  }
  int bright = prefs.getUInt("bright", 0);
  if (bright != 0) {
    brightness = bright;
    Serial.println("Set brightness:" + brightness);
    lcd.setBrightness(brightness);
  }
  if (M5.BtnC.isPressed()) {
    setMotor();
  }
  step_round = step_round_org * step_ustep;
  step_angle = 360.0 / step_round;

  initDisplay();
  updateStepTimes(STEP_TIMES_KEEP);
  last_button = cur_button = 1;
}

void loop() {
  if (step_run_cnt > 0 || step_run_flg) {
    // Auto step sequence
    if (step_run_flg) {
      setNextPhase(step_run_dir);
      drawPos(last_pos, color_bg, 1);
      drawPos(cur_pos, color_fg, 1);
      tgl_c = blinkCenter(tgl_c);
      step_run_flg = false;
    }
    if (step_run_cnt == 0) {
      blinkCenter(0);
    }
    delay(10);
  } else {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      Serial.println("Mark");
      mark2 = mark1;
      mark1 = cur_pos;
      drawVal(V_STEP * 2, step_angle / step_times * mark1, false);
      drawVal(V_STEP * 3, step_angle / step_times * mark2, false);
      drawCircle(1);
    } else if (M5.BtnB.wasPressed()) {
      int norm_min, norm_max, norm_pos;
      if (mark1 > mark2) {
        norm_min = mark2;
        norm_max = mark1;
      } else {
        norm_min = mark1;
        norm_max = mark2;
      }
      // Normalize
      norm_max = norm_max - norm_min;
      norm_pos = cur_pos - norm_min;
      norm_min = 0;
      if (norm_min == norm_pos) {
        if (norm_max - norm_min < step_round / 2) {
          step_run_dir = DIR_CCW;
          step_run_cnt = (norm_max - norm_min) / 2;
        } else {
          step_run_dir = DIR_CW;
          step_run_cnt = (step_round - (norm_max - norm_min)) / 2;
        }
      } else if (norm_max == norm_pos) {
        if (norm_max - norm_min < step_round / 2) {
          step_run_dir = DIR_CW;
          step_run_cnt = (norm_max - norm_min) / 2;
        } else {
          step_run_dir = DIR_CCW;
          step_run_cnt = (step_round - (norm_max - norm_min)) / 2;
        }
      } else {
        int norm_tgt = (norm_max - norm_min) / 2;
        if (norm_pos > norm_max) {
          norm_tgt = (step_round - (norm_max - norm_min)) / 2 + norm_max;
        }
        step_run_cnt = norm_tgt - norm_pos;
        if (step_run_cnt > 0) {
          step_run_dir = DIR_CCW;
        } else {
          step_run_dir = DIR_CW;
        }
        step_run_cnt = abs(step_run_cnt);
        Serial.printf("pos: %d\n", norm_pos);
        Serial.printf("tgt: %d\n", norm_tgt);
      }
      Serial.printf("Center: diff %d %d", step_run_cnt, step_run_dir);
      tgl_c = 1;
      setFineMode(FINE_ENABLE);
      seq_tick.attach(toggle_period_sec, countDownSeq);
    } else if (M5.BtnC.wasPressed()) {
      Serial.print('C');
      setFineMode(FINE_DISABLE);
    }
    int ec_dir = getValue();
    if (ec_dir != DIR_NONE) {
      if (M5.BtnC.isPressed()) {
        if (ec_dir == DIR_CW) {
          brightness -= 16;
          if (brightness < 0) {
            brightness = 0;
          }
          lcd.setBrightness(brightness);
        } else {
          brightness += 16;
          if (brightness > 255) {
            brightness = 255;
          }
          lcd.setBrightness(brightness);
        }
      } else {
        setNextPhase(ec_dir);
        drawPos(last_pos, color_bg, 1);
        drawPos(cur_pos, color_fg, 1);
        sendCommand(ec_dir);
      }
    }
    if (wasPressed()) {
      if (EncoderI2CAddr != I2C_ADDR_PLUS) {
        for (int i = 0; i < 12; i++) {
          setLed(i, 0x5, 0x0, 0x0);
        }
      }
    } else if (wasReleased()) {
      Serial.print('X');
      if (EncoderI2CAddr != I2C_ADDR_PLUS) {
        for (int i = 0; i < 12; i++) {
          setLed(i, 0x0, 0x0, 0x0);
        }
      }
      setFineMode(FINE_TOGGLE);
    }
  }
}
