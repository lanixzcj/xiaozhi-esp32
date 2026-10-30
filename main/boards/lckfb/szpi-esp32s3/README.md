# 实战派 ESP32-S3：听声音认一认

适用硬件：立创开发板「实战派」ESP32-S3 N16R8，320×240 ST7789、FT5x06 触摸、
ES8311 播放 / ES7210 麦克风。沿用本目录原有引脚、触摸方向、`lichuang-dev` 板型标识、
16 MB 分区和设备端 AEC 设置。

给大约一岁的宝宝和家长一起玩。设备放声音，孩子点图片。没有语音识别。
「动物」和「交通」是两个分开的游戏，不能同时进行。

## 怎么进入和退出

1. 聊天界面和配网界面右上角有「动物」「交通」。轻点其中一个进入。
2. 每轮两张大图。一进游戏就会播放其中一张对应的声音，不用先点「听一听」。
   交通是汽车、火车、自行车、飞机。动物是小猫、小狗、小鸭、母鸡、小牛、小羊的真实照片和录音。
3. 点对了，那张图会变大弹一下，边上变绿，并播放一小段欢呼，然后自动播放下一题。
   点错了没有红叉、没有惩罚音，同一段声音再放一遍，两张图保持不变，可以再点。
   声音播放时的点击不会排队。底下的「再听一次」可以重听。
4. 交通四题、动物六题之后显示「休息一下吧」。没有分数，也没有倒计时。
   家长按住「家长长按再玩」至少 1.5 秒，松开后才再开一局。短点不会重开。
   游戏若一直开着约 3 分钟，会自己进入休息页，以便麦克风恢复；屏幕上不显示剩余时间。
5. 任何时候都可轻点「返回」，或单击 BOOT，回到原界面。
   游戏以外的 BOOT 聊天、按住说话及原有配网操作保持原样。

游戏本身不调用网络、服务器 TTS、录音或外部图片服务。画面、全部中文字和六段声音
都随固件存放。原有 Wi-Fi 连接和配置服务仍可运行；这不代表设备进入了飞行模式。
进入游戏时暂停语音检测与麦克风采集、停止当前对话播放并关闭对话音频通道。
退出时先等待最后一帧播放结束，再恢复原有待机/配网状态；不会自动重新开始录音对话。

已保存 Wi-Fi 但暂时离线时也可进入。配网界面允许本地游戏，返回后继续配网。
激活、固件升级和麦克风测试进行中不允许进入，请等待该操作结束。
没有识别到触摸控制器时不会显示游戏按钮。Emote 显示模式不提供此 LVGL 小游戏；
请使用本板默认的 LVGL 显示配置。

动物声音来自真实录音，沿用六段 WAV 音源并转码为 Ogg Opus。
羊叫是同一段真实录音重复两次，中间静音 350 毫秒；其他五种为连续录音片段。
交通声音是四段真实现场录音。答对后的欢呼是一段原创合成音，不是人声，也不是录音。
照片和声音来源独立，并非同一只动物同步录制。音量使用设备现有设置，游戏不会
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

宿主端测试覆盖两个游戏的听声选择策略：两边都会成为答案、选错不换题、
声音没放完不能进入下一题、休息页不会自己重开、时间回绕。桌面还可以无窗口跑动物界面：

```sh
cmake -S main/boards/lckfb/szpi-esp32s3/tools/host_ui -B /tmp/listen-pick-ui \
  -DLVGL_SOURCE_DIR=/path/to/lvgl-v9.5.0
cmake --build /tmp/listen-pick-ui -j 4
mkdir -p /tmp/listen-pick-ui-images
/tmp/listen-pick-ui/check /tmp/listen-pick-ui-images
```

它编译真实的听声选择界面、六张照片和嵌入字体，使用明确的 app/audio 替身。
覆盖自动出题、选错重放、答对欢呼、六轮后休息、家长长按再开、播放中忽略连点、
进入/退出不增长 LVGL 堆，以及聊天屏幕仍在。这不能验证 FT5x06 物理坐标、I2S、
扬声器响度或实际网络时序。

### 实机验收清单

- 先把音量调低。确认六种动物、四种交通的图片和声音对得上，欢呼音量舒适。
- 两张图都够大；图心和图边都能点。播放中按住、连点不会叠下一声。
- 点对会明显变大并欢呼，然后自己播放下一题。点错只重放原声，题目不变。
- 「再听一次」和「返回」在出题和播放时都好点。
- 交通四题、动物六题后停在休息页。短点不能重开，按住 1.5 秒松开才再开。
- 播放中按「返回」或 BOOT。回到聊天后，唤醒、说话、播放、打断都正常。
- 已联网、断网、重连、无可用 Wi-Fi 和配网界面分别进入/退出两个游戏。
- 游戏中另一个按钮不能把游戏抢过去。游戏期间没有麦克风上传。退出后唤醒和按住说话恢复。
- 连续进入/退出后无花屏、无失效对象、无内存持续增长。

## 资源来源

- 真实照片：见 [ANIMAL_PHOTO_ASSETS.md](ANIMAL_PHOTO_ASSETS.md)，六张摄影作品独立保留
  CC BY / CC BY-SA / CC0 许可。裁切、缩放、RGB565 转换属于技术处理；无 AI 生图或修图。
- 卡通：原创界面绘图，MIT，源文件仍在仓库中。当前听声游戏只显示真实照片，不提供照片/卡通切换。
- 声音：见 [ANIMAL_SOUND_ASSETS.md](ANIMAL_SOUND_ASSETS.md)，真实录音独立保留
  PD / CC0 / CC BY-SA 3.0 许可。外部照片和录音不属于代码 MIT 许可。
- 字体：`animal_game_font.c`，从 Noto Sans CJK SC Regular 2.004 提取的 36 个字形，
  命名为 Animal Game UI；遵循 [SIL OFL 1.1](licenses/animal_game_font-OFL.txt)。
  不依赖聊天的在线动态字形或可被主题替换的字体对象。

重建字体需要 `lv_font_conv` 1.5.3 和 Noto Sans CJK SC Regular 2.004 OTF：

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_font.py \
  /path/to/NotoSansCJKsc-Regular.otf --converter /path/to/lv_font_conv --check
```

上游字体来源：https://github.com/notofonts/noto-cjk 。生成器不联网；替换字体版本或编码器
后需要重新生成、核对授权并检查全部中文显示。

## 照片资源与内存

照片原始 JPEG、署名和哈希位于 `assets/photos/`。生成的六张 296×162 RGB565 卡片
共 575,424 字节，像素和描述符为固件 Flash 常量。显示直接读取 RGB565，不在每次
切换时创建 JPEG 解码器，也不依赖主题资源、网络图片或临时图片指针。
卡片保留预览的 cover 裁切位置；新猫图背景更简洁，鸭图为真实白色家鸭。

```sh
python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_photos.py --check
```

照片生成需要 Pillow；音频生成需要 FFmpeg/FFprobe。所有生成器均从仓库内的
固定源文件离线运行，并核对源哈希。真实录音最长 45 个 60 毫秒包（2.70 秒）；
本地独占播放上限为 50 包/3 秒，网络解码的 20 包缓冲上限保持不变。
