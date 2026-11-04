# Include the Dropbear SSH server to enable remote terminal access with no password because who needs security
IMAGE_FEATURES:append = " ssh-server-dropbear allow-root-login allow-empty-password empty-root-password"

# Install custom USB networking package + startupscript for SysVinit
IMAGE_INSTALL:append = " gatorstomp-usb"