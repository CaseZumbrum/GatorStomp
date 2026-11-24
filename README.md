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

## Quickstart Guide

Follow the instructions in the sphinx documentation for [quickstart](https://casezumbrum.github.io/GatorStomp/quickstart.html).

## Completed Work

- There is a full OS image under the releases tab. This image has modules and drivers for all hardware needed for this project.
- There are modules for the DA7212 audio codec that we are using, these allow us to populate and consume audio FIFOs
- The Audio Orchestrator is defined and performs real time DSP using configurable and extendable effects
- There is a fully defined API structure for controlling the Audio Orchestrator
- There are scripts that send GPIO input to the Audio Orchestrator API
- There demo scripts for testing the entire system on PCs and the PocketBeagle2
- There are scripts for simulating user input

We are able to perform a full demo using the PocketBeagle2 and the DA7212 codec. This demo takes in an analog audio signal, performs live DSP using custom effects, and outputs an analog audio signal.

## Known Bugs

- The majority of VST plugins fail to run
- The majority of LV2 plugins fail to run
- The majority of CLAP plugins fail to run
- The audio orchestrator may take upwards of 100ms to terminate after buffers empty fully due to the monitor thread having a 100ms sleep
- The OS does not start our systems at boot, they need to be started manually
- Currently configuration of the DA7212 codec is done through a bash script, should be moved into the audio workers
