#ifndef FINGERPRINT_MODULE_H
#define FINGERPRINT_MODULE_H

#include <Arduino.h>

// 定义指纹模组的UART引脚 (ESP32S3)
#define FINGERPRINT_RX 17 // RX引脚连接到指纹模组的TX
#define FINGERPRINT_TX 18 // TX引脚连接到指纹模组的RX

#define fingerprintSerial Serial1

// 定义指令包格式
const uint8_t HEADER_HIGH = 0xEF;           // 高字节头
const uint8_t HEADER_LOW = 0x01;            // 低字节头
const uint32_t DEVICE_ADDRESS = 0xFFFFFFFF; // 设备地址

// 定义指令码
const uint8_t CMD_GET_IMAGE = 0x01;        // 获取图像
const uint8_t CMD_GEN_CHAR = 0x02;         // 生成特征
const uint8_t CMD_MATCH = 0x03;            // 精确比对指纹
const uint8_t CMD_SEARCH = 0x04;           // 搜索指纹
const uint8_t CMD_REG_MODEL = 0x05;        // 合并特征
const uint8_t CMD_STORE_CHAR = 0x06;       // 存储模板
const uint8_t CMD_CLEAR_LIB = 0x0D;        // 清空指纹库
const uint8_t CMD_READ_SYSPARA = 0x0F;     // 读模组基本参数
const uint8_t CMD_READ_INDEX_TABLE = 0x1f; // 读取索引表



// 状态机定义
enum ReceiveState
{
  WAITING_FOR_HEADER, // 等待头字节
  RECEIVING_DATA      // 接收数据字节
};

// 全局变量
extern ReceiveState receiveState;
extern unsigned long lastDataTime;
extern uint8_t dataBuffer[256]; // 数据缓冲区，假设最大256字节
extern int bufferIndex;
extern bool packetReceived; // 数据包接收完成标志位

extern int IndexTable[50]; // 索引表，用于存储搜索到的指纹索引
extern int tableNum;   // 索引表中存储的指纹索引数量
extern String newsMessage;

// 函数声明  
bool    init_FP();                                                        // 指纹初始化
bool read_FP_info();                                                      // 读取指纹模组基本参数
bool register_FP();                                                       // 注册指纹
bool search_FP();                                                         // 搜索指纹
bool clear_FP_all_lib();                                                  // 清空指纹库
bool PS_ReadIndexTable();                                                 // 读取索引表
void send_cmd(uint8_t cmd);                                               // 发送无参数指令
void send_cmd2(uint8_t cmd, uint8_t param1);                              // 发送带1个参数的指令
void sendCommand(uint8_t cmd, uint8_t param1, uint16_t param2);           // 发送带2个参数的指令
void sendCommand1(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3); // 发送带3个参数的指令
String getStatusDescription(uint8_t status);                              // 获取状态描述
void processFingerprintSerial();                                          // 处理指纹串口数据
void processMessage();                                                    // 处理消息缓冲

#endif // FINGERPRINT_MODULE_H
