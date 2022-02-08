#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "RTClib.h"
#include <rom/rtc.h>
#include "SPI.h"
#include "SD.h"
#include <Adafruit_NeoPixel.h>


RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"europe.pool.ntp.org", 3600*2, 60000);
DateTime now;
// const char* ssid = "pinguin";
// const char* password = "BulkaiMaslo@1";
 const char* ssid = "aaa";
 const char* password = "--------";



bool FirstLoop=true;

struct wifi{
  String essid;
  String password;
};
struct Solar_logs{
  String reset_reason;
  String mode;
  int system_start;
};
struct Solar_conf{
  float temp_delta;
  float temp_in_max;
  float temp_in_overheat;
  float temp_out_max;
  float temp_out_overheat;
  float time_flush_pump;

};
wifi wifi[5];
Solar_conf solar;
Solar_logs solar_log;
TaskHandle_t Task2;
TaskHandle_t Task1;


void reset_reason(){
  String tmp;
  for (int i = 0 ; i<=1; i++)
  {
    tmp = ":: CPU "+String(i) +" reset reason: ";

    switch ( rtc_get_reset_reason(i))
    {
      case 1 : tmp += "[01] POWERON_RESET"; break;          /**<1, Vbat power on reset*/
      case 3 : tmp += "[02] SW_RESET"; break;               /**<3, Software reset digital core*/
      case 4 : tmp += "[03] OWDT_RESET"; break;             /**<4, Legacy watch dog reset digital core*/
      case 5 : tmp += "[04] DEEPSLEEP_RESET"; break;        /**<5, Deep Sleep reset digital core*/
      case 6 : tmp += "[05] SDIO_RESET"; break;             /**<6, Reset by SLC module, reset digital core*/
      case 7 : tmp += "[06] TG0WDT_SYS_RESET"; break;       /**<7, Timer Group0 Watch dog reset digital core*/
      case 8 : tmp += "[07] TG1WDT_SYS_RESET"; break;       /**<8, Timer Group1 Watch dog reset digital core*/
      case 9 : tmp += "[08] RTCWDT_SYS_RESET"; break;       /**<9, RTC Watch dog Reset digital core*/
      case 10 : tmp+= "[09] INTRUSION_RESET"; break;       /**<10, Instrusion tested to reset CPU*/
      case 11 : tmp += "[10] TGWDT_CPU_RESET"; break;       /**<11, Time Group reset CPU*/
      case 12 : tmp += "[11] SW_CPU_RESET"; break;          /**<12, Software reset CPU*/
      case 13 : tmp += "[12] RTCWDT_CPU_RESET"; break;      /**<13, RTC Watch dog Reset CPU*/
      case 14 : tmp += "[13] EXT_CPU_RESET"; break;         /**<14, for APP CPU, reseted by PRO CPU*/
      case 15 : tmp += "[14] RTCWDT_BROWN_OUT_RESET"; break;/**<15, Reset when the vdd voltage is not stable*/
      case 16 : tmp += "[15] RTCWDT_RTC_RESET"; break;      /**<16, RTC Watch dog reset digital core and rtc module*/
      default : tmp += "[16] NO_MEAN";
    }
    solar_log.reset_reason+=tmp+" ";
  }
}

void load_Configuration_server(){
  HTTPClient http;

  // Send request
  http.begin("http://woojtekk.pl/solar/solar_conf.json");
  http.GET();
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error){
    Serial.println(F("Failed to read file, using default configuration"));
    return;
  }
  Serial.println("\n>>> RAW DATA <<<");
  serializeJsonPretty(doc,Serial);

  solar.temp_delta        = doc["temp_delta"].as<float>();
  solar.temp_in_max       = doc["temp_in_max"].as<float>();
  solar.temp_in_overheat  = doc["temp_in_overheat"].as<float>();
  solar.temp_out_max      = doc["temp_out_max"].as<float>();
  solar.temp_out_overheat = doc["temp_out_overheat"].as<float>();

  Serial.println("\n >>> IMPORTED CONFIG <<<");
  Serial.println(solar.temp_delta        );
  Serial.println(solar.temp_in_max       );
  Serial.println(solar.temp_in_overheat  );
  Serial.println(solar.temp_out_max      );
  Serial.println(solar.temp_out_overheat );

}

void wifi_connect(){
  Serial.println(":: WiFi start ....");

  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();

    unsigned int st = millis();
    WiFi.begin(ssid, password);
    Serial.println(":: Connecting to WiFi:");
    Serial.print("  >> SSID:     ");  Serial.println(ssid);
    Serial.print("  >> PASSWORD: ");  Serial.println(password);

    while (WiFi.status() != WL_CONNECTED && ( (millis() - st) <=15*1000) ) {
      delay(250);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\n:: WiFi Status: ");
      Serial.println(WiFi.status());
      delay(500);
    }
  }
};

void RTCadjust(){
  wifi_connect();
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    timeClient.update();
    long dt = rtc.now().unixtime() - timeClient.getEpochTime();
    if ( abs(dt) >= 10 ){
        rtc.adjust(DateTime(timeClient.getEpochTime()));
        Serial.println(":: >> RTC updated");
    }
  }

  now = rtc.now();
  String tmp = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );
  Serial.println(tmp);
}

void Task1code( void * pvParameters ){
  Serial.print(":: Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    millis();
    delay(100);
  }
}
void Task2code( void * pvParameters ){
  Serial.print(":: Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    millis();
    delay(100);
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}
void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}
void readFile(fs::FS &fs, const char * path, String & tmp){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");

  while(file.available()){
    tmp+=(char)file.read();
  }
  Serial.println(tmp);

  file.close();
}
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.println(message)){
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}
void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}
void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

void PowerCut(){
  if (FirstLoop){
    FirstLoop=false;
    String  tmp,tmp2;

    readFile(SD, "/powercut.last", tmp);
    int dt = (int)solar_log.system_start - tmp.toInt();
    if (  dt > 0 ){
      now = (DateTime)(tmp.toInt());
      String tmp3 = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );

      tmp2+="Last powercut at " + tmp3 + " \t for: " + String( dt ) + " sec.";
      Serial.println(tmp2);
      appendFile(SD,"/powercut.txt",tmp2.c_str());
    }
  }
  else{
     writeFile(SD, "/powercut.last",String(rtc.now().unixtime()).c_str());
  }

}
const uint8_t LICZBA_DIOD_RGB = 2;
const uint8_t PIN_GPIO = 4;
long i=0;
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(LICZBA_DIOD_RGB, PIN_GPIO, NEO_GRB + NEO_KHZ800);


void setup() {
  FirstLoop=true;
  Serial.begin(115200);  while (!Serial) continue;

  pixel.begin();
  Serial.println(">>>> FRESH REBOOT <<<<<");

  reset_reason();
  if (! rtc.begin()) { Serial.println("Couldn't find RTC"); }
  // RTCadjust();

  solar_log.system_start=rtc.now().unixtime();

  if(!SD.begin()){Serial.println("Card Mount Failed"); return; }
  else{Serial.println("mount OK");}
}

unsigned long data_send_period = 1000 * 60 * 1;
unsigned long ime_last_data_send;




extern unsigned int __bss_end;
extern unsigned int __heap_start;
extern void *__brkval;


float gettemp2(){

  int points = 5;
  float temp[points];

  float temp_ave=0;
  float temp_e=0;


  for (int i = 0; i < points; i++ ) {
    temp[i] = random(20,40);
    temp_ave+=temp[i];
    delay(100);
  }

  temp_ave = temp_ave/points;

  for (int i = 0; i < points; i++ ) temp_e=sq(temp[i]-temp_ave);
  if ( sqrt(temp_e/points) > 1)    gettemp2();

  return temp_ave;
}


float t_max;
float t_min;
int day=0;
int oldday=0;

void minmaxstat(float temp){
  Serial.print(">:: ");
  Serial.print(day);
    Serial.print(":: ");
  Serial.print(temp);
  Serial.print(":: ");
  Serial.print(t_max);
  Serial.print(":: ");
  Serial.println(t_min);

  if( day == oldday){
    if (temp  > t_max) t_max = temp;
    if (t_min > temp ) t_min = temp;
  }
  else{
    Serial.print(day);
    Serial.print(":: ");
    Serial.print(t_min);
    Serial.print(" :: ");
    Serial.println(t_max);
    t_max=t_min=0;

  }


  oldday = day;

}


int ii=1;
void loop() {
Serial.println(ii);




  //   float tt=gettemp2();
  //
  // minmaxstat(tt);
  // if (ii++ % 3 == 0) day++;
  // delay(1000);



  // PowerCut();
  //
  Serial.println(rtc.now().unixtime() - solar_log.system_start);
  // //
  now = rtc.now();
  String tmp = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() ) +"\r";
  // //
  // // appendFile(SD, "/data2.txt", tmp.c_str());
  // // writeFile(SD, "/data1.txt",tmp.c_str());
  // //
  Serial.println(tmp);
  delay(1000);

}
