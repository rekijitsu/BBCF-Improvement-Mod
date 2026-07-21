#include "JukeboxWindow.h"
#include "Core/logger.h"
#include "Overlay/imgui_utils.h"
#include "Overlay/Logger/ImGuiLogger.h"

#include <algorithm>
#include <iomanip>
#include <chrono>
#include <cstdio>

JukeboxWindow::JukeboxWindow(const std::string& windowTitle, bool windowClosable, ImGuiWindowFlags windowFlags)
	: IWindow(windowTitle, windowClosable, windowFlags)
{
}

void JukeboxWindow::BeforeDraw() {
	// Sensible default size on first appearance (the window is no longer
	// auto-resized; the track list fills the remaining space and scrolls).
	ImGui::SetNextWindowSize(ImVec2(440, 640), ImGuiCond_FirstUseEver);
}

void JukeboxWindow::Draw() {
	MusicManager& musicManager = GetMusicManager();

	DrawControls();
	ImGui::Separator();
	DrawCurrentTrackInfo();
	ImGui::Separator();
	DrawTrackList();
}

void JukeboxWindow::DrawControls() {
	MusicManager& musicManager = GetMusicManager();

	ImGui::TextColored(ImVec4(1, 1, 0, 1), "Jukebox");
	ImGui::SameLine();
	ImGui::TextDisabled("(Music Rotation)");

	ImGui::Spacing();

	// Enable checkbox
	bool enabled = musicManager.IsEnabled();
	if (ImGui::Checkbox("Enable Music Rotation", &enabled)) {
		musicManager.SetEnabled(enabled);
		musicManager.SavePreferences();
	}
	ImGui::SameLine();
	ImGui::ShowHelpMarker("Enable/disable automatic music rotation in-game");

	ImGui::Spacing();

	// Rotation mode (Random and Shuffle are merged: both play in shuffled order)
	ImGui::Text("Rotation Mode:");
	ImGui::SameLine();
	ImGui::ShowHelpMarker("Sequential: play tracks in order\nShuffle: play tracks in a shuffled order, no repeats until all have played");

	int modeInt = (musicManager.GetRotationMode() == MusicRotationMode::Sequential) ? 0 : 1;
	if (ImGui::Combo("##RotationMode", &modeInt, "Sequential\0Shuffle\0")) {
		musicManager.SetRotationMode(modeInt == 0 ? MusicRotationMode::Sequential : MusicRotationMode::Shuffle);
		musicManager.SavePreferences();
	}

	// Repeat settings
	ImGui::HorizontalSpacing();
	bool repeatAll = musicManager.IsRepeatAll();
	if (ImGui::Checkbox("Repeat All", &repeatAll)) {
		musicManager.SetRepeatAll(repeatAll);
		musicManager.SavePreferences();
	}
	ImGui::SameLine();
	ImGui::ShowHelpMarker("When the playlist reaches the end, start over from the beginning");

	ImGui::HorizontalSpacing();
	bool repeatSingle = musicManager.IsRepeatSingle();
	if (ImGui::Checkbox("Repeat Single", &repeatSingle)) {
		musicManager.SetRepeatSingle(repeatSingle);
		musicManager.SavePreferences();
	}
	ImGui::SameLine();
	ImGui::ShowHelpMarker("Repeat the current track instead of playing a new one");

	ImGui::Spacing();

	// Play button — advance per the rotation mode (Sequential / Shuffle).
	if (ImGui::Button("Next >|")) {
		musicManager.PlayNextTrack();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play the next track (per the rotation mode)");

	ImGui::Spacing();

	// Search bar
	ImGui::InputText("Search Tracks", m_searchBuffer, sizeof(m_searchBuffer),
		ImGuiInputTextFlags_AutoSelectAll);

	ImGui::Spacing();

	// Enable/Disable all
	if (ImGui::Button("Enable All")) {
		for (const auto& track : musicManager.GetAllTracks()) {
			if (!musicManager.IsTrackEnabled(track.id)) {
				musicManager.ToggleTrackEnabled(track.id);
			}
		}
		musicManager.SavePreferences();
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All")) {
		for (const auto& track : musicManager.GetAllTracks()) {
			if (musicManager.IsTrackEnabled(track.id)) {
				musicManager.ToggleTrackEnabled(track.id);
			}
		}
		musicManager.SavePreferences();
	}
	ImGui::SameLine();
	ImGui::ShowHelpMarker("Enable/Disable all tracks at once");

	ImGui::Spacing();

    // Reset button
    if (ImGui::Button("Reset Preferences")) {
        musicManager.ResetPreferences();
    }
    ImGui::SameLine();
    ImGui::ShowHelpMarker("Reset all settings and re-enable all tracks");
}

void JukeboxWindow::DrawCurrentTrackInfo() {
	MusicManager& musicManager = GetMusicManager();

	const MusicTrack* currentTrack = musicManager.GetCurrentTrack();
	int currentTrackId = musicManager.GetCurrentTrackId();
	bool inMatch = musicManager.ShouldShowPlayback();

	ImGui::Text("Current Track:");
	ImGui::SameLine();
	if (!inMatch) {
		// The jukebox only drives music during a match.
		ImGui::TextDisabled("None (enter a match)");
	} else if (currentTrack) {
		ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s (ID: %d)", currentTrack->name.c_str(), currentTrack->id);
	} else if (currentTrackId >= 0) {
		const char* filename = MusicManager::GetBgmFilename(currentTrackId);
		if (filename) {
			ImGui::TextDisabled("%s.pac (ID: %d)", filename, currentTrackId);
		} else {
			ImGui::TextDisabled("ID: %d (Unknown track)", currentTrackId);
		}
	} else {
		ImGui::TextDisabled("None");
	}

	// Song timer — frozen at 00:00 / 00:00 when not in a match.
	ImGui::Text("Time:");
	ImGui::SameLine();
	if (!inMatch) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "00:00");
		ImGui::SameLine();
		ImGui::TextDisabled("/ 00:00");
	} else {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", musicManager.GetSongTimeString().c_str());

		int totalFrames = musicManager.GetRotationThresholdFrames();
		if (totalFrames > 0) {
			int totalSec = totalFrames / 60;
			int totalMins = totalSec / 60;
			int totalSecs = totalSec % 60;
			ImGui::SameLine();
			ImGui::TextDisabled("/ %02d:%02d", totalMins, totalSecs);

			// Progress bar
			float progress = (float)musicManager.GetSongPlaybackFrames() / (float)totalFrames;
			if (progress > 1.0f) progress = 1.0f;
			ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
		}
	}

	ImGui::Text("Enabled Tracks: %d / %d",
		(int)musicManager.GetEnabledTracks().size(),
		(int)musicManager.GetAllTracks().size());
}

// Category colors
static const ImVec4 COLOR_BTL  = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);   // Red - Battle themes
static const ImVec4 COLOR_VS   = ImVec4(0.4f, 0.6f, 1.0f, 1.0f);   // Blue - Versus themes
static const ImVec4 COLOR_BOSS = ImVec4(0.8f, 0.4f, 1.0f, 1.0f);   // Purple - Boss themes
static const ImVec4 COLOR_SYS  = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);   // Yellow - System themes
static const ImVec4 COLOR_OLD  = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);   // Gray - Legacy/old versions
static const ImVec4 COLOR_STORY = ImVec4(0.4f, 1.0f, 0.6f, 1.0f);  // Light Green - Story themes
static const ImVec4 COLOR_ASTRAL = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // Orange - Astral Finish themes

static ImVec4 GetCategoryColor(const std::string& category) {
	if (category == "btl") return COLOR_BTL;
	if (category == "vs") return COLOR_VS;
	if (category == "boss") return COLOR_BOSS;
	if (category == "sys" || category == "sysex") return COLOR_SYS;
	if (category == "old") return COLOR_OLD;
	if (category == "story") return COLOR_STORY;
	if (category == "astral") return COLOR_ASTRAL;
	return ImVec4(1, 1, 1, 1);
}

void JukeboxWindow::DrawTrackList() {
	MusicManager& musicManager = GetMusicManager();

	const auto& allTracks = musicManager.GetAllTracks();

	// Fill the remaining vertical space so only this track container scrolls
	// (the window itself has NoScrollbar — no second scrollbar).
	float childHeight = ImGui::GetContentRegionAvail().y;
	if (childHeight < 100.0f) childHeight = 100.0f;

	ImGui::BeginChild("##TrackList", ImVec2(0, childHeight), true);

	// Build filtered track list
	std::vector<const MusicTrack*> filteredTracks;
	std::string searchStr(m_searchBuffer);
	std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

	for (const auto& track : allTracks) {
		if (searchStr.empty()) {
			filteredTracks.push_back(&track);
		} else {
			std::string trackName = track.name;
			std::transform(trackName.begin(), trackName.end(), trackName.begin(), ::tolower);
			if (trackName.find(searchStr) != std::string::npos ||
				(std::to_string(track.id)).find(searchStr) != std::string::npos) {
				filteredTracks.push_back(&track);
			}
		}
	}

	// Render tracks grouped by category
	std::string lastCategory;
	for (const MusicTrack* track : filteredTracks) {
	// Draw category header with a "check all" toggle for the whole category
		if (track->category != lastCategory) {
			lastCategory = track->category;
			ImVec4 catColor = GetCategoryColor(lastCategory);
			ImGui::Separator();
			int catState = musicManager.GetCategoryEnabledState(lastCategory);
			bool catAllOn = (catState == 1);
			if (ImGui::Checkbox(("##CatAll_" + lastCategory).c_str(), &catAllOn)) {
				musicManager.SetCategoryEnabled(lastCategory, catAllOn);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(catState == -1
					? "Mixed: click to enable ALL tracks in this category"
					: "Enable/disable ALL tracks in this category");
			}
			ImGui::SameLine();
			ImGui::TextColored(catColor, "[ %s ]", lastCategory.c_str());
		}

		bool enabled = musicManager.IsTrackEnabled(track->id);
		const MusicTrack* currentTrack = musicManager.GetCurrentTrack();
		bool isCurrent = (currentTrack && currentTrack->id == track->id);

		if (isCurrent) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
		}

		if (ImGui::Checkbox(("##Track" + std::to_string(track->id)).c_str(), &enabled)) {
			bool currentState = musicManager.IsTrackEnabled(track->id);
			if (currentState != enabled) {
				musicManager.ToggleTrackEnabled(track->id);
			}
		}

		ImGui::SameLine();
		if (ImGui::Selectable((std::to_string(track->id) + ": " + track->name + "##Sel" + std::to_string(track->id)).c_str(), isCurrent)) {
			musicManager.PlayTrack(track->id);
		}

		if (isCurrent) {
			ImGui::PopStyleColor();
		}
	}

	ImGui::EndChild();
}