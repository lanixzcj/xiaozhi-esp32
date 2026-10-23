"""Exercise transport recordings through the real bounded audio admission path."""
import re
import unittest
import test_animal_recordings as animal
import test_local_interaction as local

class TransportRecordingTests(unittest.TestCase):
    def test_actual_transport_admission(self):
        source = (animal.BOARD / 'transport_game_assets.cc').read_text()
        arrays = re.findall(r'constexpr unsigned char (k\w+Sound)\[\] = \{(.*?)\};', source, re.S)
        self.assertEqual(len(arrays), 4)
        # Reuse the shared recording admission contract with transport bytes;
        # only its historical namespace and expected packet counts differ.
        header = '#include <string_view>\nnamespace animal_game_assets {\n'
        for name, body in arrays:
            header += f'constexpr unsigned char {name}[] = {{{body}}};\n'
        header += 'constexpr size_t kSoundCount = 4;\nstd::string_view GetSound(size_t i) { switch(i) {\n'
        for i, (name, _) in enumerate(arrays):
            header += f'case {i}: return {{reinterpret_cast<const char*>({name}), sizeof({name})}};\n'
        header += 'default: return {};}}}\n'
        case = animal.AnimalRecordingTests('test_actual_admission_accepts_all_recordings_atomically')
        def compile_driver(driver):
            old = 'constexpr size_t counts[] = {40, 35, 34, 45, 41, 36};'
            self.assertIn(old, driver)
            driver = driver.replace(old, 'constexpr size_t counts[] = {27, 41, 33, 42};')
            local.LocalInteractionTests.compile_and_run(case, header + driver,
                [local.ROOT / 'main/audio/demuxer/ogg_demuxer.cc'])
        case.compile_and_run = compile_driver
        case.test_actual_admission_accepts_all_recordings_atomically()

if __name__ == '__main__':
    unittest.main()
