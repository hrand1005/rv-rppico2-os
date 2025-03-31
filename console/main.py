import argparse
import serial
import sys
import selectors
import subprocess

_OPENOCD_ARGS = [
    "openocd",
    "-s", "tcl",
    "-f", "interface/cmsis-dap.cfg",
    "-f", "target/rp2350-riscv.cfg",
    "-c", "adapter speed 5000",
]


def parse_args():
    parser = argparse.ArgumentParser(description="Connect to a UART device via pyserial.")
    parser.add_argument("-d", "--device", type=str, required=True, help="The UART device (e.g. /dev/ttyACM0)")
    parser.add_argument("-b", "--baudrate", type=int, required=True, help="Baud rate for the UART connection (e.g. 115200)")
    parser.add_argument("-l", "--logfile", type=str, required=True, help="File for openocd console logs")
    parser.add_argument("-t", "--timeout", type=int, default=1, help="Timeout value for initial UART connection")
    return parser.parse_args()


def connect_openocd(logfile):
    with open(logfile, "w") as f:
        process = subprocess.Popen(_OPENOCD_ARGS, stdout=f, stderr=f)

    def close_openocd():
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()

    return close_openocd


def repl(conn, _input=sys.stdin, _output=sys.stdout):
    sel = selectors.DefaultSelector()
    sel.register(_input, selectors.EVENT_READ)
    sel.register(conn, selectors.EVENT_READ)

    print("> ", end="", flush=True, file=_output)

    while True:
        try:
            for key, _ in sel.select():
                if key.fileobj is _input:
                    data = _input.readline().strip()
                    conn.write((data + "\n").encode("utf-8"))
                elif key.fileobj is conn:
                    line = conn.readline().decode("utf-8", errors="ignore").strip()
                    print(f"\r{line}\n", end="", flush=True, file=_output)
                    print("> ", end="", flush=True, file=_output)
        except KeyboardInterrupt:
            print("\nExiting...", file=_output)
            return


def main():
    args = parse_args()
    
    cb = connect_openocd(args.logfile)

    uart = serial.Serial(args.device, args.baudrate, timeout=args.timeout)
    uart.flush()
    repl(uart)
    uart.close()

    cb()

if __name__ == "__main__":
    main()
