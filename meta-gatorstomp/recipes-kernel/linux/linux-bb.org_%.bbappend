# Tells BitBake where to look for additional files and append them. It contains out custom .cfg for configuring required configurations
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Add this specific configuration file to the kernel recipe
SRC_URI:append = " file://gatorstomp.cfg"

# Run these commands after kernel's normal configuration step
do_configure:append() {
    # Merge our custom configurations with the existing .config
    ${S}/scripts/kconfig/merge_config.sh -m -O ${B} \
        ${B}/.config ${WORKDIR}/gatorstomp.cfg

    # Resolve any configuration dependencies and apply defaults
    oe_runmake -C ${S} O=${B} olddefconfig
}