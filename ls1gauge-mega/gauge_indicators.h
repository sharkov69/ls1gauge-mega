#pragma once
// Small immediate-mode graphics; no framebuffer and no dynamic strings.
void gaugeCircle(Adafruit_GFX& screen,uint8_t x,uint8_t y,const char* label,uint16_t color,uint8_t sx=2,uint8_t sy=2){
 screen.fillRect(x-14,y-14,29,29,GC9A01A_BLACK);
 screen.drawCircle(x,y,14,color);screen.setFont();screen.setTextSize(sx,sy);screen.setTextColor(color);
 screen.setCursor(x-((strlen(label)*6-1)*sx)/2,y-(7*sy)/2);screen.print(label);
}
void drawFuelCut(bool on){
 tft.fillRect(161,184,28,28,GC9A01A_BLACK);if(!on)return;
 const uint16_t c=GC9A01A_WHITE;
 tft.drawRect(166,188,12,20,c);tft.drawRect(168,191,8,7,c);tft.drawFastHLine(164,208,16,c);
 tft.drawLine(177,200,181,200,c);tft.drawLine(181,200,181,206,c);tft.drawLine(181,206,185,206,c);
 tft.drawLine(185,206,185,194,c);tft.drawLine(185,194,181,190,c);
 for(int8_t d=-1;d<=1;++d)tft.drawLine(163+d,209,186+d,186,GC9A01A_BLACK);
 tft.drawLine(163,209,186,186,c);tft.drawLine(164,209,187,186,c);
}
void drawFirstExtra(bool force=false){
 static uint32_t previous=0xffffffffUL;static int16_t previousSA=-32768;
 const uint32_t nowMs=millis();
 const uint16_t age=dpid.directPolling()?3000:500;
 const uint8_t gear=extra.fresh(X_PRNDL,nowMs,age)?extra.raw[X_PRNDL]:0;
 const bool sport=extra.fresh(X_SHIFT,nowMs,age)&&(extra.raw[X_SHIFT]==7||extra.raw[X_SHIFT]==8);
 const bool tcc=extra.flagActive(X_TCC,128,nowMs,age),cut=extra.fuelCut(nowMs);
 const uint32_t key=gear|((uint32_t)sport<<8)|((uint32_t)tcc<<9)|((uint32_t)cut<<10);
 if(force||key!=previous){
  char label[3]={'?',0,0};uint16_t color=0x8410;
  if(gear==1)label[0]='P';else if(gear==2){label[0]='R';color=GC9A01A_RED;}
  else if(gear==4)label[0]='N';else if(gear==8||gear==16||gear==32||gear==64){
   label[0]='D';label[1]=gear==8?'4':gear==16?'3':gear==32?'2':'1';color=GC9A01A_GREEN;
  }
  gaugeCircle(tft,66,198,label,color);gaugeCircle(tft,102,198,"S",sport?GC9A01A_RED:0x8410);
  gaugeCircle(tft,138,198,"TCC",tcc?GC9A01A_GREEN:0x8410,1,1);drawFuelCut(cut);previous=key;
 }
 const int16_t sa=extra.fresh(X_SA,nowMs,age)?extra.sparkHalfDegrees()/2:-32768;
 if(force||sa!=previousSA){
  char value[5];if(sa==-32768)strcpy(value," --");else snprintf(value,sizeof(value),"%3d",sa);
  tft.fillRect(173,160,36,16,GC9A01A_BLACK);tft.setFont();tft.setTextSize(2);tft.setTextColor(GC9A01A_WHITE);tft.setCursor(173,160);tft.print(value);previousSA=sa;
 }
}
void drawClimateIndicators(bool force=false){
 static uint8_t previous=255;
 const uint32_t nowMs=millis();const uint16_t age=dpid.directPolling()?3000:500;
 const bool ac=extra.flagActive(X_AC,1,nowMs,age),fan=extra.flagActive(X_FAN,1,nowMs,age);
 const uint8_t key=ac|(fan<<1);
 if(force||key!=previous){gaugeCircle(tft2,38,162,"A/C",ac?GC9A01A_GREEN:0x8410,1,2);gaugeCircle(tft2,202,162,"FAN",fan?GC9A01A_RED:0x8410,1,2);previous=key;}
}
