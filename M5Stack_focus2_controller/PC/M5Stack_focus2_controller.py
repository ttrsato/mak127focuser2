########################################################################################
#
# ATOM_focuser2_controller.py
#
# Copyright (c) 2020 Tatsuro Sato
# Released under the MIT license
# https://opensource.org/licenses/mit-license.php
#
# This scripts needs pySerial module
# Install pySerial as follows,
# % pip install pyserial
#
########################################################################################
#
# This script controlls M5Stack ATOM Stepper Motor Driver Development Kit (DRV8825).
# ATOM firmware for SkyWatcher MAK127SP forcuser is distribued with this script.
# Confirmed with Windows10 and macOS Catalina
#
########################################################################################

import sys
import os
import tkinter as tk
from tkinter import ttk
from tkinter import *
from tkinter.ttk import *
import math
import platform
import serial
from serial.tools import list_ports
import json
from collections import OrderedDict

ser = None

DISP_W = 320
DISP_H = 200 #240
CIR_R = 195 / 2
V_STEP = 28
H_OFST = 115
H_OFS = 70
V_OFS = 72
V_BASE = 70

mark1 = 0
mark2 = 0
cur_pos = 0
last_pos = 0
step_round_org = 200
step_ustep = 32
step_round = step_round_org * step_ustep
step_time_org = 1
step_times = 1
step_angle = 360.0 / step_round

ports = list_ports.comports()   # Get port list

step_dic = {
  "Full (LLLX)" : 1,
  "Half (LLHX)" : 2,
  "1/4  (LHLX)" : 4,
  "1/8  (LHHX)" : 8,
  "1/16 (HLLX)" : 16,
  "1/32 (HHHX)" : 32,
}

step_rev_dic = {
  1  : 0,
  2  : 1,
  4  : 2,
  8  : 3,
  16 : 4,
  32 : 5,
}

color_fg = "#ff0000"
color_bg = "#000000"

json_data = {'ver': 1.0, 'spr' : 200, 'ustep' : 32}

work_dir = os.path.dirname(__file__)
# json_file = work_dir + '/atom_fctrl2.json'
json_file = work_dir + 'atom_fctrl2.json'

# JSON file check
if os.path.exists(json_file):
  print("JSON file!")
  with open(json_file) as f:
    json_data = json.load(f)
    f.close()
else:
  print("Create JSON")
  with open(json_file, 'w') as f:
    json.dump(json_data, f, indent=4)
    f.close()

step_round_org = int(json_data['spr'])
step_ustep = int(json_data['ustep'])
step_times = step_ustep
step_round = step_round_org * step_ustep
step_angle = 360.0 / step_round

#
# GUI設定
#
root = tk.Tk()
root.title(u"ATOM focuser2 controller")
root.geometry(str(DISP_W) + "x" + str(DISP_H + 100))

count = 0

isMac = False

def moveOneStep(dir):
  if ser != None:
    data = step_times * dir
    packet = bytearray(5)
    packet[0] = 0x01
    packet[1] = (data & 0x00FF) >> 0
    packet[2] = (data & 0xFF00) >> 8
    packet[3] = step_ustep
    packet[4] = (packet[0] + packet[1] + packet[2]) & 0xFF
    ser.write(packet)
    #rcv = ser.read(1)

def mouse_wheel(event):
    global count, cur_pos
    # respond to Linux or Windows wheel event
    if isMac:
      if event.delta > 0:
          count += step_times
          if count >= step_round:
              count = 0
          drawPos(count, '#ff0000')
          moveOneStep(1)
      if event.delta < 0:
          count -= step_times
          if count < 0:
              count = step_round - 1
          drawPos(count, '#ff0000')
          moveOneStep(-1)
    else:
      if event.num == 5 or event.delta == -120:
          count += step_times
          if count >= step_round:
              count = 0
          drawPos(count, '#ff0000')
          moveOneStep(1)
      if event.num == 4 or event.delta == 120:
          count -= step_times
          if count < 0:
              count = step_round - 1
          drawPos(count, '#ff0000')
          moveOneStep(-1)
    count = count - (count % step_times)
    cur_pos = count * step_angle
    canvas.delete("cpos_txt")
    canvas.create_text(DISP_W - H_OFST + 50, V_BASE + 45, text=str(cur_pos), font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW, tag="cpos_txt")
    # print(count)

# Change step
def setFine():
  global step_times
  if vrb.get() == "ON":
    step_times = 1
  else:
    step_times = step_time_org * step_ustep
  dispStepAngle()

def dispStepAngle():
    canvas.delete("sangle_txt")
    canvas.create_text(DISP_W - H_OFST + 50, V_BASE + 10, text=str(step_angle * step_times), font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW, tag="sangle_txt")

def changeStep():
    global step_times, step_time_org, step_ustep, vrb
    vrb.set("OFF")
    if step_time_org == 1:
        step_time_org = 5
    elif step_time_org == 5:
        step_time_org = 10
    elif step_time_org == 10:
        step_time_org = 1
    step_times = step_time_org * step_ustep
    button_step['text'] = 'x' + str(step_time_org)
    dispStepAngle()

def drawPos(pos, color, fill = False):
  global step_times
  center_x = CIR_R
  center_y = CIR_R - 2
  angle = float(pos) * float(step_angle)# / float(step_times)
  # print(angle)
  sx0 = math.cos(math.radians(angle - 90))
  sy0 = math.sin(math.radians(angle - 90))
  sx1 = math.cos(math.radians(angle + 175 - 90))
  sy1 = math.sin(math.radians(angle + 175 - 90))
  sx2 = math.cos(math.radians(angle + 185 - 90))
  sy2 = math.sin(math.radians(angle + 185 - 90))
  x0 = sx0 * (CIR_R - 25) + center_x
  y0 = sy0 * (CIR_R - 25) + center_y
  x1 = sx1 * (CIR_R - 25) + center_x
  y1 = sy1 * (CIR_R - 25) + center_y
  x2 = sx2 * (CIR_R - 25) + center_x
  y2 = sy2 * (CIR_R - 25) + center_y
  canvas.delete("triangle")
  canvas.create_polygon(x0, y0, x1, y1, x2, y2, fill = color, tag = "triangle")

def drawCircle(clr):
  center_x = CIR_R
  center_y = CIR_R # - 2
  if clr == 0:
    canvas.create_oval(center_x - (CIR_R - 1), center_y - (CIR_R - 1), center_x + (CIR_R - 1), center_y + (CIR_R - 1), fill=color_fg)
    canvas.create_oval(center_x - (CIR_R - 9), center_y - (CIR_R - 9), center_x + (CIR_R - 9), center_y + (CIR_R - 9), fill=color_bg)

  for i in range(0, 360, 30):
    sx  = math.cos(math.radians(i - 90))
    sy  = math.sin(math.radians(i - 90))
    x0 = sx * (CIR_R - 6)  + center_x
    y0 = sy * (CIR_R - 6)  + center_y
    x1 = sx * (CIR_R - 20) + center_x
    y1 = sy * (CIR_R - 20) + center_y
    canvas.create_line(x0, y0, x1, y1, fill=color_fg)
  # for i in range(int(step_round / step_times)):
  for i in range(int(step_round)):
    deg = i * step_angle
    sx  = math.cos(math.sin(deg - 90))
    sy  = math.sin(math.sin(deg - 90))
    x0 = sx * (CIR_R - 18) + center_x
    y0 = sy * (CIR_R - 18) + center_y
    # Draw minute markers
    canvas.create_line(x0, y0, x0, y0, fill=color_fg)
  drawPos(cur_pos, color_fg, 1)

if platform.system() == "Darwin":
  isMac = True

frame0 = ttk.Frame(root)
frame0.pack(side=tk.TOP)

def updatePorts():
  global ports
  ports = list_ports.comports()   # Get port list
  devices = [info.device for info in ports]
  combo_port["values"] = devices

label0 = ttk.Label(frame0, text="Port")
label0.pack(side=tk.LEFT)
combo_port = ttk.Combobox(frame0, postcommand=updatePorts)
combo_port.pack(side=tk.LEFT)

def openClose():
  global ser, button0
  if combo_port.get() != "":
    if ser == None:
      ser = serial.Serial(combo_port.get(), 115200, timeout = 0.2)
      if ser != None:
        button0["text"] = "Close"
        print("Open:")
        print(combo_port.get())
    else:
      print("Close")
      ser.close()
      ser = None
      button0["text"] = "Open"

print(ser)

button0 = ttk.Button(frame0, text="Open", command=openClose)
button0.pack(side=tk.RIGHT)

canvas = tk.Canvas(root, width = DISP_W, height = DISP_H)
canvas.create_rectangle(0, 0, DISP_W, DISP_H, fill = color_bg)

step_round_org_txt = str(step_round_org)
step_ustep_txt = str(step_ustep)

drawCircle(0)
canvas.pack(side=tk.TOP)

canvas.create_rectangle(DISP_W - H_OFST - 8, 2, DISP_W, 35, fill=color_fg)
canvas.create_text(DISP_W - H_OFST, 12, text="Focus2", font = ('FixedSys', 12), fill=color_bg, anchor=tk.W)
canvas.create_text(DISP_W - H_OFST, 27, text="controller", font = ('FixedSys', 12), fill=color_bg, anchor=tk.W)
canvas.create_text(DISP_W - H_OFST + 1, 12, text="Focus2", font = ('FixedSys', 12), fill=color_bg, anchor=tk.W)
canvas.create_text(DISP_W - H_OFST + 1, 27, text="controller", font = ('FixedSys', 12), fill=color_bg, anchor=tk.W)

canvas.create_text(DISP_W - H_OFST, V_BASE, text="Step", font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW)
canvas.create_text(DISP_W - H_OFST, V_BASE + 15, text="Angle", font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW)
# canvas.create_text(DISP_W - H_OFST + 50, V_BASE + 10, text=str(step_angle), font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW, tag="sangle_txt")
dispStepAngle()

canvas.create_text(DISP_W - H_OFST, V_BASE + 35, text="Cur", font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW)
canvas.create_text(DISP_W - H_OFST, V_BASE + 51, text="Pos", font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW)
canvas.create_text(DISP_W - H_OFST + 50, V_BASE + 45, text=str(cur_pos), font = ('FixedSys', 10), fill=color_fg, anchor=tk.NW, tag="cpos_txt")

frame1 = ttk.Frame(root)
frame1.pack(side=tk.TOP)
label1 = ttk.Label(frame1, text="S/R:")
label1.pack(side=tk.LEFT)
entry1 = ttk.Entry(frame1, textvariable=step_round_org_txt, width=4)
entry1.pack(side=tk.LEFT)
entry1.insert(0, step_round_org_txt)

combo_ustep = ttk.Combobox(frame1, textvariable=step_ustep_txt, width=12)
combo_ustep["values"] = list(step_dic.keys())
combo_ustep.current(step_rev_dic[step_ustep])
combo_ustep.pack(side=tk.LEFT, padx=5)

def setMotor():
  global step_round, step_angle, step_round_org, step_ustep, step_dic
  step_round_org = int(entry1.get())
  step_ustep = int(step_dic[combo_ustep.get()])
  step_round = step_round_org * step_ustep
  step_angle = 360.0 / step_round
  json_data['spr'] = int(step_round_org)
  json_data['ustep'] = int(step_ustep)
  print('Save JSON')
  print(step_round_org)
  print(step_ustep)
  with open(json_file, 'w') as f:
    json.dump(json_data, f, indent=4)
    f.close()

button_set = ttk.Button(frame1, text="Set", command=setMotor)
button_set.pack(side=tk.RIGHT)

# Step button
frame2 = ttk.Frame(root)
frame2.pack(side=tk.TOP)

button_step = ttk.Button(frame2, text=u'x1',width=15, command=changeStep)
button_step.pack(side=tk.LEFT, pady=10)

framerb = ttk.LabelFrame(frame2, text="Fine")
framerb.pack(side=tk.LEFT, padx=10)
vrb = StringVar()
rb_on = ttk.Radiobutton(
    framerb,
    text='ON',
    value='ON',
    variable=vrb,
    command=setFine)
rb_on.pack(side=tk.LEFT)

rb_off = ttk.Radiobutton(
    framerb,
    text='OFF',
    value='OFF',
    variable=vrb,
    command=setFine)
rb_off.pack(side=tk.LEFT)
vrb.set('OFF')

setMotor()

# with Windows / Mac OS
root.bind("<MouseWheel>", mouse_wheel)
# with Linux OS
root.bind("<Button-4>", mouse_wheel)
root.bind("<Button-5>", mouse_wheel)

root.mainloop()
