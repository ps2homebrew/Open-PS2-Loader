# Building Open PS2 Loader

## Thanks to:

- github@Caio99BR, github@AKuHAK, github@ElPatas1 and github@Dr-Q
  Instructions based on Travis CI Script
- izdubar@psx-scene and jimmikaelkael@psx-scene
  <http://psx-scene.com/forums/f150/%5Blinux%5D-opl-compile-guides-63749/>
- Bat Rastard@psx-scene and doctorxyz@psx-scene
  <http://psx-scene.com/forums/f150/2015-linux-mint-vm-creation-opl-compiling-guide-ps2sdk-127047/index3.html#post1169435>
- and a lot of people around internet :)

## Environment Setup

The current instructions are for building under Ubuntu Trusty (14.04)

## Build Process - Dependencies

### Save environment dir
```sh
export OPL_ENV_DIR=$(pwd)
rm -rf ../opl-dep/; mkdir -p ../opl-dep/; cd ../opl-dep/
export DEP_ENV_DIR=$(pwd)
```

### Install Dependencies
```sh
sudo apt-get install -y gcc-4.4 patch wget make git libc6-dev zlib1g zlib1g-dev libucl1 libucl-dev ccache libmpc-dev
```

### Set up the environment
```sh
export PS2DEV="${DEP_ENV_DIR}/ps2dev"
export PS2SDK="${PS2DEV}/ps2sdk"
export GSKIT="${PS2DEV}/gsKit"
export PATH="${PATH}:${PS2DEV}/bin:${PS2DEV}/ee/bin:${PS2DEV}/iop/bin:${PS2DEV}/dvp/bin:${PS2SDK}/bin"
export LANG=C
export LC_ALL=C
```sh

### Clone all 'ps2dev' dependencies
```sh
for TARGET in 'ps2toolchain' 'ps2sdk-ports' 'gsKit' 'ps2-packer'; do git clone "https://github.com/ps2dev/${TARGET}.git" "${DEP_ENV_DIR}/${TARGET}/"; done
chmod +x $(find . -name \*sh)
```

### Build 'ps2dev/PS2Toolchain'
```sh
cd "${DEP_ENV_DIR}/ps2toolchain/"
bash ./toolchain.sh
```

### Build 'ps2dev/ps2sdk-ports'
```sh
cd "${DEP_ENV_DIR}/ps2sdk-ports/"
make zlib libpng libjpeg freetype-2.4.12
```

### Build 'ps2dev/gsKit' and 'ps2dev/PS2Packer'
```
for TARGET in 'gsKit' 'ps2-packer'; do cd "${DEP_ENV_DIR}/${TARGET}/"; make; make install; done
```

### Go back to opl dir
```sh
cd "${OPL_ENV_DIR}/"
```

## Build Process - The Open PS2 Loader

### Make a new changelog
```sh
bash ./make_changelog.sh
```

### Let's build Open PS2 Loader Release!
```sh
make release
```

### Make the lang pack
```sh
bash ./lng_pack.sh
```

