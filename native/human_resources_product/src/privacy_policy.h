#pragma once
#include <array>
#include <string_view>

namespace human_resources::product {
// Offline product facts. A future online service needs a new policy and store
// disclosure review before it becomes available; this is not a consent prompt.
inline constexpr std::array<std::string_view, 6> privacy_policy_sections{
    "PRIVACY POLICY\nHuman Resources / 25 October 2026. This version is an offline, single-player game. It does not transmit your gameplay, settings or identifiers to the developer or third parties. We do not collect or share that data.",
    "LOCAL SAVES\nProgress, Research Credits, Hangar ranks, run recovery and settings are stored on your device. The game reads its own files and uses device timing for gameplay, animation and controls. These operations do not upload data.",
    "OFFLINE PLAY\nThis version has no online accounts, cloud sync, advertising, analytics or in-app purchases. Research Credits are earned through play. Sound, reduced motion and optional vibration can be changed in Settings.",
    "DEVICE BACKUPS\nYour operating system may back up or restore app files, especially on iOS, according to your device settings. Android app backup and device transfer are excluded by this version. We do not operate a backup server.",
    "KEEP OR DELETE\nLocal saves remain until removed by you or the operating system. Uninstalling normally removes local app data; backups may retain or restore copies. Manage those copies through your device or backup provider. Local deletion does not guarantee erasure of every backup.",
    "CONTACT\nUse the developer contact shown on this game's store listing. The developer cannot retrieve your local save through this game. If a future version introduces online services, its policy and store disclosures must explain the changed data practices."};
} // namespace human_resources::product
