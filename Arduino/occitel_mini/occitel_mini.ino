/*
Copyright (c) 2010 Myles Metzer
Copyright (c) 2015-2020 Avamander
Copyright (c) 2013-2018 Grant Searle
Copyright (c) 2020-2025 Association Silicium

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
--------------------------------------------------------------------------------
Occitel (1976) by "Société Occitane Electronique"
emulated on Sparkfun Pro Micro by asso Silicium

  ####    ####    ####      #     #####  ######  #
 #    #  #    #  #    #     #       #    #       #
 #    #  #       #          #       #    #####   #
 #    #  #       #          #       #    #       #
 #    #  #    #  #    #     #       #    #       #
  ####    ####    ####      #       #    ######  ######

         #####    #   #
         #    #    # #
         #####      #
         #    #     #
         #    #     #
         #####      #

                  ####      #    #          #     ####      #    #    #  #    #
                 #          #    #          #    #    #     #    #    #  ##  ##
                  ####      #    #          #    #          #    #    #  # ## #
                      #     #    #          #    #          #    #    #  #    #
                 #    #     #    #          #    #    #     #    #    #  #    #
                  ####      #    ######     #     ####      #     ####   #    #

Based on Grant Searle's AVRPong http://searle.x10host.com/AVRPong/index.html
Kudos to Grant for his most excellent reverse engineering & code.
Kudos to Myles Metzler (Metzer?) for his brilliant TV Out library.
Thanks to Avamander for ATmega32U4 support on Leonardo.

All changes (c)2020,2021,2022,2023,2024,2025 asso Silicium under MIT License:
- merge of ATmega32U4 support from 2020 version of the TV Out library
- video pins reassigned for our carrier board
- audio reassigned to OC1B because of PB7 absent on Sparkfun Pro Micro
- game difficulty selectors matching the Occitel console by SOE in 1976
- shutdown mode: game selector on OFF stops video out
- good-health blinkenlights so as to tell if the software has crashed/frozen
- choice of 1976 or 1977 AY-3-8500 silicon by long press of reset button
- interactive "debugger" on the serial console (compile-time) ⚠︎ unreliable! ⚠︎
- delay rather than blocking wait on opening serial console, compatible with
  connection to Arduino IDE or just a USB power supply
- refactor, reformat for listing density
- tab->space using Vim ":set ts=2|set et|retab"
- bug fixes:
  - crash due to non-atomic pointer shared between background and interrupt
  - missing `volatile`
  - `&` instead of `&&`
  - operator precedence

Changelog:
- 2020-Nov-28 vid ok
- 2020-Nov-28 fixed audio for sparkfun pro micro
- 2020-Nov-29 reboot before frame counter overflow
- 2020-Nov-29 option for static allocation of framebuffer
- 2020-Dec-05 manual serve long press to switch AY-3-8500 versions
- 2020-Dec-30 fixed football paddle<->pot interleaved time-consuming
- 2021-Jan-01 copyright notice
- 2021-Aug-26 pinout documentation
- 2022-Dec-20 frame counter graceful uint overflow, placeholder for easter egg
- 2023-Jan-28 fixes and serial console improvements
- 2023-May-28 fix 1976/1977 toggle not resetting long-press counter
- 2025-Jan-11 cleanup

Initial AY-3-8500 emulation by Grant Searle published as source code added to
the source code file of arduino-tvout (published by Myles Metzler under MIT
license). It is obvious which parts Grant added for his game engine. Which
parts of arduino-tvout he modified can be determined by diff'ing and anyway
he left the initial MIT license to apply to the combined works.

Here is Grant Searle's original notice:
--------------------------------------------------------------------------------
1970's TV game recreation by Grant Searle 2013
Emulation of the PAL version of the AY-3-8500 that was used in many TV games from that era

current web page is http://searle.hostei.com/grant/AVRPong/index.html
If this page is not found (moved etc) then search for "Grant Searle" using a search engine such as Google
to find my electronics page. On there will be a link to this page

Top half of this file is my code to recreate the 4 paddle games that were on the game consoles.
Second half is a cut-down and modified version of the TV Out library (http://code.google.com/p/arduino-tvout/)
--------------------------------------------------------------------------------

end of transmission */

#include <avr/wdt.h>

// Bitmap for the edges - faster than setting individual pixels
PROGMEM const unsigned char edge[] = {
  0b10101010, 0b10101010, 0b10101010, 0b10101010 };

// Font bitmap for the 7 segment on-screen scoring display
#define FONT_W 8
#define FONT_H_PAL 15
PROGMEM const unsigned char fontPAL[] = {
  0b11111100, 0b11111100, 0b11111100, 0b11001100, 0b11001100,
  0b11001100, 0b11001100, 0b11001100, 0b11001100, 0b11001100,
  0b11001100, 0b11001100, 0b11111100, 0b11111100, 0b11111100, // 0

  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 1

  0b11111100, 0b11111100, 0b11111100, 0b00001100, 0b00001100,
  0b00001100, 0b11111100, 0b11111100, 0b11111100, 0b11000000,
  0b11000000, 0b11000000, 0b11111100, 0b11111100, 0b11111100, // 2

  0b11111100, 0b11111100, 0b11111100, 0b00001100, 0b00001100,
  0b00001100, 0b11111100, 0b11111100, 0b11111100, 0b00001100,
  0b00001100, 0b00001100, 0b11111100, 0b11111100, 0b11111100, // 3

  0b11001100, 0b11001100, 0b11001100, 0b11001100, 0b11001100,
  0b11001100, 0b11111100, 0b11111100, 0b11111100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 4

  0b11111100, 0b11111100, 0b11111100, 0b11000000, 0b11000000,
  0b11000000, 0b11111100, 0b11111100, 0b11111100, 0b00001100,
  0b00001100, 0b00001100, 0b11111100, 0b11111100, 0b11111100, // 5

  0b11111100, 0b11111100, 0b11111100, 0b11000000, 0b11000000,
  0b11000000, 0b11111100, 0b11111100, 0b11111100, 0b11001100,
  0b11001100, 0b11001100, 0b11111100, 0b11111100, 0b11111100, // 6

  0b11111100, 0b11111100, 0b11111100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 7

  0b11111100, 0b11111100, 0b11111100, 0b11001100, 0b11001100,
  0b11001100, 0b11111100, 0b11111100, 0b11111100, 0b11001100,
  0b11001100, 0b11001100, 0b11111100, 0b11111100, 0b11111100, // 8

  0b11111100, 0b11111100, 0b11111100, 0b11001100, 0b11001100,
  0b11001100, 0b11111100, 0b11111100, 0b11111100, 0b00001100,
  0b00001100, 0b00001100, 0b11111100, 0b11111100, 0b11111100, }; // 9
#define FONT_H_NTSC 10
PROGMEM const unsigned char fontNTSC[] = {
  0b11111100, 0b11111100, 0b11001100, 0b11001100, 0b11001100,
  0b11001100, 0b11001100, 0b11001100, 0b11111100, 0b11111100, // 0

  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 1

  0b11111100, 0b11111100, 0b00001100, 0b00001100, 0b11111100,
  0b11111100, 0b11000000, 0b11000000, 0b11111100, 0b11111100, // 2

  0b11111100, 0b11111100, 0b00001100, 0b00001100, 0b11111100,
  0b11111100, 0b00001100, 0b00001100, 0b11111100, 0b11111100, // 3

  0b11001100, 0b11001100, 0b11001100, 0b11001100, 0b11111100,
  0b11111100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 4

  0b11111100, 0b11111100, 0b11000000, 0b11000000, 0b11111100,
  0b11111100, 0b00001100, 0b00001100, 0b11111100, 0b11111100, // 5

  0b11111100, 0b11111100, 0b11000000, 0b11000000, 0b11111100,
  0b11111100, 0b11001100, 0b11001100, 0b11111100, 0b11111100, // 6

  0b11111100, 0b11111100, 0b00001100, 0b00001100, 0b00001100,
  0b00001100, 0b00001100, 0b00001100, 0b00001100, 0b00001100, // 7

  0b11111100, 0b11111100, 0b11001100, 0b11001100, 0b11111100,
  0b11111100, 0b11001100, 0b11001100, 0b11111100, 0b11111100, // 8

  0b11111100, 0b11111100, 0b11001100, 0b11001100, 0b11111100,
  0b11111100, 0b00001100, 0b00001100, 0b11111100, 0b11111100, }; // 9

// Sparkfun Pro Micro has: D0..10,14..16 A0..3
// D0 and D1 are labelled TXI and TXO, A6..9 muxed with D4,6,8,9
// D8 = VID, D9 = SYNC, D3 = AUDIO, A0 = left paddle, A1 = right paddle
// available pins for selectors: A2,3 D0..2,4..7,10,14..16
// 1P5T "ARRET"/"Tennis"/"Foot-ball"/"Squash"/"Exercice" = (nc), SEL_...
// 1P3T "Professionnel"/"Amateur"/"Débutant" = SEL_PRO, (nc), SEL_NOOB
// PRO = small paddles, fast ball; NOOB = small, slow; none = large, fast
// SEL_PAL_NTSC open = PAL, gnd = NTSC
// SEL_VERSION open = 1976, gnd = 1977
// PUSH_* are gnd/open; PUSH_RESET is RàZ, not ATmega reboot
// PUSH_SERVE to gnd triggers ball service => tied to gnd is auto-serve
// pins D15 D16 A2 A3 are available (can do XY joysticks)
#define LEFT_POTENTIOMETER A0
#define RIGHT_POTENTIOMETER A1
#define SEL_PAL_NTSC 14
//#define SEL_VERSION ?? -- no GPIO left, use long press of RàZ
#define SEL_PRO 0
#define SEL_NOOB 1
#define PUSH_SERVE 2
#define PUSH_RESET 10
#define SEL_TENNIS 4
#define SEL_FOOTBALL 5
#define SEL_SQUASH 6
#define SEL_EXERCISE 7

// SparkFun Pro Micro LEDs are sw-controlled by USB Serial, can be overdriven
#define LED_RX 17
#define LED_TX 30

// fast boot, for production build
#define SERIAL_USB_WAIT_DELAY_MS 1
// MacOS usually is fast enough to connect with delay=1.5s
//#define SERIAL_USB_WAIT_DELAY_MS 1500
#define MSG(STR) Serial.println(F(STR))
#define DUMP(VAR) do { Serial.print(F(#VAR"=")); Serial.println(VAR); } while(0)
//#define DEBUG_CONSOLE
// this is disabled by default because USB Serial.print() hangs after a while
// unclear if this is the same bug that others see
// or due to highjacking hardware timers from the Arduino run-time

// options
int8_t gameNumber;      // -1=??? 0=off 1=Tennis 2=Football 3=Squash 4=Solo
bool chipPAL, chip1976;
uint8_t paddleHeight, bigAngles, ballSpeed; // difficulty

// options state
uint8_t fieldHeight, soccerEdgeHeight; // from chipPAL
uint8_t ballWidth, edgeWidth, fieldWidth, leftDigX, centreX, rightDigX,
 bat1X, bat2X, bat3X, bat4X, bat5X; // from chip1976
// game state
uint8_t longPress, scoreLeft, scoreRight, squashActivePlayer;
bool ballVisible, attractMode, served;
int8_t ballXInc, ballYInc, ballX, ballY; int16_t ballYhires;
uint32_t loops; // time step unspecified (1 or 2 VBL depending on game speed)

// from TVout.h
#define WHITE 1
#define BLACK 0
extern uint8_t framebuffer[], framebufferDblRes[];

#define FB_W 80
#define FB_H_PAL 116
#define FB_H_NTSC 97
#define FB_H_HIRES 5
#define BPR (FB_W/8)

void setup() {
  Serial.begin(115200); Serial.setTimeout(0); // backwards compat with Uno
  while(!Serial && millis()<SERIAL_USB_WAIT_DELAY_MS) (void)0; // because Leonardo
  MSG("Occitel emulator for Sparkfun Pro Micro 5V/16MHz "
      "(c)2020,2021,2022,2023,2024,2025 asso Silicium");

  pinMode(LED_RX, OUTPUT); pinMode(LED_TX, OUTPUT); loops = 0;
  pinMode(SEL_PAL_NTSC, INPUT_PULLUP);
  pinMode(SEL_PRO, INPUT_PULLUP); pinMode(SEL_NOOB, INPUT_PULLUP);
  pinMode(SEL_TENNIS, INPUT_PULLUP); pinMode(SEL_FOOTBALL, INPUT_PULLUP);
  pinMode(SEL_SQUASH, INPUT_PULLUP); pinMode(SEL_EXERCISE, INPUT_PULLUP);
  pinMode(LEFT_POTENTIOMETER, INPUT); pinMode(RIGHT_POTENTIOMETER, INPUT);
  pinMode(PUSH_RESET, INPUT_PULLUP); pinMode(PUSH_SERVE, INPUT_PULLUP);
  chip1976 = true; longPress = 0; gameNumber = 0; ballSpeed = 1; initGame();
  if(digitalRead(SEL_PAL_NTSC)==HIGH) { // PAL -> AY-3-8500
    fieldHeight = FB_H_PAL-1; soccerEdgeHeight = 25; chipPAL = true;
    initVideo(2, 10, FB_W, FB_H_PAL); }
  else { // NTSC -> AY-3-8500-1
    fieldHeight = FB_H_NTSC-1; soccerEdgeHeight = 21; chipPAL = false;
    initVideo(3, 5, FB_W, FB_H_NTSC); }}

void loop() {
  if(loops%64<32) { digitalWrite(LED_RX, HIGH); digitalWrite(LED_TX, LOW); }
  else { digitalWrite(LED_RX, LOW); digitalWrite(LED_TX, HIGH); }
#ifdef DEBUG_CONSOLE
  switch(Serial.read()) {
    case 'v':
      MSG("Occitel emulator for Sparkfun Pro Micro 5V/16MHz "
          "(c)2020,2021,2022,2023,2024,2025 asso Silicium"); break;
    case 'h':
      MSG("'r': reboot\r\n'o': dump options\r\n"
          "'s': dump settings\r\n'd': dump state"); break;
    case 'o': DUMP(gameNumber); DUMP(chipPAL); DUMP(chip1976);
      DUMP(paddleHeight); DUMP(bigAngles); DUMP(ballSpeed); break;
    case 's': DUMP(fieldHeight); DUMP(soccerEdgeHeight); DUMP(ballWidth);
      DUMP(edgeWidth); DUMP(fieldWidth); DUMP(leftDigX); DUMP(centreX);
      DUMP(rightDigX); DUMP(bat1X); DUMP(bat2X); DUMP(bat3X); DUMP(bat4X);
      DUMP(bat5X); break;
    case 'd': DUMP(loops); DUMP(longPress); DUMP(scoreLeft); DUMP(scoreRight);
      DUMP(squashActivePlayer); DUMP(ballVisible); DUMP(attractMode);
      DUMP(served); DUMP(ballXInc); DUMP(ballYInc); DUMP(ballX); DUMP(ballY);
      DUMP(ballYhires); break;
    case 'r': wdt_enable(WDTO_15MS); for(;;); }
#endif
  uint8_t old = gameNumber;
  setChipVersion(); setGameOptions(); setGame(); manualServe();
  if(gameNumber>0) {
    if(gameNumber!=old) videoOn();
    gameLoop(); }
  else if(gameNumber==0 ) {
    if(gameNumber!=old) { vsync(); vsync(); vsync(); videoOff(); initGame(); }}
  else {
    /* easter egg */ }
  ++ loops; }                             // increment at VBL

void initGame() {
  squashActivePlayer = 1; scoreLeft = scoreRight = 0; ballX = 0; served = false;
  ballYhires = -18; // start outside to delay serving after reset
  ballXInc = ballYInc = ballSpeed; attractMode = ballVisible = true; }

void manualServe() {
  if(digitalRead(PUSH_SERVE)==LOW) served = true; }

void setGame() {
  if(digitalRead(SEL_TENNIS)==LOW) gameNumber = 1;
  else if(digitalRead(SEL_FOOTBALL)==LOW) gameNumber = 2;
  else if(digitalRead(SEL_SQUASH)==LOW) gameNumber = 3;
  else if(digitalRead(SEL_EXERCISE)==LOW) gameNumber = 4;
  else gameNumber = 0; }

void setChipVersion() {
  uint8_t p = digitalRead(PUSH_RESET);
  if(p==LOW) { initGame(); if(longPress<50) ++ longPress; }
  else {
    if(longPress==50) chip1976 = !chip1976;
    longPress = 0; }

  if(chip1976) {
    fieldWidth = 79; edgeWidth = 1; ballWidth = 1;
    bat1X = 2; bat2X = 18; bat3X = 58; bat4X = 60; bat5X = 76;
    leftDigX = 21; centreX = 40; rightDigX = 43; }
  else {
    fieldWidth = 74; edgeWidth = 2; ballWidth = 2;
    bat1X = 3; bat2X = 19; bat3X = 52; bat4X = 54; bat5X = 70;
    leftDigX = 19; centreX = 36; rightDigX = 37; }}

void setGameOptions() {
  bigAngles = 1;
  if(digitalRead(SEL_PRO)==LOW) { setFast(); paddleHeight = 7; }
  else if(digitalRead(SEL_NOOB)==LOW) { setSlow(); paddleHeight = 7; }
  else { setFast(); paddleHeight = 14; }}

void setFast() {
  if(ballSpeed==1) {
    ballXInc = ballXInc*2;
    if(ballYInc==-1 || ballYInc==1) {
      ballYInc = ballYInc*2; } // from 1 line to 2 lines per step
    else if(ballYInc<-1) {
      ballYInc = -5; } // from 3 lines to 5 lines per step
    else if(ballYInc>1) {
      ballYInc = 5; } // from 3 lines to 5 lines per step
    ballSpeed = 2; }}

void setSlow() {
  if(ballSpeed==2) { // Switch to lower speed if not already set
    ballXInc = ballXInc/2;
    if(ballYInc==-2 || ballYInc==2) {
      ballYInc = ballYInc/2; } // from 2 lines down to 1 line per step
    else if(ballYInc<-1) {
      ballYInc = -3; } // from 5 lines down to 3 lines per step
    else if(ballYInc>1) {
      ballYInc = 3; } // from 5 lines down to 3 lines per step
    ballSpeed = 1; }}

void gameLoop() {
  vsync(); cls();
  uint8_t leftPaddle = map(analogRead(LEFT_POTENTIOMETER),
                           0, 1023, 2-paddleHeight, fieldHeight-2);
  drawBoundary(); drawScores(); // distance ADC reads for noise reduction
  uint8_t rightPaddle = map(analogRead(RIGHT_POTENTIOMETER),
                            0, 1023, 2-paddleHeight, fieldHeight-2);
  if(gameNumber==1 || gameNumber==2) { // Tennis or Football
    drawPaddle(1, bat1X, leftPaddle);
    drawPaddle(5, bat5X, rightPaddle); }
  if(gameNumber==2) { // Football
    drawPaddle(2, bat2X, rightPaddle);
    drawPaddle(4, bat4X, leftPaddle); }
  else if(gameNumber==3) { // Squash
    drawPaddle(3, bat3X, leftPaddle);
    drawPaddle(4, bat4X, rightPaddle); }
  else if(gameNumber==4) { // Solo
    drawPaddle(3, bat3X, rightPaddle); }

  // Update the ball position
  if(served) {
    ballX = ballX+ballXInc;
    ballYhires = ballYhires+ballYInc; ballY = ballYhires/2;
    if(ballYhires<0) { ballY = (ballYhires-1)/2; }}

  // Check if a boundary has been hit
  if(gameNumber==2 && ballVisible) { // Football - left/right partial wall
    if(ballY<soccerEdgeHeight+2 || ballY>fieldHeight-soccerEdgeHeight-1) {
      if(ballXInc<0 && ballX<=edgeWidth) {
        ballXInc = -ballXInc; sound(500, 32); }
      if(ballXInc>0 && ballX >= fieldWidth-ballWidth-edgeWidth) {
        ballXInc = -ballXInc; sound(500, 32); }}}
  if(gameNumber>=3 && ballVisible) { // Squash or Solo - left full wall
    if(ballXInc<0 && ballX<=edgeWidth) {
      ballXInc = -ballXInc; sound(500, 32); }}

  if(ballXInc>0 && ballX>=fieldWidth) { // ball exited east?
    ballXInc = -ballXInc;
    if(!ballVisible) { ballVisible = true; }
    else {
      sound(2000, 32);
      if(attractMode) {
        served = false;
        if(gameNumber==3) {
          if(squashActivePlayer==1) ++ scoreRight; else ++ scoreLeft; }
        else { ++ scoreLeft; }
        if(scoreLeft>=15 || scoreRight>=15) { attractMode = false; }}
      ballX = fieldWidth*7/10; ballVisible = false; }}
  if(ballXInc<0 && ballX<=-ballWidth) { // ball exited west?
    ballXInc = -ballXInc;
    if(!ballVisible) { ballVisible = true; }
    else {
      sound(2000, 32);
      if(attractMode) {
        ++ scoreRight; served = false;
        if(scoreRight>=15) { attractMode = false; }}
      ballX = fieldWidth*3/10; ballVisible = false; }}

  // Bounce the ball off the top or bottom edge if needed
  if((ballYInc<0 && ballY<2) || (ballYInc>0 && ballY>fieldHeight-4)) {
    ballYInc = -ballYInc; sound(500, 32); }

  if(ballVisible) { drawBall(); }}

void drawPaddle(uint8_t paddleNum, uint8_t paddleX, int8_t val) {
  int8_t y1 = val, y2 = val+paddleHeight;
  if(y1<1) y1 = 1;
  if(y2>fieldHeight-2) y2 = fieldHeight-2;
  drawLine(paddleX, y1, paddleX, y2, WHITE);

  if(ballVisible && attractMode) { // Intercept ball
    if((ballXInc<0 && (ballX==paddleX || ballX == (paddleX+1)))
       || (ballXInc>0 && (ballX==(paddleX-ballWidth)
           || ballX==(paddleX-ballWidth+1)))) {
      if(ballY>=val-1 && ballY<=val+paddleHeight) {
        sound(1000,32);
        if(gameNumber==3) { //Squash
          if(ballXInc<0 || (squashActivePlayer==1 && paddleNum!=4) || (squashActivePlayer==2 && paddleNum!=3)) {
            // bat not active or passing through the left bat, don't intercept
            return; }
          if(ballXInc>0 && (squashActivePlayer==1 && paddleNum==4)) {
            ballXInc = -ballXInc; squashActivePlayer = 2; }
          if(ballXInc>0 && (squashActivePlayer==2 && paddleNum==3)) {
            ballXInc=-ballXInc; squashActivePlayer=1; }}
        else if(gameNumber==4) { //Solo
          if(ballXInc>0) { ballXInc = -ballXInc; squashActivePlayer = 2; } }
        else {
          if(ballXInc<0 && (paddleNum==1 || paddleNum==4)) ballXInc = -ballXInc;
          if(ballXInc>0 && (paddleNum==2 || paddleNum==3 || paddleNum==5))
            ballXInc = -ballXInc; }
        if(bigAngles==1 && ballY<val+paddleHeight/4) {
          if(ballSpeed==1) ballYInc = -3; else ballYInc=-5; }
        else if(ballY<val+paddleHeight/2) {
          ballYInc = -ballSpeed; }
        else if(bigAngles==0 || ballY<val+paddleHeight*3/4) {
          ballYInc = ballSpeed; }
        else {
          if(ballSpeed==1) ballYInc = 3; else ballYInc = 5; }}}}}

void drawBoundary() {
  // Set bytes directly as it is much quicker than "setPixel"
  if(gameNumber==2) { // Goals for Soccer
    if(chip1976) {
      for(uint16_t i=0;i<(soccerEdgeHeight*BPR);i=i+BPR) {
        framebuffer[i] |= 0b10000000; framebuffer[i+BPR-1] |= 0b00000010; }
      for(uint16_t i= (fieldHeight-soccerEdgeHeight)*BPR; i<fieldHeight*10;
          i += BPR) {
        framebuffer[i] |= 0b10000000; framebuffer[i+BPR-1] |= 0b00000010; }}
    else {
      for(uint16_t i = 0; i<soccerEdgeHeight*BPR; i += BPR) {
        framebuffer[i] |= 0b11000000; framebuffer[i+BPR-1] |= 0b11000000; }
      for(uint16_t i = (fieldHeight-soccerEdgeHeight)*BPR; i<fieldHeight*BPR;
          i += BPR) {
        framebuffer[i] |= 0b11000000; framebuffer[i+BPR-1] |= 0b11000000; }}}

  if(gameNumber==3 || gameNumber==4) { //Left wall for Squash or Solo
    if(chip1976) {
      for(uint16_t i = 0; i<fieldHeight*BPR; i += BPR) {
        framebuffer[i] |= 0b10000000; }}
    else {
      for(uint16_t i = 0; i<fieldHeight*BPR; i += BPR) {
        framebuffer[i] |= 0b11000000; }}}

  if(gameNumber==1 || gameNumber==2) { //Centre line for Tennis or Soccer
    uint16_t x = centreX/8; uint8_t m = 0x80>>(centreX&7);
    if(chipPAL) { // PAL - Dotted centre
      if(chip1976) {
        for(uint8_t i = 0; i<fieldHeight-2; i += 4, x += BPR*4) {
          framebuffer[x] = m; framebuffer[x+BPR] = m; }}
      else {
        x += BPR*2;
        for(uint8_t i = 2; i<fieldHeight-2; i += 4, x += BPR*4) {
          framebuffer[x] = m; framebuffer[x+BPR] = m; }}}
    else { // NTSC - solid centre
      if(chip1976) {
        for(uint8_t i = 1; i<fieldHeight; ++ i, x += BPR) {
          framebuffer[x] = 0b10000000; }}
      else {
        for(uint8_t i = 1; i<fieldHeight; ++ i, x += BPR) {
          framebuffer[x] = 0b00001000; }}}}

  // Top and bottom dotted boundary
  uint8_t i1; for( i1 = 0; i1<fieldWidth-8; i1 += 8) {
     bitmap(i1, 0, edge, 8, 1); bitmap(i1, fieldHeight-1, edge, 8, 1); }
  for(uint8_t i2 = i1-8; i2<fieldWidth; i2 += 2) {
    setPixel(i2, 0, WHITE); setPixel(i2, fieldHeight-1, WHITE); }}

void drawScores() {
  uint8_t i = scoreLeft; if(i>9) { drawDigit(leftDigX, 3, 1); i -= 10; }
  drawDigit(leftDigX+FONT_W, 3, i);
  if(gameNumber==4) return; // solo game
  i = scoreRight; if(i>9) { drawDigit(rightDigX, 3, 1); i -= 10; }
  drawDigit(rightDigX+FONT_W, 3, i); }

void drawDigit(uint8_t x, uint8_t y, uint8_t v) {
  if(chipPAL) { bitmap(x, y, fontPAL+v*FONT_H_PAL, 8, FONT_H_PAL); }
  else { bitmap(x, y, fontNTSC+v*FONT_H_NTSC, 8, FONT_H_NTSC); }}

// Draw the ball on a virtual screen area RES_W pixels across by 5 pixels high.
// This area is then displayed (double vertical resolution) instead of the
// normal background for those 5 lines.
static void inline pixBall(uint8_t x, uint8_t y) {
  if(x<80 && y<5) framebufferDblRes[x/8+y*BPR] |= 0x80>>(x&7); }

void drawBall() {
  for(uint8_t x = 0; x<BPR; ++ x) { // Copy the screen to the high res screen
    // Put the appropriate 3 lines that are being overlayed into
    // the double-res buffer
    if(ballYhires%2==0) {
      framebufferDblRes[x+BPR*0] = framebuffer[x+BPR*ballY];
      framebufferDblRes[x+BPR*1] = framebuffer[x+BPR*ballY];
      framebufferDblRes[x+BPR*2] = framebuffer[x+BPR*(ballY+1)];
      framebufferDblRes[x+BPR*3] = framebuffer[x+BPR*(ballY+1)];
      framebufferDblRes[x+BPR*4] = framebuffer[x+BPR*(ballY+2)]; }
    else {
      framebufferDblRes[x+BPR*0] = framebuffer[x+BPR*ballY];
      framebufferDblRes[x+BPR*1] = framebuffer[x+BPR*(ballY+1)];
      framebufferDblRes[x+BPR*2] = framebuffer[x+BPR*(ballY+1)];
      framebufferDblRes[x+BPR*3] = framebuffer[x+BPR*(ballY+2)];
      framebufferDblRes[x+BPR*4] = framebuffer[x+BPR*(ballY+2)]; }}

  // Mask off the sides to produce a dotted boundary (also in renderer)
  if(chip1976) {
    if(ballYhires%2==0) {
      for(uint8_t i = BPR; i<=BPR*3; i += BPR*2) {
        if(gameNumber>1) framebufferDblRes[i] &= 0b01111111;
        if(gameNumber==2) framebufferDblRes[i+BPR-1] &= 0b11111101; }}
    else {
      for(uint8_t i = 0; i<=BPR*4; i += BPR*2) {
        if(gameNumber>1) framebufferDblRes[i] &= 0b01111111;
        if(gameNumber==2) framebufferDblRes[i+BPR-1] &= 0b11111101; }}}
  else {
    if(ballYhires%2==0) {
      for(uint8_t i = BPR; i<=BPR*3; i += BPR*2) {
        if(gameNumber>1) framebufferDblRes[i] &= 0b00111111;
        if(gameNumber==2) framebufferDblRes[i+BPR-1] &= 0b00111111; }}
    else {
      for(uint8_t i = 0; i<=BPR*4; i += BPR*2) {
        if(gameNumber>1) framebufferDblRes[i] &= 0b00111111;
        if(gameNumber==2) framebufferDblRes[i+BPR-1] &= 0b00111111; }}}

  // Draw the ball on the double-resolution screen
  // This will then replace a strip across the screen at the ball
  // position, ensuring a smooth ball travel
  pixBall(ballX, 0); pixBall(ballX, 1); pixBall(ballX, 2);

  if(!chip1976) { // newer AY-3-8500 had a wider and taller ball
    pixBall(ballX, 3); pixBall(ballX, 4);
    pixBall(ballX+1, 0); pixBall(ballX+1, 1); pixBall(ballX+1, 2);
    pixBall(ballX+1, 3); pixBall(ballX+1, 4); }}

////////////////////////////////////////////////////////////////////////////////
// MODIFIED AND REDUCED TV OUT LIBRARY CODE. MODIFICATIONS BY GRANT SEARLE
// http://code.google.com/p/arduino-tvout snapshot circa 2015
// now at https://github.com/Avamander/arduino-tvout
////////////////////////////////////////////////////////////////////////////////
/*
 Copyright (c) 2010 Myles Metzer

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

// timer 1 = video
#define VID_TCNTxL      TCNT1L
#define VID_PIN         4       // pin 8 = port B4
#define VID_PORT        PORTB
#define VID_DDR         DDRB
#define SYNC_PIN        5       // pin 9 = port B5
#define SYNC_PORT       PORTB
#define SYNC_DDR        DDRB
// timer 0 = sound on OC0B (port D0, pin 3) but CTC only on OC0A (port D3) so
// we load OC0B==OC0A because Sparkfun Pro Micro has no pin for port D3
// (ATmega32U4 port D3, not to be mistaken with pin 3, which is port D0)
#define SND_PIN         0       // pin 3 = port D0 = OC0B
#define SND_PORT	      PORTD
#define SND_DDR		      DDRD
#define SND_TCCRxA      TCCR0A
#define SND_TCCRxB      TCCR0B
#define SND_OCRxA       OCR0A
#define SND_OCRxB       OCR0B
#define SND_COMxA0      COM0A0
#define SND_COMxB0      COM0B0
#define SND_WGMx1       WGM01

#define CLK_PER_US    (F_CPU/1000000)

#define HSYNC_NS          4700
#define VSYNC_NS          58850
#define VSYNC_CLK         (VSYNC_NS*CLK_PER_US/1000-1)
#define HSYNC_CLK         (HSYNC_NS*CLK_PER_US/1000-1)

#define NTSC_SCANLINE_NS  63550
#define NTSC_START_NS     12000
#define NTSC_LINES        262
#define NTSC_VSYNC        3
#define NTSC_ACTIVE       216
#define NTSC_SCANLINE_CLK (NTSC_SCANLINE_NS*CLK_PER_US/1000-1)
#define NTSC_START_CLK    (NTSC_START_NS*CLK_PER_US/1000-1)

#define PAL_SCANLINE_NS   64000
#define PAL_START_NS      12500
#define PAL_LINES         312
#define PAL_VSYNC         3
#define PAL_ACTIVE        260
#define PAL_SCANLINE_CLK  (PAL_SCANLINE_NS*CLK_PER_US/1000-1)
#define PAL_START_CLK     (PAL_START_NS*CLK_PER_US/1000-1)

volatile uint16_t beepFrames, scanLine, stopLine;
volatile uint8_t startLine, vertRes, horizRes;
uint16_t linesPerFrame, renderOffset;
uint8_t outputDelay, linesPerVertPixel, remainingLines, vsyncEnd;
void (*hsync)();
uint8_t framebuffer[max(BPR*FB_H_PAL, BPR*FB_H_NTSC)];
uint8_t framebufferDblRes[BPR*FB_H_HIRES];

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// VIDEO LIBRARY ///
////////////////////////////////////////////////////////////////////////////////
void reboot() { WDTCSR = _BV(WDCE); WDTCSR = _BV(WDE); }

void videoOff() {
  VID_DDR &= ~_BV(VID_PIN); VID_PORT &= ~_BV(VID_PIN);  // tristate the pins
  SYNC_DDR &= ~_BV(SYNC_PIN); SYNC_PORT &= ~_BV(SYNC_PIN); }
void videoOn() {
  VID_PORT &= ~_BV(VID_PIN); VID_DDR |= _BV(VID_PIN); // drive outputs
  SYNC_PORT |= _BV(SYNC_PIN); SYNC_DDR |= _BV(SYNC_PIN); }

void vsync() {
  uint16_t l;
  do { noInterrupts(); l = scanLine; interrupts(); } while(l<stopLine);
  do { noInterrupts(); l = scanLine; interrupts(); } while(l==stopLine); }

void initVideo(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  cls(); horizRes = w/8; vertRes = h; beepFrames = 0;
  if(chipPAL) {
    linesPerVertPixel = PAL_ACTIVE/vertRes;
    startLine = (PAL_LINES-vertRes*linesPerVertPixel)/2;
    outputDelay = PAL_START_CLK; vsyncEnd = PAL_VSYNC;
    linesPerFrame = PAL_LINES; ICR1 = PAL_SCANLINE_CLK; }
  else {
    linesPerVertPixel = NTSC_ACTIVE/vertRes;
    startLine = (NTSC_LINES-vertRes*linesPerVertPixel)/2;
    outputDelay = NTSC_START_CLK; vsyncEnd = NTSC_VSYNC;
    linesPerFrame = NTSC_LINES; ICR1 = NTSC_SCANLINE_CLK; }
  outputDelay += x*CLK_PER_US; startLine += y;
  stopLine = startLine+vertRes*linesPerVertPixel;
  TCCR1A = _BV(COM1A1)|_BV(COM1A0)|_BV(WGM11); // inverted fast pwm timer 1
  TCCR1B = _BV(WGM13)|_BV(WGM12)|_BV(CS10); OCR1A = VSYNC_CLK;
  scanLine = 0; hsync = &vsyncLine; OCR1A = VSYNC_CLK; TIMSK1 = _BV(TOIE1); }

ISR(TIMER1_OVF_vect) { hsync(); ++ scanLine; }

void vsyncLine() { // video sequence is vsync, top, active, bottom, ...
  if(scanLine==vsyncEnd) { OCR1A = HSYNC_CLK; hsync = &topLine; }}

void topLine() {
  if(scanLine==startLine) {
    hsync = &activeLine;
    renderOffset = 0; remainingLines = linesPerVertPixel; }}

void bottomLine() {
  if(scanLine==linesPerFrame) {
    OCR1A = VSYNC_CLK; scanLine = 0; hsync = &vsyncLine;
    if(beepFrames>0) -- beepFrames; else silence(); }}

void activeLine() {
  // The renderer will normally draw the main screen as part of the scan.
  // However, if the scanline is to draw the double-res ball area screen instead
  // then point to the appropriate screen buffer and set the offset to show
  // the correct line.
  uint16_t tempOffset = renderOffset;
  uint8_t *tempScreen = framebuffer;
  uint16_t lineY = scanLine-startLine-1; // ballYhires may be <0 but lineY is
      // always within signed int16 because scanLine<NTSC_LINES<PAL_LINES<500
  if(ballVisible && (int16_t)lineY>=ballYhires && (int16_t)lineY<(ballYhires+5)) {
    tempOffset = (lineY-ballYhires)*BPR; tempScreen = framebufferDblRes; }
    //... ballYhires is definitely >0

  __asm__ __volatile__ (
  ".macro nop3                          \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  ".endm                                \n"
  ".macro nop4                          \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  ".endm                                \n"
  ".macro nop5                          \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  "     nop                             \n"
  ".endm                                \n"
  "                                     \n"
  "     subi    %[time], 10             \n"
  "     sub     %[time], %[tcntl]       \n"
  "100:                                 \n"
  "     subi    %[time], 3              \n"
  "     brcc    100b                    \n"
  "     subi    %[time], 0-3            \n"
  "     breq    101f                    \n"
  "     dec     %[time]                 \n"
  "     breq    102f                    \n"
  "     rjmp    102f                    \n"
  "101:                                 \n"
  "     nop                             \n"
  "102:                                 \n"
  "                                     \n"
  "     add     r26, r28                \n"
  "     adc     r27, r29                \n"
  "     in      r16, %[port]            \n"
  "     andi    r16, 0xFF^(1<<%[vidPin])\n"
  "     rjmp    enterRend               \n"
  "loopRend:                            \n"
  "     bst     __tmp_reg__, 0          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "enterRend:                           \n"
  "     ld      __tmp_reg__, X+         \n"
  "     nop4                            \n"
  "     bst     __tmp_reg__, 7          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 6          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 5          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 4          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 3          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 2          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     nop5                            \n"
  "     bst     __tmp_reg__, 1          \n"
  "     bld     r16, %[vidPin]          \n"
  "     nop3                            \n"
  "     out     %[port], r16            \n"
  "     dec     %[horizRes]             \n"
  "     breq    exitRend                \n"
  "     jmp     loopRend                \n"
  "exitRend:                            \n"
  "     nop3                            \n"
  "     bst     __tmp_reg__, 0          \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  "     in      r16, %[port]            \n"
  "     andi    r16, 0xFF^(1<<%[vidPin])\n"
  "     bst     r16, %[vidPin]          \n"
  "     nop3                            \n"
  "     bld     r16, %[vidPin]          \n"
  "     out     %[port], r16            \n"
  : : [time] "a" (outputDelay), [tcntl] "a" (VID_TCNTxL),
      [port] "i" (_SFR_IO_ADDR(VID_PORT)), "x" (tempScreen), "y" (tempOffset),
      [horizRes] "d" (horizRes), [vidPin] "M" (VID_PIN)
  : "r16");

  if(-- remainingLines==0) {
    remainingLines = linesPerVertPixel; renderOffset += horizRes; }
  else {
    // If the second scanline of the same row is being displayed
    // (main screen has 2 scan lines per vertical pixel)
    // then DESTRUCTIVELY clear the left and right pixels so that the
    // second of each pair of scanlines will have a gap.
    // This produces the dotted edge as seen on the original game.
    if(chip1976) { // wipe 1 pixels out of left and right borders
      if(gameNumber>1) framebuffer[renderOffset] &= 0b01111111;
      if(gameNumber==2) framebuffer[renderOffset+BPR-1] &= 0b11111101; }
    else { // wipe 2 pixel out of left and right borders
      if(gameNumber>1) framebuffer[renderOffset] &= 0b00111111;
      if(gameNumber==2) framebuffer[renderOffset+BPR-1] &= 0b00111111; }}

  if(scanLine==stopLine) {
    hsync = &bottomLine; }}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// SOUND LIBRARY ///
////////////////////////////////////////////////////////////////////////////////
void silence() { SND_TCCRxB = 0; SND_PORT &= ~_BV(SND_PIN); }
void sound(uint16_t frequency, uint16_t duration_ms) {
  if(frequency==0) return;
  beepFrames = (uint32_t)duration_ms*(chipPAL ? 50 : 60)/1000;
  // ...THE WHOLE THING COULD HAVE BEEN PRECALCULATED...
  // we are using an 8 bit timer, scan through prescalers to find best fit
  // F_OC0A = F_CPU/(2*prescal*(1+OCR0A)) with prescal in 1,8,64,256,1024
  // => OCR0A = F_CPU/F_OC0A/2/prescal-1 for some prescaler
  // will find 500Hz->11 & 249 1000Hz->11 & 124 2000Hz->11 & 61 (2016Hz)
  uint32_t p = F_CPU/frequency, ocr = p/2*1-1; uint8_t prescal = 0b001;
  if(ocr>255) {
    prescal = 0b010; ocr = p/2/8-1;     // those are prescalers of timer 0
    if(ocr>255) {
      prescal = 0b011; ocr = p/2/64-1;  // other timers have more prescalers
      if(ocr>255) {
        prescal = 0b100; ocr = p/2/256-1;
        if(ocr>255) {
          prescal = 0b101; ocr = p/2/1024-1; }}}}
  SND_DDR |= _BV(SND_PIN); SND_PORT &= ~_BV(SND_PIN);
  SND_TCCRxA = _BV(SND_COMxA0)|_BV(SND_COMxB0)|_BV(SND_WGMx1);
  SND_TCCRxB = prescal; SND_OCRxA = SND_OCRxB = ocr; }

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// GRAPHICS LIBRARY ///
////////////////////////////////////////////////////////////////////////////////
void cls() {
  for(uint16_t i = 0; i<horizRes*vertRes; ++ i) framebuffer[i] = 0; }

static void inline _sp(uint8_t x, uint8_t y, uint8_t c) {
  if(c==WHITE) { framebuffer[x/8+y*horizRes] |= 0x80>>(x&7); }
  else if(c==BLACK) { framebuffer[x/8+y*horizRes] &= ~(0x80>>(x&7)); }
  else { framebuffer[x/8+y*horizRes] ^= 0x80>>(x&7); }}

void setPixel(uint8_t x, uint8_t y, uint8_t c) {
  if(x<horizRes*8 && y<vertRes) _sp(x, y, c); }

// ATTENTION this does not do screen edge clipping, it just refuses
void drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t c) {
  if(x0==x1) { drawColumn(x0, y0, y1, c); }
  else if(y0==y1) { drawRow(x0, y0, x1, c); }
  else { // Bresenham
    if(x0>horizRes*8 || y0>vertRes || x1>horizRes*8 || y1>vertRes) return;
    int16_t e; uint8_t x = x0, y = y0, dx, dy, swapaxes; int8_t s1, s2;
    if(x1<x0) { dx = x0-x1; s1 = -1; }
    else { dx = x1-x0; s1 = 1; }
    if(y1<y0) { dy = y0-y1; s2 = -1; }
    else { dy = y1-y0; s2 = 1; }
    swapaxes = 0; if(dy>dx) { uint8_t t = dx; dx = dy; dy = t; swapaxes = 1; }
    e = dy*2-dx;
    for(uint8_t i = 0; i<=dx; ++ i) {
      _sp(x, y, c);
      if(e>=0) {
        if(swapaxes==1) x += s1; else y += s2;
        e = e-dx*2; }
      if(swapaxes==1) y += s2; else x += s1;
      e = e+dy*2; }}}

void drawRow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t c) {
  if(x0>horizRes*8 || y0>vertRes || x1>horizRes*8) return;
  if(x0==x1) { _sp(x0, y0, c); }
  else {
    uint8_t lbit, rbit; uint8_t *p = framebuffer+horizRes*y0, *e = p;
    if(x0>x1) { uint16_t t = x0; x0 = x1; x1 = t; }
    lbit = 0xff>>(x0&7); p += x0/8; ++ x1;
    rbit = ~(0xff>>(x1&7)); e += x1/8;
    if(p==e) { lbit = lbit&rbit; rbit = 0; }
    if(c==WHITE) {
      *p ++ |= lbit; while(p<e) { *p ++ = 0xff; } *p |= rbit; }
    else if(c==BLACK) {
      *p ++ &= ~lbit; while(p<e) { *p ++ = 0; } *p &= ~rbit; }}}

void drawColumn(uint8_t x0, uint8_t y0, uint8_t y1, uint8_t c) {
  if(x0>horizRes*8 || y0>vertRes || y1>vertRes) return;
  if(y0==y1) { _sp(x0, y0, c); }
  else {
    uint8_t bit; uint16_t p;
    if(y1<y0) { uint8_t t = y0; y0 = y1; y1 = t; }
    bit = 0x80>>(x0&7); p = x0/8+y0*horizRes;
    if(c==WHITE) {
      while(y0<=y1) { framebuffer[p] |= bit; p += horizRes; ++ y0; }}
    else if(c==BLACK) {
      while(y0<=y1) { framebuffer[p] &= ~bit; p += horizRes; ++ y0; }}}}

void bitmap(uint8_t x, uint8_t y,
            const unsigned char *bmp, uint8_t w, uint8_t h) {
  uint8_t temp, lshift, rshift, save, xtra; uint16_t si = 0, i = 0;
  rshift = x&7; lshift = 8-rshift;
  xtra = w&7; w /= 8; if(xtra) ++w; // unaligned
  for(uint8_t l = 0; l<h; ++ l) {
    si = (y+l)*horizRes+x/8;
    if(w==1) temp = 0xff>>(rshift+xtra); else temp = 0;
    save = framebuffer[si];
    framebuffer[si] &= (0xff<<lshift)|temp;
    temp = pgm_read_byte((uint16_t)bmp+(i ++));
    framebuffer[si ++] |= temp>>rshift;
    for(uint16_t b = i+w-1; i<b; ++ i) {
      save = framebuffer[si];
      framebuffer[si] = temp<<lshift;
      temp = pgm_read_byte((uint16_t)bmp+i);
      framebuffer[si ++] |= temp>>rshift; }
    if(rshift+xtra<8) framebuffer[si-1] |= save&(0xff>>(rshift+xtra));
    if(rshift+xtra-8>0) framebuffer[si] &= 0xff>>(rshift+xtra-8);
    framebuffer[si] |= temp<<lshift; }}

// END OF MODIFIED TV OUT LIBRARY CODE
// vi: sts=2 et
