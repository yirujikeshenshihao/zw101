

#include "FingerprintModule.h"

String commandSerial; 
void setup()
{
  // 初始化串口
  Serial.begin(115200);
  if (!init_FP())
  {
    Serial.println("指纹模组初始化失败!");
    while (1)
      ;
  }
  command_use();
}

void loop()
{

  // 处理指纹串口接收状态机
  processFingerprintSerial();

  // 接收指令
  run_command();

  processMessage();
}

void command_use()
{
  Serial.println("------------------------------------------");
  Serial.println("命令输入:");
  Serial.println("\"1\" 表示 \"注册指纹。\"");
  Serial.println("\"2\" 表示 \"搜索指纹。\"");
  Serial.println("\"3\" 表示 \"清空指纹。\"");
  Serial.println("\"4\" 表示 \"读取索引表。\"");
  Serial.println("------------------------------------------");
}

void run_command()
{
  // 当串口有数据可读时
  if (Serial.available() > 0)
  {
    // 读取直到换行符（自动处理回车+换行）
    commandSerial = Serial.readString();
    commandSerial.trim(); // 去除首尾空白字符（包括换行和回车）
  }
  // 判断命令并执行对应操作
  if (commandSerial == "1")
  {
    if (register_FP())
    {
      Serial.println("注册指纹成功!");
      commandSerial = " ";
    }
  }
  else if (commandSerial == "2")
  {
    if (search_FP())
    {
      Serial.println("搜索指纹成功!");
      commandSerial = " ";
    }
  }
  else if (commandSerial == "3")
  {
    if (clear_FP_all_lib())
    {
      Serial.println("指纹库清空成功!");
      commandSerial = " ";
    }
  }
  else if (commandSerial == "4")
  {
    if (PS_ReadIndexTable())
    {
      Serial.println("索引表读取完成!");
      commandSerial = " ";
    }
  }
  else if (commandSerial != " ")
  {
    // 未知命令处理
    Serial.print("[错误] 未知命令: ");
    Serial.println(commandSerial);
    commandSerial = " ";
  }
}
