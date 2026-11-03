# GatorStomp

GatorStomp is a digital guitar pedal designed for guitarists who want to use the same effects in their DAW while playing live.

## Quickstart

TODO

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

## Contributing Guide

TODO