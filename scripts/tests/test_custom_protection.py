"""Compile policy combinations and verify guarded entry ordering; no device access."""
import pathlib, subprocess, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[2]
class CustomProtectionTests(unittest.TestCase):
 def test_independent_flags_default_and_normal_volume(self):
  with tempfile.TemporaryDirectory() as tmp:
   d=pathlib.Path(tmp); (d/'sdkconfig.h').write_text('')
   (d/'controlled_trial.h').write_text((ROOT/'main/controlled_trial.h').read_text())
   (d/'test.cc').write_text('#include "controlled_trial.h"\n#include <cassert>\nint main(){assert(ControlledTrial::Enabled()==EXPECTED_TRIAL);assert(ControlledTrial::FirmwareProtected()==EXPECTED_FIRMWARE);assert(ControlledTrial::PreserveSharedAssets()==EXPECTED_ASSETS);assert(ControlledTrial::ClampOutputVolume(70)==(EXPECTED_TRIAL?5:70));}\n')
   for trial, firmware, assets in [(0,0,0),(0,1,0),(0,0,1),(0,1,1),(1,0,0)]:
    flags=[f'-DEXPECTED_TRIAL={trial}',f'-DEXPECTED_FIRMWARE={trial or firmware}',f'-DEXPECTED_ASSETS={trial or assets}']
    for key,value in [('CONTROLLED_TRIAL',trial),('CUSTOM_FIRMWARE_PROTECTION',firmware),('CUSTOM_PRESERVE_SHARED_ASSETS',assets)]:
     if value: flags.append(f'-DCONFIG_{key}=1')
    subprocess.run(['clang++','-std=c++17',f'-I{d}',*flags,str(d/'test.cc'),'-o',str(d/'test')],check=True,capture_output=True)
    subprocess.run([str(d/'test')],check=True,capture_output=True)
 def test_network_refresh_and_guards_remain_separate(self):
  app=(ROOT/'main/application.cc').read_text()
  body=app.split('void Application::CheckAssetsVersion()',1)[1].split('void Application::CheckNewVersion()',1)[0]
  self.assertLess(body.index('PreserveSharedAssets()'),body.index('Settings settings'))
  self.assertLess(body.index('Assets::GetInstance().Apply()'),body.index('settings.EraseKey'))
  self.assertIn('!ControlledTrial::FirmwareProtected() && ota_->HasNewVersion()',app)
  ota=(ROOT/'main/ota.cc').read_text()
  check=ota.split('NetworkResult<> Ota::CheckVersion()',1)[1].split('void Ota::MarkCurrentVersionValid()',1)[0]
  self.assertIn('if (ControlledTrial::Enabled())',check)
  self.assertNotIn('if (ControlledTrial::FirmwareProtected())',check)
  self.assertIn('Settings settings("mqtt", true)',check)
  for sig in ['void Ota::MarkCurrentVersionValid()', 'bool Ota::Upgrade(']:
   self.assertIn('if (ControlledTrial::FirmwareProtected())',ota.split(sig,1)[1].split('return',1)[0])
  self.assertIn('if (ControlledTrial::PreserveSharedAssets())',(ROOT/'main/assets.cc').read_text().split('bool Assets::Download(',1)[1].split('return',1)[0])
if __name__=='__main__':unittest.main()
