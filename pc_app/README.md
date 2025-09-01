# 智能键盘上位机使用说明

## 环境要求
Python 3.8+，依赖库安装：
```bash
pip install -r requirements.txt
```

## 配置说明
1. 修改`main.py`中的`target_ip`为键盘实际IP（默认192.168.4.1）
2. 确保键盘固件已实现以下UDP命令解析：
   - `WIFI_SET,<ssid>,<pwd>`：设置WiFi参数
   - `LED_TOGGLE`/`LED_OFF`：控制LED开关
   - `KEY_SET,<按键名>,<功能>`：设置按键功能

## 使用步骤
1. 启动键盘并连接到上位机同一网络
2. 运行上位机：
```bash
python main.py
```
3. 在WiFi配置区输入SSID和密码，点击保存
4. 选择LED模式（拾音律动需开启音频监听）
5. 选择按键并输入自定义功能（如'打开计算器'），点击保存

## 功能扩展
- 音频律动：已集成`audio_processor.py`，可通过`get_current_volume()`获取实时音量用于LED亮度调节
- 按键列表：需与固件同步按键名称（建议通过UDP获取键盘按键列表）