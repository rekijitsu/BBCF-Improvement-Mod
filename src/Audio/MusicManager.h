#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>

struct MusicTrack {
    int id;
    std::string name;
    std::string category;
};

enum class MusicRotationMode {
    Random,       // legacy (removed from the UI; folded into Shuffle) — kept so old
                  // RotationMode=0 config values map cleanly
    Sequential,
    Shuffle
};

class MusicManager {
public:
    static MusicManager& GetInstance();

    void Initialize();
    void Update();

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    const MusicTrack* GetCurrentTrack() const { return m_currentTrack; }
    int GetCurrentTrackId() const { return m_currentTrackId; }
    int GetGameMusicId() const { return m_gameMusicId; }

    const std::vector<MusicTrack>& GetAllTracks() const { return m_tracks; }
    std::vector<MusicTrack> GetEnabledTracks() const;

    void ToggleTrackEnabled(int trackId);
    bool IsTrackEnabled(int trackId) const;

    // Enable/disable every track in a category (e.g. "btl", "vs", "old") at once.
    void SetCategoryEnabled(const std::string& category, bool enabled);
    // 1 = all enabled, 0 = all disabled, -1 = mixed (or empty category).
    int GetCategoryEnabledState(const std::string& category) const;

    void PlayTrack(int trackId);
    void PlayNextTrack();

    // True when actually in a match (Training / Challenge / local VS / online),
    // i.e. MatchState_Fight — the only time the jukebox drives music.
    bool IsInMatch() const;

    // True when the Jukebox should show playback info (current track / timer):
    // in the match scene and NOT in the middle of leaving it via the confirm
    // dialog. False at Character Select / menus and during the exit transition,
    // so the UI shows "None" / 00:00 instead of a track that stopped playing.
    bool ShouldShowPlayback() const;

    // True when the mod has taken over BGM (played a custom track via XACT) and
    // thus left non-native state in Bank[13] that scene transitions must clean up.
    bool IsControllingBgm() const { return m_modControllingBgm || m_customBgmLoaded; }

    void SetRotationMode(MusicRotationMode mode) { m_rotationMode = mode; }
    MusicRotationMode GetRotationMode() const { return m_rotationMode; }

    void SetRepeatSingle(bool val) { m_repeatSingle = val; }
    bool IsRepeatSingle() const { return m_repeatSingle; }

    void SavePreferences();
    void LoadPreferences();
    void ResetPreferences();

    static const char* GetBgmFilename(int trackId);
    static int GetTrackDuration(int trackId);

    int GetSongPlaybackFrames() const { return m_songPlaybackFrames; }
    std::string GetSongTimeString() const;

    // Effective advance threshold: the current track's true duration (read from
    // its XACT wave bank) when known, otherwise the precomputed per-track table.
    int GetRotationThresholdFrames() const;
    int GetCurrentTrackDurationFrames() const { return m_currentTrackDurationFrames; }

    // Soft-reset the custom BGM (stop+clear the bank, null the slot, reset the
    // track-id / music-select cursors). Safe no-op if no custom BGM is loaded.
    // Called on scene exit (e.g. Training -> Character Select) so the game loads
    // its normal scene BGM instead of erroring on a non-selectable leftover track.
    void UnloadCustomBgm();

    // Force-clear the mod's BGM footprint (Bank[13] -> EMPTY, null scratch slot,
    // present the selectable anchor) so the game's own Character Select XACT-init
    // rebuilds Bank[13] natively. Unlike RestoreAnchorForSceneExit (which reloads
    // via our direct-COM pipeline and re-creates foreign bank state), this leaves
    // the bank empty. Unconditional (runs even after the dialog-open restore).
    void ClearBgmForSceneExit();

    // Restore the initially-selected "anchor" track (the one chosen at Character
    // Select) through the normal pipeline, so leaving Training for Character
    // Select presents a valid selectable track everywhere (BGM slot, Bank[13],
    // audioMgr, musicSelect) instead of the non-selectable track we were playing
    // (which otherwise errors Character Select -> red debug screen). This makes
    // Character Select show the original song as if the playlist never cycled.
    void RestoreAnchorForSceneExit();

    // Backup BGM cleanup for the match-end -> victory-screen transition, for
    // flows that don't hit the primary cleanup (UpdateMusicState clearing on
    // MatchState -> VictoryScreen). Same proven cleanup as the Character
    // Select exit (ClearBgmForSceneExit), with the scratch-slot buffer
    // orphaned first. No-op unless the mod took over BGM.
    void RestoreNativeBgmForMatchEnd();

    void OnMatchInit();
    void ResetRotationTimer() { m_framesSinceLastChange = 0; m_songPlaybackFrames = 0; }

    static int* s_musicSelectX;
    static int* s_musicSelectY;

    // Asynchronous cue playback fields
    bool m_pendingPlay = false;
    void* m_pendingSoundObj = nullptr;
    std::string m_pendingCueName;
    int m_pendingPlayRetries = 0;

private:
    MusicManager();
    ~MusicManager() = default;

    void BuildTrackList();
    void ChangeMusicIfNeeded();
    void UpdateMusicState();
    void ShufflePlaylist();
    int SelectNextTrack();
    void DetectSceneExitAndUnload();
    bool PlayTrackPhysically(uintptr_t modBase, int trackId, const char* bgmName, int* outDurationFrames, int presentedId);

    std::vector<MusicTrack> m_tracks;
    std::map<int, bool> m_trackEnabled;
    const MusicTrack* m_currentTrack = nullptr;
    int m_currentTrackId = -1;
    int m_gameMusicId = -1;
    int m_framesSinceLastChange = 0;
    int m_songPlaybackFrames = 0;
    int m_sequentialIndex = 0;

    bool m_enabled = false;
    bool m_initialized = false;

    MusicRotationMode m_rotationMode = MusicRotationMode::Sequential;
    bool m_repeatSingle = false;

    std::vector<int> m_shuffledPlaylist;
    int m_shuffleIndex = 0;

    // Last-resort advance threshold (frames) used only if a track's length is
    // somehow unknown; every known track has a real duration (wave bank / table).
    static const int MIN_FRAMES_BETWEEN_CHANGES = 7200;
    // True length of the currently-playing track in frames (60fps), read from its
    // XACT wave bank. 0 = unknown -> use the precomputed per-track table.
    int m_currentTrackDurationFrames = 0;
    int m_lastGameState = -1;      // last GameState (scene); for scene-exit detection
    int m_lastMatchState = -1;     // last MatchState; for match-end (-> VictoryScreen) detection
    bool m_customBgmLoaded = false; // true once we've taken over BGM (needs soft-reset on exit)
    bool m_modControllingBgm = false; // true once the mod is the authority on the current track
    int m_anchorTrackId = 0;        // supported track id presented to the game (never a vs/old/sys id)

    // Native audioManager BGM slot 0 (+0x118) state captured before we first
    // deactivate it, so scene exit can restore it. The game's native Character
    // Select BGM init needs this slot active; leaving it deactivated is what
    // produces the red debug Character Select.
    int m_origSlot0Active = 1;
    int m_origSlot0State = 0;
    bool m_audioSlot0Captured = false;
    std::chrono::steady_clock::time_point m_songStartTime;

    // "Return to Character Select?" confirm-dialog handling: restore the anchor
    // track while the dialog is up (so the exit sees a selectable track), suspend
    // rotation, and re-play the interrupted track if the user cancels.
    bool m_confirmDialogActive = false;
    int m_preDialogTrackId = -1;
    int m_dialogClosedTimer = 0;
    bool m_dialogSeenInRender = false; // set each render frame if the confirm dialog is visible
    bool CheckConfirmDialogUp();
public:
    // Called from the render path (the dialog's message id is only present in the
    // render-phase UI buffer). Updates m_dialogSeenInRender for Update() to act on.
    void PollDialogRenderPhase();
};

MusicManager& GetMusicManager();