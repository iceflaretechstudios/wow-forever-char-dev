#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include "config.h"

TFT_eSPI tft;
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
constexpr uint16_t BG=0x0863, PANEL=0x10C5, BORDER=0x6B4D, GOLD=0xF62D, WHITE=0xEF7D, MUTED=0xAD55, BLUE=0x2188, GREEN=0x4E69;
const char *slotKeys[]={"HEAD","NECK","SHOULDER","BACK","CHEST","SHIRT","TABARD","WRIST","HANDS","WAIST","LEGS","FEET","FINGER_1","FINGER_2","TRINKET_1","TRINKET_2","MAIN_HAND","OFF_HAND","RANGED"};
const char *statKeys[]={"health","power","strength","agility","stamina","intellect","spirit","armor","attack_power","spell_power","melee_crit","spell_crit","mastery","haste","versatility"};
JsonDocument profile;
String slug, characterName="Theundying", errorText="", lastUpdate="";
int page=0, scrollY=0, contentHeight=0;
unsigned long lastFetch=0, lastAttempt=0;
bool connected=false, hasPortrait=false;

void textAt(String s,int x,int y,uint16_t color=WHITE,int font=2){tft.setTextColor(color,BG);tft.drawString(s,x,y,font);}
void panel(int x,int y,int w,int h,uint16_t fill=PANEL){tft.fillRoundRect(x,y,w,h,7,fill);tft.drawRoundRect(x,y,w,h,7,BORDER);}
void button(int x,int y,int w,String s,bool active=false){panel(x,y,w,31,active?BLUE:PANEL);tft.setTextColor(active?WHITE:GOLD,active?BLUE:PANEL);tft.drawCentreString(s,x+w/2,y+7,2);}
String getText(JsonVariantConst v,String fallback="-"){if(v.isNull())return fallback;if(v.is<const char*>())return v.as<String>();if(v.is<int>()||v.is<long>()||v.is<double>())return v.as<String>();return fallback;}
String titleCase(String s){s.replace("_"," ");bool cap=true;for(unsigned i=0;i<s.length();i++){if(cap)s.setCharAt(i,toupper(s[i]));cap=s[i]==' ';}return s;}
String clip(String s,int max){return s.length()>(unsigned)max?s.substring(0,max-2)+"..":s;}
void status(String s){errorText=s;Serial.println(s);}

String moneyDisplay(){
 JsonVariantConst ch=profile["character"];
 JsonVariantConst amount=ch["moneyCopper"];
 if(amount.isNull())amount=ch["money_copper"];
 if(amount.isNull())amount=ch["money"]["copper"];
 if(amount.isNull())amount=profile["moneyCopper"];
 if(amount.isNull())amount=profile["money_copper"];
 if(amount.isNull())amount=profile["money"]["copper"];
 if(amount.isNull())return "N/A";
 String raw=getText(amount,"");
 if(raw.isEmpty()||raw=="-")return "N/A";
 for(unsigned i=0;i<raw.length();i++){
  if(raw[i]<'0'||raw[i]>'9')return "N/A";
 }
 unsigned long long copper=strtoull(raw.c_str(),nullptr,10);
 unsigned long long gold=copper/10000ULL;
 unsigned int silver=(unsigned int)((copper/100ULL)%100ULL);
 unsigned int coins=(unsigned int)(copper%100ULL);
 char result[48];
 snprintf(result,sizeof(result),"%llug %02us %02uc",gold,silver,coins);
 return String(result);
}


bool fetchJson(const String &path,JsonDocument &out){
 if(WiFi.status()!=WL_CONNECTED){status("Wi-Fi disconnected");return false;}
 WiFiClientSecure client;client.setInsecure(); // Prototype only: replace with a CA certificate before production use.
 HTTPClient http;http.setTimeout(15000);if(!http.begin(client,String(WORKER_BASE)+path)){status("HTTPS begin failed");return false;}
 http.addHeader("Accept","application/json");http.addHeader("Cache-Control","no-cache");
 int code=http.GET();
 Serial.printf("API GET %s -> HTTP %d, declared length=%d, free heap=%u\\n",
               path.c_str(),code,http.getSize(),ESP.getFreeHeap());
 if(code!=200){status("HTTP "+String(code));http.end();return false;}
 // HTTPClient::getString handles chunked transfer encoding; getStream()
 // exposes the raw transport and may give ArduinoJson chunk framing.
 String payload=http.getString();
 http.end();
 Serial.printf("API received %u bytes, free heap=%u\\n",
               (unsigned)payload.length(),ESP.getFreeHeap());
 if(payload.isEmpty()){status("API response empty");return false;}
 DeserializationError err=deserializeJson(out,payload);
 if(err){Serial.printf("JSON parse failed: %s; bytes=%u\\n",
                       err.c_str(),(unsigned)payload.length());
         status(String("JSON: ")+err.c_str());return false;}
 return true;
}

bool chooseCharacter(){
 JsonDocument list;if(!fetchJson("/api/characters",list))return false;
 JsonArrayConst chars=list["forever"].as<JsonArrayConst>();if(chars.isNull()||chars.size()==0){status("No Forever characters");return false;}
 JsonObjectConst selected=chars[0];for(JsonObjectConst c:chars){if(getText(c["name"]).equalsIgnoreCase(PREFERRED_CHARACTER)){selected=c;break;}}
 slug=getText(selected["slug"],"");characterName=getText(selected["name"],PREFERRED_CHARACTER);
 if(slug.isEmpty()){status("Character slug missing");return false;}return true;
}

void refresh(){
 lastAttempt=millis();
 if(slug.isEmpty()&&!chooseCharacter())return;
 JsonDocument incoming;if(!fetchJson("/api/armory/forever/"+slug,incoming))return;
 profile=std::move(incoming);JsonObjectConst c=profile["character"].as<JsonObjectConst>();
 characterName=getText(c["name"],characterName);lastUpdate=getText(profile["updated"],"Updated");
 lastFetch=millis();errorText="";Serial.println("Armory profile updated");
}

bool jpgOutput(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t *bitmap){if(y>=438)return false;tft.pushImage(x,y,w,h,bitmap);return true;}
void header(){tft.fillRect(0,0,320,45,BG);textAt("ICEFLARE ARMORY",10,8,GOLD,4);tft.fillCircle(303,15,4,WiFi.status()==WL_CONNECTED?GREEN:0xF800);tft.drawFastHLine(0,42,320,BORDER);}
void footer(){
 tft.fillRect(0,441,320,39,BG);
 tft.drawFastHLine(0,440,320,BORDER);
 // Compact navigation: 25 px tall, centered vertically in the 39 px footer.
 const int y=448, h=25;
 const char* labels[]={"Armory","Stats","Talents","Prof"};
 for(int i=0;i<4;i++){
  int x=5+i*80, w=(i==3?70:75);
  uint16_t fill=(page==i?BLUE:PANEL);
  tft.fillRoundRect(x,y,w,h,6,fill);
  tft.drawRoundRect(x,y,w,h,6,BORDER);
  tft.setTextColor(page==i?WHITE:GOLD,fill);
  tft.drawCentreString(labels[i],x+w/2,y+4,2);
 }
}
void lineItem(String left,String right,int y,uint16_t color=WHITE){if(y<56||y>426)return; textAt(clip(left,23),12,y,MUTED,2);tft.setTextColor(color,BG);tft.drawRightString(clip(right,17),307,y,2);}
JsonObjectConst itemFor(String slot){for(JsonObjectConst item:profile["equipment"].as<JsonArrayConst>()){if(getText(item["slot"]["type"])==slot)return item;}return JsonObjectConst();}
void equipmentRow(int x,int y,String slot){if(y<305||y+33>438)return;JsonObjectConst item=itemFor(slot);panel(x,y,148,33);String name=getText(item["name"],"Empty");uint16_t q=WHITE;String quality=getText(item["quality"]["type"]);if(quality=="EPIC")q=0xB1FB;else if(quality=="RARE")q=0x4C9F;else if(quality=="UNCOMMON")q=GREEN; textAt(clip(titleCase(slot),12),x+5,y+2,MUTED,1);textAt(clip(name,21),x+5,y+15,q,1);}
void armory(){
 JsonObjectConst c=profile["character"].as<JsonObjectConst>();
 panel(6,50,308,50);textAt(clip(characterName,18),13,55,GOLD,4);
 String meta="Lvl "+getText(c["level"])+"  iLvl "+getText(c["averageItemLevel"])+"  "+getText(c["characterClass"]);
 textAt(clip(meta,26),13,83,MUTED,1);
 String gold=moneyDisplay();tft.setTextColor(GOLD,PANEL);tft.drawRightString("Gold: "+gold,305,83,1);
 panel(6,105,308,173);
 if(hasPortrait){TJpgDec.drawFsJpg(47,108,"/portrait.jpg",LittleFS);}else{
  textAt("CHARACTER PORTRAIT",82,173,GOLD,2);textAt("Add data/portrait.jpg",95,198,MUTED,1);
 }
 textAt("EQUIPMENT",9,284,GOLD,2);
 // The portrait and navigation stay fixed; only equipment rows scroll.
 // 19 slots arranged in two columns (10 rows), clipped above the footer.
 constexpr int listTop=305, listBottom=438, rowPitch=37, slotCount=19;
 constexpr int rowCount=(slotCount+1)/2;
 contentHeight=max(0, rowCount*rowPitch-(listBottom-listTop));
 scrollY=constrain(scrollY,0,contentHeight);
 for(int i=0;i<rowCount;i++){
  int y=listTop+i*rowPitch-scrollY;
  equipmentRow(6,y,slotKeys[i]);
  int right=i+rowCount;
  if(right<slotCount)equipmentRow(166,y,slotKeys[right]);
 }
 // Visible scroll position and guidance at the right edge.
 if(contentHeight>0){
  int track= listBottom-listTop;
  int thumb=max(16,track*(listBottom-listTop)/(rowCount*rowPitch));
  int pos=(scrollY*(track-thumb))/contentHeight;
  tft.fillRect(316,listTop,3,track,BORDER);
  tft.fillRect(316,listTop+pos,3,thumb,GOLD);
 }
}
void stats(){int y=55-scrollY;
 panel(6,55,308,36);textAt("CHARACTER STATISTICS",15,64,GOLD,2);
 y=103-scrollY;
 for(const char* k:statKeys){JsonVariantConst v=profile["stats"][k];if(!v.isNull()){String value;if(v.is<JsonObjectConst>())value=getText(v["effective"],getText(v["value"],getText(v["base"])));else value=getText(v);lineItem(titleCase(k),value,y);y+=29;}}
 JsonObjectConst ch=profile["character"].as<JsonObjectConst>();
 lineItem("Gold",moneyDisplay(),y,GOLD);y+=29;
 contentHeight=max(0,y+scrollY-420);
}
void talents(){int y=59-scrollY;textAt("TALENTS & SPECIALIZATIONS",10,y,GOLD,2);y+=31;
 JsonObjectConst sp=profile["specializations"].as<JsonObjectConst>();
 lineItem("Active spec",getText(sp["active_specialization"]["name"],getText(sp["active_spec"]["name"])),y);y+=32;
 bool found=false;
 for(JsonObjectConst entry:sp["specializations"].as<JsonArrayConst>()){
  String label=getText(entry["specialization"]["name"],"Specialization");lineItem("Spec",label,y,GOLD);y+=27;
  for(JsonObjectConst t:entry["selected_talents"].as<JsonArrayConst>()){String n=getText(t["talent"]["name"],getText(t["name"]));lineItem("Talent",n,y);y+=26;found=true;}
 }
 if(!found){lineItem("Selections","Not reported",y);y+=28;}
 contentHeight=max(0,y+scrollY-420);
}
void professions(){int y=58-scrollY;textAt("PROFESSIONS",10,y,GOLD,2);y+=35;
 JsonObjectConst p=profile["professions"].as<JsonObjectConst>();
 for(const char *group:{"primaries","secondaries"}){
  lineItem("Group",String(group),y,GOLD);y+=28;
  for(JsonObjectConst entry:p[group].as<JsonArrayConst>()){
   String name=getText(entry["profession"]["name"],"Profession");
   lineItem(name,"",y);y+=25;
   JsonArrayConst tiers=entry["tiers"].as<JsonArrayConst>();
   if(tiers.isNull()||tiers.size()==0){lineItem("Skill",getText(entry["skill_points"])+" / "+getText(entry["max_skill_points"]),y);y+=27;}
   else for(JsonObjectConst tier:tiers){lineItem(clip(getText(tier["tier"]["name"],"Skill"),19),getText(tier["skill_points"])+" / "+getText(tier["max_skill_points"]),y);y+=27;}
  }
 }
 contentHeight=max(0,y+scrollY-420);
}
void draw(){tft.fillScreen(BG);header();if(profile["character"].isNull()){textAt("Connecting to Cloudflare...",16,110,GOLD,2);textAt(clip(errorText,35),16,140,MUTED,2);}else {if(page==0)armory();else if(page==1)stats();else if(page==2)talents();else professions();}footer();}
void setup(){
 Serial.begin(115200);delay(300);
 Serial.println("ICEFLARE ARMORY: SETUP STARTED");
 pinMode(27,OUTPUT);digitalWrite(27,HIGH); // E32R40T backlight
 Serial.println("BACKLIGHT GPIO27 SET HIGH");
 pinMode(TOUCH_CS,OUTPUT);digitalWrite(TOUCH_CS,HIGH);
 SPI.begin(14,12,13); // Shared LCD / XPT2046 SPI bus
 Serial.println("SPI INITIALIZED");
 tft.init();tft.setRotation(0);tft.fillScreen(BG);tft.setTextDatum(TL_DATUM);tft.setTextWrap(false);
 Serial.println("LCD INITIALIZED");
 if(!LittleFS.begin(true))Serial.println("LittleFS mount failed");
 TJpgDec.setJpgScale(1);TJpgDec.setSwapBytes(true);TJpgDec.setCallback(jpgOutput);hasPortrait=LittleFS.exists("/portrait.jpg");
 touch.begin(SPI);touch.setRotation(0);
 Serial.println("TOUCH INITIALIZED");
 WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<15000){delay(250);}
 connected=WiFi.status()==WL_CONNECTED;if(connected)refresh();else status("Check Wi-Fi settings");draw();
 Serial.println("ICEFLARE ARMORY: READY");
}
void loop(){
 if(WiFi.status()==WL_CONNECTED&&millis()-lastAttempt>REFRESH_INTERVAL_MS){refresh();draw();}
 if(touch.touched()){
  TS_Point p=touch.getPoint();
  // Nine-point affine transform includes both axis reversals and measured skew.
  float screenX=TOUCH_AFFINE_AX0+TOUCH_AFFINE_AX1*p.x+TOUCH_AFFINE_AX2*p.y;
  float screenY=TOUCH_AFFINE_AY0+TOUCH_AFFINE_AY1*p.x+TOUCH_AFFINE_AY2*p.y;
  int x=constrain((int)lroundf(screenX),0,319);
  int y=constrain((int)lroundf(screenY),0,479);
  if(y>440){page=constrain(x/80,0,3);scrollY=0;draw();}
  else if(y>48){scrollY+=111;if(scrollY>contentHeight)scrollY=0;draw();}
  delay(230);
 }
 delay(15);
}
