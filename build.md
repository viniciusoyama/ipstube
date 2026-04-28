
Step-by-step build and flash

1. Verify PlatformIO works

pio --version

If pio isn't on PATH, you may need to add it: export PATH=$PATH:~/.platformio/penv/bin. Add to ~/.zshrc to make it
permanent.

2. Compile-check the firmware (no hardware needed)

cd /Users/viny/Projects/EleksTubeIPS
pio run -e ipstube

First run will download ~500 MB of toolchain and libraries. Subsequent builds are fast (10–30 s). If this fails, paste the
error and I'll fix.

3. Bundle the web UI

cd web
npm install            # first time only
npx gulp               # produces ../data/*.html.gz
cd ..
ls data/text.html.gz   # confirm it exists

4. Build the LittleFS filesystem image

pio run -e ipstube --target buildfs

This packages everything in data/ (including the web UI you just bundled and the existing data/ips/ clock-face folders) into
a single LittleFS partition image.

5. Plug the clock in via USB and find its serial port

ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART 2>/dev/null

Note the path. Common: /dev/cu.usbserial-110, /dev/cu.wchusbserial1410. If only one shows up, PlatformIO auto-detects it.

6. Flash the firmware

pio run -e ipstube --target upload

If auto-detect fails, pass it explicitly: pio run -e ipstube --target upload --upload-port /dev/cu.usbserial-XXXX.

If you see "could not enter bootloader" or similar, hold BOOT (or reset+BOOT) during the connection attempt. Some CH340 USB
chips have flaky auto-reset.

7. Flash the filesystem image

Disconnect, enter boot mode again and run:

pio run -e ipstube --target uploadfs

Same port handling as step 6.

8. Start the clock

If needed, run:

pio device monitor -e ipstube

You should see startup messages, WiFi connection attempts, and eventually the IP address. Press Ctrl-C then Ctrl-] to exit.

9. Open the web UI

In your browser go to http://<clock-ip>/. You should see the menu, including the new Text entry. Toggle it on and your text
appears on the displays.
