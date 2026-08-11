#include "audio_manager.h"
#include <iostream>

AudioManager::AudioManager()
    : audioDevice(0), mixer(nullptr), musicTrack(nullptr)
{
}

AudioManager::~AudioManager()
{
    // Only call Clear if we haven't already
    if (mixer || !audioFiles.empty())
    {
        Clear();
    }
}

bool AudioManager::Init()
{
    // Initialize SDL Audio subsystem
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        std::cout << "Failed to initialize SDL Audio: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize SDL_mixer
    if (!MIX_Init())
    {
        std::cout << "Failed to initialize SDL_mixer: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Open the default audio device
    audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!audioDevice)
    {
        std::cout << "Failed to open audio device: " << SDL_GetError() << std::endl;
        MIX_Quit();
        return false;
    }
    
    // Create mixer with the device
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 44100;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    
    mixer = MIX_CreateMixerDevice(audioDevice, &spec);
    if (!mixer)
    {
        std::cout << "Failed to create mixer device: " << SDL_GetError() << std::endl;
        SDL_CloseAudioDevice(audioDevice);
        MIX_Quit();
        return false;
    }
    
    // Set master gain (1.0f = 100% volume)
    MIX_SetMasterGain(mixer, 1.0f);
    
    // Create dedicated music track
    musicTrack = MIX_CreateTrack(mixer);
    if (!musicTrack)
    {
        std::cout << "Failed to create music track: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create a pool of tracks for sound effects (16 should be plenty)
    CreateSoundTracks(16);
    
    return true;
}

void AudioManager::CreateSoundTracks(int count)
{
    for (int i = 0; i < count; i++)
    {
        MIX_Track* track = MIX_CreateTrack(mixer);
        if (track)
        {
            soundTracks.push_back(track);
        }
    }
}

bool AudioManager::LoadAudio(const std::string& name, const std::string& filepath)
{
    // Check if already loaded
    if (audioFiles.find(name) != audioFiles.end())
    {
        return true;
    }
    
    // Load the audio file
    // The true parameter means SDL_mixer will predecode the audio (important for looping!)
    MIX_Audio* audio = MIX_LoadAudio(mixer, filepath.c_str(), true);
    
    if (!audio)
    {
        std::cout << "Failed to load audio " << filepath << ": " << SDL_GetError() << std::endl;
        return false;
    }
    
    audioFiles[name] = audio;
    return true;
}

MIX_Track* AudioManager::PlayAudio(const std::string& name, int loops)
{
    auto it = audioFiles.find(name);
    if (it == audioFiles.end())
    {
        std::cout << "Audio not found: " << name << std::endl;
        return nullptr;
    }
    
    MIX_Audio* audio = it->second;
    MIX_Track* track = nullptr;
    
    // If looping (music), use the dedicated music track
    if (loops == -1)
    {
        track = musicTrack;
        
        // Stop any currently playing music (0 = no fade out)
        if (MIX_TrackPlaying(track))
        {
            MIX_StopTrack(track, 0);
            // Give it a moment to stop
            SDL_Delay(10);
        }
    }
    else
    {
        // Find an available sound track (one that's not playing)
        for (MIX_Track* soundTrack : soundTracks)
        {
            if (!MIX_TrackPlaying(soundTrack))
            {
                track = soundTrack;
                break;
            }
        }
        
        // If all tracks are busy, use the first one (oldest sound gets cut off)
        if (!track && !soundTracks.empty())
        {
            track = soundTracks[0];
            MIX_StopTrack(track, 0);
            SDL_Delay(5);
        }
    }
    
    if (!track)
    {
        std::cout << "No available track to play audio" << std::endl;
        return nullptr;
    }
    
    // Set the audio to the track
    if (!MIX_SetTrackAudio(track, audio))
    {
        std::cout << "Failed to set track audio: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    // Create properties for playback
    SDL_PropertiesID props = SDL_CreateProperties();
    
    // Set loop count (-1 = loop forever, 0 = play once)
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    
    // Play the track
    bool success = MIX_PlayTrack(track, props);
    
    SDL_DestroyProperties(props);
    
    if (!success)
    {
        std::cout << "Failed to play track: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    return track;
}

void AudioManager::StopTrack(MIX_Track* track)
{
    if (track)
    {
        MIX_StopTrack(track, 0);  // 0 = no fade out
    }
}

void AudioManager::StopAllTracks()
{
    MIX_StopAllTracks(mixer, 0);  // 0 = no fade out
}

void AudioManager::SetMasterVolume(float gain)
{
    if (mixer)
    {
        MIX_SetMasterGain(mixer, gain);
    }
}

void AudioManager::SetTrackVolume(MIX_Track* track, float gain)
{
    if (track)
    {
        MIX_SetTrackGain(track, gain);
    }
}

void AudioManager::PauseTrack(MIX_Track* track)
{
    if (track)
    {
        MIX_PauseTrack(track);
    }
}

void AudioManager::ResumeTrack(MIX_Track* track)
{
    if (track)
    {
        MIX_ResumeTrack(track);
    }
}

void AudioManager::PauseAllTracks()
{
    if (mixer)
    {
        MIX_PauseAllTracks(mixer);
    }
}

void AudioManager::ResumeAllTracks()
{
    if (mixer)
    {
        MIX_ResumeAllTracks(mixer);
    }
}

bool AudioManager::IsTrackPlaying(MIX_Track* track)
{
    return track && MIX_TrackPlaying(track);
}

void AudioManager::Clear()
{
    // Prevent double-cleanup
    if (!mixer && audioFiles.empty())
    {
        return;  // Already cleaned up
    }
    
    // Stop all audio first (but check if SDL is still initialized)
    if (mixer && SDL_WasInit(SDL_INIT_AUDIO))
    {
        MIX_StopAllTracks(mixer, 0);
        SDL_Delay(10);
    }
    
    // Clear track references (they'll be destroyed with the mixer)
    soundTracks.clear();
    musicTrack = nullptr;
    
    // Free all loaded audio BEFORE destroying mixer
    for (auto& pair : audioFiles)
    {
        if (pair.second)
        {
            MIX_DestroyAudio(pair.second);
        }
    }
    audioFiles.clear();
    
    // Destroy mixer (this destroys all tracks too)
    if (mixer)
    {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }
    
    // Close audio device (only if SDL audio is still initialized)
    if (audioDevice && SDL_WasInit(SDL_INIT_AUDIO))
    {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    MIX_Quit();
}
