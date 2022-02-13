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
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define DHTPIN 23   //DHT11 DATA 数据引脚
#define DHTTYPE DHT11  //选择的类型

#define HALLPIN1 33
#define HALLPIN2 18
#define HALLPIN3 16
#define HALLPIN4 17
#define CLOCK_SDA_PIN 21
#define CLOCK_SCL_PIN 22
#define DUMP_ENERGY_PIN 32
#define RGBLUMINANCE 5
#define SLEEPTIME 1//睡眠时间
//#define wifi_sta
#define SAVELOG
#define LEDS_COUNT  1    //彩灯数目
#define LEDS_PIN	25    //ESP32控制ws2812的引脚
#define CHANNEL		0    //控制通道，最多8路
//#define test

const char* ssid = "web_show";//设置热点名称(自定义)
const char* password = "1234567890";//设置热点密码(自定义)
const char* ssid_sta = "abc";//设置热点名称(自定义)
const char* password_sta = "1234567890";//设置热点密码(自定义)
AsyncWebServer server(80);//web服务器端口号
RTC_DS3231 rtc;
DHT dht(DHTPIN , DHTTYPE);
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT , LEDS_PIN , CHANNEL , TYPE_GRB);//申请一个彩灯控制对象

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
	uint32_t code = 0;
	bool state = false;
} VerificationCode_;

double sendRunData_R[4][60] = {};//4个跑轮，每个60个数据
int sendHumiture_R[2][24] = {};//温湿度，24小时数据
int time_num_mm = 0;//用于记录跑动数据存储数组的索引值
int time_num_hh = 0;//用于记录温湿度存储数组的索引值
//float mouseRunNum1 = 0 , mouseRunNum2 = 0 , mouseRunNum3 = 0 , mouseRunNum4 = 0;

bool loginSYS = false;//是否已经注册过设备（低功耗）

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
volatile double runData1 = 0 , runData2 = 0 , runData3 = 0 , runData4 = 0;//老鼠跑动的数据

u8 m_color[5][3] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0} };//彩灯颜色数组

void showData(int data , bool printType = false , bool type = true);
void showData(double data , bool printType = false , bool type = true);
void showData(String data , bool printType = false , bool type = true);

void setup()
{

	Serial.begin(115200);//开启串口打印

	//SerialBT.begin("mouse");
	//初始化闪存系统
	Serial.print("正在打开闪存系统...");
	Serial.print("test");
	strip.begin();      //初始化彩灯控制引脚
	strip.setBrightness(RGBLUMINANCE);//设置彩灯亮度
	strip.setLedColorData(0 , m_color[0][0] , m_color[0][1] , m_color[0][2]);//指定彩灯显示的颜色
	strip.show();//显示彩灯，不调用时彩灯不显示
	while ( !SPIFFS.begin(true) )
	{
		Serial.print("...");
		delay(1000);
	}
	Serial.println("OK!");
	// 可用空间总和（单位：字节）
	Serial.print("可用空间总和: ");
	Serial.print(SPIFFS.totalBytes());
	Serial.println(" Bytes");

	// 已用空间（单位：字节）
	Serial.print("已用空间: ");
	Serial.print(SPIFFS.usedBytes());
	Serial.println(" Bytes");

	Wire.begin(CLOCK_SDA_PIN , CLOCK_SCL_PIN);

	if ( !rtc.begin() )
	{
		Serial.println("时钟未连接/未连接正确");
		Serial.flush();
		while ( 1 ) delay(10);
	}

	if ( rtc.lostPower() )
	{
		Serial.println("时钟时间错误，现在修改，稍后请务必手动校准时间");
		rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
	}
	/*Serial.println(__DATE__);
	Serial.println(__DATE__== "Jan 2 2022");*/
	//rtc.adjust(DateTime(F(__DATE__) , F(__TIME__)));
	pinMode(HALLPIN1 , INPUT_PULLUP);
	pinMode(HALLPIN2 , INPUT_PULLUP);
	pinMode(HALLPIN3 , INPUT_PULLUP);
	pinMode(HALLPIN4 , INPUT_PULLUP);

	//设置中断触发程序

	dht.begin();

#ifndef wifi_sta
	WiFi.softAP(ssid , password);
#endif // !1

	//#ifdef wifi_sta
	  //#endif // wifi_sta
	  //savaRunData();
	  //saveHumitureData();
	  //模拟个文件，方便调试
	File dataFile = SPIFFS.open("/runData/20220101" , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
	dataFile.print("This is test file");       // 向dataFile写入字符串信息
	dataFile.close();
	saveLog("设备开机");
	//更新当前索引存储数据的
	DateTime now = rtc.now();
	time_num_mm = now.minute();
	pinMode(0 , INPUT);

	WiFi.mode(WIFI_MODE_NULL);

	strip.setLedColorData(0 , m_color[1][0] , m_color[1][1] , m_color[1][2]);//指定彩灯显示的颜色
	strip.show();//显示彩灯，不调用时彩灯不显示
	delay(3000);
	strip.setLedColorData(0 , m_color[4][0] , m_color[4][1] , m_color[4][2]);//指定彩灯显示的颜色
	strip.show();//显示彩灯，不调用时彩灯不显示
	xTaskCreatePinnedToCore(
		runData_tack                                     //创建任务
		, "runData_tack"                                 //创建任务的名称
		, 1024 * 2                                   //分配的运行空间大小
		, NULL                                       //任务句柄
		, 2                                           //任务优先级
		, NULL                                        //回调函数句柄
		, 1);
	xTaskCreatePinnedToCore(
		showWifi                                     //创建任务
		, "showWifi"                                 //创建任务的名称
		, 1024 * 2                                   //分配的运行空间大小
		, NULL                                       //任务句柄
		, 3                                           //任务优先级
		, NULL                                        //回调函数句柄
		, 1);
}

void showWifi(void* pvParameters)
{
	int num = 0;
	int rgbNum = RGBLUMINANCE;
	while ( true )
	{
		if ( WiFi.getMode() != WIFI_MODE_NULL )
		{
			strip.setBrightness(rgbNum--);
			if ( rgbNum < 1 )
			{
				rgbNum = RGBLUMINANCE;
			}
			strip.setLedColorData(0 , m_color[2][0] , m_color[2][1] , m_color[2][2]);//指定彩灯显示的颜色
			strip.show();//显示彩灯，不调用时彩灯不显示
		}
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
			for ( size_t i = 0; i < 3; i++ )
			{
				strip.setLedColorData(0 , m_color[2][0] , m_color[2][1] , m_color[2][2]);//指定彩灯显示的颜色
				strip.show();//显示彩灯，不调用时彩灯不显示
				vTaskDelay(300);
				strip.setLedColorData(0 , m_color[4][0] , m_color[4][1] , m_color[4][2]);//指定彩灯显示的颜色
				strip.show();//显示彩灯，不调用时彩灯不显示
				vTaskDelay(300);
			}

			if ( WiFi.getMode() == WIFI_MODE_NULL )
			{
				Serial.println(!loginSYS);
				if ( !loginSYS )
				{
					Serial.println("正在注册设备");
					WiFi.mode(WIFI_MODE_APSTA);
					WiFi.softAP(ssid , password);
					WiFi.begin(ssid_sta , password_sta);
					Serial.print("正在连接到：");
					Serial.print(ssid_sta);
					int conn_try_num = 0;//尝试连接wifi的时间
					while ( WiFi.status() != WL_CONNECTED )
					{ // 尝试进行wifi连接。
						delay(1000);
						conn_try_num++;
						Serial.print(".");
						if ( conn_try_num > 30 )
						{
							Serial.println("wifi连接超时。。。");
							break;
						}
					}
					Serial.print("IP address:\t");            // 以及
					Serial.println(WiFi.localIP());           // NodeMCU的IP地址



					Serial.print("Access Point: ");    // 通过串口监视器输出信息
					Serial.println(ssid);              // 告知用户NodeMCU所建立的WiFi名
					Serial.print("IP address: ");      // 以及NodeMCU的IP地址
					Serial.println(WiFi.softAPIP());   // 通过调用WiFi.softAPIP()可以得到NodeMCU的IP地址

					if ( SPIFFS.exists("/index.html") )
					{
						Serial.println("存在控制台文件，开启服务器");
						AsyncStaticWebHandler* handler = &server.serveStatic("/log" , SPIFFS , "/").setDefaultFile("log");
						handler->setAuthentication("log" , "admin");
						AsyncStaticWebHandler* handler2 = &server.serveStatic("/runData" , SPIFFS , "/runData/");
						handler2->setCacheControl("max-age=1");
						//handler2->setAuthentication("data" , "admin");
						AsyncStaticWebHandler* handler1 = &server.serveStatic("/" , SPIFFS , "/").setDefaultFile("index.html");
						handler1->setAuthentication("admin" , "admin");
						handler1->setLastModified("Fri, 14 Jan 2022 15:43:02 GMT");

						server.on("/reqRunData" , HTTP_POST , sendRunData);//给前端反馈跑动信息
						server.on("/reqHumiture" , HTTP_POST , sendHumiture);//给前端反馈温湿度信息
						server.on("/getServerTime" , HTTP_POST , getServerTime);
						server.on("/setServerTime" , HTTP_POST , setServerTime);
						server.on("/getdumpEnergy" , HTTP_POST , getdumpEnergy);
						server.on("/getAllFileName" , HTTP_POST , sendFileName);
						server.on("/getVerificationCode" , HTTP_POST , getVerificationCode);
						server.on("/restoreFactorySet" , HTTP_POST , restoreFactorySet);
						server.begin();//开启web服务器 

						//WiFi.mode(WIFI_MODE_NULL);
					} else
					{
						Serial.println("不存在控制台文件");
					}
					loginSYS = true;
					continue;
				}
				if ( WiFi.status() != WL_CONNECTED )
				{
					Serial.print("尝试重新连接wifi中。。。");
					WiFi.disconnect();
					WiFi.reconnect();
				} else
				{
					Serial.print("wifi状态正常");
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
			Serial.print("正在恢复wifi连接，请稍等...");
			int conn_try_num = 0;//尝试连接wifi的时间
			while ( WiFi.status() != WL_CONNECTED )
			{ // 尝试进行wifi连接。
				delay(1000);
				conn_try_num++;
				Serial.print(".");
				if ( conn_try_num > 30 )
				{
					Serial.println("wifi连接超时。。。");
					break;
				}
			}
			if ( WiFi.status() == WL_CONNECTED )
			{
				Serial.print("IP address:\t");            // 以及
				Serial.println(WiFi.localIP());           // NodeMCU的IP地址
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
	  //删除两个日期之间的所有文件文件
	  while ( true )
	  {
		if ( time_.YY == time_yymmdd[0] && time_.MM == time_yymmdd[1] && time_.DD == time_yymmdd[2] )
		{
		  Serial.println("成功！！！");
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
			Serial.println("系统错误！！！");
			return;
		  }
		}
	  }
	  //向后调时，如果跨天需检查跳转的时间有没有文件
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
		Serial.println("当天的文件删除成功");

	  } else
	  {
		Serial.println("当天的文件删除失败");
		request->send(500 , "text/plain" , "FILE_DELETION_FAILED");
		ESP.restart();
	  }
	} else
	{//向后调时，如果跨天需检查跳转的时间有没有文件
	  DateTime now = rtc.now();
	  if ( time_.DD != now.day() )
	  {
		Serial.println("向后调时，如果跨天需检查跳转的时间有没有文件！！！");
		return;
	  } else
	  {
		Serial.println("未跨天！！！");
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
	DATE += " ";//如果时间设置失败尝试改为1/2个空格
	DATE += time_.DD;
	DATE += " ";
	DATE += time_.YY;
	Serial.println(DATE);
	Serial.println(time_.time);
	Serial.print("写入数据DATE:");
	Serial.println(DATE);
	Serial.print("写入数据TIME:");
	Serial.println(time_.time);
	rtc.adjust(DateTime(DATE.c_str() , time_.time.c_str()));
	Serial.print("设置时间:成功");
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
		Serial.print("请求老鼠跑动数据:");

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
		Serial.print("请求温湿度数据");
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
	request->send(200 , "text/plain" , getAllFileName(false));
}

void getVerificationCode(AsyncWebServerRequest* request)
{
	VerificationCode_.state = true;
	VerificationCode_.code = random(100000 , 999999);
	Serial.print("验证码:");
	Serial.println(VerificationCode_.code);
	request->send(200);
}

void restoreFactorySet(AsyncWebServerRequest* request)
{
	uint32_t code = request->arg("code").toInt();
	Serial.print("用户输入验证码:");
	Serial.println(code);
	if ( VerificationCode_.state && code == VerificationCode_.code )
	{
		request->send(200);
		getAllFileName(true);
	} else
	{
		request->send(500);
	}
}

String getAllFileName(bool delFile)
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
			if ( delFile )
			{
				Serial.println("恢复出厂设置完毕！");
				ESP.restart();
			}
			// 如果没有文件则跳出循环
			break;
		}
		String data = entry.name();
		if ( delFile )
		{
			Serial.print("删除文件：");
			Serial.println(data);
			SPIFFS.remove(data);
			continue;
		}
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
	//映射出电压
	uint8_t voltage = map(adNum , 0 , 4095 , 0 , 33);
	if ( !( 27 <= voltage * 2 && voltage * 2 <= 42 ) )
	{
		Serial.print("电压实际读数：");
		Serial.print(voltage);
		Serial.println("V");
		return 101;//返回错误值
	}
	return map(voltage * 2 , 27 , 42 , 0 , 100);
}

//向客户端发送老鼠跑动的数据
String sendData_mouseRun(String date)
{
	//if ( date.length()!=11 )//hhhhmmdd@hh
	//{
	//    Serial.println("请求日期有误");
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

//向客户端发送温湿度数据
String sendData_humiture(String date)
{
	//if ( date.length() != 8 )//hhhhmmdd
	//{
	//    Serial.println("请求日期有误");
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

//保存老鼠跑动的数据
void savaRunData()
{
	//必须结束中断，否则容易被狗复位

	Serial.print("闪存剩余空间: ");
	int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
	Serial.print(freeSpace);
	Serial.println(" Bytes");
	if ( freeSpace < 3000 )
	{
		Serial.println("存储空间告急！！！");
	} else if ( freeSpace < 1500 )
	{
		Serial.println("存储空间已经用尽，请手动清理！！！");
		return;
	} else
	{
		//Serial.println("存储空间充足！！！");
		showData("存储空间充足！！！" , true);
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

	/* Serial.print("当前老鼠跑动的数据存储文件名：");
	 Serial.println(fileName);*/

	showData("当前老鼠跑动的数据存储文件名：" , true , false);
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
	File dataFile = SPIFFS.open(fileName , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
	dataFile.println(runData);       // 向dataFile写入字符串信息
	dataFile.close();

	saveLog("savaRunData");
}

//保存温湿度
void saveHumitureData()
{
	//必须结束中断，否则容易被狗复位

	Serial.print("闪存剩余空间: ");
	int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
	Serial.print(freeSpace);
	Serial.println(" Bytes");
	if ( freeSpace < 3000 )
	{
		Serial.println("存储空间告急！！！");
	} else if ( freeSpace < 1500 )
	{
		Serial.println("存储空间已经用尽，请手动清理！！！");
		return;
	} else
	{
		Serial.println("存储空间充足！！！");
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
	Serial.print("当前温湿度的数据存储文件名：");
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
	File dataFile = SPIFFS.open(fileName , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
	dataFile.println(runData);       // 向dataFile写入字符串信息
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
		if ( VerificationCodeTime > 1 )
		{
			VerificationCode_.state = false;
		}
	}
#ifndef test
	delay(60 * 1000);//等待一分钟
	wifiConnNum++;
	if ( wifiConnNum >= 5 )//wifi连接5分钟自动关闭
	{
		if ( WiFi.getMode() != WIFI_MODE_NULL )
		{
			Serial.println("断开wifi");
			WiFi.disconnect();//断开wifi
			WiFi.mode(WIFI_MODE_NULL);//wifi模式为不连接
			WiFi.setSleep(true);//wifi开始休眠
			strip.setLedColorData(0 , m_color[4][0] , m_color[4][1] , m_color[4][2]);//指定彩灯显示的颜色
			strip.show();//显示彩灯，不调用时彩灯不显示
			wifiConnNum = 0;
		}

	}
	if ( runData1 == 0 && runData2 == 0 && runData3 == 0 && runData4 == 0 )
	{
		mouseRestTime++;
		Serial.print("这一分钟老鼠没有跑动，如果该状态持续，设备将在");
		Serial.print(SLEEPTIME - mouseRestTime);
		Serial.println("分钟后休眠");
	} else
	{
		mouseRestTime = 0;
		/*Serial.println("这一分钟老鼠没有跑动");*/
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
		String sData_t = "时间:";
		sData_t += nowTimeStr;
		//free(nowTimeStr);
		//nowTimeStr = NULL;
		sData_t += "\t跑轮1数据：";
		sData_t += runData1;
		sData_t += "\t跑轮2数据：";
		sData_t += runData2;
		sData_t += "\t跑轮3数据：";
		sData_t += runData3;
		sData_t += "\t跑轮4数据：";
		sData_t += runData4;
		showData(sData_t , true);
		//写入文件
		Serial.print("闪存剩余空间: ");
		int freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
		Serial.print(freeSpace);
		Serial.println(" Bytes");
		if ( freeSpace < 3000 )
		{
			Serial.println("存储空间告急！！！");
		} else if ( freeSpace < 1500 )
		{
			Serial.println("存储空间已经用尽，请手动清理！！！");
			return;
		} else
		{
			//Serial.println("存储空间充足！！！");
			showData("存储空间充足！！！" , true);
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

		Serial.print("当前操作的文件：");
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

					Serial.println("读取温湿度异常,再次尝试读取中。。。");
					h = dht.readHumidity();
					t = dht.readTemperature();
					if ( isnan(h) || isnan(t) )
					{
						Serial.println("再次读取失败！");
						th.add(0);
						th.add(0);
					} else
					{
						th.add(t);
						th.add(h);
					}
				} else
				{
					th.add(t);
					th.add(h);
				}


				serializeJson(doc , outData);
				outData += "@";
				nowWriteDay = now.day();
				//新的一天
				showData("新的一天！！！" , true);
				Serial.print("写入数据：");
				Serial.println(outData);
				File dataFile = SPIFFS.open(fileName , "w");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
				dataFile.print(outData);       // 向dataFile写入字符串信息
				dataFile.close();

			} else
			{

				Serial.println("刚刚开机，第一次写入");
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

					Serial.println("读取温湿度异常,再次尝试读取中。。。");
					h = dht.readHumidity();
					t = dht.readTemperature();
					if ( isnan(h) || isnan(t) )
					{
						Serial.println("再次读取失败！");
						th.add(0);
						th.add(0);
					} else
					{
						th.add(t);
						th.add(h);
					}
				} else
				{
					th.add(t);
					th.add(h);
				}

				serializeJson(doc , outData);
				outData += "@";
				nowWriteDay = now.day();
				//天数没有发生变化
				showData("继续写这一天！！！" , true);
				Serial.print("写入数据：");
				Serial.println(outData);
				File dataFile = SPIFFS.open(fileName , "a");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
				dataFile.print(outData);       // 向dataFile写入字符串信息
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

					Serial.println("读取温湿度异常,再次尝试读取中。。。");
					h = dht.readHumidity();
					t = dht.readTemperature();
					if ( isnan(h) || isnan(t) )
					{
						Serial.println("再次读取失败！");
						th.add(0);
						th.add(0);
					} else
					{
						th.add(t);
						th.add(h);
					}
				} else
				{
					th.add(t);
					th.add(h);
				}

				serializeJson(doc , outData);
				outData += "@";
				nowWriteDay = now.day();
				//天数没有发生变化
				showData("继续写这一天！！！" , true);
				Serial.print("写入数据：");
				Serial.println(outData);
				File dataFile = SPIFFS.open(fileName , "a");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
				dataFile.print(outData);       // 向dataFile写入字符串信息
				dataFile.close();
			} else
			{
				Serial.println("系统写入文件，效验时间出错");
				saveLog("系统写入文件，效验时间出错");
			}
		}



	}
	if ( mouseRestTime >= SLEEPTIME )
	{
		Serial.println("设备即将休眠");
		saveLog("设备休眠");

		if ( WiFi.getMode() != WIFI_MODE_NULL )
		{
			Serial.println("断开wifi");
			WiFi.disconnect();//断开wifi
			WiFi.mode(WIFI_MODE_NULL);//wifi模式为不连接
			WiFi.setSleep(true);//wifi开始休眠
			wifiConnNum = 0;
			strip.setLedColorData(0 , m_color[4][0] , m_color[4][1] , m_color[4][2]);//指定彩灯显示的颜色
			strip.show();//显示彩灯，不调用时彩灯不显示
		}

		//进入低功耗模式
		esp_sleep_enable_ext0_wakeup(GPIO_NUM_33 , 0);//设置外部按键唤醒
		//esp_sleep_enable_timer_wakeup(20000000);

		delay(1000);
		esp_light_sleep_start();//开始睡眠
		//esp_deep_sleep_start();
		//Serial.println(esp_sleep_get_wakeup_cause());
		String log_ = "设备被唤醒，唤醒类型@";
		log_ += esp_sleep_get_wakeup_cause();
		saveLog(log_);
		Serial.print("设备被唤醒@");
		DateTime now1 = rtc.now();//更新时间
		  //时钟唤醒
		switch ( esp_sleep_get_wakeup_cause() ) //获取唤醒原因
		{
			case ESP_SLEEP_WAKEUP_TIMER: Serial.println("通过定时器唤醒"); break;
			case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("通过触摸唤醒"); break;
			case ESP_SLEEP_WAKEUP_EXT0: Serial.println("使用 RTC_IO 的外部信号引起的唤醒"); break;
			case ESP_SLEEP_WAKEUP_EXT1: Serial.println("使用 RTC_CNTL 的外部信号引起的唤醒"); break;
			case ESP_SLEEP_WAKEUP_ULP: Serial.println("通过ULP唤醒"); break;
			default: Serial.println("并非从DeepSleep中唤醒"); break;
		}
		//time_num_mm = now1.minute();
	}

#endif // !test

	time_num_mm++;

#ifdef test
	delay(1000);//等待一分钟
	wifiConnNum++;
	if ( wifiConnNum >= 30 )//wifi连接5分钟自动关闭
	{
		if ( WiFi.getMode() != WIFI_MODE_NULL )
		{
			Serial.println("断开wifi");
			WiFi.disconnect();//断开wifi
			WiFi.mode(WIFI_MODE_NULL);//wifi模式为不连接
			WiFi.setSleep(true);//wifi开始休眠
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
		Serial.println("设备即将休眠");

		DateTime now = rtc.now();
		int nowMinute = now.minute();
		int timeNum = 60 - nowMinute;
		Serial.print(timeNum - 1);
		Serial.println("分钟后自动唤醒");
		mouseRestTime = 0;
		//将这一个时间点后的所以数据填充为0
		for ( size_t i = nowMinute; i < timeNum; i++ )
		{
			sendRunData_R[0][nowMinute - 1] = 0;
			sendRunData_R[1][nowMinute - 1] = 0;
			sendRunData_R[2][nowMinute - 1] = 0;
			sendRunData_R[3][nowMinute - 1] = 0;
		}
		//进入低功耗模式
		esp_sleep_enable_ext0_wakeup(GPIO_NUM_33 , 0);//设置外部按键唤醒
		esp_sleep_enable_timer_wakeup(( (uint64_t) (timeNum) -1 ) * 60 * 1000 * 1000);//如果没有被外部触发到这一个小时的59分自动唤醒，已保持这一个小时的数据
		delay(1000);
		esp_light_sleep_start();//开始睡眠
		//Serial.println(esp_sleep_get_wakeup_cause());
		Serial.print("设备被唤醒@");
		DateTime now1 = rtc.now();//更新时间
		if ( now.minute() > now1.minute() )
		{
			//时钟唤醒
			switch ( esp_sleep_get_wakeup_cause() ) //获取唤醒原因

			{

				case ESP_SLEEP_WAKEUP_TIMER: Serial.println("通过定时器唤醒"); break;

				case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("通过触摸唤醒"); break;

				case ESP_SLEEP_WAKEUP_EXT0: Serial.println("通过EXT0唤醒"); break;

				case ESP_SLEEP_WAKEUP_EXT1: Serial.println("通过EXT1唤醒"); break;

				case ESP_SLEEP_WAKEUP_ULP: Serial.println("通过ULP唤醒"); break;

				default: Serial.println("并非从DeepSleep中唤醒"); break;

			}
			time_num_mm = now1.minute();
		} else
		{
			//外部唤醒
			switch ( esp_sleep_get_wakeup_cause() ) //获取唤醒原因

			{

				case ESP_SLEEP_WAKEUP_TIMER: Serial.println("通过定时器唤醒"); break;

				case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("通过触摸唤醒"); break;

				case ESP_SLEEP_WAKEUP_EXT0: Serial.println("通过EXT0唤醒"); break;

				case ESP_SLEEP_WAKEUP_EXT1: Serial.println("通过EXT1唤醒"); break;

				case ESP_SLEEP_WAKEUP_ULP: Serial.println("通过ULP唤醒"); break;

				default: Serial.println("并非从DeepSleep中唤醒"); break;

			}
			//保存上一个小时的数据
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

	/*if ( time_num_mm >= 59 )//数据量达到一小时
	{
	  float h = dht.readHumidity();
	  float t = dht.readTemperature();
	  if ( isnan(h) || isnan(t) )
	  {
		Serial.println("温湿度读取异常！");
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
	  //向闪存中写入这一小时老鼠跑动的数据
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

//中断服务程序
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

		File dataFile = SPIFFS.open("/log" , "a");// 建立File对象用于向SPIFFS中的file对象（即/notes.txt）写入信息
		dataFile.println(wData);       // 向dataFile写入字符串信息
		dataFile.close();
		showData("\t日志保存成功" , true);
	} else
	{
		showData("\t不存在日志文件" , true);
	}


}