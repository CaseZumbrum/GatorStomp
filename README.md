# GatorStomp

GatorStomp is a digital guitar pedal designed for guitarists who want to use the same effects in their DAW while playing live.


## Documentation

This project uses Sphinx for it's documentation. The documentation is hosted [here](https://casezumbrum.github.io/GatorStomp/).

The documentation requires Doxygen to build the C/C++ documentation. It can be installed on linux with:

```bash
sudo apt install doxygen
```

or via [this link](https://www.doxygen.nl/download.html) for windows.

The docs can can be built with the following commands.

```bash
python -m venv .venv
source ./.venv/scripts/activate
pip install -r requirements.dev.txt
cd docs
make html
```

## Completed Work

- There is a working OS image in the releases tab. This image has our needed modules and can be flashed to the PocketBeagle2 Dev Board we are using during development (instructions in the docs)
- There are working drivers for the input buttons and encoders, as well as a testing system for simulating sending/receiving events that involve them.
- There is a defined packet structure for controlling the main audio processor
- There are drivers for the ADC/DAC that populate/empty FIFOs with live audio data
- There is a simple audio orchestrator that permits c/c++ transforms on data from/to FIFOs

## Known Bugs
- The audio orchestrator API does not do anything currently (cannot be controlled)
- The audio orchestrator API does not function with multiple connected clients
- The audio orchestrator API does not function with 0 connected clients
- The OS is lacking a python and octave interpreter
- The ADC/DAC drivers currently use anonymous FIFOs, we are planning to switch to named ones
- Misaligned packets to the audio orchestrator can cause out of sync errors that destroy audio integrity