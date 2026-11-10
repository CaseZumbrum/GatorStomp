# Include the Dropbear SSH server to enable remote terminal access with no password because who needs security
IMAGE_FEATURES:append = " ssh-server-dropbear allow-root-login allow-empty-password empty-root-password"

# Install custom USB networking package + startupscript for SysVinit + python interpreter + octave for matlab files
IMAGE_INSTALL:append = " gatorstomp-usb python3 python3-modules octave"

# Install ALSA tools to help our audio worker communicate with ALSA
IMAGE_INSTALL:append = " alsa-lib alsa-utils"

# Install the GatorStomp ADC/DAC worker executable.
IMAGE_INSTALL:append = " gatorstomp-audio-io"

# Install our boot-time FIFO initialization script.
IMAGE_INSTALL:append = " gatorstomp-fifos"