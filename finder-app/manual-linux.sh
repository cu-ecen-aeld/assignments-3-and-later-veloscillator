#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
MAKEFLAGS=-j$(nproc)
SYSROOT=`${CROSS_COMPILE}gcc -print-sysroot`

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    # Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper  # Deep clean, remove config.
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig # Setup default config.
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all       # Build vmlinux.
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules   # Build modules.
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs      # Build devicetree.
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
ROOT="${OUTDIR}/rootfs"
mkdir -p "$ROOT"
mkdir "${ROOT}/bin" "${ROOT}/dev" "${ROOT}/etc" "${ROOT}/home" "${ROOT}/lib" "${ROOT}/lib64" "${ROOT}/proc" \
      "${ROOT}/sbin" "${ROOT}/sys" "${ROOT}/tmp" "${ROOT}/usr" "${ROOT}/usr/bin" "${ROOT}/usr/sbin" \
      "${ROOT}/var" "${ROOT}/var/log"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${ROOT} install

echo "Library dependencies"
regex='/[a-zA-Z0-9\./-]*'
interpreter=`${CROSS_COMPILE}readelf -a "${ROOT}/bin/busybox" | grep "program interpreter" | grep -o '/[a-zA-Z0-9\./-]*'`
libs=`${CROSS_COMPILE}readelf -a "${ROOT}/bin/busybox" | grep "Shared library" | grep -o '\[.*\]'`

# Add program interpreter and library dependencies to rootfs.
cp "${SYSROOT}${interpreter}" "${ROOT}/lib/"
for lib in $libs; do
    cp "${SYSROOT}/lib64/${lib:1:-1}" "${ROOT}/lib64/"
done

# Make device nodes
sudo mknod "${ROOT}/dev/null" -m 666 c 1 3
sudo mknod "${ROOT}/dev/console" -m 666 c 5 1

# Clean and build the writer utility
cd "${FINDER_APP_DIR}"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer "${ROOT}/home/"
cp writer.sh "${ROOT}/home/"
cp finder.sh "${ROOT}/home/"
cp -r ../conf "${ROOT}/home/"
cp finder-test.sh "${ROOT}/home/"
cp autorun-qemu.sh "${ROOT}/home/"

# Chown the root directory
sudo chown -R root "${ROOT}"

# Create initramfs.cpio.gz
cd "${ROOT}"
rm -f "${OUTDIR}/initramfs.cpio.gz"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip "${OUTDIR}/initramfs.cpio"
cd "${FINDER_APP_DIR}"

# Copy kernel Image.
cp "${OUTDIR}/linux-stable/arch/arm64/boot/Image" "${OUTDIR}/"
