sudo apt install g++-aarch64-linux-gnu -y

mkdir -p ./cross_deps/alsa

(cd ./cross_deps/alsa && wget http://ports.ubuntu.com/ubuntu-ports/pool/main/a/alsa-lib/libasound2-dev_1.2.6.1-1ubuntu1_arm64.deb)
(cd ./cross_deps/alsa && wget http://ports.ubuntu.com/ubuntu-ports/pool/main/a/alsa-lib/libasound2_1.2.6.1-1ubuntu1_arm64.deb)

(cd ./cross_deps/alsa && dpkg-deb -x libasound2-dev_*.deb .)
(cd ./cross_deps/alsa && dpkg-deb -x libasound2_*.deb .)

cmake -S . -B cross_build -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DALSA_INCLUDE_DIR=$PWD/cross_deps/alsa/usr/include -DALSA_LIBRARY=$PWD/cross_deps/alsa/usr/lib/aarch64-linux-gnu/libasound.so
