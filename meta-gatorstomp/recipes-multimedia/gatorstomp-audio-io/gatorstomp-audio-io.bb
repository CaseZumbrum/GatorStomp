SUMMARY = "GatorStomp ADC/DAC audio workers"
LICENSE = "CLOSED"

# Tell BitBake wher etheh C source files are located (use default locations in GitHub repo)
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../src/audio_io:"

# Only include files required by audio_io_worker.
SRC_URI = " \
    file://audio_io_worker.c \
    file://audio_io_fifo.c \
    file://audio_io_fifo.h \
"

# Use ALSA as required
DEPENDS = "alsa-lib"

# Directory containing fetched source files
S = "${WORKDIR}"

# Compile programs using Yocto's cross-compiler
do_compile() {
    ${CC} ${CFLAGS} ${CPPFLAGS} -I${S} \
        ${S}/audio_io_worker.c ${S}/audio_io_fifo.c \
        -o audio_io_worker ${LDFLAGS} -lasound
}

# Install the executable into the OS.
do_install() {
    install -d ${D}${bindir}
    install -m 0755 audio_io_worker \
        ${D}${bindir}/audio_io_worker
}