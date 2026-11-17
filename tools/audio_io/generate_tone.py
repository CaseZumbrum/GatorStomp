"""Generate one second of 440 Hz, 96000 Hz stereo signed packed S24_3LE PCM."""
import math
import sys

#create file containing simulated audio
with open(sys.argv[1], "wb") as output:
    #generate 96000 frames (sampling rate 96000 per second)
    for frame in range(96000):
        value = round(0.25 * ((1 << 23) - 1) * math.sin(2 * math.pi * 440 * frame / 96000))
        sample = value.to_bytes(3, byteorder="little", signed=True)
        output.write(sample * 2)  # Identical left and right samples.
print("Generated 96000 stereo frames (576000 bytes), 440 Hz")
