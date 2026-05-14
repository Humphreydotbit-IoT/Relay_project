Import("env")

def merge_bin_action(source, target, env):
    firmware_path = str(target[0])
    board = env.BoardConfig()
    mcu = board.get("build.mcu", "esp32")
    flash_size = board.get("upload.flash_size", "4MB")
    flash_mode = env.subst("$BOARD_FLASH_MODE")
    flash_freq = env.subst("${__get_board_f_flash(__env__)}")
    app_offset = "0x10000"

    output_path = firmware_path.replace(".bin", "_merged.bin")

    # Get esptool.py path from PlatformIO's tool package
    import os
    platform = env.PioPlatform()
    tool_dir = platform.get_package_dir("tool-esptoolpy") or ""
    esptool_path = os.path.join(tool_dir, "esptool.py")

    # Collect all flash images (bootloader, partitions, boot_app0, etc.)
    flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", []))

    # Build esptool merge_bin command
    cmd = [
        '"$PYTHONEXE"', esptool_path,
        "--chip", mcu,
        "merge_bin",
        "--flash_size", flash_size,
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "-o", output_path,
    ]

    # Add bootloader, partitions, etc. (address + file pairs)
    i = 0
    while i < len(flash_images):
        cmd.append(flash_images[i])      # address
        cmd.append(flash_images[i + 1])  # file path
        i += 2

    # Add the main firmware
    cmd.append(app_offset)
    cmd.append(firmware_path)

    env.Execute(env.VerboseAction(" ".join(cmd), "Merging firmware into single binary..."))
    print("*** Merged firmware: %s ***" % output_path)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
