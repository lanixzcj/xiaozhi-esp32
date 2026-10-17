# 实战派 ESP32-S3：离线动物触摸小游戏

适用硬件：立创开发板「实战派」ESP32-S3 N16R8，320×240 ST7789、FT5x06 触摸、
ES8311 播放 / ES7210 麦克风。沿用本目录原有引脚、触摸方向、`lichuang-dev` 板型标识、
16 MB 分区和设备端 AEC 设置。

## 怎么进入和退出

1. 刷入本功能固件后，聊天界面右下角会出现「动物」按钮；轻点进入。
2. 小猫、小狗、小鸭依次出现。轻点并松开大图片，播放一次对应的短叫声。
   声音结束、短暂停留后才显示下一只，下一只仍然需要手动点按。
3. 长按图片不会连播；播放期间的连点会被忽略，不排队、不叠加。
4. 三只动物结束后显示「休息一下吧」。家长按住「家长长按再玩」至少 1.5 秒，
   松开后才重新开始。90 秒未完成也会进入休息页；没有倒计时、评分、奖励或自动重开。
5. 任何时候都可轻点右上角「返回」，或单击 BOOT，回到原界面。
   游戏以外的 BOOT 聊天、按住说话及原有配网操作保持原样。

游戏本身不调用网络、服务器 TTS、录音或外部图片服务。画面、全部中文字和三段声音
都随固件存放。原有 Wi-Fi 连接和配置服务仍可运行；这不代表设备进入了飞行模式。
进入游戏时暂停语音检测与麦克风采集、停止当前对话播放并关闭对话音频通道。
退出时先等待最后一帧播放结束，再恢复原有待机/配网状态；不会自动重新开始录音对话。

已保存 Wi-Fi 但暂时离线时也可进入。配网界面允许本地游戏，返回后继续配网。
激活、固件升级和麦克风测试进行中不允许进入，请等待该操作结束。
没有识别到触摸控制器时不会显示游戏按钮。Emote 显示模式不提供此 LVGL 小游戏；
请使用本板默认的 LVGL 显示配置。

音效是原创的轻柔合成叫声，不是动物实录或语音朗读。音量使用设备现有设置，游戏不会
改写音量或 NVS。请家长先在实际扬声器上试听、调低到舒适音量，并陪同孩子使用；
短时长设计不是音量或儿童使用安全的保证。

## 版本和安装边界

此分支基于 fork 的 `e0d2390a1ca415473508bbc0c66eaf1d8dc04e37`，
仓库版本号原本为 **2.5.3**，最低要求 ESP-IDF **6.0.1**。没有合并上游新版本，
也没有将用户设备上口述的「2.3.3」视为已经核实。

**不要仅凭「ESP32-S3」就直接刷入。** 先确认板型、16 MB Flash / 8 MB PSRAM、
当前固件来源、分区表以及现有配置的备份方法。如果设备确实在 2.3.3 或其他定制分区上，
应先核对迁移路径，不能假设仅替换 app 就兼容。

- `build/xiaozhi.bin` 是 app 镜像，只有确认现有 bootloader、分区、资源和 OTA 路径兼容
  后，才可用于相应 app/OTA 更新。
- `build/merged-binary.bin` 是完整合并镜像，包含 bootloader、分区和资源。
  **写入它可能覆盖设备配网、激活和其他保存数据；不是保留配置的安全升级方式。**
- 本变更不执行刷机、擦除、合并到 main 或修改设备设置。不要把编译成功等同于实机验证。

## 从源码构建

在装有官方 ESP-IDF 6.1（或项目支持的 6.0.1 以上版本）的构建环境中：

```sh
git clone --branch feat/lichuang-animal-touch-game https://github.com/lanixzcj/xiaozhi-esp32.git
cd xiaozhi-esp32
source /path/to/esp-idf/export.sh
idf.py --version
python3 scripts/build.py lckfb/szpi-esp32s3 --name lichuang-dev --language zh-CN
```

脚本负责选择板型、PSRAM、Flash 和分区；不要使用其他 S3 板型替代。
构建依赖首次下载需要网络，小游戏运行不需要网络。

## 验证方法

```sh
python3 -m unittest discover -s scripts/tests -v
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_sounds.py --check
```

宿主端测试覆盖会话三轮上限、快速重复点击、不自动重启、时间回绕、独占音频排队、
应用生命周期和过期回调。真实 LVGL UI 还可在桌面无窗口渲染并执行交互断言：

```sh
cmake -S main/boards/lckfb/szpi-esp32s3/tools/host_ui -B /tmp/animal-ui-build \
  -DLVGL_SOURCE_DIR=/path/to/lvgl-v9.5.0
cmake --build /tmp/animal-ui-build -j 4
mkdir -p /tmp/animal-ui-images
/tmp/animal-ui-build/check /tmp/animal-ui-images
```

它编译真实的小游戏 UI 和嵌入字体，使用明确的 app/audio 替身，输出 cat/dog/duck/rest
四张 PPM 截图；覆盖长按不发声、忙时忽略点击、家长重启、30 次进入/退出及聊天屏幕保留。
这不能验证 FT5x06 物理坐标、I2S、扬声器响度或实际网络时序。

### 实机验收清单

- 先试听低音量；确认猫、狗、鸭图片、中文和叫声对应。
- 图片中心和四角都能点；按住 3 秒、连点、按下后划出图片都不会连播。
- 三轮后保持休息页；短点不能重开，1.5 秒长按松开能重开。
- 播放中按「返回」/ BOOT；回到聊天后重新唤醒、说话、播放、打断均正常。
- 已联网、断网、重连、无可用 Wi-Fi 和配网界面分别测试进入/退出。
- 检查游戏期间没有麦克风上传/录音；退出后原唤醒和按住说话模式恢复。
- 连续进入/退出、切换主题/资源更新后无花屏、无失效对象、无内存持续增长。

## 资源来源

- 图片：`animal_touch_game.cc` 中的原创 LVGL 矢量绘制，无外部图片资源。
- 声音：见 [ANIMAL_SOUND_ASSETS.md](ANIMAL_SOUND_ASSETS.md)，原创本地合成，MIT。
- 字体：`animal_game_font.c`，从 Noto Sans CJK SC Regular 2.004 提取的 28 个字形，
  命名为 Animal Game UI；遵循 [SIL OFL 1.1](licenses/animal_game_font-OFL.txt)。
  不依赖聊天的在线动态字形或可被主题替换的字体对象。

重建字体需要 `lv_font_conv` 1.5.3 和 Noto Sans CJK SC Regular 2.004 OTF：

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_font.py \
  /path/to/NotoSansCJKsc-Regular.otf --converter /path/to/lv_font_conv --check
```

上游字体来源：https://github.com/notofonts/noto-cjk 。生成器不联网；替换字体版本或编码器
后需要重新生成、核对授权并检查全部中文显示。
