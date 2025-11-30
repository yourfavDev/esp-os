# esp-os

This is the `esp-os` project, a personal operating system built for ESP32.

## Project Structure

- `main/`: Contains the core application logic and main source file (`esp-os.c`).
- `components/`: Custom components can be placed here.
- `docs/`: Documentation for the project.
- `tests/`: Unit and integration tests.
- `CMakeLists.txt`: Top-level CMake file for the project.
- `sdkconfig`: ESP-IDF project configuration.

## Getting Started

To build and flash this project, ensure you have the ESP-IDF installed and configured.

1.  **Clone the repository (if applicable) or navigate to the project directory.**
    *(Remember to rename the project directory from `hello` to `esp-os` if you haven't already).*
2.  **Set up ESP-IDF environment variables.**
3.  **Build the project:**
    ```bash
    idf.py build
    ```
4.  **Flash to your ESP32 device:**
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```
    (Replace `/dev/ttyUSB0` with your ESP32 device's serial port.)
5.  **Monitor serial output:**
    ```bash
    idf.py -p /dev/ttyUSB0 monitor
    ```

## Contributing

(Add contributing guidelines here if applicable)

## License

(Add license information here)
