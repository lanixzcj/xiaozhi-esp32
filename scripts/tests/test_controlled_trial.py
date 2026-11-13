"""Real entry/settings compilation with fake NVS; never touches a device."""
import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class ControlledTrialTests(unittest.TestCase):
 def test_fault_injection(self):
  with tempfile.TemporaryDirectory() as tmp:
   d=pathlib.Path(tmp)
   files={
    'sdkconfig.h':'#pragma once\n',
    'esp_err.h':'''#pragma once
#include <cassert>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NO_FREE_PAGES=1,ESP_ERR_NVS_NEW_VERSION_FOUND=2,ESP_ERR_NVS_NOT_FOUND=3;
inline const char* esp_err_to_name(int){return "fault";}
#define ESP_ERROR_CHECK(x) assert((x)==0)
''',
    'esp_log.h':'#pragma once\n#define ESP_LOGW(...) ((void)0)\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGE(...) ((void)0)\n',
    'nvs.h':'''#pragma once
#include "esp_err.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
using nvs_handle_t=unsigned;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
inline int result=0,inits=0,erases=0,writes=0,commits=0,mode=-1,closes=0;
inline bool missing=false;
inline int nvs_open(const char*,int m,unsigned*h){mode=m;*h=missing?0:7;return missing?3:0;}
inline int nvs_commit(unsigned){++commits;return 0;}
inline void nvs_close(unsigned){++closes;}
inline int nvs_set_str(unsigned,const char*,const char*){++writes;return 0;}
inline int nvs_set_i32(unsigned,const char*,int32_t){++writes;return 0;}
inline int nvs_set_u8(unsigned,const char*,uint8_t){++writes;return 0;}
inline int nvs_erase_key(unsigned,const char*){++writes;return 0;}
inline int nvs_erase_all(unsigned){++writes;return 0;}
inline int nvs_get_str(unsigned,const char*,char*p,size_t*n){if(p)std::memcpy(p,"saved",6);*n=6;return 0;}
inline int nvs_get_i32(unsigned,const char*,int32_t*v){*v=42;return 0;}
inline int nvs_get_u8(unsigned,const char*,uint8_t*v){*v=1;return 0;}
''',
    'nvs_flash.h':'#pragma once\n#include "nvs.h"\ninline int nvs_flash_init(){return ++inits==1?result:0;}\ninline int nvs_flash_erase(){++erases;return 0;}\n',
    'application.h':'#pragma once\ninline int app_inits=0,runs=0;\nclass Application{public:static Application& GetInstance(){static Application a;return a;}void Initialize(){++app_inits;}void Run(){++runs;}};\n',
    'driver/gpio.h':'','esp_event.h':'','freertos/FreeRTOS.h':'','freertos/task.h':''}
   for n,s in files.items():
    p=d/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s)
   for n in ['main.cc','settings.cc','settings.h','controlled_trial.h']:(d/n).write_text((ROOT/'main'/n).read_text())
   (d/'test.cc').write_text(r'''
#include "settings.h"
#include "controlled_trial.h"
#include "application.h"
#include <cstdlib>
extern "C" void app_main();
int main(int argc,char**argv){
 bool t=ControlledTrial::Enabled();result=argc>1?std::atoi(argv[1]):0;app_main();
 if(result){assert(app_inits==0&&runs==0&&erases==0&&inits==1);}
 else{assert(app_inits==1&&runs==1&&erases==0);}
 {Settings s("test",true);assert(mode==(t?NVS_READONLY:NVS_READWRITE));
 assert(s.GetString("s")=="saved"&&s.GetInt("i")==42&&s.GetBool("b"));
 s.SetString("s","new");s.SetInt("i",2);s.SetBool("b",false);s.EraseKey("x");s.EraseAll();}
 assert(writes==(t?0:5)&&commits==(t?0:1)&&closes==1);
 if(t){missing=true;Settings a("missing",true);assert(a.GetString("x","fallback")=="fallback"&&a.GetInt("x",17)==17);a.SetInt("x",4);assert(writes==0);}
}
''')
   for t in [False,True]:
    b=d/('trial' if t else 'normal')
    cmd=['clang++','-std=c++17','-Wall','-Wextra','-Werror',f'-I{d}']+(['-DCONFIG_CONTROLLED_TRIAL=1'] if t else [])
    p=subprocess.run(cmd+[str(d/'main.cc'),str(d/'settings.cc'),str(d/'test.cc'),'-o',str(b)],capture_output=True,text=True)
    self.assertEqual(p.returncode,0,p.stderr)
    for status in [0,1,2,99]:subprocess.run([str(b),str(status)],check=True,capture_output=True)
 def test_guard_precedes_original_body(self):
  cases={
   'ota.cc':['NetworkResult<> Ota::CheckVersion() {','void Ota::MarkCurrentVersionValid() {','bool Ota::Upgrade('],
   'assets.cc':['bool Assets::Download('],
   'boards/common/wifi_board.cc':['void WifiBoard::StartNetwork() {','void WifiBoard::TryWifiConnect() {','void WifiBoard::OnWifiConnectTimeout(','void WifiBoard::StartWifiConfigMode() {','void WifiBoard::EnterWifiConfigMode() {','void WifiBoard::SetPowerSaveLevel('],
   'boards/common/system_reset.cc':['void SystemReset::ResetNvsFlash() {','void SystemReset::ResetToFactory() {'],
   'application.cc':['void Application::ActivationTask() {','void Application::CheckAssetsVersion() {','void Application::CheckNewVersion() {','bool Application::UpgradeFirmware('],
   'mcp_server.cc':['void McpServer::AddTool(std::unique_ptr<McpTool> tool) {'],
   'boards/lckfb/szpi-esp32s3/lichuang_dev_board.cc':['void InitializeTools() {']}
  for path,ss in cases.items():
   source=(ROOT/'main'/path).read_text()
   for signature in ss:
    with self.subTest(path=path,signature=signature):
     body=source.split(signature,1)[1]
     if not signature.endswith('{'):body=body.split('{',1)[1]
     self.assertTrue(body.lstrip().startswith(('if (ControlledTrial::Enabled()) {', 'if (ControlledTrial::FirmwareProtected()) {', 'if (ControlledTrial::PreserveSharedAssets()) {')));self.assertIn('return',body.split('}',1)[0])
  s=(ROOT/'main/Kconfig.projbuild').read_text().split('config CONTROLLED_TRIAL',1)[1].split('config OTA_URL',1)[0]
  self.assertIn('default n',s);self.assertIn('BOARD_TYPE_LICHUANG_DEV_S3 && !USE_EMOTE_MESSAGE_STYLE',s)
if __name__=='__main__':unittest.main()
