#!/bin/bash
#
# VibeOS Pi Installer
# Downloads Raspberry Pi firmware and installs VibeOS to SD card
#
# Usage: ./scripts/install-pi.sh <disk>
#   e.g.: ./scripts/install-pi.sh disk5s2
#         ./scripts/install-pi.sh /dev/disk5s2
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FIRMWARE_DIR="$PROJECT_DIR/firmware"
KERNEL_IMG="$PROJECT_DIR/build/kernel8.img"

# Firmware URLs (from official Raspberry Pi firmware repo)
FIRMWARE_BASE="https://github.com/raspberrypi/firmware/raw/master/boot"
FIRMWARE_FILES="bootcode.bin start.elf fixup.dat"

usage() {
    echo "Usage: $0 <disk>"
    echo ""
    echo "Examples:"
    echo "  $0 disk5s2"
    echo "  $0 /dev/disk5s2"
    echo ""
    echo "To find your SD card disk:"
    echo "  diskutil list"
    echo ""
    echo "Look for a ~32GB or similar sized disk that appeared when you inserted the SD card."
    exit 1
}

error() {
    echo -e "${RED}ERROR: $1${NC}" >&2
    exit 1
}

warn() {
    echo -e "${YELLOW}WARNING: $1${NC}"
}

info() {
    echo -e "${GREEN}$1${NC}"
}

# Check arguments
if [ -z "$1" ]; then
    usage
fi

DISK="$1"

# Normalize disk path
if [[ ! "$DISK" =~ ^/dev/ ]]; then
    DISK="/dev/$DISK"
fi

# Verify disk exists
if [ ! -e "$DISK" ]; then
    error "Disk $DISK does not exist"
fi

# Safety check - don't write to main disk
if [[ "$DISK" =~ disk0 ]] || [[ "$DISK" =~ disk1 ]]; then
    error "Refusing to write to $DISK - this looks like your main disk!"
fi

# Check if kernel8.img exists
if [ ! -f "$KERNEL_IMG" ]; then
    echo "kernel8.img not found. Building for Pi..."
    cd "$PROJECT_DIR"
    make clean
    make TARGET=pi
    if [ ! -f "$KERNEL_IMG" ]; then
        error "Failed to build kernel8.img"
    fi
fi

info "=== VibeOS Pi Installer ==="
echo ""
echo "Target disk: $DISK"
echo "Kernel: $KERNEL_IMG"
echo ""

# Download firmware if needed
mkdir -p "$FIRMWARE_DIR"

for file in $FIRMWARE_FILES; do
    if [ ! -f "$FIRMWARE_DIR/$file" ]; then
        info "Downloading $file..."
        curl -L -o "$FIRMWARE_DIR/$file" "$FIRMWARE_BASE/$file"
    else
        echo "$file already downloaded"
    fi
done

# Create config.txt if it doesn't exist
if [ ! -f "$FIRMWARE_DIR/config.txt" ]; then
    info "Creating config.txt..."
    cat > "$FIRMWARE_DIR/config.txt" << 'EOF'
# VibeOS Pi Configuration
arm_64bit=1
kernel=kernel8.img
disable_overscan=1
gpu_mem=64
EOF
fi

echo ""
warn "This will ERASE all data on $DISK"
echo ""
read -p "Are you sure? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# Get the base disk (without partition number) for formatting
BASE_DISK=$(echo "$DISK" | sed 's/s[0-9]*$//')

echo ""
info "Unmounting disk..."
diskutil unmountDisk "$BASE_DISK" 2>/dev/null || true

info "Creating partition scheme..."
# Create MBR with two partitions:
# - Partition 1: 256MB FAT32 boot partition (firmware + kernel)
# - Partition 2: Rest of disk FAT32 data partition (VibeOS files)

# First, completely erase and partition the disk
diskutil partitionDisk "$BASE_DISK" MBR \
    FAT32 VIBEBOOT 256M \
    FAT32 VIBEOS 0b

# Find the mount points
sleep 2
BOOT_MOUNT="/Volumes/VIBEBOOT"
DATA_MOUNT="/Volumes/VIBEOS"

if [ ! -d "$BOOT_MOUNT" ]; then
    diskutil mount "${BASE_DISK}s1" 2>/dev/null || true
    sleep 1
fi

if [ ! -d "$DATA_MOUNT" ]; then
    diskutil mount "${BASE_DISK}s2" 2>/dev/null || true
    sleep 1
fi

if [ ! -d "$BOOT_MOUNT" ]; then
    error "Could not find boot mount point. Try mounting manually: diskutil mount ${BASE_DISK}s1"
fi

if [ ! -d "$DATA_MOUNT" ]; then
    error "Could not find data mount point. Try mounting manually: diskutil mount ${BASE_DISK}s2"
fi

info "Copying firmware files to boot partition..."
for file in $FIRMWARE_FILES; do
    cp "$FIRMWARE_DIR/$file" "$BOOT_MOUNT/"
    echo "  Copied $file"
done

cp "$FIRMWARE_DIR/config.txt" "$BOOT_MOUNT/"
echo "  Copied config.txt"

info "Copying kernel8.img to boot partition..."
cp "$KERNEL_IMG" "$BOOT_MOUNT/"
echo "  Copied kernel8.img"

info "Setting up data partition..."
mkdir -p "$DATA_MOUNT/home/user"
mkdir -p "$DATA_MOUNT/bin"
mkdir -p "$DATA_MOUNT/etc"
mkdir -p "$DATA_MOUNT/tmp"
echo "Welcome to VibeOS!" > "$DATA_MOUNT/etc/motd"

# Copy user programs if they exist
USER_BUILD="$PROJECT_DIR/build/user"
if [ -d "$USER_BUILD" ]; then
    info "Copying user programs to data partition..."
    for prog in "$USER_BUILD"/*.elf; do
        if [ -f "$prog" ]; then
            name=$(basename "$prog" .elf)
            cp "$prog" "$DATA_MOUNT/bin/$name"
            echo "  Copied /bin/$name"
        fi
    done
fi

# Copy optional files
if [ -d "$PROJECT_DIR/Music" ]; then
    mkdir -p "$DATA_MOUNT/home/user/Music"
    cp -r "$PROJECT_DIR/Music/"* "$DATA_MOUNT/home/user/Music/"
    echo "  Copied Music/"
fi

if [ -d "$PROJECT_DIR/fonts" ]; then
    mkdir -p "$DATA_MOUNT/fonts"
    cp -r "$PROJECT_DIR/fonts/"* "$DATA_MOUNT/fonts/"
    echo "  Copied fonts/"
fi

# Show what's on the card
echo ""
info "Boot partition contents:"
ls -la "$BOOT_MOUNT/"
echo ""
info "Data partition contents:"
ls -la "$DATA_MOUNT/"
ls -la "$DATA_MOUNT/bin/" 2>/dev/null || true

# Clean up macOS cruft from both partitions
for mount in "$BOOT_MOUNT" "$DATA_MOUNT"; do
    dot_clean "$mount" 2>/dev/null || true
    find "$mount" -name '._*' -delete 2>/dev/null || true
    find "$mount" -name '.DS_Store' -delete 2>/dev/null || true
    rm -rf "$mount/.fseventsd" 2>/dev/null || true
    rm -rf "$mount/.Spotlight-V100" 2>/dev/null || true
    rm -rf "$mount/.Trashes" 2>/dev/null || true
done

info "Ejecting disk..."
diskutil eject "$BASE_DISK"

echo ""
info "=== Done! ==="
echo ""
echo "SD card layout:"
echo "  Partition 1 (VIBEBOOT): Firmware + kernel8.img"
echo "  Partition 2 (VIBEOS):   VibeOS filesystem (/bin, /home, etc.)"
echo ""
echo "Insert the SD card into your Pi Zero 2W and power on."
echo ""
