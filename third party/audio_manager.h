#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <map>
#include <vector>

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();
    
    bool Init();
    
    bool LoadAudio(const std::string& name, const std::string& filepath);
    
    // Play audio
    // For sounds: loops = 0 (play once)
    // For music: loops = -1 (loop forever)
    MIX_Track* PlayAudio(const std::string& name, int loops = 0);
    
    // Stop audio
    void StopTrack(MIX_Track* track);
    void StopAllTracks();
    
    // Volume control (0.0f to 1.0f, can go higher to amplify)
    void SetMasterVolume(float gain);
    void SetTrackVolume(MIX_Track* track, float gain);
    
    // Pause/Resume
    void PauseTrack(MIX_Track* track);
    void ResumeTrack(MIX_Track* track);
    void PauseAllTracks();
    void ResumeAllTracks();
    
    // Check if track is playing
    bool IsTrackPlaying(MIX_Track* track);
    
    // Get a track for continuous music playback
    MIX_Track* GetMusicTrack() { return musicTrack; }
    
    // Cleanup
    void Clear();

private:
    SDL_AudioDeviceID audioDevice;
    MIX_Mixer* mixer;
    
    // Store loaded audio files
    std::map<std::string, MIX_Audio*> audioFiles;
    
    // Track management
    std::vector<MIX_Track*> soundTracks;  // Pool of tracks for sound effects
    MIX_Track* musicTrack;                 // Dedicated track for music
    
    // Create additional sound effect tracks
    void CreateSoundTracks(int count);
};

#endif
