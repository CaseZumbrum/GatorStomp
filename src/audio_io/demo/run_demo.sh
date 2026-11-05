#!/usr/bin/env bash

# Run the GatorStomp FIFO audio demo.
set -euo pipefail


bin_dir=$(realpath "${1:-build/bin}")
#finds the directory where run_demo.sh itself is located
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

#make a temporary folder for the audio files and pipes.
demo_dir=$(mktemp -d)
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

#generate simulated audio input
python3 "$script_dir/generate_tone.py" "$demo_dir/input.raw"

#create the two named pipes
#IMPORTANT PART pipes are created before any of audio programs start
mkfifo "$demo_dir/capture.pcm"
mkfifo "$demo_dir/playback.pcm"

echo "Starting GatorStomp DAC/ADC demo..."

# DAC: read audio from playback FIFO and save it.
timeout --kill-after=2s 20s "$bin_dir/audio_io_worker" playback \
    "$demo_dir/playback.pcm" file "$demo_dir/output.raw" &
pids+=("$!")

# Pass-through: forward audio from capture FIFO to playback FIFO.
timeout --kill-after=2s 20s "$bin_dir/audio_io_passthrough" \
    "$demo_dir/capture.pcm" "$demo_dir/playback.pcm" &
pids+=("$!")

# ADC: read simulated audio and send it into capture FIFO.
timeout --kill-after=2s 20s "$bin_dir/audio_io_worker" capture \
    "$demo_dir/capture.pcm" file "$demo_dir/input.raw" &
pids+=("$!")

# Wait for all three processes to finish.
for pid in "${pids[@]}"; do
    wait "$pid"
done

# Compare the original and received audio.
if cmp -s "$demo_dir/input.raw" "$demo_dir/output.raw"; then
    echo "PASS: Input and output audio match"
else
    echo "FAIL: Audio files do not match"
    exit 1
fi