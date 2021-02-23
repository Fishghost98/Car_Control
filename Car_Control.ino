/*DC电机     运行状态    IN1    IN2    IN3     IN4
 * 电机A     反转（调速） 1/PWM   0
 * 电机A     正转（调速） 0     1/PWM
 * 刹车                   0       0
 * 空转                   1       1
 * 电机B     反转（调速）               1/PWM    0
 * 电机B     正转（调速）                 0     1/PWM
 * 刹车                                   0      0
 * 空转                                   1      1
 * This example code is in the public domain.
*/
#include <ESP8266WiFi.h>      // 本程序使用ESP8266WiFi库
#include <ESP8266WiFiMulti.h> // 本程序使用ESP8266WiFiMulti库
#include <ESP8266WebServer.h> // 本程序使用ESP8266WebServer库
#include <Servo.h>

#define WIFI_MODE 1                // 1: AP模式，NodeMCU自身起一个wifi信号；2：STA模式，NodeMCU连上一个已有wifi。
#define SSID_AP "NodeMCU_WiFi_Car" // for AP mode
#define PASSWORD_AP "12345678"     // for AP mode

ESP8266WiFiMulti wifiMulti; // 建立ESP8266WiFiMulti对象,对象名称是'wifiMulti'

IPAddress local_ip(192, 168, 1, 1); //IP for AP mode
IPAddress gateway(192, 168, 1, 1);  //IP for AP mode
IPAddress subnet(255, 255, 255, 0); //IP for AP mode
ESP8266WebServer server(80);        // 建立网络服务器对象，该对象用于响应HTTP请求。监听端口（80）

// 电机设置
#define RIGHT_MOTOR_PIN1 13 // 右一 (D7)
#define RIGHT_MOTOR_PIN2 15 // 右二 (D8)
#define LEFT_MOTOR_PIN1 12  // 左一 (D6)
#define LEFT_MOTOR_PIN2 14  // 左二 (D5)
int MOTOR_SPEED = 1200;     // 电机转速

int car_mode = 0;
int last_mode;
Servo servo;
//定义超声波模块引脚及变量
const int trigPin = 4; //D2
const int echoPin = 5; //D1

//舵机控制引脚
int servoPin = 2;

int nowDistance;
int newDistance = 10;
bool isFollow = true;
bool isAvoidAuto = false;

void setup()
{
  pinMode(RIGHT_MOTOR_PIN1, OUTPUT);
  pinMode(RIGHT_MOTOR_PIN2, OUTPUT);
  pinMode(LEFT_MOTOR_PIN1, OUTPUT);
  pinMode(LEFT_MOTOR_PIN2, OUTPUT);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  
  servo.attach(2);   
  servo.write(90);
  delay(1000);
  

  car_control(); // stop the car

  Serial.begin(9600);

  if (WIFI_MODE == 1)
  { // 默认进入AP模式，否者STA模式
    WiFi.softAP(SSID_AP, PASSWORD_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
  }
  else
  {                                                              // STA mode
                                                                 //通过addAp函数存储  WiFi名称       WiFi密码
    wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1"); // 将需要连接的一系列WiFi ID和密码输入这里
    wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2"); 
    wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3"); 
    Serial.println("Connecting ...");                            
    int i = 0;
    while (wifiMulti.run() != WL_CONNECTED)
    { // 在当前环境中搜索addAP函数所存储的WiFi
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      Serial.print(i++);
      Serial.print('.'); // 将会连接信号最强的那一个WiFi信号。
    }
    // WiFi连接成功后将通过串口监视器输出连接成功信息
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println('\n');           // WiFi连接成功后
    Serial.print("Connected to ");  // NodeMCU将通过串口监视器输出。
    Serial.println(WiFi.SSID());    // 连接的WiFI名称
    Serial.print("IP address:\t");  // 以及
    Serial.println(WiFi.localIP()); // NodeMCU的IP地址
  }

  if (SPIFFS.begin())
  { // 启动闪存文件系统
    Serial.println("SPIFFS Started.");
  }
  else
  {
    Serial.println("SPIFFS Failed to Start.");
  }

  server.on("/forward", handle_Forward); // 告知系统如何处理请求
  server.on("/backward", handle_Backward);
  server.on("/left", handle_Left);
  server.on("/right", handle_Right);
  server.on("/stop", handle_Stop);
  server.on("/LowSpeed", handle_LowSpeed);
  server.on("/HighSpeed", handle_HighSpeed);
  server.on("/follow", handle_Follow);
  server.on("/avoidAuto", handle_avoidAuto);
  server.onNotFound(handleUserRequest);

  server.begin(); // 启动网站服务
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient(); //处理用户请求
  getDistance();
  if(isAvoidAuto){
    avoidAuto();
  }else{
    car_control();
    if(isFollow){
      FollowCar();
    }
  }
}

//超声波避障
void avoidAuto()
{
  forward();
  if (int(getDistance()) < 30)
  {
    stop();

    servoControl(90, 180);
    delay(100);
    if (int(getDistance()) < 30)
    {h'g
      servoControl(180,0);
      delay(100);
      if (int(getDistance()) < 30)
      {
        servoControl(0,90);
        backward();
        delay(2000);
        turnLeft();
        delay(300);
      }
      else
      {
        servoControl(0,90);
        delay(100);
        turnRight();
        delay(300);
        forward();
      }
    }
    else
    {
      servoControl(180,90);
      delay(100);
      turnLeft();
      delay(300);
      forward();
    }
  }
  else
  {
    forward();
  }
}

//自动行驶（超声波避障）
void handle_avoidAuto()
{
  String autoState = server.arg("autoState");
  if (autoState == "true")
  {
    isAvoidAuto = true;
    Serial.println("AvoidAuto open...");
  }
  else if (autoState == "false")
  {
    isAvoidAuto = false;
    Serial.println("AvoidAuto closed...");
  }
  server.send(200, "text/html");
}

//舵机控制，转动更加自然
void servoControl(int startPos,int endPos)
{
  if (startPos < endPos)
  {
    servo.attach(2);
    for (int pos = startPos; pos < endPos; pos++)
    {
      servo.write(pos);
      delay(15);
    }
    servo.detach();
  }
  else
  {
    servo.attach(2);
    for (int pos = startPos; pos > endPos; pos--)
    {
      servo.write(pos);
      delay(15);
    }
    servo.detach();
  }
}

//获取超声波传感器数据
float getDistance()
{
  float duration;
  float distance;
  // 初始trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  Serial.print("echoPin: ");
  Serial.print(echoPin);
  // 计算距离
  distance = duration * 0.034 / 2.0;
  nowDistance = distance;
  Serial.print("Distance: ");
  Serial.println(distance);
  delay(100);
  return distance;
}

//小车跟随及退让功能
void FollowCar()
{
  servo.detach(); //舵机离线，防止抖动
  if (nowDistance < newDistance + 2)
  {
    car_mode = 1;   
  }
  else if (nowDistance > newDistance + 10 && nowDistance < 40)
  {
    car_mode = 2;
  }
  else
  {
    car_mode = 0;
  }
}

//处理Follow请求
void handle_Follow()
{
  String followState = server.arg("followState");
  if (followState == "true")
  {
    isFollow = true;
    Serial.println("Follow open...");
  }
  else if (followState == "false")
  {
    isFollow = false;
    Serial.println("Follow closed...");
  }
  server.send(200, "text/html");
}

//改变行驶速度
void handle_LowSpeed()
{
  MOTOR_SPEED = 700;
  Serial.println("LowSpeed...");
  server.send(200, "text/html", "低速");
}

//改变行驶速度
void handle_HighSpeed()
{
  MOTOR_SPEED = 1200;
  Serial.println("HighSpeed...");
  server.send(200, "text/html", "高速");
}

//处理急停请求
void handle_Stop()
{
  car_mode = 0;
  Serial.println("Stopped");
  server.send(200, "text/html", "停止前进");
}

//处理前进请求
void handle_Forward()
{
  car_mode = 2;
  Serial.println("Go forward...");
  server.send(200, "text/html", "前进");
}

//处理后退请求
void handle_Backward()
{
  car_mode = 1;
  Serial.println("Go backward...");
  server.send(200, "text/html", "后退");
}

//处理左转请求
void handle_Left()
{
  car_mode = 3;
  Serial.println("Turn left...");
  server.send(200, "text/html", "左转");
}

//处理右转请求
void handle_Right()
{
  car_mode = 4;
  Serial.println("Turn right...");
  server.send(200, "text/html", "右转");
}

// 处理用户浏览器的HTTP访问
void handleUserRequest()
{
  // 获取用户请求资源(Request Resource）
  String reqResource = server.uri();
  Serial.print("reqResource: ");
  Serial.println(reqResource);

  // 通过handleFileRead函数处处理用户请求资源
  bool fileReadOK = handleFileRead(reqResource);

  // 如果在SPIFFS无法找到用户访问的资源，则回复404 (Not Found)
  if (!fileReadOK)
  {
    server.send(404, "text/plain", "404 Not Found");
  }
}

bool handleFileRead(String resource)
{ //处理浏览器HTTP访问

  if (resource.endsWith("/"))
  {                                 // 如果访问地址以"/"为结尾
    resource = "/Car_Control.html"; // 则将访问地址修改为/index.html便于SPIFFS访问
  }

  String contentType = getContentType(resource); // 获取文件类型

  if (SPIFFS.exists(resource))
  {                                         // 如果访问的文件可以在SPIFFS中找到
    File file = SPIFFS.open(resource, "r"); // 则尝试打开该文件
    server.streamFile(file, contentType);   // 并且将该文件返回给浏览器
    file.close();                           // 并且关闭文件
    return true;                            // 返回true
  }
  return false; // 如果文件未找到，则返回false
}

// 获取文件类型
String getContentType(String filename)
{
  if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

//小车控制
void car_control()
{
  switch (car_mode)
  {
  case 0:
    stop();
    break;
  case 1: // go backward
    backward();
    break;
  case 2: // go forward
    forward();
    break;
  case 3: // turn left
    turnLeft();
    break;
  case 4: // turn right
    turnRight();
    break;
  default:
    Serial.println("命令有误！");
    break;
  }
}

//停止
void stop()
{
  digitalWrite(RIGHT_MOTOR_PIN1, LOW);
  digitalWrite(RIGHT_MOTOR_PIN2, LOW);
  digitalWrite(LEFT_MOTOR_PIN1, LOW);
  digitalWrite(LEFT_MOTOR_PIN2, LOW);
}

//前进
void forward()
{
  digitalWrite(RIGHT_MOTOR_PIN1, LOW);
  analogWrite(RIGHT_MOTOR_PIN2, MOTOR_SPEED);
  digitalWrite(LEFT_MOTOR_PIN1, LOW);
  analogWrite(LEFT_MOTOR_PIN2, MOTOR_SPEED);
}

//后退
void backward()
{
  analogWrite(RIGHT_MOTOR_PIN1, MOTOR_SPEED);
  digitalWrite(RIGHT_MOTOR_PIN2, LOW);
  analogWrite(LEFT_MOTOR_PIN1, MOTOR_SPEED);
  digitalWrite(LEFT_MOTOR_PIN2, LOW);
}

//左转
void turnLeft()
{
  analogWrite(RIGHT_MOTOR_PIN1, MOTOR_SPEED);
  digitalWrite(RIGHT_MOTOR_PIN2, LOW);
  digitalWrite(LEFT_MOTOR_PIN1, LOW);
  analogWrite(LEFT_MOTOR_PIN2, MOTOR_SPEED);
}

//右转
void turnRight()
{
  digitalWrite(RIGHT_MOTOR_PIN1, LOW);
  analogWrite(RIGHT_MOTOR_PIN2, MOTOR_SPEED);
  analogWrite(LEFT_MOTOR_PIN1, MOTOR_SPEED);
  digitalWrite(LEFT_MOTOR_PIN2, LOW);
}
