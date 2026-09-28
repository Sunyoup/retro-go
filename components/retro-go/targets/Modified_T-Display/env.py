# This file is injected late into rg_tool.py, you can run arbitrary python code here
# For example override python variables or set environment variables with os.putenv

# Espressif chip in the device
IDF_TARGET = "esp32"
# .fw file format, if supported by the device
FW_FORMAT = "none"
# Default apps to build when none is specified (comment to build all)
# Only the launcher and retro-core (nofrendo NES emulator) for now
DEFAULT_APPS = "launcher retro-core"
