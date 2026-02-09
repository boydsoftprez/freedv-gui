## TCI Protocol Audio Integration (2026)

FreeDV now supports full audio TX/RX via the TCI protocol in this version/hack, enabling seamless integration with ExpertSDR3 radios and other TCI-compatible software.

This is an "alpha" so expect buggs...

### Key Features
- **TCI Frequency and Mode Control:** Frequency control over TCI both ways.
- **TCI Audio TX/RX:** Transmit and receive digital voice over the TCI network interface—no soundcard cabling required.
- **Modular Audio Configuration:** Configure only your microphone and speaker; radio audio is handled by TCI.
- **Protocol Compliance:** Uses 32-bit float, stereo, 48kHz audio streams as per TCI specification.
- **Automatic TX_CHRONO Handling:** Ensures reliable, low-latency transmission with correct PTT signaling.
- **Single TCI Device Model:** Simplifies setup and avoids device duplication issues.

### Quick Start for TCI Audio
1. In **Easy Setup** select TCI for PTT and Audio with your server port and location.
2. In **Tools → Audio Config**, set:
   - **Receive → Output:** Your speaker/headphones
   - **Transmit → Input from mic:** Your microphone
   - **Transmit → Input to radio:** `none`

...will try to add IQ-data over TCI next.

**Speciall thanks to Owen, SA0LSD for the help of testing this!**