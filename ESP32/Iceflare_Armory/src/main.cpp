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
XPT2046_Touchscreen touch(TOUCH_CS);
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

bool fetchJson(const String &path,JsonDocument &out){
 if(WiFi.status()!=WL_CONNECTED){status("Wi-Fi disconnected");return false;}
 WiFiClientSecure client;client.setInsecure(); // Prototype only: replace with a CA certificate before production use.
 HTTPClient http;http.setTimeout(15000);if(!http.begin(client,String(WORKER_BASE)+path)){status("HTTPS begin failed");return false;}
 http.addHeader("Accept","application/json");http.addHeader("Cache-Control","no-cache");
 int code=http.GET();if(code!=200){status("HTTP "+String(code));http.end();return false;}
 DeserializationError err=deserializeJson(out,http.getStream());http.end();
 if(err){status(String("JSON: ")+err.c_str());return false;}return true;
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
void footer(){tft.fillRect(0,441,320,39,BG);tft.drawFastHLine(0,440,320,BORDER);button(5,446,75,"Armory",page==0);button(85,446,75,"Stats",page==1);button(165,446,75,"Talents",page==2);button(245,446,70,"Prof",page==3);}
void lineItem(String left,String right,int y,uint16_t color=WHITE){if(y<56||y>426)return; textAt(clip(left,23),12,y,MUTED,2);tft.setTextColor(color,BG);tft.drawRightString(clip(right,17),307,y,2);}
JsonObjectConst itemFor(String slot){for(JsonObjectConst item:profile["equipment"].as<JsonArrayConst>()){if(getText(item["slot"]["type"])==slot)return item;}return JsonObjectConst();}
void equipmentRow(int x,int y,String slot){if(y<60||y>425)return;JsonObjectConst item=itemFor(slot);panel(x,y,148,33);String name=getText(item["name"],"Empty");uint16_t q=WHITE;String quality=getText(item["quality"]["type"]);if(quality=="EPIC")q=0xB1FB;else if(quality=="RARE")q=0x4C9F;else if(quality=="UNCOMMON")q=GREEN; textAt(clip(titleCase(slot),12),x+5,y+2,MUTED,1);textAt(clip(name,21),x+5,y+15,q,1);}
void armory(){
 JsonObjectConst c=profile["character"].as<JsonObjectConst>();
 panel(6,50,308,50);textAt(clip(characterName,18),13,55,GOLD,4);
 String meta="Lvl "+getText(c["level"])+"  iLvl "+getText(c["averageItemLevel"])+"  "+getText(c["characterClass"]);
 textAt(clip(meta,39),13,83,MUTED,1);
 panel(6,105,308,173);
 if(hasPortrait){TJpgDec.drawFsJpg(47,108,"/portrait.jpg",LittleFS);}else{
  textAt("CHARACTER PORTRAIT",82,173,GOLD,2);textAt("Add data/portrait.jpg",95,198,MUTED,1);
 }
 textAt("EQUIPMENT",9,284,GOLD,2);
 for(int i=0;i<8;i++){equipmentRow(6,305+i*37,slotKeys[i]);equipmentRow(166,305+i*37,slotKeys[i+9]);}
}
void stats(){int y=55-scrollY;
 panel(6,55,308,36);textAt("CHARACTER STATISTICS",15,64,GOLD,2);
 y=103-scrollY;
 for(const char* k:statKeys){JsonVariantConst v=profile["stats"][k];if(!v.isNull()){String value;if(v.is<JsonObjectConst>())value=getText(v["effective"],getText(v["value"],getText(v["base"])));else value=getText(v);lineItem(titleCase(k),value,y);y+=29;}}
 JsonObjectConst ch=profile["character"].as<JsonObjectConst>();
 if(ch["moneyCopper"].is<long long>()){long long copper=ch["moneyCopper"].as<long long>();lineItem("Gold",String((long)(copper/10000)),y,GOLD);y+=29;}
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
void setup(){Serial.begin(115200);delay(300);tft.init();tft.setRotation(0);tft.fillScreen(BG);tft.setTextDatum(TL_DATUM);tft.setTextWrap(false);
 if(!LittleFS.begin(true))Serial.println("LittleFS mount failed");
 TJpgDec.setJpgScale(1);TJpgDec.setSwapBytes(true);TJpgDec.setCallback(jpgOutput);hasPortrait=LittleFS.exists("/portrait.jpg");
 touch.begin();touch.setRotation(0);
 WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<15000){delay(250);}
 connected=WiFi.status()==WL_CONNECTED;if(connected)refresh();else status("Check Wi-Fi settings");draw();}
void loop(){
 if(WiFi.status()==WL_CONNECTED&&millis()-lastAttempt>REFRESH_INTERVAL_MS){refresh();draw();}
 if(touch.touched()){
  TS_Point p=touch.getPoint();int rawX=p.x,rawY=p.y;
  if(TOUCH_SWAP_XY){int a=rawX;rawX=rawY;rawY=a;}
  int x=map(constrain(rawX,TOUCH_MIN_X,TOUCH_MAX_X),TOUCH_MIN_X,TOUCH_MAX_X,0,319);
  int y=map(constrain(rawY,TOUCH_MIN_Y,TOUCH_MAX_Y),TOUCH_MIN_Y,TOUCH_MAX_Y,0,479);
  if(TOUCH_INVERT_X)x=319-x;if(TOUCH_INVERT_Y)y=479-y;
  if(y>440){page=constrain(x/80,0,3);scrollY=0;draw();}
  else if(y>48){scrollY+=120;if(scrollY>contentHeight)scrollY=0;draw();}
  delay(230);
 }
 delay(15);
}
