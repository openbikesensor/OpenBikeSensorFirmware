# This is a small script to debug the OBSPro ultrasonic sensors (based on the PGA460 chip).
# Its focus is on optimizing the time-variable-gain (TVGAIN), threshold values and timings.
# To use it you need to rebuild the firmware with the PGA_DUMP_ENABLE define set to 1.
# You can find this value in the src/pgaSensor.h file.
# Once enabled, the pgaSensor.cpp will print out the raw PGA values for each sensor in a
# comma-separated format over the USB-serial line. The output will look like this:
# dump,0,123,45,67,89,23,45,67,...
# dump,1,234,56,78,90,34,56,78,...
# meas,0,1234,345,56
# meas,1,2345,678,67
# First value after dump/meas is always the sensor ID (0 or 1).
# The "dump" line contains the raw PGA values over time (128 values).
# The "meas" line contains: time-of-flight [µs], peak width [µs], peak amplitude
# Note that the measurement will probably never exactly match the dump values, because the
# dump and measurement lines are taken from different measurements. Also the measurement
# value is updated much more frequently than the dump values.
# Requirements and setup:
#   - python3 -m venv .venv
#   - source .venv/bin/activate
#   - pip install pyserial matplotlib numpy

import time
import matplotlib.pyplot as plt
import numpy as np
import serial

PORT = "/dev/ttyUSB0"
BAUD = 115200
SAMPLE_US = 288.0


def parse_line(line):
    parts = [p.strip() for p in line.split(",")]
    if len(parts) < 2:
        return None
    if parts[0] not in {"dump", "meas"}:
        return None
    try:
        sensor_id = int(parts[1])
    except ValueError:
        return None
    if sensor_id not in (0, 1):
        return None
    if parts[0] == "dump":
        try:
            return "dump", sensor_id, [float(v) for v in parts[2:]]
        except ValueError:
            return None
    if len(parts) < 5:
        return None
    try:
        return "meas", sensor_id, float(parts[2]), float(parts[4])
    except ValueError:
        return None


def open_serial():
    return serial.Serial(
        port=PORT,
        baudrate=BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )


def update_plot(ax, sensor_id, state):
    dump_values = state["dump"][sensor_id]
    meas = state["meas"][sensor_id]
    ax.clear()
    if dump_values:
        x = np.arange(len(dump_values), dtype=float) * SAMPLE_US
        ax.plot(x, dump_values, label="dump")
    if meas:
        ax.plot([meas[0]], [meas[1]], "ro", label="meas")

    # Plot thresholds
    # These thresholds are defined in the firmware, so copy them from there!
    th_t = np.array([1000, 1400, 2400, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000])
    th_l = np.array([240, 50, 30, 30, 30, 25, 25, 20, 20, 20, 20, 20])
    last_t = 0
    last_l = th_l[0]
    for i in range(12):
        ax.plot([last_t, last_t + th_t[i]], [last_l, th_l[i]], "k-", label="threshold" if i == 0 else "")
        last_t += th_t[i]
        last_l = th_l[i]

    # ax.set_xlim(0, 300)
    ax.set_ylim(0, 256)
    ax.set_title(f"Sensor {sensor_id}")
    ax.set_xlabel("Time (us)")
    ax.set_ylabel("Amplitude")
    ax.grid(True)
    ax.legend(loc="upper right")


def main():
    plt.ion()
    fig, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)

    state = {
        "dump": {0: None, 1: None},
        "meas": {0: None, 1: None},
    }

    ser = open_serial()
    print(f"Connected to {PORT} @ {BAUD}")
    rx_buffer = bytearray()

    while plt.fignum_exists(fig.number):
        changed = False

        bytes_waiting = ser.in_waiting
        if bytes_waiting > 0:
            rx_buffer.extend(ser.read(bytes_waiting))

        while True:
            newline_index = rx_buffer.find(b"\n")
            if newline_index < 0:
                break
            raw_line = bytes(rx_buffer[:newline_index])
            del rx_buffer[: newline_index + 1]
            line = raw_line.decode("ascii", errors="ignore").strip()
            if not line:
                continue
            parsed = parse_line(line)
            if parsed is not None:
                if parsed[0] == "dump":
                    state["dump"][parsed[1]] = parsed[2]
                    changed = True
                else:
                    state["meas"][parsed[1]] = (parsed[2], parsed[3])
                    changed = True

        if changed:
            for sensor_id, ax in enumerate(axes):
                update_plot(ax, sensor_id, state)
            fig.canvas.draw_idle()

        plt.pause(0.001)

    if ser is not None:
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
