# Include the Dropbear SSH server to enable remote terminal access with no password because who needs security
IMAGE_FEATURES:append = " ssh-server-dropbear allow-root-login allow-empty-password empty-root-password"

# Install custom USB networking package + startupscript for SysVinit + python interpreter + octave for matlab files
IMAGE_INSTALL:append = " gatorstomp-usb python3 python3-modules octave"