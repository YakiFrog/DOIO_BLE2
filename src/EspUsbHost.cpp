#include "EspUsbHost.h"

void EspUsbHost::begin(void) {
  usbTransferSize = 0;
  usbInterfaceSize = 0;
  isReady = false;
  interval = 20; // デフォルト20ms間隔（負荷軽減）
  lastCheck = 0;
  claim_err = ESP_FAIL;
  
  // キーマトリックス初期化
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      kb16_key_states[i][j] = false;
    }
  }

  const usb_host_config_t config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  esp_err_t err = usb_host_install(&config);
  if (err != ESP_OK) {
    ESP_LOGI("EspUsbHost", "usb_host_install() err=%x", err);
  } else {
    ESP_LOGI("EspUsbHost", "usb_host_install() ESP_OK");
  }

  const usb_host_client_config_t client_config = {
    .is_synchronous = true,
    .max_num_event_msg = 10,
    .async = {
      .client_event_callback = this->_clientEventCallback,
      .callback_arg = this,
    }
  };
  err = usb_host_client_register(&client_config, &this->clientHandle);
  if (err != ESP_OK) {
    ESP_LOGI("EspUsbHost", "usb_host_client_register() err=%x", err);
  } else {
    ESP_LOGI("EspUsbHost", "usb_host_client_register() ESP_OK");
  }
}

void EspUsbHost::_clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg) {
  EspUsbHost *usbHost = (EspUsbHost *)arg;

  esp_err_t err;
  switch (eventMsg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGI("EspUsbHost", "USB_HOST_CLIENT_EVENT_NEW_DEV new_dev.address=%d", eventMsg->new_dev.address);
      err = usb_host_device_open(usbHost->clientHandle, eventMsg->new_dev.address, &usbHost->deviceHandle);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_device_open() err=%x", err);
      } else {
        ESP_LOGI("EspUsbHost", "usb_host_device_open() ESP_OK");
      }

      usb_device_info_t dev_info;
      err = usb_host_device_info(usbHost->deviceHandle, &dev_info);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_device_info() err=%x", err);
      } else {
        ESP_LOGI("EspUsbHost", "usb_host_device_info() ESP_OK");
      }

      const usb_device_desc_t *dev_desc;
      err = usb_host_get_device_descriptor(usbHost->deviceHandle, &dev_desc);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_get_device_descriptor() err=%x", err);
      } else {
        usbHost->device_vendor_id = dev_desc->idVendor;
        usbHost->device_product_id = dev_desc->idProduct;
        ESP_LOGI("EspUsbHost", "VID: 0x%04X, PID: 0x%04X", usbHost->device_vendor_id, usbHost->device_product_id);
      }

      // デバイス情報を通知
      usbHost->onNewDevice(dev_info);
      
      // コンフィグレーション処理
      const usb_config_desc_t *config_desc;
      err = usb_host_get_active_config_descriptor(usbHost->deviceHandle, &config_desc);
      if (err != ESP_OK) {
        ESP_LOGI("EspUsbHost", "usb_host_get_active_config_descriptor() err=%x", err);
      } else {
        ESP_LOGI("EspUsbHost", "usb_host_get_active_config_descriptor() ESP_OK");
        usbHost->_configCallback(config_desc);
      }
      break;

    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      ESP_LOGI("EspUsbHost", "USB_HOST_CLIENT_EVENT_DEV_GONE");
      
      // 転送とインターフェースをクリーンアップ
      for (int i = 0; i < usbHost->usbTransferSize; i++) {
        if (usbHost->usbTransfer[i] != NULL) {
          usb_host_transfer_free(usbHost->usbTransfer[i]);
          usbHost->usbTransfer[i] = NULL;
        }
      }
      usbHost->usbTransferSize = 0;

      for (int i = 0; i < usbHost->usbInterfaceSize; i++) {
        usb_host_interface_release(usbHost->clientHandle, usbHost->deviceHandle, usbHost->usbInterface[i]);
        usbHost->usbInterface[i] = 0;
      }
      usbHost->usbInterfaceSize = 0;
      
      usbHost->isReady = false;
      usb_host_device_close(usbHost->clientHandle, usbHost->deviceHandle);
      
      usbHost->onGone(eventMsg);
      break;

    default:
      ESP_LOGI("EspUsbHost", "Unknown event: %d", eventMsg->event);
      break;
  }
}

void EspUsbHost::task(void) {
  // ライブラリイベント処理（タイムアウトを短縮）
  esp_err_t err = usb_host_lib_handle_events(10, &this->eventFlags); // 10msタイムアウト
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI("EspUsbHost", "usb_host_lib_handle_events() err=%x eventFlags=%x", err, this->eventFlags);
  }

  // クライアントイベント処理（タイムアウトを短縮）
  err = usb_host_client_handle_events(this->clientHandle, 10); // 10msタイムアウト
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI("EspUsbHost", "usb_host_client_handle_events() err=%x", err);
  }

  // USB転送処理（頻度を制限）
  if (this->isReady) {
    unsigned long now = millis();
    if ((now - this->lastCheck) > this->interval) {
      this->lastCheck = now;

      for (int i = 0; i < this->usbTransferSize; i++) {
        if (this->usbTransfer[i] == NULL) {
          continue;
        }

        esp_err_t err = usb_host_transfer_submit(this->usbTransfer[i]);
        if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
          // エラーログは頻繁になるため抑制
        }
      }
    }
  }
}

String EspUsbHost::getUsbDescString(const usb_str_desc_t *str_desc) {
  if (str_desc == nullptr) {
    return "";
  }
  
  String result = "";
  int len = (str_desc->bLength - 2) / 2; // UTF-16のバイト数を文字数に変換
  for (int i = 0; i < len; i++) {
    uint16_t utf16_char = str_desc->wData[i];
    if (utf16_char < 0x80) {
      result += (char)utf16_char;
    }
  }
  return result;
}

void EspUsbHost::updateKB16KeyState(uint8_t row, uint8_t col, bool pressed) {
  if (row < 4 && col < 4) {
    kb16_key_states[row][col] = pressed;
  }
}

bool EspUsbHost::getKB16KeyState(uint8_t row, uint8_t col) {
  if (row < 4 && col < 4) {
    return kb16_key_states[row][col];
  }
  return false;
}

uint8_t EspUsbHost::getKeycodeToAscii(uint8_t keycode, uint8_t shift) {
  const uint8_t hidKeycodeTo[][2] = { HID_KEYCODE_TO_ASCII_JA };
  
  if (keycode < sizeof(hidKeycodeTo) / sizeof(hidKeycodeTo[0])) {
    return hidKeycodeTo[keycode][shift ? 1 : 0];
  }
  return 0;
}

void EspUsbHost::onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {
  // 基本的なキーボード処理の実装
  for (int i = 0; i < 6; i++) {
    if (report.keycode[i] != 0) {
      bool isNewKey = true;
      for (int j = 0; j < 6; j++) {
        if (report.keycode[i] == last_report.keycode[j]) {
          isNewKey = false;
          break;
        }
      }
      
      if (isNewKey) {
        uint8_t ascii = getKeycodeToAscii(report.keycode[i], report.modifier & 0x22);
        onKeyboardKey(ascii, report.keycode[i], report.modifier);
      }
    }
  }
}

void EspUsbHost::onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) {
  // デフォルトの実装（オーバーライド用）
}

void EspUsbHost::onMouse(hid_mouse_report_t report, uint8_t last_buttons) {
  // マウス処理の基本実装
}

void EspUsbHost::onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons) {
  // マウスボタン処理の基本実装
}

void EspUsbHost::onMouseMove(hid_mouse_report_t report) {
  // マウス移動処理の基本実装
}

void EspUsbHost::_onDataGamepad() {
  // ゲームパッド処理の基本実装
}

void EspUsbHost::setHIDLocal(hid_local_enum_t code) {
  hidLocal = code;
}

void EspUsbHost::_printPcapText(const char *title, uint16_t function, uint8_t direction, uint8_t endpoint, uint8_t type, uint8_t size, uint8_t stage, const uint8_t *data) {
  // PCAPテキスト出力（デバッグ用）
  String data_str = "";
  for (int i = 0; i < size; i++) {
    if (data[i] < 16) {
      data_str += "0";
    }
    data_str += String(data[i], HEX) + " ";
  }
  ESP_LOGI("EspUsbHost", "[PCAP] %s: %s", title, data_str.c_str());
}

esp_err_t EspUsbHost::submitControl(const uint8_t bmRequestType, const uint8_t bDescriptorIndex, const uint8_t bDescriptorType, const uint16_t wInterfaceNumber, const uint16_t wDescriptorLength) {
  // コントロール転送の実装
  return ESP_OK;
}

void EspUsbHost::_onReceiveControl(usb_transfer_t *transfer) {
  // コントロール転送受信処理
}

void EspUsbHost::_onReceive(usb_transfer_t *transfer) {
  EspUsbHost *usbHost = (EspUsbHost *)transfer->context;
  
  // HIDクラスのデバイスかチェック
  if (transfer->actual_num_bytes >= 8) {
    static hid_keyboard_report_t last_report = {};
    
    // レポートデータが変化した場合のみ処理
    if (memcmp(&last_report, transfer->data_buffer, sizeof(last_report))) {
      hid_keyboard_report_t report = {};
      report.modifier = transfer->data_buffer[0];
      report.reserved = transfer->data_buffer[1];
      report.keycode[0] = transfer->data_buffer[2];
      report.keycode[1] = transfer->data_buffer[3];
      report.keycode[2] = transfer->data_buffer[4];
      report.keycode[3] = transfer->data_buffer[5];
      report.keycode[4] = transfer->data_buffer[6];
      report.keycode[5] = transfer->data_buffer[7];

      // キーボード処理を呼び出し
      usbHost->onKeyboard(report, last_report);
      
      memcpy(&last_report, &report, sizeof(last_report));
    }
  }
}

void EspUsbHost::_configCallback(const usb_config_desc_t *config_desc) {
  const uint8_t *p = &config_desc->val[0];
  uint8_t bLength;

  for (int i = 0; i < config_desc->wTotalLength; i += bLength, p += bLength) {
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1);
      this->onConfig(bDescriptorType, p);
    } else {
      return;
    }
  }
}

void EspUsbHost::onConfig(const uint8_t bDescriptorType, const uint8_t *p) {
  static uint8_t _bInterfaceNumber = 0;
  static uint8_t _bInterfaceClass = 0;
  static uint8_t _bInterfaceSubClass = 0;
  static uint8_t _bInterfaceProtocol = 0;
  static uint8_t _bCountryCode = 0;
  
  switch (bDescriptorType) {
    case USB_INTERFACE_DESC:
      {
        const usb_intf_desc_t *intf_desc = (const usb_intf_desc_t *)p;
        _bInterfaceNumber = intf_desc->bInterfaceNumber;
        _bInterfaceClass = intf_desc->bInterfaceClass;
        _bInterfaceSubClass = intf_desc->bInterfaceSubClass;
        _bInterfaceProtocol = intf_desc->bInterfaceProtocol;
        
        ESP_LOGI("EspUsbHost", "USB_INTERFACE_DESC Interface=%d Class=0x%02X SubClass=0x%02X Protocol=0x%02X", 
                 _bInterfaceNumber, _bInterfaceClass, _bInterfaceSubClass, _bInterfaceProtocol);
        
        // HIDインターフェースの場合はクレーム
        if (_bInterfaceClass == USB_CLASS_HID) {
          esp_err_t err = usb_host_interface_claim(this->clientHandle, this->deviceHandle, _bInterfaceNumber, 0);
          if (err != ESP_OK) {
            ESP_LOGI("EspUsbHost", "usb_host_interface_claim() err=%x", err);
            this->claim_err = err;
            return;
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_interface_claim() ESP_OK Interface=%d", _bInterfaceNumber);
            this->usbInterface[this->usbInterfaceSize] = _bInterfaceNumber;
            this->usbInterfaceSize++;
            this->claim_err = ESP_OK;
          }
        }
      }
      break;

    case USB_ENDPOINT_DESC:
      {
        const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)p;
        
        ESP_LOGI("EspUsbHost", "USB_ENDPOINT_DESC Addr=0x%02X Attr=0x%02X MaxPacket=%d", 
                 ep_desc->bEndpointAddress, ep_desc->bmAttributes, ep_desc->wMaxPacketSize);
        
        // HIDクラスかつINT転送のエンドポイントをセットアップ
        if (_bInterfaceClass == USB_CLASS_HID && 
            (ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT &&
            (ep_desc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK)) {
          
          if (this->claim_err != ESP_OK) {
            ESP_LOGI("EspUsbHost", "Skipping endpoint due to claim error");
            return;
          }

          // 転送バッファを割り当て
          esp_err_t err = usb_host_transfer_alloc(ep_desc->wMaxPacketSize + 1, 0, &this->usbTransfer[this->usbTransferSize]);
          if (err != ESP_OK) {
            this->usbTransfer[this->usbTransferSize] = NULL;
            ESP_LOGI("EspUsbHost", "usb_host_transfer_alloc() err=%x", err);
            return;
          } else {
            ESP_LOGI("EspUsbHost", "usb_host_transfer_alloc() ESP_OK size=%d", ep_desc->wMaxPacketSize + 1);
          }

          // 転送設定
          this->usbTransfer[this->usbTransferSize]->device_handle = this->deviceHandle;
          this->usbTransfer[this->usbTransferSize]->bEndpointAddress = ep_desc->bEndpointAddress;
          this->usbTransfer[this->usbTransferSize]->callback = this->_onReceive;
          this->usbTransfer[this->usbTransferSize]->context = this;
          this->usbTransfer[this->usbTransferSize]->num_bytes = ep_desc->wMaxPacketSize;
          this->interval = ep_desc->bInterval;
          this->isReady = true;
          this->usbTransferSize++;
          
          ESP_LOGI("EspUsbHost", "HID endpoint configured successfully");
        }
      }
      break;

    case USB_HID_DESC:
      {
        ESP_LOGI("EspUsbHost", "USB_HID_DESC detected");
      }
      break;

    default:
      break;
  }
}
