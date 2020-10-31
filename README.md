# mak127focuser2
M5Stack/ATOM MAK127 forcuser controller

# DEMO
 
[![](https://img.youtube.com/vi/nWg7AWjUTGg/0.jpg)](https://www.youtube.com/watch?v=nWg7AWjUTGg)

# Features

SkyWatcher の MAK127SP の電動フォーカサーです。
ピントが合わせられるだけ。と言えばそれまでですが。。
 
# Requirement

* MAK127SP (SkyWatcher)
* M5Stack Faces + Encoder panel
* M5Stack Basic + PLUS Encoder module
* Stepper Motor Driver Development Kit (DRV8825)

M5Stack Faces と M5Stac Basic はいずれか一つでいいです。
また PC 版の Python script を動作させるためには、
 
* Python3
* PySerial

が必要です。

# Installation

* CP2104 のドライバーを下記からダウンロードしてインストールします。
https://m5stack.com/pages/download

* M5Stack をコントローラとして使う場合
M5Stack と Stepper Motor Driver Development Kit の ATOM lite に Arduino IDE 環境を使ってプログラムをダウンロードします。
ライブラリ LovyanGFX と ESP32_I2C_Slave が必要です。

* PC をコントローラとして使う場合
バイナリ版と Python スクリプト版があります。 
M5Stack_focus2_controller/PC 以下に Python スクリプトから作成した Windows または Mac 用のバイナリがあるので、ダブルクリックすると実行できます。特にインストール作業は必要ありません。
M5Stack_focus2_controller.py を実行するには、Python3 と PySerial をインストールする必要があります。

PySerial は以下のようにしてインストールします。

```bash
pip install pyserial
```
 
# Usage

ブログの方に書いてます。
https://ttrsato.blogspot.com/2020/11/mak127sp-focuser-ver2.html
  
# Author
 
https://twitter.com/ttrsato
 
# License
 
* mak127forcuser2 is under [MIT license](https://en.wikipedia.org/wiki/MIT_License).


