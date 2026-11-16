#pragma once

// The one built-in ImGui settings page.
namespace SettingsPage {
// Installs the overlay hooks and registers the page. Returns false if the
// renderer hooks could not be installed.
bool Install();
} // namespace SettingsPage
