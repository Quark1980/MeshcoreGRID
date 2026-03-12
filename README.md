# MeshcoreGRID

MeshcoreGRID is the current touch-screen GUI build built on top of MeshCore. This repository keeps the original MeshCore codebase and capabilities, but the active focus of this branch is the Heltec V4 TFT touch experience named GRID.

## Current Build Status

This build is based on MeshCore 1.14 and adds a screen-driven GUI layer rather than replacing the underlying mesh stack.

What that means in practice:

- The MeshCore routing, packet handling, LoRa transport, and companion concepts remain the foundation.
- The current work adds a display-first interface for supported TFT/touch hardware.
- Existing MeshCore concepts such as channels, direct messages, repeating, and companion connectivity are still relevant.
- GRID is currently the touch UI shell and chat experience layered onto the MeshCore firmware base.

## What Has Been Added In MeshcoreGRID

The current MeshcoreGRID work focuses on the Heltec V4 TFT BLE target and a touch-friendly operating layer.

Current implemented direction includes:

- App-drawer style launcher
- Screen-based navigation and window management
- Touch-capable GUI shell for the Heltec V4 TFT
- Messenger/chat workflow for channels and direct messages
- On-device compose flow with keyboard and send action
- Live unread badge support for chat
- Compact message bubble layout with sender and hop metadata
- Echo merge support for locally-sent messages with heard/repeat tracking
- Integration with the existing MeshCore transport and companion-radio flow
- Battery ADC reading (GPIO37 conflict with TFT MISO resolved; MISO disabled as display is write-only)
- Permanent wake toast for "RX while screen off" packet counts, shown above the bottom navigation bar
- Contacts screen shows name (left) and last-heard clock time (right) per row with guaranteed no overlap
- Long-press any contact to view full advert metadata: public key, type, flags, path length, GPS position, advert and sync timestamps
- Sort contacts by **Recent** (last heard, newest first) or **Name** (alphabetical), accessible from the right-nav action
- Channel list automatically hides unnamed placeholder entries that normalise to the bare word "channel"
- Nodes screen lists only adverts heard during the current boot session
- Radio screen includes manual **Advert Zero-Hop** and **Advert Flood** actions
- BLE screen shows the active generated PIN, BLE enabled state, and live connection state

This is not a brand new protocol or a separate mesh implementation. It is MeshCore with an added GUI layer and screen workflow.

## About MeshCore

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects using LoRa and other packet radios. It is designed for developers who want to create resilient, decentralized communication networks that work without the internet.

## What MeshCore Is

MeshCore supports a range of LoRa devices and can be used as companion firmware, repeater firmware, room server firmware, modem bridge firmware, and embedded application infrastructure.

MeshCore provides the ability to create wireless mesh networks where devices relay messages through intermediate nodes. This is useful in off-grid, emergency, field, tactical, and infrastructure-poor environments.

Compared with other LoRa networking projects, MeshCore emphasizes lightweight multi-hop routing and embedded flexibility while remaining practical to deploy on real hardware.

## Key Features

- Multi-hop packet routing across embedded LoRa nodes
- Configurable hop behavior to control network spread and efficiency
- Fixed-role behavior support, including companion nodes that do not repeat traffic
- Support for multiple LoRa-capable hardware targets including Heltec, RAK Wireless, and others in the project
- Decentralized operation with no server or internet requirement
- Low-power friendly design for battery or solar nodes
- Example applications that can be built directly from the repository

## What You Can Use It For

- Off-grid communication
- Emergency response and disaster recovery networking
- Outdoor and expedition communications
- Private field communications
- Embedded telemetry and sensor networks
- Touch-screen handheld mesh interfaces through MeshcoreGRID

## MeshcoreGRID Targets

The current GUI work is centered on the Heltec V4 TFT touch hardware.

Current relevant target:

- `heltec_v4_tft_grid_os_ble`

Related groundwork and status:

- Heltec V4 TFT touch hardware support is confirmed using FT6336 (`0x38`) on SDA=5, SCL=6, RST=41.
- Touch now runs in polling mode on this target to avoid the Heltec V4 GPIO7 conflict with LoRa PA power.
- The TFT display path and touch plumbing are integrated for GUI operation.
- GRID is the name of the touch GUI project for MeshcoreGRID.

## GRID Screens And Functions

The current GRID firmware is an app-drawer based handheld UI with a fixed status bar at the top and a fixed navigation bar at the bottom.

### Shared UI behavior

- Top status bar shows clock, live radio signal bars with SNR text, BLE icon when BLE is enabled, and battery percentage.
- Bottom navigation bar provides a left **Back** action and a right context action, which defaults to **Home**.
- Buttons use amber pressed-state feedback for clearer touch confirmation.
- Screen timeout is configurable in Settings. When the display wakes after sleep, GRID can show a toast above the navbar reporting how many packets were received while the screen was off.
- LoRa receive stays active while the display is asleep on the Heltec V4 TFT build.

### Home / App Drawer

- Main launcher screen for all GRID apps.
- Shows app cards for Messenger, Nodes, Radio, BLE, Settings, and Power.
- Displays a live unread badge on the Messenger card when unread chat exists.

### Messenger

- Main chat application for channels and direct messages.
- Channels tab lists available named channels and opens a thread view for channel chat.
- Contacts tab lists known contacts with left-aligned names and right-aligned last-heard time.
- Contact sorting toggles between **Recent** and **Name** using the right-nav **Sort** action.
- Long-press on a contact opens a details popup with advert metadata including public key, flags, GPS position, timestamps, and unread count.
- Thread view supports incoming and outgoing bubbles, sender labels, hop metadata, local echo merge, and a composer with on-screen keyboard.

### Nodes

- Shows only node adverts heard during the current boot session.
- Does not mirror the persisted contact store.
- Useful for checking live node discovery activity after boot or after sending adverts.

### Radio

- Starts on the **Advert** tab.
- **Advert** tab provides two manual advert actions:
   - **Advert Zero-Hop** sends the node advert only to neighbor nodes.
   - **Advert Flood** sends the node advert using flood routing across the mesh.
- **Metrics** tab shows live radio and runtime values in one screen, including frequency, bandwidth, spreading factor, coding rate, TX power, airtime factor, duty limit, RSSI, SNR, packet count, last raw packet timestamp, RX call count, dispatcher raw-hit count, and received flood/direct counters.

### BLE

- Shows whether companion BLE is enabled.
- Shows the active BLE PIN currently generated from node settings.
- Shows live connection state as connected or disconnected.
- Provides a single button to turn BLE on or off.

### Settings

- Editable runtime and node settings screen.
- Current fields exposed in GRID:
   - Node Name
   - Frequency (MHz)
   - Bandwidth (kHz)
   - Spreading Factor
   - Coding Rate
   - TX Power (dBm)
   - BLE Pin
   - Screen Timeout (seconds, `0` for manual-only sleep)
- Uses an on-screen editor panel and keyboard for field updates.
- Radio parameter changes are applied immediately and preferences are persisted.

### Power

- Shows battery voltage in millivolts.
- **Reboot** restarts the board.
- **Hibernate** turns off display/radio related hardware and enters deep sleep.

## How To Get Started

If you want to use the current MeshcoreGRID build:

1. Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com).
2. Clone this repository.
3. Open the repository in VS Code.
4. Build the active GUI target:
   - `pio run -e heltec_v4_tft_grid_os_ble`
5. Upload to hardware:
   - `pio run -e heltec_v4_tft_grid_os_ble -t upload`

If you want to work with the broader MeshCore project as a developer, the original example applications are still present in this repository.

## Developer Examples Still Included

The original MeshCore examples remain available and are still useful reference points:

- [Companion Radio](./examples/companion_radio) - For use with an external chat app over BLE, USB, or WiFi.
- [KISS Modem](./examples/kiss_modem) - Serial KISS protocol bridge for host applications. See [protocol docs](./docs/kiss_modem_protocol.md).
- [Simple Repeater](./examples/simple_repeater) - Extends network coverage by relaying messages.
- [Simple Room Server](./examples/simple_room_server) - A simple BBS-style shared post server.
- [Simple Secure Chat](./examples/simple_secure_chat) - Terminal-based secure text communication.
- [Simple Sensor](./examples/simple_sensor) - Remote sensor node with telemetry and alerting.

The Simple Secure Chat example can still be used through the VS Code Serial Monitor or another serial terminal.

## MeshCore Flasher And Clients

The broader MeshCore ecosystem information remains relevant for the non-GUI firmware types and for understanding the upstream project.

### MeshCore Flasher

Prebuilt firmware for supported MeshCore devices is available at:

- https://flasher.meshcore.co.uk

### MeshCore Clients

Companion firmware can be connected to via BLE, USB, or WiFi depending on the firmware type.

- Web: https://app.meshcore.nz
- Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
- iOS: https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone
- NodeJS: https://github.com/liamcottle/meshcore.js
- Python: https://github.com/fdlamotte/meshcore-cli

Repeater and room server firmware can be configured via:

- https://config.meshcore.dev

They can also be managed over LoRa using remote management features in the mobile app.

## Hardware Compatibility

MeshCore remains designed for devices listed in the MeshCore flasher and the targets included in this repository. MeshcoreGRID specifically focuses on screen-enabled hardware, currently led by the Heltec V4 TFT touch build.

## Project Direction

At this stage, this repository should be understood as:

- MeshCore base: retained
- MeshCore version basis: 1.14
- Main active addition: screen + GUI workflow
- Main active product direction: MeshcoreGRID

The intention is to preserve MeshCore's core behavior while evolving a practical handheld touch interface on supported hardware.

## Contributing

Please submit PRs using `dev` as the base branch when contributing upstream-oriented changes.

General project principles still apply:

- Keep it simple and embedded-focused.
- Avoid unnecessary abstraction layers.
- Avoid dynamic allocation outside setup/begin paths unless there is a strong reason.
- Preserve existing code style in core modules and avoid unrelated reformatting.

## Roadmap / To-Do

There are a number of major features in the pipeline, with no fixed timeframes attached.

- [X] Companion radio: UI redesign
- [X] GRID UI (Meshcore-Touch): advanced launcher, chat flow, and DMs
- [X] Repeater + Room Server: add ACLs (like Sensor Node has)
- [X] Standardize bridge mode for repeaters
- [ ] Repeater/Bridge: standardize transport codes for zoning/filtering
- [X] Core + Repeater: enhanced zero-hop neighbor discovery
- [ ] Core: round-trip manual path support
- [ ] Companion + Apps: support for multiple sub-meshes and off-grid client repeat mode
- [ ] Core + Apps: support for LZW message compression
- [ ] Core: dynamic CR (coding rate) for weak vs strong hops
- [ ] Core: framework for hosting multiple virtual nodes on one physical device
- [ ] V2 protocol spec: discussion and consensus around V2 packet protocol, including path hashes and new encryption specs

## License

MeshCore is open-source software released under the MIT License. You are free to use, modify, and distribute it for personal and commercial projects.

## Get Support

- Report bugs and request features on the [GitHub Issues](https://github.com/ripplebiz/MeshCore/issues) page.
- Find additional guides and components on [my site](https://buymeacoffee.com/ripplebiz).
- Join [MeshCore Discord](https://discord.gg/BMwCtwHj5V) to chat with developers and the community.
