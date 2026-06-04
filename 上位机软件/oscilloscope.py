import sys
import queue
import serial
import serial.tools.list_ports
import struct
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QPushButton, QComboBox, QLabel, QLineEdit, QSpinBox,
                             QDoubleSpinBox, QGroupBox, QGridLayout, QCheckBox)
from PyQt5.QtCore import QThread, pyqtSignal, Qt, QTimer
from PyQt5.QtGui import QPainter, QPen, QColor, QFont
import pyqtgraph as pg

class SerialThread(QThread):
    data_received = pyqtSignal(list)
    status_received = pyqtSignal(bool)
    io_activity = pyqtSignal(int, int)
    error_received = pyqtSignal(str)
    
    def __init__(self, port, baud_rate):
        super().__init__()
        self.port = port
        self.baud_rate = baud_rate
        self.ser = None
        self.running = True
        self.buffer = bytearray()
        self.command_queue = queue.Queue()
        self.rx_bytes = 0
        self.tx_commands = 0
    
    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud_rate, timeout=0.05, write_timeout=0.05)
            self.io_activity.emit(self.tx_commands, self.rx_bytes)
            while self.running:
                did_work = False
                if self._drain_commands():
                    did_work = True

                if self.ser.in_waiting > 0:
                    chunk = self.ser.read(self.ser.in_waiting)
                    self.rx_bytes += len(chunk)
                    self.buffer.extend(chunk)
                    self.process_buffer()
                    self.io_activity.emit(self.tx_commands, self.rx_bytes)
                    did_work = True

                if not did_work:
                    self.msleep(2)
        except Exception as e:
            print(f"Serial error: {e}")
            self.error_received.emit(str(e))
        finally:
            if self.ser:
                self._drain_commands()
                self.ser.close()
    
    def process_buffer(self):
        while len(self.buffer) >= 4:
            if self.buffer[0] == 0xAA and self.buffer[1] == 0xBB:
                if len(self.buffer) < 6:
                    break
                length = (self.buffer[2] << 8) | self.buffer[3]
                if length <= 0 or length > 2048:
                    self.buffer = self.buffer[1:]
                    continue
                total_packet_len = 4 + length * 2 + 2
                
                if len(self.buffer) >= total_packet_len:
                    if self.buffer[4 + length * 2] == 0xCC and self.buffer[4 + length * 2 + 1] == 0xDD:
                        data = []
                        for i in range(length):
                            idx = 4 + i * 2
                            val = (self.buffer[idx] << 8) | self.buffer[idx + 1]
                            data.append(val)
                        self.data_received.emit(data)
                        self.buffer = self.buffer[total_packet_len:]
                    else:
                        self.buffer = self.buffer[1:]
                else:
                    break
            elif self.buffer[0] == 0xAA and self.buffer[1] == 0xCC:
                if len(self.buffer) >= 4:
                    if self.buffer[2] == 0x01:
                        status = self.buffer[3] == 0x01
                        self.status_received.emit(status)
                        self.buffer = self.buffer[4:]
                    else:
                        self.buffer = self.buffer[1:]
                else:
                    break
            else:
                self.buffer = self.buffer[1:]
    
    def send_command(self, cmd):
        self.command_queue.put(cmd)

    def _drain_commands(self):
        did_work = False
        while not self.command_queue.empty():
            try:
                cmd = self.command_queue.get_nowait()
            except queue.Empty:
                break
            if self.ser and self.ser.is_open:
                self.ser.write(bytes([cmd]))
                self.tx_commands += 1
                self.io_activity.emit(self.tx_commands, self.rx_bytes)
                did_work = True
        return did_work
    
    def stop(self):
        self.running = False
        self.wait(500)

class OscilloscopeWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32示波器与信号源")
        self.setGeometry(100, 100, 1400, 900)
        
        self.sample_rate = 10000
        self.adc_ref_voltage = 3.3
        self.max_time_ms = 10000
        self.data_buffer = np.zeros(int(self.sample_rate * self.max_time_ms / 1000))
        self.voltage_range = 3.3
        self.time_base = 100
        self.trigger_level = 1.65
        self.trigger_mode = 'auto'
        self.running = False
        self.signal_source_on = False
        self.packet_count = 0
        self.sample_count = 0
        self.tx_commands = 0
        self.rx_bytes = 0
        self.serial_error = ""
        self.last_min_voltage = 0.0
        self.last_max_voltage = 0.0
        self.display_points = 1024
        self.max_plot_points = 2000
        self.time_axis = np.linspace(0, self.time_base * 10, self.display_points)
        self.plot_dirty = True
        self.info_dirty = True
        
        self.init_ui()
        self.serial_thread = None
        self.ui_timer = QTimer(self)
        self.ui_timer.timeout.connect(self.refresh_display)
        self.ui_timer.start(50)
    
    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)
        
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('w')
        self.plot_widget.setLabel('left', '电压 (V)')
        self.plot_widget.setLabel('bottom', '时间 (ms)')
        self.plot_widget.setXRange(0, self.time_base * 10)
        self.plot_widget.setYRange(0, self.voltage_range)
        self.plot_curve = self.plot_widget.plot(pen=pg.mkPen(color='b', width=2))
        self.plot_widget.disableAutoRange()
        
        self.crosshair_v = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen(color='r', style=Qt.DashLine))
        self.crosshair_h = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen(color='r', style=Qt.DashLine))
        self.plot_widget.addItem(self.crosshair_v)
        self.plot_widget.addItem(self.crosshair_h)
        
        self.plot_widget.scene().sigMouseMoved.connect(self.update_crosshair)
        
        control_panel = QGroupBox("串口控制")
        control_layout = QGridLayout(control_panel)
        
        port_label = QLabel("串口:")
        self.port_combo = QComboBox()
        self.refresh_ports()
        self.port_combo.currentTextChanged.connect(self.on_port_change)
        
        baud_label = QLabel("波特率:")
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(['9600', '19200', '38400', '57600', '115200'])
        self.baud_combo.setCurrentText('115200')
        
        self.start_btn = QPushButton("开始采集")
        self.start_btn.clicked.connect(self.toggle_acquisition)
        
        self.refresh_btn = QPushButton("刷新串口")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        
        control_layout.addWidget(port_label, 0, 0)
        control_layout.addWidget(self.port_combo, 0, 1)
        control_layout.addWidget(baud_label, 1, 0)
        control_layout.addWidget(self.baud_combo, 1, 1)
        control_layout.addWidget(self.start_btn, 2, 0)
        control_layout.addWidget(self.refresh_btn, 2, 1)
        
        left_layout.addWidget(self.plot_widget)
        left_layout.addWidget(control_panel)
        
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        
        settings_panel = QGroupBox("示波器参数")
        settings_layout = QGridLayout(settings_panel)
        
        time_label = QLabel("时基 (ms/div):")
        self.time_spin = QDoubleSpinBox()
        self.time_spin.setRange(0.1, 1000)
        self.time_spin.setValue(100)
        self.time_spin.valueChanged.connect(self.update_time_base)
        
        voltage_label = QLabel("电压范围 (V):")
        self.voltage_spin = QDoubleSpinBox()
        self.voltage_spin.setRange(0.1, 10)
        self.voltage_spin.setValue(3.3)
        self.voltage_spin.valueChanged.connect(self.update_voltage_range)
        
        trigger_label = QLabel("触发电平 (V):")
        self.trigger_spin = QDoubleSpinBox()
        self.trigger_spin.setRange(0, 3.3)
        self.trigger_spin.setValue(1.65)
        self.trigger_spin.valueChanged.connect(self.update_trigger_level)
        
        mode_label = QLabel("触发模式:")
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(['auto', 'rising', 'falling'])
        self.mode_combo.currentTextChanged.connect(self.update_trigger_mode)
        
        settings_layout.addWidget(time_label, 0, 0)
        settings_layout.addWidget(self.time_spin, 0, 1)
        settings_layout.addWidget(voltage_label, 1, 0)
        settings_layout.addWidget(self.voltage_spin, 1, 1)
        settings_layout.addWidget(trigger_label, 2, 0)
        settings_layout.addWidget(self.trigger_spin, 2, 1)
        settings_layout.addWidget(mode_label, 3, 0)
        settings_layout.addWidget(self.mode_combo, 3, 1)
        
        signal_panel = QGroupBox("信号源控制")
        signal_layout = QGridLayout(signal_panel)
        
        wave_label = QLabel("波形类型:")
        self.wave_combo = QComboBox()
        self.wave_combo.addItems(['正弦波', '三角波', '方波'])
        self.wave_combo.currentIndexChanged.connect(self.on_wave_change)
        
        self.signal_btn = QPushButton("开启信号源")
        self.signal_btn.clicked.connect(self.toggle_signal_source)
        
        signal_layout.addWidget(wave_label, 0, 0)
        self.wave_combo.setEnabled(False)
        signal_layout.addWidget(self.wave_combo, 0, 1)
        signal_layout.addWidget(self.signal_btn, 1, 0, 1, 2)
        
        info_panel = QGroupBox("实时信息")
        info_layout = QVBoxLayout(info_panel)
        
        self.info_label = QLabel("状态: 未连接\n采样率: 10000 Hz\n信号源: 关闭")
        self.info_label.setFont(QFont('Arial', 10))
        
        info_layout.addWidget(self.info_label)
        
        right_layout.addWidget(settings_panel)
        right_layout.addWidget(signal_panel)
        right_layout.addWidget(info_panel)
        right_layout.addStretch()
        
        main_layout.addWidget(left_panel, 3)
        main_layout.addWidget(right_panel, 1)
    
    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(port.device)
    
    def on_port_change(self):
        if self.serial_thread and self.running:
            self.stop_acquisition()
    
    def update_time_base(self, value):
        self.time_base = value
        self.update_time_axis()
        self.update_plot()
    
    def update_voltage_range(self, value):
        self.voltage_range = value
        self.update_plot()
    
    def update_trigger_level(self, value):
        self.trigger_level = value
    
    def update_trigger_mode(self, value):
        self.trigger_mode = value
    
    def on_wave_change(self, index):
        if self.signal_source_on and self.serial_thread:
            wave_cmds = [0x10, 0x11, 0x12]
            self.serial_thread.send_command(wave_cmds[index])
    
    def toggle_acquisition(self):
        if self.running:
            self.stop_acquisition()
        else:
            self.start_acquisition()

    def open_serial(self):
        if self.serial_thread and self.serial_thread.isRunning():
            return True
        if self.serial_thread:
            self.serial_thread = None

        port = self.port_combo.currentText()
        baud = int(self.baud_combo.currentText())
        
        if not port:
            self.info_label.setText("错误: 请选择串口")
            return False
        
        self.serial_thread = SerialThread(port, baud)
        self.serial_thread.data_received.connect(self.on_data_received)
        self.serial_thread.status_received.connect(self.on_status_received)
        self.serial_thread.io_activity.connect(self.on_io_activity)
        self.serial_thread.error_received.connect(self.on_serial_error)
        self.serial_thread.start()
        return True
    
    def start_acquisition(self):
        if not self.open_serial():
            return
        
        self.packet_count = 0
        self.sample_count = 0
        self.last_min_voltage = 0.0
        self.last_max_voltage = 0.0
        self.data_buffer.fill(0)
        self.plot_dirty = True
        self.info_dirty = True
        QTimer.singleShot(300, lambda: self.serial_thread and self.serial_thread.send_command(0x01))
        self.running = True
        self.start_btn.setText("停止采集")
        self.wave_combo.setEnabled(True)
        self.update_info()
    
    def stop_acquisition(self):
        if self.serial_thread:
            self.serial_thread.send_command(0x02)
        
        self.running = False
        self.start_btn.setText("开始采集")
        self.wave_combo.setEnabled(True)
        self.update_info()
    
    def toggle_signal_source(self):
        if not self.open_serial():
            return
        
        if self.signal_source_on:
            self.serial_thread.send_command(0x13)
            self.signal_source_on = False
            self.signal_btn.setText("开启信号源")
        else:
            wave_cmds = [0x10, 0x11, 0x12]
            self.serial_thread.send_command(wave_cmds[self.wave_combo.currentIndex()])
            self.signal_source_on = True
            self.signal_btn.setText("关闭信号源")
        
        self.update_info()
    
    def on_data_received(self, data):
        adc_values = np.array(data)
        voltage_values = adc_values * self.adc_ref_voltage / 4095.0
        self.packet_count += 1
        self.sample_count += len(voltage_values)
        if len(voltage_values) > 0:
            self.last_min_voltage = float(np.min(voltage_values))
            self.last_max_voltage = float(np.max(voltage_values))
        
        self.data_buffer = np.roll(self.data_buffer, -len(voltage_values))
        self.data_buffer[-len(voltage_values):] = voltage_values
        self.plot_dirty = True
        self.info_dirty = True
    
    def on_status_received(self, status):
        if self.running and not status:
            return
        self.running = status
        if status:
            self.start_btn.setText("停止采集")
        else:
            self.start_btn.setText("开始采集")
        self.update_info()

    def on_io_activity(self, tx_commands, rx_bytes):
        self.tx_commands = tx_commands
        self.rx_bytes = rx_bytes
        self.info_dirty = True

    def on_serial_error(self, message):
        self.serial_error = message
        self.info_dirty = True
    
    def refresh_display(self):
        if self.plot_dirty:
            self.update_plot()
            self.plot_dirty = False
        if self.info_dirty:
            self.update_info()
            self.info_dirty = False

    def update_time_axis(self):
        visible_time_ms = self.time_base * 10
        points = int(self.sample_rate * visible_time_ms / 1000)
        points = max(10, min(points, len(self.data_buffer)))
        self.display_points = points
        self.plot_widget.setXRange(0, visible_time_ms)

    def update_plot(self):
        visible_data = self.data_buffer[-self.display_points:]
        step = max(1, int(np.ceil(len(visible_data) / self.max_plot_points)))
        plot_data = visible_data[::step]
        time_axis = np.linspace(0, self.time_base * 10, len(plot_data))
        self.plot_curve.setData(time_axis, plot_data)
        self.plot_widget.setYRange(0, self.voltage_range)
    
    def update_crosshair(self, pos):
        mouse_point = self.plot_widget.plotItem.vb.mapSceneToView(pos)
        self.crosshair_v.setPos(mouse_point.x())
        self.crosshair_h.setPos(mouse_point.y())
    
    def update_info(self):
        status_text = "采集ing" if self.running else "已停止"
        signal_text = "开启" if self.signal_source_on else "关闭"
        self.info_label.setText(
            f"状态: {status_text}\n"
            f"采样率: {self.sample_rate} Hz\n"
            f"信号源: {signal_text}\n"
            f"Packets: {self.packet_count}\n"
            f"Samples: {self.sample_count}\n"
            f"TX Cmds: {self.tx_commands}\n"
            f"RX Bytes: {self.rx_bytes}\n"
            f"Serial: {self.serial_error or 'OK'}\n"
            f"Last: {self.last_min_voltage:.3f} - {self.last_max_voltage:.3f} V"
        )
    
    def closeEvent(self, event):
        if self.serial_thread:
            self.serial_thread.send_command(0x13)
            self.serial_thread.send_command(0x02)
            self.serial_thread.stop()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = OscilloscopeWindow()
    window.show()
    sys.exit(app.exec_())

