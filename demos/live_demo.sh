#!/usr/bin/env bash

# Run the GatorStomp FIFO audio demo.
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
bin_dir="$script_dir/../build/bin"
tools_dir="$script_dir/../tools/audio_io"

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

    # unload the virtual audio interface
    if [[ -n "${MODULE_ID:-}" ]]; then
        pactl unload-module "$MODULE_ID" 2>/dev/null || true
    fi
}
#this means whenever this script exits, execute the cleanup function
trap cleanup EXIT

echo "Converting wav file to a PCM raw input..."
python3 "$tools_dir/wav_to_pcm.py" "$1" "$demo_dir/input.raw"

# create the two named pipes
mkfifo "$demo_dir/capture.pcm"
mkfifo "$demo_dir/playback.pcm"

echo "Creating virtual audio interface"
pactl load-module module-null-sink \
    sink_name=TestSink \
    rate=44100 \
    channels=2

echo "Starting GatorStomp DAC/ADC demo..."

echo "Spinning up DAC process..."
# DAC: read audio from playback FIFO
PULSE_SINK=RDPSink \
"$bin_dir/audio_io_worker" playback \
    "$demo_dir/playback.pcm" alsa pulse 256 &
pids+=("$!")

echo "Spinning up audio orchestrator..."
"$bin_dir/audio_orchestrator" &
pids+=("$!")


echo "Spinning up ADC process..."
# ADC: read simulated audio and send it into capture FIFO.
PULSE_SOURCE=TestSink.monitor \
"$bin_dir/audio_io_worker" capture \
    "$demo_dir/capture.pcm" alsa pulse 256 &
pids+=("$!")

echo "Beginning audio playback across virtual interface"
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