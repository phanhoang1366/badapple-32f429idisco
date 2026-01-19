import sys 
import glob 
W = 160 
H = 120

def load_pbm(path):
    with open(path, "rb") as f:
        if f.readline().strip() != b"P4":
            raise ValueError("Not P4 PBM")

        # skip comments
        while True:
            line = f.readline()
            if not line.startswith(b"#"):
                break

        w, h = map(int, line.split())
        assert w == W and h == H

        data = f.read()

    frame = [[0]*W for _ in range(H)]
    idx = 0

    for y in range(H):
        for x in range(0, W, 8):
            byte = data[idx]
            idx += 1
            for b in range(8):
                if x + b < W:
                    frame[y][x + b] = (byte >> (7 - b)) & 1

    return frame

def encode_rle_scanline(line):
    out = bytearray()
    x = 0
    while x < W:
        color = line[x]
        run = 1
        while (
            x + run < W and
            line[x + run] == color and
            run < 127
        ):
            run += 1

        out.append((color << 7) | run)
        x += run

    out.append(0x00)  # EOL
    return out

def encode_kframe(frame):
    out = bytearray()
    out.append(0x00)  # frame type: K-frame

    for y in range(H):
        out.extend(encode_rle_scanline(frame[y]))

    return bytes(out), len(out)

def encode_iframe(prev, curr):
    out = bytearray()
    out.append(0x01)  # frame type: I-frame

    # for y in range(H):
    #     if curr[y] == prev[y]:
    #         out.append(0x00)  # unchanged line
    #     else:
    #         out.append(0x01)  # changed line
    #         out.extend(encode_rle_scanline(curr[y]))
    
    bitmap = bytearray(15)

    for y in range(H):
        if curr[y] != prev[y]:
            bitmap[y >> 3] |= (1 << (7 - (y & 7)))

    out.extend(bitmap)

    for y in range(H):
        if bitmap[y >> 3] & (1 << (7 - (y & 7))):
            out.extend(encode_rle_scanline(curr[y]))

    return bytes(out), len(out)

def is_equal(frame1, frame2):
    for y in range(H):
        for x in range(W):
            if frame1[y][x] != frame2[y][x]:
                return False
    return True

def encode_dframe(prev, curr):
    out = bytearray()
    out.append(0x02)  # frame type: D-frame if frame is same as frame i-1

    return bytes(out), len(out)

global iframes_encoded, kframes_encoded, dframes_encoded

iframes_encoded = 0
kframes_encoded = 0
dframes_encoded = 0

def encode_frame(prev, curr, force_k=False):
    global iframes_encoded, kframes_encoded, dframes_encoded
    if prev is None or force_k:
        return encode_kframe(curr)
    
    if is_equal(prev, curr):
        dframes_encoded += 1
        return encode_dframe(prev, curr)
    
    k_data, k_size = encode_kframe(curr)
    i_data, i_size = encode_iframe(prev, curr)

    if i_size < k_size:
        iframes_encoded += 1
        return i_data, i_size
    else:
        kframes_encoded += 1
        return k_data, k_size
    
def main(): 
    initial_path = "outputs/output_frame" 
    if len(sys.argv) > 1: 
        f = initial_path + sys.argv[1] + ".pbm" 
        frame = load_pbm(f)
        encoded, _ = encode_frame(None, frame, force_k=True)
        with open(sys.argv[1] + ".bin", "wb") as out_file:
            out_file.write(encoded)

    else:
        files = sorted(glob.glob(initial_path + "*.pbm"))

        video_file = open("video_data.bin", "wb") 
        
        header_file = open("frame_sizes.h", "w") 
        header_file.write("#ifndef FRAME_SIZES_H_\n") 
        header_file.write("#define FRAME_SIZES_H_\n\n") 
        header_file.write("const uint16_t frame_sizes[] = {\n")

        prev_frame = None
        for i, f in enumerate(files):
            frame = load_pbm(f)
            encoded, size = encode_frame(prev_frame, frame, force_k=(i % 15 == 0))
            video_file.write(encoded)
            header_file.write(f"    {size}, // frame {i}\n")
            prev_frame = frame

        print(f"Encoded {kframes_encoded} K-frames, {iframes_encoded} I-frames, {dframes_encoded} D-frames.")
        
        header_file.write("};\n\n#endif // FRAME_SIZES_H_\n")
        video_file.close()
        header_file.close()

if __name__ == "__main__":
    main()
