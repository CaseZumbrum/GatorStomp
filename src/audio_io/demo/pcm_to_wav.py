import wave
import sys

SAMPLE_RATE = 44100
CHANNELS = 2

with open(sys.argv[1], "rb") as f:
    data = f.read()

with wave.open(sys.argv[2], "wb") as wav:
    wav.setnchannels(CHANNELS)
    wav.setsampwidth(3)
    wav.setframerate(SAMPLE_RATE)
    wav.writeframes(data)
