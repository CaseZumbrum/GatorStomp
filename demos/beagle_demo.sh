#!/usr/bin/env bash

# Run the GatorStomp FIFO audio demo.
set -euo pipefail

bin_dir="."

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

# set ALSA settings
# Enable AUX input
amixer cset name='Aux Switch' on
amixer cset name='Aux Volume' 90%

# Route AUX left/right into the input mixers
amixer cset name='Mixin Left Aux Left Switch' on
amixer cset name='Mixin Right Aux Right Switch' off
amixer cset name='Mixin Right Mixin Left Switch' on

# Enable input PGA and ADC
amixer cset name='ADC Switch' on
amixer cset name='ADC Volume' 80%
amixer cset name='Mixin PGA Switch' on
amixer cset name='Mixin PGA Volume' 50%

# Select digital audio interface as DAC source
amixer cset name='DAC Left Source MUX' 'DAI Input Left'
amixer cset name='DAC Right Source MUX' 'DAI Input Right'

# Route DAC to headphone mixer
amixer cset name='Mixout Left DAC Left Switch' on
# possibly remove this, might only be needed for mono guitar hack
amixer cset name='Mixout Right DAC Right Switch' off

# Enable headphone output
amixer cset name='DAC Volume' 80%
amixer cset name='Headphone Volume' 80%
amixer cset name='Headphone Switch' on 

# create the two named pipes
mkfifo "$demo_dir/capture.pcm"
mkfifo "$demo_dir/playback.pcm"

echo "Starting GatorStomp DAC/ADC demo..."

echo "Spinning up DAC process..."
# DAC: read audio from playback FIFO
"$bin_dir/audio_io_worker" playback \
    "$demo_dir/playback.pcm" alsa plughw:0,0 256 &
pids+=("$!")

echo "Spinning up audio orchestrator..."
"$bin_dir/audio_orchestrator" "$demo_dir/capture.pcm" "$demo_dir/playback.pcm" "$demo_dir/gs.sock" &
pids+=("$!")


echo "Spinning up ADC process..."
# ADC: read simulated audio and send it into capture FIFO.
"$bin_dir/audio_io_worker" capture \
    "$demo_dir/capture.pcm" alsa plughw:0,0 256 &
pids+=("$!")

# Wait for all three processes to finish.
for pid in "${pids[@]}"; do
    wait "$pid"
done

echo "Done!"