# Tells BitBake where to look for additional files and append them. It contains out custom .cfg for configuring required configurations
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Add this specific configuration file and audio device tree to the kernel recipe
SRC_URI:append = " file://gatorstomp.cfg \
                   file://gatorstomp-audio.dtsi"

# Run these commands after kernel's normal configuration step
do_configure:append() {
    # Merge our custom configurations with the existing .config
    ${S}/scripts/kconfig/merge_config.sh -m -O ${B} \
        ${B}/.config ${WORKDIR}/gatorstomp.cfg

    # Resolve any configuration dependencies and apply defaults
    oe_runmake -C ${S} O=${B} olddefconfig
}

# Integrate GatorStomp audio device tree
do_configure:prepend() {
    DTS_DIR="${S}/arch/arm64/boot/dts/ti"

    # Copy our custom device tree into the kernel source
    install -m 0644 ${WORKDIR}/gatorstomp-audio.dtsi \
        "$DTS_DIR/gatorstomp-audio.dtsi"

    # Include our custom device tree in the existing board device tree
    if ! grep -Fxq '#include "gatorstomp-audio.dtsi"' \
        "$DTS_DIR/k3-am6232-pocketbeagle2.dts"; then

        printf '\n#include "gatorstomp-audio.dtsi"\n' \
            >> "$DTS_DIR/k3-am6232-pocketbeagle2.dts"
    fi
}