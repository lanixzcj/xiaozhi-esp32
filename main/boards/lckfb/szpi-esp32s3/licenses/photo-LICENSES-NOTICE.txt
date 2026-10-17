# Real animal photo assets

These six photographs retain their individual licenses. They are not covered by the repository code MIT license. The original publisher thumbnails, SHA-256 checksums, creators and source links are preserved in `assets/photos/provenance.json`.

Technical adaptations: aspect-preserving crop and LANCZOS resize to 296×162, then RGB565 color quantization. No AI generation, background removal, compositing of animal content or retouching. Cat uses a centered cover crop. Duck removes only the empty top 150 pixels of the 960×905 source, then uses contain with #eee6d7 side bars to preserve beak, body and both feet. Other photos retain the approved preview cover positions. Photos and recordings have independent sources and are not synchronous recordings of one animal.

The cat photo and its RGB565 derivative are available under CC BY-SA 3.0. Other independent files retain their own licenses. No original creator endorses this project.

## cat: Cat August 2010-4.jpg

Creator: Alvesgaspar
Source: https://commons.wikimedia.org/wiki/File:Cat_August_2010-4.jpg
License: CC BY-SA 3.0 — https://creativecommons.org/licenses/by-sa/3.0/
Original thumbnail SHA-256: `835123381887386ee293925e761e91eaacc76bf94dedd544e7e26dd0281f66b9`
Derivative: `cat` pixels in `animal_game_photos.c`, same license as the source photograph.

## dog: Golden Retriever Hund Dog.JPG

Creator: Kintaiyo
Source: https://commons.wikimedia.org/wiki/File:Golden_Retriever_Hund_Dog.JPG
License: CC BY 3.0 — https://creativecommons.org/licenses/by/3.0/
Original thumbnail SHA-256: `6aeca0694def73b18d67098856de2657f3d0d198cb9b010bc0417b6fd6d918ec`
Derivative: `dog` pixels in `animal_game_photos.c`, same license as the source photograph.

## duck: Aylesbury Duck. (9373723157).jpg

Creator: Bernard Spragg. NZ
Source: https://commons.wikimedia.org/wiki/File:Aylesbury_Duck._(9373723157).jpg
License: CC0 1.0 — https://creativecommons.org/publicdomain/zero/1.0/
Original thumbnail SHA-256: `cc7fe6adc2a05f42020bd1ae916eba726a99a7092829446cc2f97f3f41692ae3`
Derivative: `duck` pixels in `animal_game_photos.c`, same license as the source photograph.

## chicken: Hen 0001.jpg

Creator: Lilly M
Source: https://commons.wikimedia.org/wiki/File:Hen_0001.jpg
License: CC BY 2.5 — https://creativecommons.org/licenses/by/2.5/
Original thumbnail SHA-256: `6da74abf2ac499e9704e65af486c6c6c615769f82f2fbe5b1abb1d53a7bb0947`
Derivative: `chicken` pixels in `animal_game_photos.c`, same license as the source photograph.

## cow: Holstein cow walking on farm in La Petite Gaspesie 2025.jpg

Creator: Hayden Soloviev
Source: https://commons.wikimedia.org/wiki/File:Holstein_cow_walking_on_farm_in_La_Petite_Gaspesie_2025.jpg
License: CC BY 4.0 — https://creativecommons.org/licenses/by/4.0/
Original thumbnail SHA-256: `fe1edcf8789a5349428b0d012953e278a6cc29199b6ea515770342c11176b1d3`
Derivative: `cow` pixels in `animal_game_photos.c`, same license as the source photograph.

## sheep: White sheep - Avebury, Wiltshire, England - DSC00332.jpg

Creator: Daderot
Source: https://commons.wikimedia.org/wiki/File:White_sheep_-_Avebury,_Wiltshire,_England_-_DSC00332.jpg
License: CC0 1.0 — https://creativecommons.org/publicdomain/zero/1.0/
Original thumbnail SHA-256: `7123a6fba94b2d000a27671247ce9e99d781c19c814a01c1d28f89a44e93d544`
Derivative: `sheep` pixels in `animal_game_photos.c`, same license as the source photograph.

## Reproduce

`python3 main/boards/lckfb/szpi-esp32s3/tools/generate_animal_photos.py --check`

Requires Pillow. Source inputs are committed; generation is offline. Per-card output hashes and transformation details are in `assets/photos/processing.json`. Pixel bytes and LVGL descriptors are immutable flash-resident resources.
