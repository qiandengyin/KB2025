import sys
import socket
from PyQt5.QtWidgets import QApplication, QWidget, QFormLayout, QLineEdit, QPushButton, QComboBox, QListWidget, QVBoxLayout

class KeyboardControlApp(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.init_network()

    def init_ui(self):
        self.setWindowTitle('智能键盘上位机')
        self.setGeometry(100, 100, 600, 400)

        main_layout = QVBoxLayout()

        # WiFi配置模块
        wifi_layout = QFormLayout()
        self.ssid_input = QLineEdit()
        self.pwd_input = QLineEdit()
        self.pwd_input.setEchoMode(QLineEdit.Password)
        self.save_wifi_btn = QPushButton('保存WiFi配置')
        self.save_wifi_btn.clicked.connect(self.save_wifi_config)
        wifi_layout.addRow('WiFi名称:', self.ssid_input)
        wifi_layout.addRow('WiFi密码:', self.pwd_input)
        wifi_layout.addRow(self.save_wifi_btn)

        # LED控制模块
        led_layout = QFormLayout()
        self.led_mode = QComboBox()
        self.led_mode.addItems(['常亮', '拾音律动', '音乐律动'])
        self.led_switch = QPushButton('开关LED')
        self.led_switch.clicked.connect(self.toggle_led)
        led_layout.addRow('LED模式:', self.led_mode)
        led_layout.addRow(self.led_switch)

        # 按键自定义模块
        key_layout = QFormLayout()
        self.key_list = QListWidget()
        self.key_custom = QLineEdit()
        self.save_key_btn = QPushButton('保存按键功能')
        self.save_key_btn.clicked.connect(self.save_key_config)
        key_layout.addRow('按键列表:', self.key_list)
        key_layout.addRow('自定义功能:', self.key_custom)
        key_layout.addRow(self.save_key_btn)

        main_layout.addLayout(wifi_layout)
        main_layout.addLayout(led_layout)
        main_layout.addLayout(key_layout)
        self.setLayout(main_layout)

    def init_network(self):
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.target_ip = '192.168.4.1'  # 假设键盘的默认IP
        self.target_port = 12345

    def save_wifi_config(self):
        ssid = self.ssid_input.text()
        pwd = self.pwd_input.text()
        if ssid and pwd:
            cmd = f'WIFI_SET,{ssid},{pwd}'
            self.udp_socket.sendto(cmd.encode(), (self.target_ip, self.target_port))

    def toggle_led(self):
        cmd = 'LED_TOGGLE' if self.led_switch.text() == '打开LED' else 'LED_OFF'
        self.udp_socket.sendto(cmd.encode(), (self.target_ip, self.target_port))
        self.led_switch.setText('关闭LED' if cmd == 'LED_TOGGLE' else '打开LED')

    def save_key_config(self):
        selected_key = self.key_list.currentItem().text() if self.key_list.currentItem() else ''
        custom_func = self.key_custom.text()
        if selected_key and custom_func:
            cmd = f'KEY_SET,{selected_key},{custom_func}'
            self.udp_socket.sendto(cmd.encode(), (self.target_ip, self.target_port))

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = KeyboardControlApp()
    ex.show()
    sys.exit(app.exec_())