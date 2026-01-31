#include "FingerprintModule.h"

// 全局变量定义 - 状态机和通信相关
ReceiveState receiveState = WAITING_FOR_HEADER; // 当前接收状态：等待包头或接收数据
unsigned long lastDataTime = 0;                 // 最后接收数据的时间戳，用于超时检测
uint8_t dataBuffer[256];                        // 数据缓冲区，存储接收到的数据包
int bufferIndex = 0;                            // 当前缓冲区写入位置
bool packetReceived = false;                    // 数据包接收完成标志位
// 定义缓冲区ID
uint8_t BUFFER_ID = 0; // 缓冲区ID，用于标识不同的指令包

// 定义模板存储位置
uint16_t TEMPLATE_ID = 0; // 模板ID，用于存储和检索指纹模板
// 指纹库管理变量
int IndexTable[50]; // 索引表，存储已注册指纹的ID（最多50个）
int tableNum = 0;   // 当前索引表中的指纹数量
String newsMessage; // 消息缓冲区，用于批量输出信息

/**
 * 初始化指纹模组 - 发送初始化指令
 * @return true: 成功初始化, false: 初始化失败
 */
bool init_FP()
{

    bool isSuccess = false; // 操作结果标志

    fingerprintSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
    unsigned long startTime = millis();
    while ((millis() - startTime < 5000))
    {
        isSuccess = read_FP_info();
        if (isSuccess)
        {
            break;
        }
    }

    if (isSuccess)
    {
        startTime = millis();
        while ((millis() - startTime < 5000))
        {
            processFingerprintSerial();
            isSuccess = PS_ReadIndexTable();
            if (isSuccess)
            {
                break;
            }
        }
    }

    return isSuccess;
}
/**
 * 读取指纹模组基本参数 - 使用状态机实现异步读取
 * @return true: 成功获取参数, false: 获取失败
 */
bool read_FP_info()
{
    static int state = 0;   // 状态机状态变量
    bool isSuccess = false; // 操作结果标志

    processFingerprintSerial(); // 处理串口接收数据

    switch (state)
    {
    case 0: // 发送读取系统参数指令
    {
        Serial.println("ZW101指纹模组信息:"); // 打印标题
        send_cmd(CMD_READ_SYSPARA);           // 发送读取系统参数指令
        state++;                              // 进入下一状态
        break;
    }
    case 1: // 等待串口数据接收完成
    {
        if (packetReceived) // 检查数据包是否接收完成
        {
            state++;                // 进入下一状态
            packetReceived = false; // 重置接收标志
        }
        break;
    }
    case 2: // 解析接收到的参数数据
    {
        // 检查确认码是否为成功(0x00)
        if (dataBuffer[9] == 0x00)
        {
            // 从数据缓冲区解析各个参数（大端字节序）
            uint16_t register_cnt = (uint16_t)(dataBuffer[10] << 8) | dataBuffer[11];                                                  // 已注册指纹数量
            uint16_t fp_temp_size = (uint16_t)(dataBuffer[12] << 8) | dataBuffer[13];                                                  // 模板大小
            uint16_t fp_lib_size = (uint16_t)(dataBuffer[14] << 8) | dataBuffer[15];                                                   // 指纹库容量
            uint16_t score_level = (uint16_t)(dataBuffer[16] << 8) | dataBuffer[17];                                                   // 匹配等级
            uint32_t device_addr = (uint32_t)(dataBuffer[18] << 24) | (dataBuffer[19] << 16) | (dataBuffer[20] << 8) | dataBuffer[21]; // 设备地址
            uint16_t data_pack_size = (uint16_t)(dataBuffer[22] << 8) | dataBuffer[23];                                                // 数据包大小码

            // 根据数据包大小码转换为实际大小
            if (0 == data_pack_size)
            {
                data_pack_size = 32;
            }
            else if (1 == data_pack_size)
            {
                data_pack_size = 64;
            }
            else if (2 == data_pack_size)
            {
                data_pack_size = 128;
            }
            else if (3 == data_pack_size)
            {
                data_pack_size = 256;
            }

            uint16_t baud_set = (uint16_t)(dataBuffer[24] << 8) | dataBuffer[25]; // 波特率设置值

            // 输出所有参数信息到串口
            Serial.print("已注册数量:");
            Serial.println(register_cnt);
            Serial.print("模板大小:0x");
            Serial.println(fp_temp_size, HEX);
            Serial.print("库容量:");
            Serial.println(fp_lib_size);
            Serial.print("匹配等级:");
            Serial.println(score_level);
            Serial.print("设备地址:0x");
            Serial.println(device_addr, HEX);
            Serial.print("数据包大小:");
            Serial.println(data_pack_size);
            Serial.print("波特率:");
            Serial.println(baud_set * 9600); // 实际波特率 = 设置值 * 9600

            isSuccess = true; // 设置成功标志
        }
        else
        {
            isSuccess = false; // 设置失败标志
        }
        state = 0; // 重置状态机
        break;
    }
    default:
    {
        break;
    }
    }

    return isSuccess;
}

// 注册指纹 - 使用状态机重构
bool register_FP()
{
    static int registerState = 0;
    static uint8_t currentBufferID = 1;
    static unsigned long delayStartTime = 0;
    int isSuccess = 0;

    switch (registerState)
    {
    case 0: // 初始化
    {
        currentBufferID = 1;
        newsMessage += "开始注册指纹...\n";
        registerState = 1;
        break;
    }
    case 1:
    {

        registerState = 2;
        break;
    }
    case 2: // 等待索引表读取完成
    {
        while (!PS_ReadIndexTable())
            ;
        registerState = 3;

        break;
    }
    case 3: // 初始化循环
    {
        currentBufferID = 1;
        registerState = 4;
        break;
    }
    case 4: // 检查循环条件
    {
        if (currentBufferID <= 5)
        {
            newsMessage += "请按下手指\n";
            send_cmd(CMD_GET_IMAGE);
            registerState = 5;
        }
        else
        {
            registerState = 10;
        }
        break;
    }
    case 5: // 等待获取图像响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "请松手\n";
                delayStartTime = millis();
                registerState = 6;
            }
            else
            {
                String errorMsg = getStatusDescription(dataBuffer[9]);
                newsMessage += errorMsg + "\n";
                registerState = 4;
            }
        }
        break;
    }
    case 6: // 等待延迟
    {
        if (millis() - delayStartTime >= 1000)
        {
            send_cmd2(CMD_GEN_CHAR, currentBufferID);
            registerState = 7;
        }
        break;
    }
    case 7: // 等待生成特征响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                currentBufferID++;
                newsMessage += String("第") + currentBufferID + "个特征生成成功\n";
                registerState = 4;
            }
            else
            {
                String errorMsg = getStatusDescription(dataBuffer[9]);
                newsMessage += errorMsg + "\n";
                registerState = 4;
            }
        }
        break;
    }
    case 10: // 发送合并特征
    {
        newsMessage += "正在合并特征...\n";
        send_cmd(CMD_REG_MODEL);
        registerState = 11;
        break;
    }
    case 11: // 等待合并特征响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "特征合并成功\n";
                registerState = 12;
            }
            else
            {
                String errorMsg = getStatusDescription(dataBuffer[9]);
                newsMessage += errorMsg + "\n";
                isSuccess = 0;
                registerState = 0;
            }
        }
        break;
    }
    case 12: // 检查TEMPLATE_ID
    {
        int found = 0;
        for (int i = 0; i < tableNum; i++)
        {
            if (IndexTable[i] == TEMPLATE_ID)
            {
                found = 1;
                break;
            }
        }
        if (found)
        {
            int nextID = 0;
            while (true)
            {
                int used = 0;
                for (int i = 0; i < tableNum; i++)
                {
                    if (IndexTable[i] == nextID)
                    {
                        used = 1;
                        break;
                    }
                }
                if (!used)
                {
                    TEMPLATE_ID = nextID;
                    break;
                }
                nextID++;
            }
        }
        newsMessage += "使用TEMPLATE_ID: " + String(TEMPLATE_ID) + "\n";
        registerState = 13;
        break;
    }
    case 13: // 发送存储模板
    {
        newsMessage += "正在存储模板...\n";
        sendCommand(CMD_STORE_CHAR, 1, TEMPLATE_ID);
        registerState = 14;
        break;
    }
    case 14: // 等待存储模板响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "模板存储成功\n";
                registerState = 15;
            }
            else
            {
                String errorMsg = getStatusDescription(dataBuffer[9]);
                newsMessage += errorMsg + "\n";
                isSuccess = 0;
                registerState = 0;
            }
        }
        break;
    }
    case 15: // 发送读取索引表更新
    {

        registerState = 16;
        break;
    }
    case 16: // 等待索引表更新完成
    {
        while (!PS_ReadIndexTable())
            ;
        // if (PS_ReadIndexTable())
        // {
        isSuccess = 1;
        registerState = 0;
        // }
        break;
    }
    default:
    {
        break;
    }
    }

    return isSuccess;
}

// 搜索指纹 - 使用状态机重构
bool search_FP()
{
    static int searchState = 0;
    static int serch_cnt = 0;
    static unsigned long delayStartTime = 0;
    int isSuccess = 0;

    switch (searchState)
    {
    case 0: // 初始化
    {
        if (tableNum == 0)
        {
            newsMessage += "索引表为空，无法搜索\n";
            isSuccess = 1;
            searchState = 0;
        }
        else
        {
            serch_cnt = 0;
            BUFFER_ID = 1;
            newsMessage += "开始搜索指纹...\n";
            searchState = 1;
        }
        break;
    }
    case 1: // 发送获取图像
    {
        if (serch_cnt <= 5)
        {
            newsMessage += "请按下手指进行搜索\n";
            send_cmd(CMD_GET_IMAGE);
            searchState = 2;
        }
        else
        {
            newsMessage += "搜索失败，尝试次数过多\n";
            isSuccess = 0;
            searchState = 0;
        }
        break;
    }
    case 2: // 等待获取图像响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "图像获取成功\n";
                send_cmd2(CMD_GEN_CHAR, BUFFER_ID);
                searchState = 3;
            }
            else
            {
                newsMessage += "图像获取失败，重试...\n";
                serch_cnt++;
                delayStartTime = millis();
                searchState = 4;
            }
        }
        break;
    }
    case 4: // 等待延迟后重试
    {
        if (millis() - delayStartTime >= 100)
        {
            searchState = 1;
        }
        break;
    }
    case 3: // 等待生成特征响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "特征生成成功，开始搜索\n";
                searchState = 5;
            }
            else
            {
                newsMessage += "特征生成失败，重试...\n";
                serch_cnt++;
                delayStartTime = millis();
                searchState = 4;
            }
        }
        break;
    }
    case 5: // 发送搜索指纹
    {
        BUFFER_ID = 1;
        sendCommand1(CMD_SEARCH, BUFFER_ID, 1, 1);
        searchState = 6;
        break;
    }
    case 6: // 等待搜索响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "指纹匹配成功\n";
                isSuccess = 1;
                searchState = 0;
            }
            else
            {
                newsMessage += "指纹匹配失败\n";
                isSuccess = 0;
                searchState = 0;
            }
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return isSuccess;
}

// 清空指纹库函数 - 使用状态机重构
bool clear_FP_all_lib()
{
    static int clearState = 0;
    static unsigned long delayStartTime = 0;
    int isSuccess = 0;
    switch (clearState)
    {
    case 0: // 发送清空指令
    {
        newsMessage += "正在发送清空指纹库指令...\n";
        send_cmd(CMD_CLEAR_LIB);
        clearState = 1;
        break;
    }
    case 1: // 等待响应
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                newsMessage += "指纹库清空成功\n";
                delayStartTime = millis();
                clearState = 2;
            }
            else
            {
                newsMessage += "指纹库清空失败\n";
                isSuccess = 0;
                clearState = 0;
            }
        }
        break;
    }
    case 2: // 延时5秒
    {
        if (millis() - delayStartTime >= 5000)
        {
            newsMessage += "延时结束，开始查询新索引表\n";
            clearState = 3;
        }
        break;
    }
    case 3: // 发送读取索引表
    {

        clearState = 4;
        break;
    }
    case 4: // 等待索引表查询完成
    {
        if (PS_ReadIndexTable())
        {
            newsMessage += "新索引表查询完成\n";
            isSuccess = 1;
            clearState = 0;
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return isSuccess;
}

// 读取索引表
bool PS_ReadIndexTable()
{
    static int searchIndex = 0;
    bool indexTableReceived = false;

    processFingerprintSerial(); // 处理指纹串口数据

    switch (searchIndex)
    {
    case 0:
    {
        tableNum = 0;
        send_cmd2(CMD_READ_INDEX_TABLE, 0);
        searchIndex++;
    }
    break;
    case 1:
    {
        if (packetReceived)
        {
            packetReceived = false;
            if (dataBuffer[9] == 0x00)
            {
                searchIndex++;
                newsMessage += "数据正确，开始读取索引表\n";
            }
            else if (dataBuffer[9] == 0x01)
            {
                // 处理索引表数据
                newsMessage += "收容包数据有误\n";
            }
            else if (dataBuffer[9] == 0x01)
            {
            }
            else if (dataBuffer[9] == 0x0b)
            {
                newsMessage += "表示问指纹库时地址序号超出指纹库范围\n";
            }
        }
    }
    break;

    case 2:
    {
        searchIndex++;
        for (int i = 0; i < 32; i++)
        {
            byte byte = dataBuffer[10 + i];
            for (int j = 0; j < 8; j++)
            {

                if (byte >> j & 0x01)
                {
                    IndexTable[tableNum] = i * 8 + j;
                    tableNum++;
                }
            }
        }
    }
    break;
    case 3:
    {
        searchIndex++;
        newsMessage += "索引表读取完成:\n";
        for (int i = 0; i < tableNum; i++)
        {
            newsMessage += String(IndexTable[i]) + " ";
        }
        newsMessage += "\n";
    }
    break;
    case 4:
    {
        searchIndex = 0;
        indexTableReceived = true;
    }
    break;

    default:
        break;
    }

    return indexTableReceived;
}

// 发送指令包函数（无参数）
void send_cmd(uint8_t cmd)
{
    uint8_t packet[12];
    uint16_t length = 3;                  // 指令包长度（指令码1字节 + 校验和2字节）
    uint16_t checksum = 1 + length + cmd; // 计算校验和

    // 构建指令包
    packet[0] = HEADER_HIGH;                   // 包头高字节
    packet[1] = HEADER_LOW;                    // 包头低字节
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF; // 设备地址高字节
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF; // 设备地址低字节
    packet[6] = 0x01;                  // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF;  // 包长度高字节
    packet[8] = length & 0xFF;         // 包长度低字节
    packet[9] = cmd;                   // 指令码

    packet[10] = (checksum >> 8) & 0xFF; // 校验和高字节
    packet[11] = checksum & 0xFF;        // 校验和低字节

    // 发送指令包到指纹模组
    for (int i = 0; i < (2 + 4 + 3 + length); i++)
    {
        fingerprintSerial.write(packet[i]);
    }
}
// 发送指令包
void send_cmd2(uint8_t cmd, uint8_t param1)
{
    uint8_t packet[13];
    uint16_t length = 4;
    uint16_t checksum = 1 + length + cmd + param1;

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (checksum >> 8) & 0xFF;
    packet[12] = checksum & 0xFF;

    // 发送指令包
    for (int i = 0; i < (2 + 4 + 3 + length); i++)
    {
        fingerprintSerial.write(packet[i]);
    }
}

// 发送指令包
//    sendCommand(CMD_STORE_CHAR, 1, TEMPLATE_ID); // 使用缓冲区1存储
void sendCommand(uint8_t cmd, uint8_t param1, uint16_t param2)
{
    uint8_t packet[15];
    uint16_t length = 6;
    uint16_t checksum = 1 + length + cmd + param1 + (param2 >> 8) + (param2 & 0xFF);

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (param2 >> 8) & 0xFF;
    packet[12] = param2 & 0xFF;
    packet[13] = (checksum >> 8) & 0xFF;
    packet[14] = checksum & 0xFF;

    // 发送指令包
    for (int i = 0; i < (2 + 4 + 3 + length); i++)
    {
        fingerprintSerial.write(packet[i]);
    }
}

// 发送指令包
void sendCommand1(uint8_t cmd, uint8_t param1, uint16_t param2, uint16_t param3)
{
    uint8_t packet[17];
    uint16_t length = 8;
    uint16_t checksum = 1 + length + cmd + param1 + (param2 >> 8) + (param2 & 0xFF) + (param3 >> 8) + (param3 & 0xFF);

    // 构建指令包
    packet[0] = HEADER_HIGH;
    packet[1] = HEADER_LOW;
    packet[2] = (DEVICE_ADDRESS >> 24) & 0xFF;
    packet[3] = (DEVICE_ADDRESS >> 16) & 0xFF;
    packet[4] = (DEVICE_ADDRESS >> 8) & 0xFF;
    packet[5] = DEVICE_ADDRESS & 0xFF;
    packet[6] = 0x01;                 // 包标识：命令包
    packet[7] = (length >> 8) & 0xFF; // 包长度高字节
    packet[8] = length & 0xFF;        // 包长度低字节
    packet[9] = cmd;
    packet[10] = param1;
    packet[11] = (param2 >> 8) & 0xFF;
    packet[12] = param2 & 0xFF;
    packet[13] = (param3 >> 8) & 0xFF;
    packet[14] = param3 & 0xFF;
    packet[15] = (checksum >> 8) & 0xFF;
    packet[16] = checksum & 0xFF;

    // 发送指令包
    for (int i = 0; i < (2 + 4 + 3 + length); i++)
    {
        fingerprintSerial.write(packet[i]);
    }
}

/**
 * 将指纹识别模块返回的状态码转换为可读字符串
 * @param status 状态码字节
 * @return 对应的状态描述字符串
 */
String getStatusDescription(uint8_t status)
{
    // 将 byte 转换为无符号整数以便比较
    int statusCode = status & 0xFF;

    switch (statusCode)
    {
    case 0x00:
        return "指令执行完毕或OK";
    case 0x01:
        return "数据包接收错误";
    case 0x02:
        return "传感器上没有手指";
    case 0x03:
        return "录入指纹图像失败";
    case 0x04:
        return "指纹图像太干、太淡而生不成特征";
    case 0x05:
        return "指纹图像太湿、太糊而生不成特征";
    case 0x06:
        return "指纹图像太乱而生不成特征";
    case 0x07:
        return "指纹图像正常，但特征点太少（或面积太小）而生不成特征";
    case 0x08:
        return "指纹不匹配";
    case 0x09:
        return "没搜索到指纹";
    case 0x0A:
        return "特征合并失败";
    case 0x0B:
        return "访问指纹库时地址序号超出指纹库范围";
    case 0x0C:
        return "从指纹库读模板出错或无效";
    case 0x0D:
        return "上传特征失败";
    case 0x0E:
        return "模组不能接收后续数据包";
    case 0x0F:
        return "上传图像失败";
    case 0x10:
        return "删除模板失败";
    case 0x11:
        return "清空指纹库失败";
    case 0x12:
        return "不能进入低功耗状态";
    case 0x13:
        return "口令不正确";
    case 0x14:
        return "系统复位失败";
    case 0x15:
        return "缓冲区内没有有效原始图而生不成图像";
    case 0x17:
        return "残留指纹或两次采集之间手指没有移动过";
    case 0x18:
        return "读写FLASH出错";
    case 0x1A:
        return "无效寄存器号";
    case 0x1B:
        return "寄存器设定内容错误号";
    case 0x1C:
        return "记事本页码指定错误";
    case 0x1D:
        return "端口操作失败";
    case 0x1E:
        return "自动注册（enroll）失败";
    case 0x1F:
        return "指纹库满";
    case 0x20:
        return "设备地址错误";
    case 0x21:
        return "密码有误";
    case 0x22:
        return "指纹模板非空";
    case 0x23:
        return "指纹模板为空";
    case 0x24:
        return "指纹库为空";
    case 0x25:
        return "录入次数设置错误";
    case 0x26:
        return "超时";
    case 0x27:
        return "指纹已存在";
    case 0x28:
        return "指纹模板有关联";
    case 0x29:
        return "传感器初始化失败";
    case 0x2A:
        return "模组信息非空";
    case 0x2B:
        return "模组信息为空";
    case 0x33:
        return "图像面积小";
    case 0x34:
        return "图像不可用";
    case 0x35:
        return "非法数据";
    case 0x40:
        return "注册次数少于规定次数";
    default:
        return "未知状态码";
    }
}

/**
 * 指纹串口接收状态机处理函数 - 解析指纹模组返回的数据包
 * 使用状态机实现数据包的接收和解析，自动处理包头检测和超时
 */
void processFingerprintSerial()
{
    // 检查串口是否有数据可读
    if (fingerprintSerial.available())
    {
        uint8_t data = fingerprintSerial.read(); // 读取一个字节

        switch (receiveState)
        {
        case WAITING_FOR_HEADER: // 等待包头状态
        {
            if (bufferIndex == 0 && data == HEADER_HIGH) // 第一个字节必须是包头高字节
            {
                dataBuffer[bufferIndex++] = data; // 存储到缓冲区
            }
            else if (bufferIndex == 1 && data == HEADER_LOW) // 第二个字节必须是包头低字节
            {
                dataBuffer[bufferIndex++] = data; // 存储到缓冲区
                receiveState = RECEIVING_DATA;    // 切换到接收数据状态
                lastDataTime = millis();          // 记录开始接收时间
            }
            else
            {
                // 包头不匹配，重置缓冲区索引，重新开始检测
                bufferIndex = 0;
            }
        }
        break;

        case RECEIVING_DATA: // 接收数据状态
        {
            if (bufferIndex < sizeof(dataBuffer)) // 防止缓冲区溢出
            {
                dataBuffer[bufferIndex++] = data; // 存储数据到缓冲区
                lastDataTime = millis();          // 更新最后接收时间
            }
        }
        break;
        }
    }

    // 检查接收超时（100ms无数据认为包接收完成）
    if (receiveState == RECEIVING_DATA && millis() - lastDataTime >= 100)
    {
        // 数据包接收完成，设置标志位供其他函数使用
        packetReceived = true;
        // 重置状态机，准备接收下一个数据包
        bufferIndex = 0;
        receiveState = WAITING_FOR_HEADER;
    }
}

/**
 * 处理消息缓冲区 - 批量输出串口信息
 * 将newsMessage中的内容一次性输出到串口，然后清空缓冲区
 * 这样可以减少串口输出的频率，提高效率
 */
void processMessage()
{
    if (newsMessage.length() > 0) // 检查消息缓冲区是否有内容
    {
        Serial.print(newsMessage); // 输出所有缓冲的消息
        newsMessage = "";          // 清空消息缓冲区
    }
}
