"""scripts/sound/extract.py <all.snd> <id> <out.wav>: sound `id` of all.snd as a RIFF image, as SoundEffects::LoadSound
(0x0049DF60) rebuilds it: the index from 0x1C (0x2C bytes a sound: size at +0x14, the low 26 bits; offset at +0x18; kind
at +0x20, the low 2 bits); a kind 0 sound is its fmt record, put after the 0x18 bytes of the header template; every
kind but 1 gets the header fields patched (the fmt moved to +0x14, its size 0x10, the RIFF size, "data" and its size).
"""

import struct
import sys

INDEX_AT = 0x1C
RECORD_BYTES = 0x2C
SIZE_MASK = 0x3FFFFFF
TEMPLATE = b'RIFF....WAVEfmt ....0123456789012345data..'  # 0x004C03FC
BARE_FMT_KIND = 0
WHOLE_RIFF_KIND = 1
BARE_FMT_PAD = 0x18


def main():
    path, ident, out_path = sys.argv[1], int(sys.argv[2], 0), sys.argv[3]
    data = open(path, 'rb').read()
    record = INDEX_AT + RECORD_BYTES * (ident - 1)
    size = struct.unpack_from('<I', data, record + 0x14)[0] & SIZE_MASK
    offset = struct.unpack_from('<I', data, record + 0x18)[0]
    kind = struct.unpack_from('<I', data, record + 0x20)[0] & 3
    if offset == 0:
        sys.exit(f'no sound {ident}')
    pad = BARE_FMT_PAD if kind == BARE_FMT_KIND else 0
    image = bytearray(max(size + pad, 0x2C))
    image[:len(TEMPLATE) + 1] = TEMPLATE + b'\0'
    image[pad:pad + size] = data[offset:offset + size]
    if kind != WHOLE_RIFF_KIND:
        image[0x14:0x14 + 0x12] = bytes(image[0x18:0x18 + 0x12])
        struct.pack_into('<I', image, 0x10, 0x10)
        struct.pack_into('<I', image, 4, size)
        image[0x26:0x28] = b'ta'
        struct.pack_into('<I', image, 0x28, size - 0x18)
    open(out_path, 'wb').write(image)
    channels, rate, _, _, bits = struct.unpack_from('<HIIHH', image, 0x16)
    print(f'sound {ident}: kind {kind}, {size} bytes, {channels} channel(s), {rate} Hz, {bits} bits')


main()
