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

- Heltec V4 TFT touch hardware support is confirmed using FT6336 (`0x38`) on SDA=5, SCL=6, INT=7, RST=41.
- The TFT display path and touch plumbing are integrated for GUI operation.
- GRID is the name of the touch GUI project for MeshcoreGRID.

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
