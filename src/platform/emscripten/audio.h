/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef EP_AUDIO_EMSCRIPTEN_H
#define EP_AUDIO_EMSCRIPTEN_H

#include "audio_generic.h"
#include "filesystem_stream.h"
#include "game_config.h"
#include <array>
#include <atomic>
#include <emscripten/em_asm.h>
#include <emscripten/wasm_worker.h>
#include <emscripten/webaudio.h>

/**
 * Requires cross-origin isolation due to wasm memory space using SAB.
 * See <https://developer.mozilla.org/en-US/docs/Web/API/Window/crossOriginIsolated>
 */
class EmscriptenAudio : public GenericAudio {
public:
	// AKA the "buffer size" as it's usually described by games requiring low latency.
	// This many frames of audio processed per callback (see PullSamples.)
	static constexpr size_t quantum_size = 128;
	static constexpr size_t output_channels = 2;

	EmscriptenAudio(const Game_ConfigAudio& cfg);
	~EmscriptenAudio();

	void BGM_Play(Filesystem_Stream::InputStream stream, int volume, int pitch, int fadein, int balance) override;
	void LockMutex() const override;
	void UnlockMutex() const override;

	void PullSamples(float* outputs, int channels);
	bool Running() const { return !quit.load(std::memory_order_relaxed); }

	static bool Supported() {
		return MAIN_THREAD_EM_ASM_INT({ return !!globalThis.crossOriginIsolated }) != 0;
	}

private:
	EMSCRIPTEN_WEBAUDIO_T audioContext = 0;
	mutable emscripten_lock_t lock;
	std::atomic<bool> quit = false;

	std::array<float, quantum_size * output_channels> mix;
};

#endif
