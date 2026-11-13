"""Compile actual codec startup/setters with hardware/NVS doubles, no device."""
from pathlib import Path
import tempfile,subprocess,unittest
ROOT=Path(__file__).resolve().parents[2]
class TrialVolumeTest(unittest.TestCase):
 def test_startup_and_hardware_setter_limits_with_default_unchanged(self):
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp)
   for name,data in {'sdkconfig.h':'#pragma once\n','board.h':'#pragma once\n','esp_log.h':'#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n','driver/i2s_common.h':'#pragma once\n','settings.h':'#pragma once\n#include <string>\ninline int saved_volume=70;\nclass Settings{public:Settings(const char*,bool){} int GetInt(const char*,int){return saved_volume;}void SetInt(const char*,int){}};\n','audio_codec.h':'''#pragma once
#include <vector>
#include <cstdint>
class AudioCodec {public:AudioCodec();virtual ~AudioCodec();void Start();void OutputData(std::vector<int16_t>&);bool InputData(std::vector<int16_t>&);void SetOutputVolume(int);void SetInputGain(float);void EnableInput(bool);void EnableOutput(bool);int output_volume(){return output_volume_;}protected:int output_volume_=70;float input_gain_=0;bool input_enabled_=false,output_enabled_=false;int Write(const int16_t*,int){return 0;}int Read(int16_t*,int){return 0;}};
'''}.items():
    p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data)
   (d/'controlled_trial.h').write_text((ROOT/'main/controlled_trial.h').read_text())
   (d/'codec.cc').write_text((ROOT/'main/audio/audio_codec.cc').read_text())
   box=(ROOT/'main/audio/codecs/box_audio_codec.cc').read_text().split('void BoxAudioCodec::SetOutputVolume(int volume) {',1)[1].split('\n}',1)[0]
   (d/'test.cc').write_text('''#include "audio_codec.h"
#include "controlled_trial.h"
#include "settings.h"
#include <cassert>
inline int hardware_volume=-99;
inline int esp_codec_dev_set_out_vol(void*,int v){hardware_volume=v;return 0;}
#define ESP_ERROR_CHECK(x) assert((x)==0)
class BoxAudioCodec:public AudioCodec {public:void* output_dev_=nullptr;void SetOutputVolume(int);};
void BoxAudioCodec::SetOutputVolume(int volume) {'''+box+'''
}
int main(){for(int value:{-9,0,1,3,5,6,70,100}){
 saved_volume=value;BoxAudioCodec c;c.Start();
 int expect=ControlledTrial::Enabled()?ControlledTrial::ClampOutputVolume(value):(value<=0?10:value);
 assert(c.output_volume()==expect);
 c.SetOutputVolume(value);expect=ControlledTrial::ClampOutputVolume(value);
 assert(c.output_volume()==expect&&hardware_volume==expect);
}}
''')
   for trial in [0,1]:
    binary=d/f'volume-{trial}'
    p=subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',f'-DCONFIG_CONTROLLED_TRIAL={trial}',f'-I{d}',str(d/'codec.cc'),str(d/'test.cc'),'-o',str(binary)],text=True,capture_output=True)
    self.assertEqual(p.returncode,0,p.stderr);subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
