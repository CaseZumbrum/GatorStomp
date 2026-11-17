#!/usr/bin/env bash

# Run the GatorStomp FIFO audio demo
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
}
#thi means whenever this script exits, execute the cleanup function
trap cleanup EXIT

echo "Converting wav file to a PCM raw input..."
python3 "$tools_dir/wav_to_pcm.py" "$1" "$demo_dir/input.raw"

#create the two named pipes
#IMPORTANT PART pipes are created before any of audio programs start
mkfifo "$demo_dir/capture.pcm"
mkfifo "$demo_dir/playback.pcm"

echo "Starting GatorStomp DAC/ADC demo..."

echo "Spinning up DAC process..."
# DAC: read audio from playback FIFO and save it.
"$bin_dir/audio_io_worker" playback \
    "$demo_dir/playback.pcm" file "$demo_dir/output.raw" &
pids+=("$!")

echo "Spinning up audio orchestrator..."
"$bin_dir/audio_orchestrator" &
pids+=("$!")

echo "Spinning up ADC process..."
# ADC: read simulated audio and send it into capture FIFO.
"$bin_dir/audio_io_worker" capture \
    "$demo_dir/capture.pcm" file "$demo_dir/input.raw" &
pids+=("$!")

# Wait for all three processes to finish.
for pid in "${pids[@]}"; do
    wait "$pid"
done

echo "Writing PCM output to a wav file..."
python3 "$tools_dir/pcm_to_wav.py" "$demo_dir/output.raw" "$2"

echo "Done!"