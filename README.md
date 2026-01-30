# Bad Apple!! on STM32F429I-DISC1

*[Phiên bản tiếng Việt](README_vi.md)*

This project implements the iconic "Bad Apple!!" animation on the STM32F429I-DISC1 development board. The animation is displayed on the onboard LCD screen, showcasing the capabilities of the STM32F4 series microcontroller.

![IMG_0900_comp](https://github.com/user-attachments/assets/c089c4a7-4a08-4d95-8248-2419e20599e4)

### Team and member's roles

Team name: NoMMU

| No. | Name | Student Code | Roles |
|-----|------|--------------|-------|
| 1 | Phan Huy Hoang | 20235729 | Implement and optimize custom codec, write encoder for the video |
| 2 | Dang Tien Cuong | 20220020 | Implement seeking for the video and audio |
| 3 | Trinh Tuan Thanh | 20225532 | Video coder, DAC and ADPCM implementation, lyrics show/hide |

### Features

- 160x120, 15FPS video playback on STM32F429I-DISC1's built in ILI9341 display
- 4000Hz ADPCM sample played on STM32's DAC
- Support for external buttons to use additional features: Play/Pause, Seek forward/backward 5 seconds and display/hide lyrics.

### Additional hardware

- 4 Button Board
- LM386 Amplifier Board
- RC circuit for signal filtering

## Schematic Config (on CubeMX)

| STM32F429 | Button | Amplifier |
|-----------|--------|------------|
| PA0 | Default User Button for Play/Pause | |
| PA5 | | DAC Output |
| PE2 | External Button for Lyrics show/hide | |
| PE3 | External Button for Seeking Forward | |
| PE4 | External Button for Seeking Backward | |
| PE5 | External Button for Play/Pause | |
| 5V | | VCC |
| GND | GND | GND |

## Video Implementation
The video pipeline is built around a compact, per-frame RLE bitstream and a double-buffered LCD renderer. Each frame is encoded offline into a custom format with three frame types.

### K-frame (0x00)
Full-frame snapshot. Every scanline is RLE-compressed with 1-bit pixels and run lengths up to 127. This is used periodically as a clean refresh point or when a delta would be larger than a keyframe. It provides fast, predictable decode time at the cost of a larger payload.

### I-frame (0x01)
Delta frame that updates only the rows that changed. A 15-byte bitmap marks which of the 120 rows are modified (1 bit per row). Only those rows are then RLE-encoded and patched. At decode time, the previous frame is copied first, then the marked rows are overlaid, keeping bandwidth low and decode cost small.

### D-frame (0x02)
Duplicate frame used when the current image is identical to the previous one. It contains only the frame type byte and no payload. The decoder simply reuses the last framebuffer contents, making it the smallest possible frame.

At runtime, the firmware looks up a frame’s byte offset from the precomputed `frame_offset` table, decodes the frame, and writes into the back framebuffer. A VSync-synchronized swap (`screen_flip_buffers()`) makes the new frame visible while avoiding tearing. This keeps decoding fast and reduces flash bandwidth while maintaining 15 FPS at 160×120.

See the [encoder folder](encoder/) if you want to use another video instead. The video should have very high contrast for it to be useful.

## Audio

ADPCM Mono 4000Hz is used for audio playback. The [encoder folder](encoder/) also has instructions to make your own.

## Lyrics

We used and edited the lyrics from [this video](https://www.youtube.com/watch?v=UsIAaRLUI9s) to help better align with the two languages.

## Seeking

The video has a keyframe every second to help with seeking to the keyframe every time. As ADPCM is stateful, we introduced separate states every second to help us reset to the right state.

## Known limitations

- The video's implementation is not efficient enough as there is always room for improvement. For example, more types of frames can be implemented, putting the width and height in the beginning of the video, and other compression methods can work better.

- Touch input is not implemented, as our board does not have proper working touchscreen.

## Acknowledgments

This project uses the ADPCM encoder/decoder implementation provided by STMicroelectronics (© 2009 STMicroelectronics), originally distributed as part of STM32 firmware examples.
The code is used for educational purposes only.