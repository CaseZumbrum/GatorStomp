SUMMARY = "GatorStomp USB networking startup"

# private software
LICENSE = "CLOSED"

# Tell BitBake to include our startup script when building
SRC_URI = "file://gatorstomp-usb"

# Use SysVinit to run this on startup
inherit update-rc.d

# Name of the startup script that we want to register
INITSCRIPT_NAME = "gatorstomp-usb"

# Register it in the normal startup/shutdown runlevels (90 is the priority)
INITSCRIPT_PARAMS = "defaults 90"

# Runs when BitBake installs the package into the image's temporary staging directory
do_install() {
    # Create /etc/init.d directory in the image
    install -d ${D}${sysconfdir}/init.d
    
    # Copy our shell script for configuring USB and connecting it to hardware in that directory (0755 makes it executable)
    install -m 0755 ${WORKDIR}/gatorstomp-usb \
    ${D}${sysconfdir}/init.d/gatorstomp-usb
}