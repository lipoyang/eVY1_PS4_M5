#include <Arduino.h>
#include <PS4Controller.h>
#include "note.h"

// M5StackのUI処理(画面表示とボタン操作)
void DisplayUI_begin();
void DisplayUI_loop(int octave, int key12, int vol);
void DipslayUI_error(const char* error);

// 設定 (UIで変更可能)
int master_vol = 32 ;    // マスター音量
int tone_no = 0;         // 音色
int scale = 0;           // 音階(ハ長調からどれだけ上がるか下がるか)

// ドレミファソラシドの歌詞文字列 (Vocaloidの発音記号)
static const char* DoReMi[] 
  = {"d o",    "d e", "4 e", "4' i", "m' i", "p\\ a", 
     "p\\' i", "s o", "s a", "4 a",  "tS i", "S i"};
// 発声中のキー
static int key1 = NOTE_C4;
static int key2 = NOTE_C4;
static int key3 = NOTE_C4;

// MIDIメッセージ送信 (パラメータ1バイト)
static void sendMidiMessage(int cmd, int d1)
{
  Serial2.write(cmd);
  Serial2.write(d1);
}

// MIDIメッセージ送信 (パラメータ2バイト)
static void sendMidiMessage(int cmd, int d1, int d2)
{
  Serial2.write(cmd);
  Serial2.write(d1);
  Serial2.write(d2);
}

// MIDIメッセージ送信 (歌詞)
void sendLylic(const char* lylic)
{
  // ヘッダ
  Serial2.write(0xF0);
  Serial2.write(0x43);
  Serial2.write(0x79);
  Serial2.write(0x09);
  Serial2.write((uint8_t)0x00);
  Serial2.write(0x50);
  Serial2.write(0x10);
  // 歌詞
  Serial2.write(lylic);
  // フッタ
  Serial2.write((uint8_t)0x00);
  Serial2.write(0xF7);
}

// 初期化
void setup()
{
  // UIの初期化
  DisplayUI_begin();
//Serial.begin(115200); // ← DisplayUI_begin()を実行する場合には不要

  // DualShock4の初期化
  PS4.begin();

  // Serial2のTX(17)をMIDI送信に使用
  // MIDIのボーレートは31250固定
  Serial2.begin(31250);
  // eVY1の初期化を待つ
  delay(5000);

  // 音色の設定 (初期値ピアノ)
  sendMidiMessage(0xC1, 0);
  sendMidiMessage(0xC2, 0);
  sendMidiMessage(0xC3, 0);
  
  Serial.println("start!");
}

// メインループ
void loop()
{
  static int octave = 0;  // オクターブ
  static int key12 = 0;   // 12音音階番号
  static int vol = 0;     // 音量
  static int tone_prev = -1; // 前回の音色

  // DualShock4が接続されている場合
  if (PS4.isConnected())
  {
    // ボタンの状態を取得
    static ps4_button_t button_prev = {0};
    ps4_button_t button = PS4.getButton();
    int key = -1;
    int semitone = 0;
    if (!button_prev.down     && button.down)     key = NOTE_C4;
    if (!button_prev.left     && button.left)     key = NOTE_D4;
    if (!button_prev.right    && button.right)    key = NOTE_E4;
    if (!button_prev.up       && button.up)       key = NOTE_F4;
    if (!button_prev.cross    && button.cross)    key = NOTE_G4;
    if (!button_prev.square   && button.square)   key = NOTE_A4;
    if (!button_prev.circle   && button.circle)   key = NOTE_B4;
    if (!button_prev.triangle && button.triangle) key = NOTE_C5;
    if (button.options) key = -2;
    int octaveUpDown = 0;
    if (button.l1) octaveUpDown = -1;
    if (button.r1) octaveUpDown =  1;
    if (button.l3) semitone = -1;
    if (button.r3) semitone =  1;
    button_prev = button;

    // ノートオフするか？
    if(key == -2)
    {
      sendMidiMessage(0x80,key1, 0x7f);
      sendMidiMessage(0x81,key1, 0x7f);
      sendMidiMessage(0x82,key2, 0x7f);
      sendMidiMessage(0x83,key3, 0x7f);
      vol = 0;
    }
    // ノートオンするか？
    if(key >= 0)
    {
      // まずノートオフ
      sendMidiMessage(0x80,key1, 0);
      sendMidiMessage(0x81,key1, 0);
      sendMidiMessage(0x82,key2, 0);
      sendMidiMessage(0x83,key3, 0);
      delay(10);

      // キーの計算
      key1 = key + semitone + octaveUpDown * 12 + scale;
      key2 = key1 + 4; // 長3和音(長3度)
      key3 = key1 + 7; // 長3和音(完全5度)
      int velocity = (master_vol > 0) ? master_vol * 4 - 1 : 0;
      int velocity_chord = velocity * 3 / 4;

      key12 = key1 % 12;
      octave = key1 / 12 - 1;
      vol = 0x7f;
     
      // 歌詞送信
      Serial.println(DoReMi[key12]);
      sendLylic(DoReMi[key12]);
      delay(10);

      // ノートオン
      if(tone_no <= 1) sendMidiMessage(0x90,key1,velocity);
      if(tone_no == 1 || tone_no == 2) {
        sendMidiMessage(0x91,key1,velocity_chord);
        sendMidiMessage(0x92,key2,velocity_chord);
        sendMidiMessage(0x93,key3,velocity_chord);
      }
      if(tone_no >= 3) sendMidiMessage(0x91,key1,velocity);
    }
  }
  // UIの処理
  DisplayUI_loop(octave, key12, vol);
  if(tone_no != tone_prev)
  {
    // 音色の設定
    static const int TONE_TABLE[] 
      = {0, 0, 0, 0, 19, 81}; // 0:ピアノ, 19:パイプオルガン, 81:シンセ
    sendMidiMessage(0xC1, TONE_TABLE[tone_no]);
    tone_prev = tone_no;
  }
  delay(10);
}

