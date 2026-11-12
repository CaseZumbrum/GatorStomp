import soundfile as sf
import numpy as np
import sys

# Read WAV file
audio, sample_rate = sf.read(sys.argv[1], dtype="int32")

print(f"Sample rate: {sample_rate}")
print(f"Channels: {audio.shape[1] if audio.ndim > 1 else 1}")

# Convert to signed 24-bit
samples = audio >> 8

# Write packed 24-bit little-endian PCM
with open(sys.argv[2], "wb") as output:
    for sample in samples.flatten():
        output.write(int(sample).to_bytes(3, byteorder="little", signed=True))

print("Conversion complete")
