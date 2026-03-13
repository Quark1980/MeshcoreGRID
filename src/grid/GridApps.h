#pragma once

class WindowManager;
class MeshBridge;

void registerHomeApp(WindowManager& wm);
void registerChatApp(WindowManager& wm, MeshBridge& bridge);
void registerNodesApp(WindowManager& wm, MeshBridge& bridge);
void registerMapApp(WindowManager& wm, MeshBridge& bridge);
void registerRadioApp(WindowManager& wm);
void registerBleApp(WindowManager& wm);
void registerSettingsApp(WindowManager& wm);
void registerPowerApp(WindowManager& wm);
