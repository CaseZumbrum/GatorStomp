#!/usr/bin/env bash

#sudo apt install alsa-utils pulseaudio-utils libasound2-plugins
# Run the GatorStomp FIFO audio demo.
set -euo pipefail


bin_dir="build/bin"
#finds the directory where run_demo.sh itself is located
script_dir="src/audio_io/demo"

#make a temporary folder for the audio files and pipes.
mkdir -p /tmp/gs_demo

demo_dir="/tmp/gs_demo"
pids=()

cleanup() {
    #stop any remaining processes
    for pid in "${pids[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    #wait for processes to exit
    for pid in "${pids[@]}"; do
        wait "$pid" 2>/dev/null || true
    done
    #removes the temporary folder and everything inside it
    rm -rf -- "$demo_dir"

    if [[ -n "${MODULE_ID:-}" ]]; then
        pactl unload-module "$MODULE_ID" 2>/dev/null || true
    fi
}
#thi means whenever this script exits, execute the cleanup function
trap cleanup EXIT

#generate simulated audio input
# python3 "$script_dir/generate_tone.py" "$demo_dir/input.raw" "input.png"

echo "Converting wav file to a PCM raw input..."

python3 "$script_dir/wav_to_pcm.py" "stairway.wav" "$demo_dir/input.raw"

#create the two named pipes
#IMPORTANT PART pipes are created before any of audio programs start
mkfifo "$demo_dir/capture.pcm"
mkfifo "$demo_dir/playback.pcm"

echo "Starting GatorStomp DAC/ADC demo..."

pactl load-module module-null-sink \
    sink_name=TestSink \
    rate=44100 \
    channels=2

echo "Spinning up DAC process..."

# DAC: read audio from playback FIFO and save it.
PULSE_SINK=RDPSink \
"$bin_dir/audio_io_worker" playback \
    "$demo_dir/playback.pcm" alsa pulse 256 &
pids+=("$!")

echo "Spinning up audio orchestrator..."


# Pass-through: forward audio from capture FIFO to playback FIFO.
"$bin_dir/audio_orchestrator" &
pids+=("$!")


echo "Spinning up ADC process..."


# ADC: read simulated audio and send it into capture FIFO.
PULSE_SOURCE=TestSink.monitor \
"$bin_dir/audio_io_worker" capture \
    "$demo_dir/capture.pcm" alsa pulse 256 &
pids+=("$!")

paplay \
    --raw \
    --format=s24le \
    --rate=44100 \
    --channels=2 \
    --device=TestSink \
    "$demo_dir/input.raw"

# Wait for all three processes to finish.
for pid in "${pids[@]}"; do
    wait "$pid"
done

echo "Done!"


# Compare the original and received audio.
# if cmp -s "$demo_dir/input.raw" "$demo_dir/output.raw"; then
#     echo "PASS: Input and output audio match"
# else
#     echo "FAIL: Audio files do not match"
#     exit 1
# fi