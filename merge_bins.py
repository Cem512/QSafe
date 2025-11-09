Import("env")
import os
import shutil
import time

def merge_bin(source, target, env):
    """
    Merge bootloader, partitions, and firmware into a single binary for ESP Web Tools
    """
    print("=" * 60)
    print("Creating merged firmware for ESP Web Tools...")
    print("=" * 60)

    # Get the firmware path
    firmware_path = str(target[0])
    build_dir = os.path.dirname(firmware_path)

    # Define file paths
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partitions_path = os.path.join(build_dir, "partitions.bin")
    merged_path = os.path.join(build_dir, "qsafe-merged.bin")

    # Wait for all required files to exist (max 10 seconds)
    max_wait = 10
    for i in range(max_wait):
        if os.path.exists(bootloader_path) and os.path.exists(partitions_path) and os.path.exists(firmware_path):
            break
        time.sleep(1)

    # Check if all required files exist
    if not os.path.exists(bootloader_path):
        print(f"Warning: Bootloader not found at {bootloader_path}")
        print("Skipping merged binary creation. Use 'pio run' to build normally.")
        return

    if not os.path.exists(partitions_path):
        print(f"Warning: Partitions not found at {partitions_path}")
        print("Skipping merged binary creation. Use 'pio run' to build normally.")
        return

    if not os.path.exists(firmware_path):
        print(f"Warning: Firmware not found at {firmware_path}")
        print("Skipping merged binary creation. Use 'pio run' to build normally.")
        return

    # Create merged binary (4MB flash size)
    flash_size = 0x400000  # 4MB
    merged = bytearray([0xFF] * flash_size)

    # Bootloader at 0x1000
    print(f"Adding bootloader.bin at offset 0x1000...")
    with open(bootloader_path, "rb") as f:
        bootloader = f.read()
        merged[0x1000:0x1000 + len(bootloader)] = bootloader

    # Partition table at 0x8000
    print(f"Adding partitions.bin at offset 0x8000...")
    with open(partitions_path, "rb") as f:
        partitions = f.read()
        merged[0x8000:0x8000 + len(partitions)] = partitions

    # Firmware at 0x10000
    print(f"Adding firmware.bin at offset 0x10000...")
    with open(firmware_path, "rb") as f:
        firmware = f.read()
        merged[0x10000:0x10000 + len(firmware)] = firmware

    # Write merged binary
    print(f"Writing merged firmware to: {merged_path}")
    with open(merged_path, "wb") as f:
        f.write(merged)

    # Get file size
    file_size = os.path.getsize(merged_path)
    file_size_kb = file_size / 1024
    file_size_mb = file_size_kb / 1024

    print("=" * 60)
    print(f"✓ Merged firmware created successfully!")
    print(f"  File: {merged_path}")
    print(f"  Size: {file_size_mb:.2f} MB ({file_size_kb:.0f} KB)")
    print("=" * 60)
    print("")
    print("Upload to ESP32 using ESP Web Tools:")
    print("  1. Visit: https://web.esphome.io/")
    print("  2. Click 'Install' → 'Choose File'")
    print(f"  3. Select: {os.path.basename(merged_path)}")
    print("  4. Click 'Install' and follow prompts")
    print("=" * 60)

# Register the callback to run after building
# Use a different target to ensure bootloader and partitions are built first
def post_program_action(source, target, env):
    merge_bin(source, target, env)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", post_program_action)
