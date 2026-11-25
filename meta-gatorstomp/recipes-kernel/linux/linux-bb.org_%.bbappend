# Tells BitBake where to look for additional files and append them. It contains out custom .cfg for configuring required configurations
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Add this specific configuration file and audio device tree to the kernel recipe
SRC_URI:append = " file://gatorstomp.cfg \
                   file://gatorstomp-audio.dtsi \
                   file://gatorstomp-controls.dtsi"

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

    # Copy audio device tree
    install -m 0644 ${WORKDIR}/gatorstomp-audio.dtsi \
        "$DTS_DIR/gatorstomp-audio.dtsi"

    # Copy controls device tree
    install -m 0644 ${WORKDIR}/gatorstomp-controls.dtsi \
        "$DTS_DIR/gatorstomp-controls.dtsi"

    # Adds the device tree for both 6232 and 6254 processors (ours is 6254)
    for dts in k3-am6232-pocketbeagle2.dts k3-am62-pocketbeagle2.dts; do
        for dtsi in gatorstomp-audio.dtsi gatorstomp-controls.dtsi; do
            if ! grep -Fxq "#include \"$dtsi\"" "$DTS_DIR/$dts"; then
                printf '\n#include "%s"\n' "$dtsi" >> "$DTS_DIR/$dts"
            fi
        done
    done
}