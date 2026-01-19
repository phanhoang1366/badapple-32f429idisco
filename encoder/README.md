Convert the video to a sequence of images, and ADPCM audio using ffmpeg:

```bash
ffmpeg -i BadApplePV.webm -an -vf "scale=-1:120:flags=neighbor,fps=15,format=gray,boxblur=1:1,lut=y='if(gte(val,110),255,0)'" outputs/output_frame%04d.pbm
ffmpeg -i BadApplePV.webm -vn -ac 1 -ar 4000 -f wav - | sox -v 0.99 -t wav - audio.ima
```

This will create a series of PBM images in the `outputs` directory and an ADPCM audio file named `audio.ima`. The video is scaled to a height of 120 pixels, converted to grayscale, blurred slightly, and thresholded to create a binary image. The audio is converted to mono with a sample rate of 4000 Hz.

Next, run the encoder to convert the images into the desired format and make it a C header file:

```bash
python3 encoder.py
xxd -i video_data.bin > video_data.h
xxd -i audio.ima > audio_data.h
```

Now copy the generated `video_data.h`, `frame_sizes.h`, and `audio_data.h` files into your project directory to use them in your application.