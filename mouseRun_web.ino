#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <SPI.h>
#include "RTClib.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include "DHT.h"
#include "Adafruit_Sensor.h"
#define DHTPIN 23   //DHT11 DATA ��������
#define DHTTYPE DHT11  //ѡ�������

#define HALLPIN1 33
#define HALLPIN2 18
#define HALLPIN3 5
#define HALLPIN4 17
#define CLOCK_SDA_PIN 4
#define CLOCK_SCL_PIN 16
#define DUMP_ENERGY_PIN 32
#define SLEEPTIME 10//˯��ʱ��
#define wifi_sta
#define SAVELOG

//#define test

const char* ssid = "web_show";//�����ȵ�����(�Զ���)
const char* password = "1234567890";//�����ȵ�����(�Զ���)
const char* ssid_sta = "long";//�����ȵ�����(�Զ���)
const char* password_sta = "yy201011";//�����ȵ�����(�Զ���)
AsyncWebServer server(80);//web�������˿ں�
RTC_DS3231 rtc;
DHT dht(DHTPIN , DHTTYPE);

//BluetoothSerial SerialBT;

#ifdef test
int mm_test = 1 , dd_test = 10 , hh_test = 0;
#endif // test


struct Time
{
  uint16_t YY;
  uint8_t MM;
  uint8_t DD;

  uint8_t week;
  String time;
} time_;

struct VerificationCode
{
  uint32_t code=0;
  bool state=false;
} VerificationCode_;

double sendRunData_R[4][60] = {};//4�����֣�ÿ��60������
int sendHumiture_R[2][24] = {};//��ʪ�ȣ�24Сʱ����
int time_num_mm = 0;//���ڼ�¼�ܶ����ݴ洢���������ֵ
int time_num_hh = 0;//���ڼ�¼��ʪ�ȴ洢���������ֵ
//float mouseRunNum1 = 0 , mouseRunNum2 = 0 , mouseRunNum3 = 0 , mouseRunNum4 = 0;


char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
volatile double runData1 = 0 , runData2 = 0 , runData3 = 0 , runData4 = 0;//�����ܶ�������

void showData(int data , bool printType = false , bool type = true);
void showData(double data , bool printType = false , bool type = true);
void showData(String data , bool printType = false , bool type = true);

void setup()
{

  Serial.begin(115200);//�������ڴ�ӡ

  //SerialBT.begin("mouse");
  //��ʼ������ϵͳ
  Serial.print("���ڴ�����ϵͳ...");
  Serial.print("test");
  while ( !SPIFFS.begin(true) )
  {
    Serial.print("...");
    delay(1000);
  }
  Serial.println("OK!");
  // ���ÿռ��ܺͣ���λ���ֽڣ�
  Serial.print("���ÿռ��ܺ�: ");
  Serial.print(SPIFFS.totalBytes());
  Serial.println(" Bytes");

  // ���ÿռ䣨��λ���ֽڣ�
  Serial.print("���ÿռ�: ");
  Serial.print(SPIFFS.usedBytes());
  Serial.println(" Bytes");

  Wire.begin(CLOCK_SDA_PIN , CLOCK_SCL_PIN);

  if ( !rtc.begin() )
  {
    Serial.println("ʱ��δ����/δ������ȷ");
    Serial.flush();
    while ( 1 ) delay(10);
  }

  if ( rtc.lostPower() )
  {
    Serial.println("ʱ��ʱ����������޸ģ��Ժ�������ֶ�У׼ʱ��");
    rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
  }
  /*Serial.println(__DATE__);
  Serial.println(__DATE__== "Jan 2 2022");*/
  //rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
  pinMode(HALLPIN1 , INPUT_PULLUP);
  pinMode(HALLPIN2 , INPUT_PULLUP);
  pinMode(HALLPIN3 , INPUT_PULLUP);
  pinMode(HALLPIN4 , INPUT_PULLUP);

  //�����жϴ�������

  dht.begin();

#ifndef wifi_sta
  WiFi.softAP(ssid , password);
#endif // !1

#ifdef wifi_sta
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(ssid , password);
  WiFi.begin(ssid_sta , password_sta);
  Serial.print("�������ӵ���");
  Serial.print(ssid_sta);
  int conn_try_num = 0;//��������wifi��ʱ��
  while ( WiFi.status() != WL_CONNECTED )
  { // ���Խ���wifi���ӡ�
    delay(1000);
    conn_try_num++;
    Serial.print(".");
    if ( conn_try_num > 30 )
    {
      Serial.println("wifi���ӳ�ʱ������");
      break;
    }
  }
  Serial.print("IP address:\t");            // �Լ�
  Serial.println(WiFi.localIP());           // NodeMCU��IP��ַ
#endif // wifi_sta


  Serial.print("Access Point: ");    // ͨ�����ڼ����������Ϣ
  Serial.println(ssid);              // ��֪�û�NodeMCU��������WiFi��
  Serial.print("IP address: ");      // �Լ�NodeMCU��IP��ַ
  Serial.println(WiFi.softAPIP());   // ͨ������WiFi.softAPIP()���Եõ�NodeMCU��IP��ַ

  if ( SPIFFS.exists("/index.html") )
  {
    Serial.println("���ڿ���̨�ļ�������������");
    AsyncStaticWebHandler* handler = &server.serveStatic("/log" , SPIFFS , "/").setDefaultFile("log");
    AsyncStaticWebHandler* handler2 = &server.serveStatic("/runData" , SPIFFS , "/runData/");
    handler2->setCacheControl("max-age=1");
    handler2->setAuthentication("data" , "admin");
    if ( WiFi.status() == WL_CONNECTED )
    {
      AsyncStaticWebHandler* handler1 = &server.serveStatic("/" , SPIFFS , "/").setDefaultFile("index_conn.html");
      handler1->setAuthentication("admin" , "admin");
      handler1->setLastModified("Fri, 14 Jan 2022 15:43:02 GMT");
    } else
    {
      AsyncStaticWebHandler* handler1 = &server.serveStatic("/" , SPIFFS , "/").setDefaultFile("index.html");
      handler1->setAuthentication("admin" , "admin");
      handler1->setLastModified("Fri, 14 Jan 2022 15:43:02 GMT");
    }
    handler->setAuthentication("log" , "admin");

    server.on("/reqRunData" , HTTP_POST , sendRunData);//��ǰ�˷����ܶ���Ϣ
    server.on("/reqHumiture" , HTTP_POST , sendHumiture);//��ǰ�˷�����ʪ����Ϣ
    server.on("/getServerTime" , HTTP_POST , getServerTime);
    server.on("/setServerTime" , HTTP_POST , setServerTime);
    server.on("/getdumpEnergy" , HTTP_POST , getdumpEnergy);
    server.on("/getAllFileName" , HTTP_POST , sendFileName);
    server.on("/getVerificationCode" , HTTP_POST , getVerificationCode);
    server.on("/restoreFactorySet" , HTTP_POST , restoreFactorySet);
    server.begin();//����web������ 

    //WiFi.mode(WIFI_MODE_NULL);
  } else
  {
    Serial.println("�����ڿ���̨�ļ�");
  }
  //savaRunData();
  //saveHumitureData();
  //ģ����ļ����������
  File dataFile = SPIFFS.open("/runData/20220101" , "w");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
  dataFile.print("This is test file");       // ��dataFileд���ַ�����Ϣ
  dataFile.close();
  saveLog("�豸����");
  //���µ�ǰ�����洢���ݵ�
  DateTime now = rtc.now();
  time_num_mm = now.minute();
  pinMode(0 , INPUT);
  xTaskCreatePinnedToCore(
    runData_tack                                     //��������
    , "runData_tack"                                 //�������������
    , 1024 * 2                                   //��������пռ��С
    , NULL                                       //������
    , 2                                           //�������ȼ�
    , NULL                                        //�ص��������
    , 1);
  xTaskCreatePinnedToCore(
    showWifi                                     //��������
    , "showWifi"                                 //�������������
    , 1024 * 2                                   //��������пռ��С
    , NULL                                       //������
    , 3                                           //�������ȼ�
    , NULL                                        //�ص��������
    , 1);
}

void showWifi(void* pvParameters)
{
  int num = 0;
  while ( true )
  {
    if ( !digitalRead(0) )
    {
      num++;
      Serial.println(num);
    } else
    {
      num = 0;
    }
    if ( num == 3 )
    {
      num = 0;
      if ( WiFi.getMode() != WIFI_MODE_NULL )
      {
        if ( WiFi.status() != WL_CONNECTED )
        {
          Serial.print("������������wifi�С�����");
          WiFi.disconnect();
          WiFi.reconnect();
        } else
        {
          Serial.print("wifi״̬����");
        }
        continue;
      }
      /* WiFi.mode(WIFI_MODE_NULL);
       vTaskDelay(1000);*/
      WiFi.setSleep(false);
      num = 0;
      WiFi.mode(WIFI_MODE_APSTA);
      WiFi.softAP(ssid , password);
      WiFi.begin(ssid_sta , password_sta);
      Serial.print("���ڻָ�wifi���ӣ����Ե�...");
      int conn_try_num = 0;//��������wifi��ʱ��
      while ( WiFi.status() != WL_CONNECTED )
      { // ���Խ���wifi���ӡ�
        delay(1000);
        conn_try_num++;
        Serial.print(".");
        if ( conn_try_num > 30 )
        {
          Serial.println("wifi���ӳ�ʱ������");
          break;
        }
      }
      if ( WiFi.status() == WL_CONNECTED )
      {
        Serial.print("IP address:\t");            // �Լ�
        Serial.println(WiFi.localIP());           // NodeMCU��IP��ַ
      }

      //vTaskDelay(10000);
      //WiFi.mode(WIFI_MODE_NULL);
    }
    vTaskDelay(1000);
  }
}

void runData_tack1(void* pvParameters)
{
  while ( true )
  {
    if ( digitalRead(HALLPIN1) == LOW )
    {
      while ( true )
      {
        if ( digitalRead(HALLPIN1) == HIGH )
        {
          runData1 += 0.25;
          showData("mouseRun1 " , true , false);
          showData(runData1 , true);
          break;
        }
        vTaskDelay(25);
      }
    }
    vTaskDelay(12);
  }
}

void runData_tack2(void* pvParameters)
{
  while ( true )
  {
    if ( digitalRead(HALLPIN2) == LOW )
    {
      while ( true )
      {
        if ( digitalRead(HALLPIN2) == HIGH )
        {
          runData2 += 0.25;
          showData("mouseRun2 " , true , false);
          showData(runData2 , true);
          break;
        }
        vTaskDelay(25);
      }
    }
    vTaskDelay(10);
  }
}

void runData_tack3(void* pvParameters)
{
  while ( true )
  {
    if ( digitalRead(HALLPIN3) == LOW )
    {
      while ( true )
      {
        if ( digitalRead(HALLPIN3) == HIGH )
        {
          runData3 += 0.25;
          showData("mouseRun3 " , true , false);
          showData(runData3 , true);
          break;
        }
        vTaskDelay(25);
      }
    }
    vTaskDelay(8);
  }
}

void runData_tack4(void* pvParameters)
{
  while ( true )
  {
    if ( digitalRead(HALLPIN4) == LOW )
    {
      while ( true )
      {
        if ( digitalRead(HALLPIN4) == HIGH )
        {
          runData4 += 0.25;
          showData("mouseRun4 " , true , false);
          showData(runData4 , true);
          break;
        }
        vTaskDelay(25);
      }
    }
    vTaskDelay(9);
  }
}

void runData_tack(void* pvParameters)
{
  volatile bool state[4] = {};
  while ( true )
  {
    if ( digitalRead(HALLPIN1) == LOW )
    {
      state[0] = true;
    }
    if ( digitalRead(HALLPIN2) == LOW )
    {
      state[1] = true;
    }
    if ( digitalRead(HALLPIN3) == LOW )
    {
      state[2] = true;
    }
    if ( digitalRead(HALLPIN4) == LOW )
    {
      state[3] = true;
    }
    if ( state[0] && digitalRead(HALLPIN1) == HIGH )
    {
      runData1 += 0.25;
      showData("mouseRun1 " , true , false);
      showData(runData1 , true);
      state[0] = false;
    }
    if ( state[1] && digitalRead(HALLPIN2) == HIGH )
    {
      runData2 += 0.25;
      showData("mouseRun2 " , true , false);
      showData(runData2 , true);
      state[1] = false;
    }
    if ( state[2] && digitalRead(HALLPIN3) == HIGH )
    {
      runData3 += 0.25;
      showData("mouseRun3 " , true , false);
      showData(runData3 , true);
      state[2] = false;
    }
    if ( state[3] && digitalRead(HALLPIN4) == HIGH )
    {
      runData4 += 0.25;
      showData("mouseRun4 " , true , false);
      showData(runData4 , true);
      state[3] = false;
    }
    delay(10);
  }
}
void getServerTime(AsyncWebServerRequest* request)
{
  DateTime now = rtc.now();

  String fileName = "";
  fileName += now.year();
  fileName += "-";
  fileName += now.month();
  fileName += "-";
  fileName += now.day();
  fileName += " ";
  fileName += now.hour();
  fileName += ":";
  fileName += now.minute();
  fileName += ":";
  fileName += now.second();
  fileName += " ";
  /*String MM_[12] = { "Jan","Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
  fileName += MM_[now.dayOfTheWeek()];*/
  Serial.println(fileName);
  request->send(200 , "text/plain" , fileName);
}
void setServerTime(AsyncWebServerRequest* request)
{
  String reqDate;
  time_.YY = request->arg("yy").toInt();
  time_.MM = request->arg("mm").toInt();
  time_.DD = request->arg("dd").toInt();
  //int deldata = request->arg("deldata").toInt();
  /*if ( deldata == 1 )
  {
    DateTime now = rtc.now();
    int time_yymmdd[3] = { now.year() ,now.month(),now.day() };
    //ɾ����������֮��������ļ��ļ�
    while ( true )
    {
      if ( time_.YY == time_yymmdd[0] && time_.MM == time_yymmdd[1] && time_.DD == time_yymmdd[2] )
      {
        Serial.println("�ɹ�������");
        String fileName = "/runData/";
        fileName += now.year();
        if ( now.month() < 10 )
        {
          fileName += "0";
        }
        fileName += now.month();
        if ( now.day() < 10 )
        {
          fileName += "0";
        }
        fileName += now.day();
        if ( true )
        {

        }
        SPIFFS.remove(fileName);
        return;
      } else
      {
        time_yymmdd[2]--;
        if ( time_yymmdd[2] < 1 )
        {
          time_yymmdd[2] = 31;
          time_yymmdd[1]--;
        }
        if ( time_yymmdd[1] < 1 )
        {
          time_yymmdd[1] = 12;
          time_yymmdd[0]--;
        }
        if ( time_yymmdd[0] < 2022 )
        {
          Serial.println("ϵͳ���󣡣���");
          return;
        }
      }
    }
    //����ʱ���������������ת��ʱ����û���ļ�
    String fileName = "/runData/";
    fileName += now.year();
    if ( now.month() < 10 )
    {
      fileName += "0";
    }
    fileName += now.month();
    if ( now.day() < 10 )
    {
      fileName += "0";
    }
    fileName += now.day();
    SPIFFS.remove(fileName);
    if ( !SPIFFS.exists(fileName) )
    {
      Serial.println("������ļ�ɾ���ɹ�");

    } else
    {
      Serial.println("������ļ�ɾ��ʧ��");
      request->send(500 , "text/plain" , "FILE_DELETION_FAILED");
      ESP.restart();
    }
  } else
  {//����ʱ���������������ת��ʱ����û���ļ�
    DateTime now = rtc.now();
    if ( time_.DD != now.day() )
    {
      Serial.println("����ʱ���������������ת��ʱ����û���ļ�������");
      return;
    } else
    {
      Serial.println("δ���죡����");
      return;
    }
  }*/
  Serial.print(time_.YY);
  Serial.print("\t");
  Serial.print(time_.MM);
  Serial.print("\t");
  Serial.println(time_.DD);
  time_.week = request->arg("week").toInt();
  time_.time = request->arg("time");
  String MM_[12] = { "Jan","Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
  String DATE = "";
  DATE += MM_[time_.MM - 1];
  DATE += "  ";
  DATE += time_.DD;
  DATE += " ";
  DATE += time_.YY;
  Serial.println(DATE);
  Serial.println(time_.time);
  Serial.print("д������DATE:");
  Serial.println(DATE);
  Serial.print("д������TIME:");
  Serial.println(time_.time);
  rtc.adjust(DateTime(DATE.c_str() , time_.time.c_str()));
  Serial.print("����ʱ��:�ɹ�");
  /*if ( deldata == 1 )
  {
    ESP.restart();
  }*/
  request->send(200 , "text/plain" , "setTime_OK");
}
void sendRunData(AsyncWebServerRequest* request)
{
  String reqDate;
  reqDate += "/";
  if ( request->hasArg("date") )
  {
    reqDate += request->arg("date");
    Serial.print("���������ܶ�����:");

    Serial.println(reqDate);

    request->send(200 , "text/plain" , sendData_mouseRun(reqDate));
  }
}
void sendHumiture(AsyncWebServerRequest* request)
{
  String reqDate;
  reqDate += "/";
  if ( request->hasArg("date") )
  {
    reqDate += request->arg("date");
    Serial.print("������ʪ������");
    Serial.println(reqDate);
    request->send(200 , "text/plain" , sendData_humiture(reqDate));
  }
}
void getdumpEnergy(AsyncWebServerRequest* request)
{
  request->send(200 , "text/plain" , (String) getEnergy());
}

void sendFileName(AsyncWebServerRequest* request)
{
  request->send(200 , "text/plain" , getAllFileName());
}

void getVerificationCode(AsyncWebServerRequest* request)
{
  VerificationCode_.state = true;
  VerificationCode_.code = random(100000,999999);
  Serial.print("��֤��:");
  Serial.println(VerificationCode_.code);
  request->send(200);
}

void restoreFactorySet(AsyncWebServerRequest* request)
{
  uint32_t code = request->arg("code").toInt();
  Serial.print("�û�������֤��:");
  Serial.println(code);
  if ( VerificationCode_.state && code== VerificationCode_.code )
  {
    request->send(200);
  } else
  {
    request->send(500);
  }
}

String getAllFileName()
{
  File myFile = SPIFFS.open("/runData");
  String outData;
  StaticJsonDocument<2048> doc;
  JsonArray fileName = doc.createNestedArray("fileName");
  //fileName.add("20220101");
  bool state = true;
  int i = 0;
  while ( true )
  {

    File entry = myFile.openNextFile();
    i++;
    if ( !entry )
    {
      // ���û���ļ�������ѭ��
      break;
    }
    String data = entry.name();

    fileName.add(data.substring(9));
    Serial.print(data.substring(9));
    entry.close();
    if ( i >= 60 )
    {
      state = false;
    }
  }
  doc["state"] = state;

  serializeJson(doc , outData);
  return outData;
}

uint8_t getEnergy()
{
  //max4.2 min2.7
  int adNum = analogRead(DUMP_ENERGY_PIN);
  //ӳ�����ѹ
  uint8_t voltage = map(adNum , 0 , 4095 , 0 , 33);
  if ( !( 27 <= voltage * 2 && voltage * 2 <= 42 ) )
  {
    Serial.print("��ѹʵ�ʶ�����");
    Serial.print(voltage);
    Serial.println("V");
    return 101;//���ش���ֵ
  }
  return map(voltage * 2 , 27 , 42 , 0 , 100);
}

//��ͻ��˷��������ܶ�������
String sendData_mouseRun(String date)
{
  //if ( date.length()!=11 )//hhhhmmdd@hh
  //{
  //    Serial.println("������������");
  //    return "err:Incorrect request date";
  //}
  if ( SPIFFS.exists(date) )
  {
    File dataFile = SPIFFS.open(date , "r");
    long dataSize = dataFile.size();
    String fsData;
    for ( int i = 0; i < dataSize; i++ )
    {
      fsData += (char) dataFile.read();
    }
    dataFile.close();
    return fsData;
  } else
  {
    showData("err:The file was not found" , true);
    return "err:The file was not found";
  }
}

//��ͻ��˷�����ʪ������
String sendData_humiture(String date)
{
  //if ( date.length() != 8 )//hhhhmmdd
  //{
  //    Serial.println("������������");
  //    return "err:Incorrect request date";
  //}
  if ( SPIFFS.exists(date) )
  {
    File dataFile = SPIFFS.open(date , "r");
    long dataSize = dataFile.size();
    String fsData;
    for ( int i = 0; i < dataSize; i++ )
    {
      fsData += (char) dataFile.read();
    }
    dataFile.close();
    return fsData;
  } else
  {
    return "err:The file was not found";
  }
}

//���������ܶ�������
void savaRunData()
{
  //��������жϣ��������ױ�����λ

  Serial.print("����ʣ��ռ�: ");
  int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  Serial.print(freeSpace);
  Serial.println(" Bytes");
  if ( freeSpace < 3000 )
  {
    Serial.println("�洢�ռ�漱������");
  } else if ( freeSpace < 1500 )
  {
    Serial.println("�洢�ռ��Ѿ��þ������ֶ���������");
    return;
  } else
  {
    //Serial.println("�洢�ռ���㣡����");
    showData("�洢�ռ���㣡����" , true);
  }
  String runData;

  DynamicJsonDocument doc(4096);
  DateTime now = rtc.now();

  String fileName = "/";
  fileName += now.year();
  if ( now.month() < 10 )
  {
    fileName += "0";
}
  fileName += now.month();
  if ( now.day() < 10 )
  {
    fileName += "0";
  }
  fileName += now.day();
  fileName += "@";
  fileName += now.hour();
#ifdef test
  if ( mm_test > 12 )
  {
    mm_test = 1;
  }
  if ( dd_test > 28 )
  {
    dd_test = 1;
  }
  if ( hh_test > 24 )
  {
    hh_test = 1;
  }
  fileName = "/2022";
  if ( mm_test < 10 )
  {
    fileName += "0";
  }
  fileName += mm_test;
  if ( dd_test < 10 )
  {
    fileName += "0";
  }
  fileName += dd_test;
  fileName += "@";
  fileName += hh_test;
#endif // test

  /* Serial.print("��ǰ�����ܶ������ݴ洢�ļ�����");
   Serial.println(fileName);*/

  showData("��ǰ�����ܶ������ݴ洢�ļ�����" , true , false);
  showData(fileName , true);
  doc["dataType"] = "runData";
  doc["date"] = fileName;//hhhhyydd@hh

  JsonArray data1 = doc.createNestedArray("data1");
  //data1.add(10);

  for ( size_t j = 0; j < 60; j++ )
  {
    data1.add(sendRunData_R[0][j]);
  }


  JsonArray data2 = doc.createNestedArray("data2");
  for ( size_t j = 0; j < 60; j++ )
  {
    data2.add(sendRunData_R[1][j]);
  }


  JsonArray data3 = doc.createNestedArray("data3");

  for ( size_t j = 0; j < 60; j++ )
  {
    data3.add(sendRunData_R[2][j]);
  }

  JsonArray data4 = doc.createNestedArray("data4");

  for ( size_t j = 0; j < 60; j++ )
  {
    data4.add(sendRunData_R[3][j]);
  }

  serializeJson(doc , runData);

  //Serial.println(fileName);
  //Serial.println(runData);
  showData(runData , true);
  File dataFile = SPIFFS.open(fileName , "w");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
  dataFile.println(runData);       // ��dataFileд���ַ�����Ϣ
  dataFile.close();

  saveLog("savaRunData");
}

//������ʪ��
void saveHumitureData()
{
  //��������жϣ��������ױ�����λ

  Serial.print("����ʣ��ռ�: ");
  int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  Serial.print(freeSpace);
  Serial.println(" Bytes");
  if ( freeSpace < 3000 )
  {
    Serial.println("�洢�ռ�漱������");
  } else if ( freeSpace < 1500 )
  {
    Serial.println("�洢�ռ��Ѿ��þ������ֶ���������");
    return;
  } else
  {
    Serial.println("�洢�ռ���㣡����");
  }

  String runData;

  StaticJsonDocument<1024> doc;

  DateTime now = rtc.now();
  String fileName = "/";
  fileName += now.year();
  if ( now.month() < 10 )
  {
    fileName += "0";
  }
  fileName += now.month();
  if ( now.day() < 10 )
  {
    fileName += "0";
  }
  fileName += now.day();

#ifdef test
  if ( mm_test > 12 )
  {
    mm_test = 1;
  }
  if ( dd_test > 28 )
  {
    dd_test = 1;
  }
  fileName = "/2022";
  if ( mm_test < 10 )
  {
    fileName += "0";
  }
  fileName += mm_test;
  if ( dd_test < 10 )
  {
    fileName += "0";
  }
  fileName += dd_test;

#endif // test
  Serial.print("��ǰ��ʪ�ȵ����ݴ洢�ļ�����");
  Serial.println(fileName);
  doc["dataType"] = "humitureData";
  doc["date"] = fileName;//hhhhmmdd

  JsonArray temperature = doc.createNestedArray("temperature");
  for ( size_t i = 0; i < 24; i++ )
  {
    temperature.add(sendHumiture_R[0][i]);
  }

  JsonArray humidity = doc.createNestedArray("humidity");
  for ( size_t i = 0; i < 24; i++ )
  {
    humidity.add(sendHumiture_R[1][i]);
  }
  serializeJson(doc , runData);

  Serial.println(fileName);
  Serial.println(runData);
  File dataFile = SPIFFS.open(fileName , "w");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
  dataFile.println(runData);       // ��dataFileд���ַ�����Ϣ
  dataFile.close();


  saveLog("saveHumitureData");
}


uint8_t wifiConnNum = 0;
uint8_t mouseRestTime = 0;
uint8_t nowWriteDay = 0;
uint8_t VerificationCodeTime = 0;
void loop()
{
  if ( VerificationCode_.state )
  {
    VerificationCodeTime++;
    if ( VerificationCodeTime>1 )
    {
      VerificationCode_.state = false;
    }
  }
#ifndef test
  delay(60 * 1000);//�ȴ�һ����
  wifiConnNum++;
  if ( wifiConnNum >= 5 )//wifi����5�����Զ��ر�
  {
    if ( WiFi.getMode() != WIFI_MODE_NULL )
    {
      Serial.println("�Ͽ�wifi");
      WiFi.disconnect();//�Ͽ�wifi
      WiFi.mode(WIFI_MODE_NULL);//wifiģʽΪ������
      WiFi.setSleep(true);//wifi��ʼ����
      wifiConnNum = 0;
    }

  }
  if ( runData1 == 0 && runData2 == 0 && runData3 == 0 && runData4 == 0 )
  {
    mouseRestTime++;
    Serial.print("��һ��������û���ܶ��������״̬�������豸����");
    Serial.print(SLEEPTIME - mouseRestTime);
    Serial.println("���Ӻ�����");
  } else
  {
    mouseRestTime = 0;
    /*Serial.println("��һ��������û���ܶ�");*/
    DateTime now = rtc.now();
    String nowTimeStr = "";
    nowTimeStr += now.year();
    nowTimeStr += "-";
    nowTimeStr += now.month();
    nowTimeStr += "-";
    nowTimeStr += now.day();
    nowTimeStr += " ";
    nowTimeStr += now.hour();
    nowTimeStr += ":";
    nowTimeStr += now.minute();
    nowTimeStr += ":";
    nowTimeStr += now.second();
    String sData_t = "ʱ��:";
    sData_t += nowTimeStr;
    //free(nowTimeStr);
    //nowTimeStr = NULL;
    sData_t += "\t����1���ݣ�";
    sData_t += runData1;
    sData_t += "\t����2���ݣ�";
    sData_t += runData2;
    sData_t += "\t����3���ݣ�";
    sData_t += runData3;
    sData_t += "\t����4���ݣ�";
    sData_t += runData4;
    showData(sData_t , true);
    //д���ļ�
    Serial.print("����ʣ��ռ�: ");
    int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    Serial.print(freeSpace);
    Serial.println(" Bytes");
    if ( freeSpace < 3000 )
    {
      Serial.println("�洢�ռ�漱������");
    } else if ( freeSpace < 1500 )
    {
      Serial.println("�洢�ռ��Ѿ��þ������ֶ���������");
      return;
    } else
    {
      //Serial.println("�洢�ռ���㣡����");
      showData("�洢�ռ���㣡����" , true);
    }

    String fileName = "/runData/";
    fileName += now.year();
    if ( now.month() < 10 )
    {
      fileName += "0";
    }
    fileName += now.month();
    if ( now.day() < 10 )
    {
      fileName += "0";
    }
    fileName += now.day();

    Serial.print("��ǰ�������ļ���");
    Serial.println(fileName);

    if ( now.day() != nowWriteDay )
    {
      if ( !SPIFFS.exists(fileName) )
      {
        String outData = "";
        StaticJsonDocument<192> doc;

        String Time = "";
        Time += now.year();
        Time += "-";
        Time += now.month();
        Time += "-";
        Time += now.day();
        Time += " ";
        Time += now.hour();
        Time += ":";
        Time += now.minute();
        Time += ":00";

        doc["t"] = Time;

        JsonArray p = doc.createNestedArray("p");
        p.add(runData1);
        p.add(runData2);
        p.add(runData3);
        p.add(runData4);

        JsonArray th = doc.createNestedArray("th");
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        if ( isnan(h) || isnan(t) )
        {
          th.add(0);
          th.add(0);
          Serial.println("��ȡ��ʪ���쳣��");
        } else
        {
          th.add(t);
          th.add(h);
        }


        serializeJson(doc , outData);
        outData += "@";
        nowWriteDay = now.day();
        //�µ�һ��
        showData("�µ�һ�죡����" , true);
        Serial.print("д�����ݣ�");
        Serial.println(outData);
        File dataFile = SPIFFS.open(fileName , "w");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
        dataFile.print(outData);       // ��dataFileд���ַ�����Ϣ
        dataFile.close();

      } else
      {

        Serial.println("�ոտ�������һ��д��");
        String outData = "";
        StaticJsonDocument<192> doc;

        String Time = "";
        Time += now.year();
        Time += "-";
        Time += now.month();
        Time += "-";
        Time += now.day();
        Time += " ";
        Time += now.hour();
        Time += ":";
        Time += now.minute();
        Time += ":00";

        doc["t"] = Time;

        JsonArray p = doc.createNestedArray("p");
        p.add(runData1);
        p.add(runData2);
        p.add(runData3);
        p.add(runData4);

        JsonArray th = doc.createNestedArray("th");
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        if ( isnan(h) || isnan(t) )
        {
          th.add(0);
          th.add(0);
        } else
        {
          th.add(t);
          th.add(h);
        }

        serializeJson(doc , outData);
        outData += "@";
        nowWriteDay = now.day();
        //����û�з����仯
        showData("����д��һ�죡����" , true);
        Serial.print("д�����ݣ�");
        Serial.println(outData);
        File dataFile = SPIFFS.open(fileName , "a");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
        dataFile.print(outData);       // ��dataFileд���ַ�����Ϣ
        dataFile.close();
      }
    } else
    {
      if ( SPIFFS.exists(fileName) )
      {
        String outData = "";
        StaticJsonDocument<192> doc;

        String Time = "";
        Time += now.year();
        Time += "-";
        Time += now.month();
        Time += "-";
        Time += now.day();
        Time += " ";
        Time += now.hour();
        Time += ":";
        Time += now.minute();
        Time += ":00";

        doc["t"] = Time;

        JsonArray p = doc.createNestedArray("p");
        p.add(runData1);
        p.add(runData2);
        p.add(runData3);
        p.add(runData4);

        JsonArray th = doc.createNestedArray("th");
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        if ( isnan(h) || isnan(t) )
        {
          th.add(0);
          th.add(0);
        } else
        {
          th.add(t);
          th.add(h);
        }

        serializeJson(doc , outData);
        outData += "@";
        nowWriteDay = now.day();
        //����û�з����仯
        showData("����д��һ�죡����" , true);
        Serial.print("д�����ݣ�");
        Serial.println(outData);
        File dataFile = SPIFFS.open(fileName , "a");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
        dataFile.print(outData);       // ��dataFileд���ַ�����Ϣ
        dataFile.close();
      } else
      {
        Serial.println("ϵͳд���ļ���Ч��ʱ�����");
        saveLog("ϵͳд���ļ���Ч��ʱ�����");
      }
    }



  }
  if ( mouseRestTime >= SLEEPTIME )
  {
    Serial.println("�豸��������");
    saveLog("�豸����");

    if ( WiFi.getMode() != WIFI_MODE_NULL )
    {
      Serial.println("�Ͽ�wifi");
      WiFi.disconnect();//�Ͽ�wifi
      WiFi.mode(WIFI_MODE_NULL);//wifiģʽΪ������
      WiFi.setSleep(true);//wifi��ʼ����
      wifiConnNum = 0;
    }

    //����͹���ģʽ
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33 , 0);//�����ⲿ��������
    delay(1000);
    esp_light_sleep_start();//��ʼ˯��
    //Serial.println(esp_sleep_get_wakeup_cause());
    String log_ = "�豸�����ѣ���������@";
    log_ += esp_sleep_get_wakeup_cause();
    saveLog(log_);
    Serial.print("�豸������@");
    DateTime now1 = rtc.now();//����ʱ��
      //ʱ�ӻ���
    switch ( esp_sleep_get_wakeup_cause() ) //��ȡ����ԭ��
    {
      case ESP_SLEEP_WAKEUP_TIMER: Serial.println("ͨ����ʱ������"); break;
      case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("ͨ����������"); break;
      case ESP_SLEEP_WAKEUP_EXT0: Serial.println("ʹ�� RTC_IO ���ⲿ�ź�����Ļ���"); break;
      case ESP_SLEEP_WAKEUP_EXT1: Serial.println("ʹ�� RTC_CNTL ���ⲿ�ź�����Ļ���"); break;
      case ESP_SLEEP_WAKEUP_ULP: Serial.println("ͨ��ULP����"); break;
      default: Serial.println("���Ǵ�DeepSleep�л���"); break;
    }
    //time_num_mm = now1.minute();
  }

#endif // !test

  time_num_mm++;

#ifdef test
  delay(1000);//�ȴ�һ����
  wifiConnNum++;
  if ( wifiConnNum >= 30 )//wifi����5�����Զ��ر�
  {
    if ( WiFi.getMode() != WIFI_MODE_NULL )
    {
      Serial.println("�Ͽ�wifi");
      WiFi.disconnect();//�Ͽ�wifi
      WiFi.mode(WIFI_MODE_NULL);//wifiģʽΪ������
      WiFi.setSleep(true);//wifi��ʼ����
      wifiConnNum = 0;
    }

  }
  if ( runData1 == 0 )
  {
    mouseRestTime++;
  } else
  {
    mouseRestTime = 0;
  }
  if ( mouseRestTime >= 15 )
  {
    Serial.println("�豸��������");

    DateTime now = rtc.now();
    int nowMinute = now.minute();
    int timeNum = 60 - nowMinute;
    Serial.print(timeNum - 1);
    Serial.println("���Ӻ��Զ�����");
    mouseRestTime = 0;
    //����һ��ʱ����������������Ϊ0
    for ( size_t i = nowMinute; i < timeNum; i++ )
    {
      sendRunData_R[0][nowMinute - 1] = 0;
      sendRunData_R[1][nowMinute - 1] = 0;
      sendRunData_R[2][nowMinute - 1] = 0;
      sendRunData_R[3][nowMinute - 1] = 0;
    }
    //����͹���ģʽ
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33 , 0);//�����ⲿ��������
    esp_sleep_enable_timer_wakeup(( (uint64_t) (timeNum) -1 ) * 60 * 1000 * 1000);//���û�б��ⲿ��������һ��Сʱ��59���Զ����ѣ��ѱ�����һ��Сʱ������
    delay(1000);
    esp_light_sleep_start();//��ʼ˯��
    //Serial.println(esp_sleep_get_wakeup_cause());
    Serial.print("�豸������@");
    DateTime now1 = rtc.now();//����ʱ��
    if ( now.minute() > now1.minute() )
    {
      //ʱ�ӻ���
      switch ( esp_sleep_get_wakeup_cause() ) //��ȡ����ԭ��

      {

        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("ͨ����ʱ������"); break;

        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("ͨ����������"); break;

        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("ͨ��EXT0����"); break;

        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("ͨ��EXT1����"); break;

        case ESP_SLEEP_WAKEUP_ULP: Serial.println("ͨ��ULP����"); break;

        default: Serial.println("���Ǵ�DeepSleep�л���"); break;

      }
      time_num_mm = now1.minute();
  } else
    {
      //�ⲿ����
      switch ( esp_sleep_get_wakeup_cause() ) //��ȡ����ԭ��

      {

        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("ͨ����ʱ������"); break;

        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("ͨ����������"); break;

        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("ͨ��EXT0����"); break;

        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("ͨ��EXT1����"); break;

        case ESP_SLEEP_WAKEUP_ULP: Serial.println("ͨ��ULP����"); break;

        default: Serial.println("���Ǵ�DeepSleep�л���"); break;

      }
      //������һ��Сʱ������
      time_num_mm = now1.minute();
    }
}
  //delay(2000);
  sendRunData_R[0][time_num_mm - 1] = runData1;
  sendRunData_R[1][time_num_mm - 1] = runData2;
  sendRunData_R[2][time_num_mm - 1] = runData3;
  sendRunData_R[3][time_num_mm - 1] = runData4;
  String sData_t = (String) time_num_mm;
  sData_t += " minute:runData1@";
  sData_t += sendRunData_R[0][time_num_mm - 1];
  sData_t += "\trunData2@";
  sData_t += sendRunData_R[1][time_num_mm - 1];
  sData_t += "\trunData3@";
  sData_t += sendRunData_R[2][time_num_mm - 1];
  sData_t += "\trunData4@";
  sData_t += sendRunData_R[3][time_num_mm - 1];
  showData(sData_t , true);
#endif // test

  runData1 = 0 , runData2 = 0 , runData3 = 0 , runData4 = 0;

  /*if ( time_num_mm >= 59 )//�������ﵽһСʱ
  {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if ( isnan(h) || isnan(t) )
    {
      Serial.println("��ʪ�ȶ�ȡ�쳣��");
      sendHumiture_R[0][time_num_hh] = 0;
      sendHumiture_R[1][time_num_hh] = 0;
    #ifdef test
      hh_test++;
      sendHumiture_R[0][time_num_hh] = random(0 , 100);
      sendHumiture_R[1][time_num_hh] = random(0 , 100);
    #endif // test
    } else
    {
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %t");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.println(" *C ");
      sendHumiture_R[0][time_num_hh] = t;
      sendHumiture_R[1][time_num_hh] = h;
    #ifdef test
      hh_test++;
      sendHumiture_R[0][time_num_hh] = random(0 , 100);
      sendHumiture_R[1][time_num_hh] = random(0 , 100);
    #endif // test

    }
    time_num_hh++;
    //��������д����һСʱ�����ܶ�������
    //savaRunData();
    time_num_mm = 0;

    if ( time_num_hh >= 23 )
    {
      time_num_hh = 0;
      //saveHumitureData();
    #ifdef test
      dd_test++;
    #endif // test

    }
    }*/
}

//�жϷ������
//void mouseRun1()
//{
//    runData1 += 0.25;
//    /* Serial.print("mouseRun1 ");
//     Serial.println(runData1);*/
//    showData("mouseRun1 " , true , false);
//    showData(runData1 , true);
//}
//void mouseRun2()
//{
//    runData2 += 0.25;
//    Serial.print("mouseRun2 ");
//    Serial.println(runData2);
//}
//void mouseRun3()
//{
//    runData3 += 0.25;
//    Serial.print("mouseRun3 ");
//    Serial.println(runData3);
//}
//void mouseRun4()
//{
//    runData4 += 0.25;
//    Serial.print("mouseRun4 ");
//    Serial.println(runData4);
//}

void showData(String data , bool printType , bool type)
{
  if ( type )
  {
    Serial.println(data);
    if ( printType )
    {
      //SerialBT.println(data);
    }
  } else
  {
    Serial.print(data);
    if ( printType )
    {
      //SerialBT.print(data);
    }
  }
}

void showData(double data , bool printType , bool type)
{
  if ( type )
  {
    Serial.println(data);
    if ( printType )
    {
      //SerialBT.println(data);
    }
  } else
  {
    Serial.print(data);
    if ( printType )
    {
      //SerialBT.print(data);
    }
  }
}

void showData(int data , bool printType , bool type)
{
  if ( type )
  {
    Serial.println(data);
    if ( printType )
    {
      //SerialBT.println(data);
    }
  } else
  {
    Serial.print(data);
    if ( printType )
    {
      //SerialBT.print(data);
    }
  }
}

void saveLog(String data)
{
#ifndef SAVELOG
  return;
#endif // savaLog

  DateTime now = rtc.now();
  String date = "";
  date += now.year();
  date += "-";
  date += now.month();
  date += "-";
  date += now.day();
  date += " ";
  date += now.hour();
  date += ":";
  date += now.minute();
  date += ":";
  date += now.second();
  Serial.print(date);



  if ( SPIFFS.exists("/log") )
  {
    String wData = date;
    wData += "  \t";
    wData += data;
    wData += "\n";

    File dataFile = SPIFFS.open("/log" , "a");// ����File����������SPIFFS�е�file���󣨼�/notes.txt��д����Ϣ
    dataFile.println(wData);       // ��dataFileд���ַ�����Ϣ
    dataFile.close();
    showData("\t��־����ɹ�" , true);
  } else
  {
    showData("\t��������־�ļ�" , true);
  }


}