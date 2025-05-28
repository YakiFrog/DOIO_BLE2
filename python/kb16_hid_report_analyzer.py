#!/usr/bin/env python3
# filepath: /Users/kotaniryota/Documents/PlatformIO/Projects/DOIO_Bluetooth/python/kb16_hid_report_analyzer.py
"""
DOIO KB16 HIDレポート形式アナライザー

このスクリプトは、DOIO KB16キーボードのHIDレポート形式を解析し、
QMKファームウェアのHIDレポート構造を調べるためのものです。
"""

import sys
import os
import time
import json
import argparse
from datetime import datetime

# hidapiをインポート
try:
    import hid
except ImportError:
    print("hidapiモジュールがインストールされていません。インストールしてください。")
    print("pip install hidapi")
    sys.exit(1)

# PySide6をインポート（オプション - GUIビジュアライザーを使用する場合）
try:
    from PySide6 import QtCore, QtWidgets, QtGui
    HAS_GUI = True
except ImportError:
    print("PySide6がインストールされていません。コマンドライン版のみで実行します。")
    print("GUIを使用するには: pip install PySide6")
    HAS_GUI = False

# DOIO KB16の標準的なVendor IDとProduct ID
# 注意: これは実際のデバイスに合わせて変更する必要があります
DEFAULT_VID = 0xD010 
DEFAULT_PID = 0x1601

# HIDレポートのサイズ試行リスト
REPORT_SIZES = [8, 16, 24, 32, 64]

class KB16HIDReportAnalyzer:
    """DOIO KB16 HIDレポートアナライザークラス"""
    
    def __init__(self, vid=DEFAULT_VID, pid=DEFAULT_PID, output_dir=None):
        """初期化"""
        self.vid = vid
        self.pid = pid
        self.device = None
        # DOIO KB16の場合は16バイト固定
        self.report_size = 16 if (vid == DEFAULT_VID and pid == DEFAULT_PID) else None
        self.report_format = None
        self.last_report = None
        self.output_dir = output_dir or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'kb16_analysis')
        
        # 出力ディレクトリが存在しない場合は作成
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)
        
        self.log_file = None
        self.json_file = None
        
    def keycode_to_string(self, keycode, shift=False):
        """HIDキーコードを文字列に変換する"""
        # 標準的なUSキーボードマッピング
        keycode_table = {
            # アルファベット (0x08-0x21: a-z)
            0x08: ('a', 'A'), 0x09: ('b', 'B'), 0x0A: ('c', 'C'), 0x0B: ('d', 'D'),
            0x0C: ('e', 'E'), 0x0D: ('f', 'F'), 0x0E: ('g', 'G'), 0x0F: ('h', 'H'),
            0x10: ('i', 'I'), 0x11: ('j', 'J'), 0x12: ('k', 'K'), 0x13: ('l', 'L'),
            0x14: ('m', 'M'), 0x15: ('n', 'N'), 0x16: ('o', 'O'), 0x17: ('p', 'P'),
            0x18: ('q', 'Q'), 0x19: ('r', 'R'), 0x1A: ('s', 'S'), 0x1B: ('t', 'T'),
            0x1C: ('u', 'U'), 0x1D: ('v', 'V'), 0x1E: ('w', 'W'), 0x1F: ('x', 'X'),
            0x20: ('y', 'Y'), 0x21: ('z', 'Z'),
            
            # 数字と記号
            0x1E: ('1', '!'), 0x1F: ('2', '@'), 0x20: ('3', '#'), 0x21: ('4', '$'),
            0x22: ('5', '%'), 0x23: ('6', '^'), 0x24: ('7', '&'), 0x25: ('8', '*'),
            0x26: ('9', '('), 0x27: ('0', ')'),
            
            # 一般的なキー
            0x28: ('Enter', 'Enter'), 0x29: ('Esc', 'Esc'), 0x2A: ('Backspace', 'Backspace'),
            0x2B: ('Tab', 'Tab'), 0x2C: ('Space', 'Space'), 0x2D: ('-', '_'), 0x2E: ('=', '+'),
            0x2F: ('[', '{'), 0x30: (']', '}'), 0x31: ('\\', '|'), 0x33: (';', ':'),
            0x34: ("'", '"'), 0x35: ('`', '~'), 0x36: (',', '<'), 0x37: ('.', '>'),
            0x38: ('/', '?'),
            
            # ファンクションキー
            0x3A: ('F1', 'F1'), 0x3B: ('F2', 'F2'), 0x3C: ('F3', 'F3'),
            0x3D: ('F4', 'F4'), 0x3E: ('F5', 'F5'), 0x3F: ('F6', 'F6'),
            0x40: ('F7', 'F7'), 0x41: ('F8', 'F8'), 0x42: ('F9', 'F9'),
            0x43: ('F10', 'F10'), 0x44: ('F11', 'F11'), 0x45: ('F12', 'F12'),
            
            # 特殊キー
            0x49: ('Insert', 'Insert'), 0x4A: ('Home', 'Home'), 0x4B: ('PageUp', 'PageUp'),
            0x4C: ('Delete', 'Delete'), 0x4D: ('End', 'End'), 0x4E: ('PageDown', 'PageDown'),
            0x4F: ('Right', 'Right'), 0x50: ('Left', 'Left'), 
            0x51: ('Down', 'Down'), 0x52: ('Up', 'Up'),
            
            # テンキー
            0x54: ('/', '/'), 0x55: ('*', '*'), 0x56: ('-', '-'), 0x57: ('+', '+'),
            0x58: ('Enter', 'Enter'), 0x59: ('1', '1'), 0x5A: ('2', '2'), 0x5B: ('3', '3'),
            0x5C: ('4', '4'), 0x5D: ('5', '5'), 0x5E: ('6', '6'), 0x5F: ('7', '7'),
            0x60: ('8', '8'), 0x61: ('9', '9'), 0x62: ('0', '0'), 0x63: ('.', '.'),
            
            # 制御キー
            0xE0: ('Ctrl', 'Ctrl'), 0xE1: ('Shift', 'Shift'), 0xE2: ('Alt', 'Alt'),
            0xE3: ('GUI', 'GUI'), 0xE4: ('右Ctrl', '右Ctrl'), 0xE5: ('右Shift', '右Shift'),
            0xE6: ('右Alt', '右Alt'), 0xE7: ('右GUI', '右GUI')
        }
        
        if keycode in keycode_table:
            return keycode_table[keycode][1 if shift else 0]
        else:
            return f"不明(0x{keycode:02X})"
    
    def open(self):
        """デバイスを開く"""
        print(f"DOIO KB16 (VID=0x{self.vid:04X}, PID=0x{self.pid:04X})を検索中...")
        
        try:
            self.device = hid.device()
            self.device.open(self.vid, self.pid)
            print(f"デバイスを開きました: {self.device.get_manufacturer_string()} {self.device.get_product_string()}")
            print(f"シリアル番号: {self.device.get_serial_number_string()}")
            return True
        except Exception as e:
            print(f"エラー: デバイスを開けませんでした - {e}")
            return False
    
    def _detect_report_size(self):
        """HIDレポートのサイズを自動検出する"""
        # DOIO KB16の場合は16バイト固定
        if self.vid == DEFAULT_VID and self.pid == DEFAULT_PID:
            print("DOIO KB16を検出: 16バイト固定レポートサイズを使用します")
            self.report_size = 16
            return 16
        
        print("HIDレポートサイズを検出中...")
        
        for size in REPORT_SIZES:
            try:
                print(f"  {size}バイトのレポートサイズをテスト中...")
                data = self.device.read(size, timeout_ms=1000)
                if data and len(data) > 0:
                    print(f"✓ {size}バイトのレポートサイズを検出しました")
                    self.report_size = size
                    return size
            except Exception as e:
                print(f"  {size}バイトでの読み取りに失敗: {e}")
        
        # デフォルトサイズを使用
        print("レポートサイズの自動検出に失敗しました。32バイトを使用します。")
        self.report_size = 32
        return 32
    
    def _detect_modifier_byte(self, report_data):
        """修飾キーのバイトを自動検出する"""
        # 各バイトを修飾キーとして解釈した場合の妥当性をチェック
        modifier_candidates = []
        
        for i, byte_val in enumerate(report_data):
            # 1バイト目（インデックス0）は修飾キーではないことが判明しているのでスキップ
            if i == 0:
                continue
                
            # 修飾キーらしい特徴をチェック
            score = 0
            
            # 0x00の場合は修飾キーが押されていない可能性が高い
            if byte_val == 0x00:
                score += 10
            
            # 修飾キーの典型的なパターン（0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80）
            if byte_val in [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80]:
                score += 20
            
            # 複数ビットが立っている場合（複数の修飾キーが押されている）
            bit_count = bin(byte_val).count('1')
            if 1 <= bit_count <= 4:  # 1-4個の修飾キーが押されている
                score += 15
            
            # 0xFF は修飾キーではない可能性が高い
            if byte_val == 0xFF:
                score -= 20
            
            # キーコードらしい値（0x04-0xDD）は修飾キーではない可能性が高い
            if 0x04 <= byte_val <= 0xDD:
                score -= 25  # より強いペナルティ
            
            # 1バイト目以外の一般的でない値も修飾キーではない可能性が高い
            if byte_val > 0x80 and byte_val not in [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80]:
                score -= 15
            
            modifier_candidates.append((i, score, byte_val))
        
        # スコアでソート
        modifier_candidates.sort(key=lambda x: x[1], reverse=True)
        
        return modifier_candidates
    
    def _analyze_report_format(self, report_data):
        """HIDレポート形式を解析する"""
        if not self.report_format:
            # 修飾キーのバイトを自動検出
            modifier_candidates = self._detect_modifier_byte(report_data)
            
            # 最も可能性の高い修飾キーバイトを選択（ただし、デバッグモードでは無効化）
            modifier_index = None
            if hasattr(self, 'debug_no_modifier') and self.debug_no_modifier:
                modifier_index = -1  # 修飾キー解析を無効化
            else:
                # 通常は最初のバイトまたは最もスコアの高いバイトを使用
                if modifier_candidates:
                    modifier_index = modifier_candidates[0][0]
                else:
                    modifier_index = 0
            
            # 最初のレポートから形式を推測
            self.report_format = {
                "size": len(report_data),
                "modifier_index": modifier_index,
                "modifier_candidates": modifier_candidates,  # デバッグ用
                "reserved_index": 1,  # 予約バイト
                "key_indices": [2, 3, 4, 5, 6, 7],  # 標準的な6KROレイアウト
                "format": "Standard"
            }
            
            # NKROの検出 (多くのバイトにキーコードが分散している場合)
            non_zero_count = sum(1 for b in report_data[2:] if b != 0)
            if non_zero_count > 6 or len(report_data) > 8:
                self.report_format["format"] = "NKRO"
                # NKROでは通常、各ビットが1つのキーに対応
                self.report_format["key_indices"] = list(range(2, len(report_data)))
            
        return self.report_format
    
    def start_logging(self):
        """ロギングを開始する"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_file = os.path.join(self.output_dir, f"kb16_capture_{timestamp}.csv")
        self.json_file = os.path.join(self.output_dir, f"kb16_capture_{timestamp}.json")
        
        # CSVヘッダーを書き込む
        with open(self.log_file, "w") as f:
            f.write("timestamp,raw_data\n")
        
        # JSONファイルの初期化
        with open(self.json_file, "w") as f:
            json.dump({"device": {"vid": self.vid, "pid": self.pid}, "reports": []}, f)
        
        print(f"ログファイル: {self.log_file}")
        print(f"JSONファイル: {self.json_file}")
    
    def log_report(self, report_data):
        """レポートをログに記録する"""
        if not self.log_file:
            return
        
        # CSVに追記
        with open(self.log_file, "a") as f:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            f.write(f"{timestamp},{','.join(f'{b:02X}' for b in report_data)}\n")
        
        # JSONに追記
        with open(self.json_file, "r+") as f:
            data = json.load(f)
            report = {
                "timestamp": datetime.now().isoformat(),
                "data": [b for b in report_data]
            }
            data["reports"].append(report)
            f.seek(0)
            json.dump(data, f, indent=2)
            f.truncate()
    
    def pretty_print_report(self, report_data):
        """レポートを整形して表示する"""
        report_format = self._analyze_report_format(report_data)
        
        # バイト列を16進数で表示
        hex_data = " ".join([f"{b:02X}" for b in report_data])
        print(f"\nHIDレポート [{len(report_data)}バイト]: {hex_data}")
        
        # 修飾キー (Shift, Ctrl, Alt, GUI)
        shift_pressed = False
        if report_format["format"] == "Standard" or report_format["format"] == "NKRO":
            # 修飾キー候補のデバッグ表示
            if hasattr(self, 'debug_mode') and self.debug_mode:
                print("修飾キー候補の分析:")
                for i, (byte_idx, score, byte_val) in enumerate(report_format.get("modifier_candidates", [])):
                    print(f"  バイト{byte_idx}: 0x{byte_val:02X} (スコア: {score})")
            
            # 修飾キー解析が無効化されている場合
            if report_format["modifier_index"] == -1:
                print(f"修飾キー: 解析無効化")
            else:
                modifier = report_data[report_format["modifier_index"]]
                mod_str = ""
                if modifier & 0x01: mod_str += "L-Ctrl "
                if modifier & 0x02: 
                    mod_str += "L-Shift "
                    shift_pressed = True
                if modifier & 0x04: mod_str += "L-Alt "
                if modifier & 0x08: mod_str += "L-GUI "
                if modifier & 0x10: mod_str += "R-Ctrl "
                if modifier & 0x20: 
                    mod_str += "R-Shift "
                    shift_pressed = True
                if modifier & 0x40: mod_str += "R-Alt "
                if modifier & 0x80: mod_str += "R-GUI "
                print(f"修飾キー (バイト{report_format['modifier_index']}): {mod_str or 'なし'}")
            
            # 押されているキー
            pressed_keys = []
            pressed_chars = []
            if report_format["format"] == "Standard":
                # 標準的な6KROレポート
                for idx in report_format["key_indices"]:
                    if idx < len(report_data) and report_data[idx] != 0:
                        keycode = report_data[idx]
                        pressed_keys.append(f"0x{keycode:02X}")
                        # キーコードを文字に変換
                        key_str = self.keycode_to_string(keycode, shift_pressed)
                        pressed_chars.append(key_str)
            else:
                # NKRO形式
                for i, b in enumerate(report_data[2:], start=2):
                    for bit in range(8):
                        if b & (1 << bit):
                            keycode = (i - 2) * 8 + bit + 4
                            pressed_keys.append(f"0x{keycode:02X}")
                            # キーコードを文字に変換
                            key_str = self.keycode_to_string(keycode, shift_pressed)
                            pressed_chars.append(key_str)
            
            print(f"押されているキー: {', '.join(pressed_keys) or 'なし'}")
            if pressed_chars:
                print(f"文字表現: {', '.join(pressed_chars)}")
        
        # 変更の検出
        if self.last_report:
            changes = []
            for i, (old, new) in enumerate(zip(self.last_report, report_data)):
                if old != new:
                    changes.append(f"バイト{i}: 0x{old:02X} -> 0x{new:02X}")
            
            if changes:
                print("変更点:")
                for change in changes:
                    print(f"  {change}")
        
        self.last_report = report_data.copy()
    
    def run(self, duration=None, gui_mode=False):
        """メインループ - レポートを読み取って表示"""
        if not self.open():
            return False
        
        # レポートサイズの確認と設定
        if not self.report_size:
            self._detect_report_size()
        
        self.start_logging()
        
        print("\nHIDレポートをリアルタイムで監視中...")
        print("Ctrl+Cで終了")
        print("-" * 60)
        
        try:
            start_time = time.time()
            while duration is None or time.time() - start_time < duration:
                try:
                    # タイムアウト付きで読み取り
                    report_data = self.device.read(self.report_size, timeout_ms=100)
                    if report_data:
                        self.pretty_print_report(report_data)
                        self.log_report(report_data)
                    
                    # GUIイベントループの処理（GUIモードの場合）
                    if gui_mode and HAS_GUI:
                        QtWidgets.QApplication.processEvents()
                        
                except Exception as e:
                    if "timed out" not in str(e).lower():
                        print(f"読み取りエラー: {e}")
                    time.sleep(0.1)  # エラー時の遅延
        
        except KeyboardInterrupt:
            print("\n中断されました")
        
        finally:
            if self.device:
                self.device.close()
                print("デバイスを閉じました")
                
            print(f"\n結果は以下のファイルに保存されました:")
            print(f"CSVファイル: {self.log_file}")
            print(f"JSONファイル: {self.json_file}")
            
            return True


class HIDReportVisualizer(QtWidgets.QMainWindow):
    """HIDレポートビジュアライザーのGUIクラス（PySide6が必要）"""
    
    def __init__(self, analyzer):
        super().__init__()
        self.analyzer = analyzer
        self.setWindowTitle("DOIO KB16 HIDレポートアナライザー")
        self.resize(800, 600)
        
        # 中央ウィジェット
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        layout = QtWidgets.QVBoxLayout(central_widget)
        
        # デバイス情報
        device_group = QtWidgets.QGroupBox("デバイス情報")
        device_layout = QtWidgets.QFormLayout(device_group)
        self.device_info_label = QtWidgets.QLabel("未接続")
        device_layout.addRow("ステータス:", self.device_info_label)
        layout.addWidget(device_group)
        
        # レポート表示
        report_group = QtWidgets.QGroupBox("HIDレポート")
        report_layout = QtWidgets.QVBoxLayout(report_group)
        self.report_text = QtWidgets.QTextEdit()
        self.report_text.setReadOnly(True)
        self.report_text.setFont(QtGui.QFont("Monospace", 10))
        report_layout.addWidget(self.report_text)
        layout.addWidget(report_group)
        
        # コントロールパネル
        control_group = QtWidgets.QGroupBox("コントロール")
        control_layout = QtWidgets.QHBoxLayout(control_group)
        
        self.connect_btn = QtWidgets.QPushButton("接続")
        self.connect_btn.clicked.connect(self.toggle_connection)
        control_layout.addWidget(self.connect_btn)
        
        self.clear_btn = QtWidgets.QPushButton("クリア")
        self.clear_btn.clicked.connect(self.clear_output)
        control_layout.addWidget(self.clear_btn)
        
        control_layout.addStretch()
        layout.addWidget(control_group)
        
        # ステータスバー
        self.statusBar().showMessage("準備完了")
        
        # タイマー設定
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.update_report)
        
        self.is_connected = False
    
    def toggle_connection(self):
        """接続/切断を切り替え"""
        if self.is_connected:
            self.timer.stop()
            if self.analyzer.device:
                self.analyzer.device.close()
                self.analyzer.device = None
            self.is_connected = False
            self.connect_btn.setText("接続")
            self.device_info_label.setText("切断されました")
            self.statusBar().showMessage("切断されました")
        else:
            if self.analyzer.open():
                if not self.analyzer.report_size:
                    self.analyzer._detect_report_size()
                self.analyzer.start_logging()
                self.is_connected = True
                self.connect_btn.setText("切断")
                
                # デバイス情報の更新
                device_info = f"{self.analyzer.device.get_manufacturer_string()} {self.analyzer.device.get_product_string()}"
                self.device_info_label.setText(device_info)
                self.statusBar().showMessage(f"接続成功: VID=0x{self.analyzer.vid:04X}, PID=0x{self.analyzer.pid:04X}")
                
                # タイマー開始（100msごとにレポートを更新）
                self.timer.start(100)
            else:
                self.statusBar().showMessage("接続に失敗しました")
    
    def update_report(self):
        """HIDレポートの更新"""
        if not self.analyzer.device:
            return
        
        try:
            report_data = self.analyzer.device.read(self.analyzer.report_size, timeout_ms=10)
            if report_data:
                # レポートデータをログに記録
                self.analyzer.log_report(report_data)
                
                # レポートの整形表示用の文字列を生成
                output = []
                
                # 時刻
                output.append(f"時刻: {datetime.now().strftime('%H:%M:%S.%f')[:-3]}")
                
                # バイト列を16進数で表示
                hex_data = " ".join([f"{b:02X}" for b in report_data])
                output.append(f"HIDレポート [{len(report_data)}バイト]: {hex_data}")
                
                # レポート形式の解析
                report_format = self.analyzer._analyze_report_format(report_data)
                
                # 修飾キー (Shift, Ctrl, Alt, GUI)
                modifier = report_data[report_format["modifier_index"]]
                mod_str = ""
                shift_pressed = False
                if modifier & 0x01: mod_str += "L-Ctrl "
                if modifier & 0x02: 
                    mod_str += "L-Shift "
                    shift_pressed = True
                if modifier & 0x04: mod_str += "L-Alt "
                if modifier & 0x08: mod_str += "L-GUI "
                if modifier & 0x10: mod_str += "R-Ctrl "
                if modifier & 0x20: 
                    mod_str += "R-Shift "
                    shift_pressed = True
                if modifier & 0x40: mod_str += "R-Alt "
                if modifier & 0x80: mod_str += "R-GUI "
                output.append(f"修飾キー: {mod_str or 'なし'}")
                
                # 押されているキー
                pressed_keys = []
                pressed_chars = []
                if report_format["format"] == "Standard":
                    # 標準的な6KROレポート
                    for idx in report_format["key_indices"]:
                        if idx < len(report_data) and report_data[idx] != 0:
                            keycode = report_data[idx]
                            pressed_keys.append(f"0x{keycode:02X}")
                            # キーコードを文字に変換
                            key_str = self.analyzer.keycode_to_string(keycode, shift_pressed)
                            pressed_chars.append(key_str)
                else:
                    # NKRO形式
                    for i, b in enumerate(report_data[2:], start=2):
                        for bit in range(8):
                            if b & (1 << bit):
                                keycode = (i - 2) * 8 + bit + 4
                                pressed_keys.append(f"0x{keycode:02X}")
                                # キーコードを文字に変換
                                key_str = self.analyzer.keycode_to_string(keycode, shift_pressed)
                                pressed_chars.append(key_str)
                
                output.append(f"押されているキー: {', '.join(pressed_keys) or 'なし'}")
                if pressed_chars:
                    output.append(f"文字表現: {', '.join(pressed_chars)}")
                
                # 変更の検出
                if self.analyzer.last_report:
                    changes = []
                    for i, (old, new) in enumerate(zip(self.analyzer.last_report, report_data)):
                        if old != new:
                            changes.append(f"バイト{i}: 0x{old:02X} -> 0x{new:02X}")
                    
                    if changes:
                        output.append("変更点:")
                        output.extend([f"  {change}" for change in changes])
                
                self.analyzer.last_report = report_data.copy()
                
                # テキスト出力領域に追加
                self.report_text.append("\n".join(output))
                self.report_text.append("-" * 50)
                
                # スクロールを一番下に
                scrollbar = self.report_text.verticalScrollBar()
                scrollbar.setValue(scrollbar.maximum())
                
        except Exception as e:
            if "timed out" not in str(e).lower():
                self.statusBar().showMessage(f"エラー: {e}")
    
    def clear_output(self):
        """出力をクリア"""
        self.report_text.clear()
        self.statusBar().showMessage("出力をクリアしました")


def find_kb16_device():
    """接続されているHIDデバイスからDOIO KB16と思われるデバイスを探す"""
    print("接続されているHIDデバイスを検索中...\n")
    
    try:
        devices = hid.enumerate()
        keyboard_devices = []
        
        print(f"検出されたデバイス数: {len(devices)}")
        print("-" * 60)
        
        for i, dev in enumerate(devices):
            # 基本情報を表示
            vendor_name = dev.get("manufacturer_string", "不明")
            product_name = dev.get("product_string", "不明")
            
            print(f"デバイス {i+1}:")
            print(f"  ベンダーID:   0x{dev['vendor_id']:04X}")
            print(f"  プロダクトID: 0x{dev['product_id']:04X}")
            print(f"  製造元:       {vendor_name}")
            print(f"  製品名:       {product_name}")
            print(f"  使用法:       {dev.get('usage_page', 0):04X}:{dev.get('usage', 0):04X}")
            print(f"  インタフェース番号: {dev.get('interface_number', -1)}")
            
            # HIDキーボードの判定
            is_keyboard = False
            if dev.get("usage_page") == 0x01 and dev.get("usage") == 0x06:
                is_keyboard = True
                print("  タイプ:       キーボード")
                keyboard_devices.append(dev)
            elif "keyboard" in product_name.lower() or "keypad" in product_name.lower():
                is_keyboard = True
                print("  タイプ:       キーボード（製品名から判断）")
                keyboard_devices.append(dev)
            
            print("-" * 60)
        
        if keyboard_devices:
            print(f"\n検出されたキーボードデバイス: {len(keyboard_devices)}個")
            for i, dev in enumerate(keyboard_devices):
                print(f"{i+1}. {dev.get('manufacturer_string', '不明')} {dev.get('product_string', '不明')} - 0x{dev['vendor_id']:04X}:0x{dev['product_id']:04X}")
            
            return keyboard_devices
        else:
            print("\nキーボードデバイスが見つかりませんでした。")
            return []
            
    except Exception as e:
        print(f"エラー: HIDデバイスの列挙に失敗しました - {e}")
        return []


def main():
    """メイン関数"""
    parser = argparse.ArgumentParser(description="DOIO KB16 HIDレポートアナライザー")
    parser.add_argument("--vid", type=lambda x: int(x, 0), help="ベンダーID (例: 0x0483)")
    parser.add_argument("--pid", type=lambda x: int(x, 0), help="プロダクトID (例: 0x572F)")
    parser.add_argument("--size", type=int, help="HIDレポートサイズ (バイト数)")
    parser.add_argument("--list", action="store_true", help="接続されているHIDデバイスを一覧表示")
    parser.add_argument("--gui", action="store_true", help="GUIモードで実行 (PySide6が必要)")
    parser.add_argument("--duration", type=int, help="実行時間 (秒)")
    parser.add_argument("--output", help="出力ディレクトリ")
    args = parser.parse_args()
    
    # デバイス一覧を表示
    if args.list:
        find_kb16_device()
        return
    
    # GUIモードの確認
    if args.gui and not HAS_GUI:
        print("GUIモードには PySide6 が必要です。インストールするには:")
        print("pip install PySide6")
        return
    
    # VID/PIDの設定
    vid = args.vid if args.vid is not None else DEFAULT_VID
    pid = args.pid if args.pid is not None else DEFAULT_PID
    
    # デバイスIDが指定されていない場合、検索を提案
    if not args.vid or not args.pid:
        print("ヒント: 正確なVID/PIDが不明な場合、--listオプションでデバイスを検索できます。")
        print("例: python kb16_hid_report_analyzer.py --list\n")
    
    # アナライザーの初期化
    analyzer = KB16HIDReportAnalyzer(vid=vid, pid=pid, output_dir=args.output)
    
    # レポートサイズの設定（指定されている場合）
    if args.size:
        analyzer.report_size = args.size
    
    # GUIモードで実行
    if args.gui and HAS_GUI:
        app = QtWidgets.QApplication(sys.argv)
        window = HIDReportVisualizer(analyzer)
        window.show()
        sys.exit(app.exec())
    
    # コマンドラインモードで実行
    else:
        print("DOIO KB16 HIDレポートアナライザー")
        print("=" * 60)
        analyzer.run(duration=args.duration, gui_mode=False)


if __name__ == "__main__":
    main()
