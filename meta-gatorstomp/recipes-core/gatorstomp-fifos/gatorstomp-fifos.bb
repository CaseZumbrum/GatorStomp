SUMMARY = "GatorStomp FIFO Initialization"
LICENSE = "CLOSED"

# Include the startup script defined in /files
SRC_URI = "file://gatorstomp-fifos"

# Register the script with SysVinit to run on startup
inherit update-rc.d

# Name of startup script
INITSCRIPT_NAME = "gatorstomp-fifos"

# Run during startup with priority 20
INITSCRIPT_PARAMS = "defaults 20"

# Install script into target Linux filesystem
do_install() {
    install -d ${D}${sysconfdir}/init.d

    install -m 0755 ${WORKDIR}/gatorstomp-fifos \
        ${D}${sysconfdir}/init.d/gatorstomp-fifos
}