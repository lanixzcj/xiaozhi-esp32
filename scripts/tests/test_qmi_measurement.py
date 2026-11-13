"""Exercise the actual QMI driver with a register-backed I2C substitute."""
from pathlib import Path
import subprocess
import tempfile
import unittest

BOARD = Path(__file__).resolve().parents[2] / 'main/boards/lckfb/szpi-esp32s3'


class QmiMeasurementTests(unittest.TestCase):
    def test_normal_measurement_signed_axes_and_readback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            files = {
                'driver/i2c_master.h': r'''
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
using i2c_master_bus_handle_t = void*;
using i2c_master_dev_handle_t = void*;
struct i2c_device_config_t {
 int dev_addr_length; uint16_t device_address; uint32_t scl_speed_hz;
 uint32_t scl_wait_us; struct { unsigned disable_ack_check : 1; } flags;
};
constexpr int ESP_OK=0, I2C_ADDR_BIT_LEN_7=0;
inline uint8_t registers[256]={};
inline bool corrupt_config=false;
inline int removed=0;
inline int i2c_master_probe(void*,uint8_t,int) {return ESP_OK;}
inline int i2c_master_bus_add_device(void*,const i2c_device_config_t*,void** d) {
 *d=reinterpret_cast<void*>(2);return ESP_OK;
}
inline int i2c_master_bus_rm_device(void*) {++removed;return ESP_OK;}
inline int i2c_master_transmit(void*,const uint8_t* b,size_t n,int) {
 if(n==2) registers[b[0]]=b[1]; return ESP_OK;
}
inline int i2c_master_transmit_receive(void*,const void* tx,size_t,void* rx,size_t n,int) {
 auto reg=*static_cast<const uint8_t*>(tx);
 std::memcpy(rx,registers+reg,n);
 if(corrupt_config && reg==3) *static_cast<uint8_t*>(rx)|=0x80;
 return ESP_OK;
}
''',
                'freertos/FreeRTOS.h': '#pragma once\n#define pdMS_TO_TICKS(x) (x)\n',
                'freertos/task.h': '#pragma once\ninline void vTaskDelay(int) {}\n',
                'esp_log.h': '#pragma once\n#define ESP_LOGW(...) ((void)0)\n#define ESP_LOGI(...) ((void)0)\n',
                'check.cc': r'''
#include "qmi8658.h"
#include <cassert>
#include <initializer_list>
void Axis(int reg,int16_t value) {
 registers[reg]=uint16_t(value)&255;registers[reg+1]=uint16_t(value)>>8;
}
int main() {
 registers[0]=5;
 Qmi8658 imu; assert(imu.Init(reinterpret_cast<void*>(1)));
 assert(registers[2]==0x40); // auto-increment, little endian
 assert(registers[3]==0x15 && !(registers[3]&0x80)); // ±4g/250Hz, no self-test
 assert(registers[4]==0x55 && !(registers[4]&0x80)); // 512dps/250Hz, no self-test
 assert(registers[8]==3);
 for(int16_t x : {int16_t(-8192),int16_t(8192)}) {
  for(int16_t y : {int16_t(-8192),int16_t(8192)}) {
   Axis(0x35,x);Axis(0x37,y);Axis(0x39,8192);
   int16_t a,b,c;assert(imu.ReadAccel(a,b,c));assert(a==x&&b==y&&c==8192);
  }
 }
 corrupt_config=true;
 Qmi8658 bad;assert(!bad.Init(reinterpret_cast<void*>(1)));
 assert(!bad.available() && removed==1);
}
''',
            }
            for name, text in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            subprocess.run(['g++', '-std=c++17', '-I', str(root), '-I', str(BOARD),
                            str(BOARD / 'qmi8658.cc'), str(root / 'check.cc'),
                            '-o', str(root / 'check')], check=True)
            subprocess.run([str(root / 'check')], check=True)
