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

    # Get the build directory from the ELF target
    elf_path = str(target[0])
    build_dir = os.path.dirname(elf_path)

    # Define file paths - use firmware.bin, not the .elf file
    firmware_path = os.path.join(build_dir, "firmware.bin")
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partitions_path = os.path.join(build_dir, "partitions.bin")
    merged_path = os.path.join(build_dir, "qsafe-merged.bin")

    # Wait for all required files to exist (max 15 seconds)
    print(f"Waiting for build artifacts...")
    print(f"  Looking for: {os.path.basename(firmware_path)}")

    max_wait = 15
    for i in range(max_wait):
        if os.path.exists(bootloader_path) and os.path.exists(partitions_path) and os.path.exists(firmware_path):
            print(f"  ✓ All files found after {i+1} seconds")
            break
        if i > 0 and i % 3 == 0:
            print(f"  Still waiting... ({i}/{max_wait}s)")
        time.sleep(1)
    else:
        print(f"  ✗ Timeout after {max_wait} seconds")

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
        print(f"✗ ERROR: Firmware not found at {firmware_path}")
        print(f"Files in build directory:")
        try:
            for file in os.listdir(build_dir):
                if file.endswith('.bin') or file.endswith('.elf'):
                    size = os.path.getsize(os.path.join(build_dir, file))
                    print(f"  - {file} ({size} bytes)")
        except:
            pass
        print("Skipping merged binary creation.")
        return

    # Read firmware size first to calculate required size
    firmware_size = os.path.getsize(firmware_path)

    # Calculate minimum required size (rounded up to 1MB boundary)
    # Firmware starts at 0x10000, so total = 0x10000 + firmware_size
    min_size = 0x10000 + firmware_size
    flash_size = ((min_size + 0xFFFFF) // 0x100000) * 0x100000  # Round up to 1MB

    # Cap at 4MB for ESP32
    flash_size = min(flash_size, 0x400000)

    print(f"Firmware size: {firmware_size / 1024:.1f} KB")
    print(f"Merged binary size: {flash_size / 1024 / 1024:.1f} MB")

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

    # Write merged binary (full flash image)
    print(f"Writing merged firmware to: {merged_path}")
    with open(merged_path, "wb") as f:
        f.write(merged)

    # Also create OTA-only binary (just the firmware, no bootloader/partitions)
    ota_path = os.path.join(build_dir, "qsafe-ota.bin")
    print(f"Creating OTA-only firmware: {ota_path}")

    # Delete old OTA file if it exists to avoid appending
    if os.path.exists(ota_path):
        os.remove(ota_path)
        print(f"  Removed old OTA file")

    # Copy firmware.bin to qsafe-ota.bin
    shutil.copy2(firmware_path, ota_path)

    # Verify the copy worked correctly
    firmware_size_check = os.path.getsize(firmware_path)
    ota_size_check = os.path.getsize(ota_path)

    if firmware_size_check != ota_size_check:
        print(f"WARNING: OTA file size mismatch!")
        print(f"  firmware.bin: {firmware_size_check} bytes")
        print(f"  qsafe-ota.bin: {ota_size_check} bytes")
    else:
        print(f"  ✓ OTA binary verified: {ota_size_check} bytes")

    # Get file sizes
    merged_size = os.path.getsize(merged_path)
    ota_size = os.path.getsize(ota_path)

    print("=" * 60)
    print(f"✓ Firmware binaries created successfully!")
    print(f"")
    print(f"  Full image (web flashing):  {os.path.basename(merged_path)}")
    print(f"    Size: {merged_size / 1024 / 1024:.2f} MB")
    print(f"    Use: Initial flash via USB or web flasher")
    print(f"")
    print(f"  OTA image (updates):        {os.path.basename(ota_path)}")
    print(f"    Size: {ota_size / 1024:.1f} KB")
    print(f"    Use: Upload to GitHub releases for OTA updates")
    print("=" * 60)
    print("")
    print("Upload to ESP32 using ESP Web Tools:")
    print("  1. Visit: https://web.esphome.io/")
    print("  2. Click 'Install' → 'Choose File'")
    print(f"  3. Select: {os.path.basename(merged_path)}")
    print("  4. Click 'Install' and follow prompts")
    print("=" * 60)

# Register the callback to run after building the binary file
def post_program_action(source, target, env):
    merge_bin(source, target, env)

# Run after firmware.bin is created (not after .elf)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)
