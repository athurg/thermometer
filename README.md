# Wi-Fi温度计(Thermometer)

基于ESP8266和SHT30的Wi-Fi温度计。获取温度，并以[Prometheus](https://prometheus.io)采集指标的格式导出。

## 使用前配置

首先下载[ESP8266的IDF框架](https://github.com/espressif/ESP8266_RTOS_SDK)。并配置环境变量`IDF_PATH`为该框架的目录。

然后按照该框架[README](https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/README.md)描述的，下载对应环境下的编译工具链，并将工具链的`bin`目录配置到`PATH`。

最后下载本项目，使用 `make menuconfig` 进行配置。主要配置项：

* Example Connection Configuration ->
  1. WIFI SSID: 设置Wi-Fi热点的SSID
  2. WIFI Password: 设置Wi-Fi热点的密码
* Serial flasher config ->
  1. Default serial port: 设置串口设备路径
* Component config ->
  1. SHT3x Configuration: 设置SHT30连接的端口信息

## 使用方法

首次执行，请通过`make flash monitor`编译烧写所有组建并监控。

此后修改代码，只需要执行`make app app-flash monitor`编译烧写应用代码即可。

## 温湿度获取地址

所有的温度、湿度以及SHT30芯片的序列号，都以Prometheus指标的形式，暴露在/metrics 路径下。

你可以通过浏览器直接访问 `http://IP/metrics` 获取。

由于默认情况下IDF不支持浮点数的打印，因此温度、湿度的数值都必须是整数。将获取到的值除以100就是真实的数据。
