/*
 * VibeOS Virtio Sound Driver
 *
 * Implements virtio-snd for audio playback on QEMU virt machine.
 * Based on virtio 1.2 spec (modern mode).
 */

#ifndef VIRTIO_SOUND_H
#define VIRTIO_SOUND_H

#include <stdint.h>

// Initialize the virtio sound device
// Returns 0 on success, -1 on failure
int virtio_sound_init(void);

// Play raw PCM audio
// data: pointer to PCM samples (S16LE, stereo, 44100Hz by default)
// samples: number of samples (not bytes)
// Returns 0 on success, -1 on failure
int virtio_sound_play(const int16_t *data, uint32_t samples);

// Play a WAV file from memory
// data: pointer to WAV file data
// size: size of WAV file in bytes
// Returns 0 on success, -1 on failure
int virtio_sound_play_wav(const void *data, uint32_t size);

// Stop playback
void virtio_sound_stop(void);

// Check if sound is currently playing
int virtio_sound_is_playing(void);

// Set volume (0-100)
void virtio_sound_set_volume(int volume);

// Get current playback position in samples
uint32_t virtio_sound_get_position(void);

#endif // VIRTIO_SOUND_H
