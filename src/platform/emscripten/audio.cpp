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

#include <emscripten/em_asm.h>
#ifdef SUPPORT_AUDIO
#include <algorithm>
#include <iterator>
#include <vector>
#include "audio.h"
#include "output.h"

// The longest track in Yume 2kki is ~30 MB.
static constexpr std::streamoff bgm_warning_size = 40 * 1024 * 1024;

static bool audioCallback(
	int /*numInputs*/, const AudioSampleFrame */*inputs*/,
	int /*numOutputs*/, AudioSampleFrame *outputs, int /*numParams*/,
	const AudioParamFrame */*params*/, void* userdata)
{
	auto* audio = static_cast<EmscriptenAudio*>(userdata);
	audio->PullSamples(outputs->data, outputs->numberOfChannels);
	return audio->Running();
}

static Filesystem_Stream::InputStream ReadIntoMemory(Filesystem_Stream::InputStream stream) {
	if (!stream) {
		return stream;
	}

	std::string name(stream.GetName());
	auto size = stream.GetSize();

	std::vector<uint8_t> data;
	if (size > 0) {
		data.resize(static_cast<size_t>(size));
		stream.read(reinterpret_cast<char*>(data.data()), size);
		data.resize(static_cast<size_t>(stream.gcount()));
	} else {
		std::istreambuf_iterator<char> first(stream), last;
		data.assign(first, last);
	}

	if (static_cast<std::streamoff>(data.size()) > bgm_warning_size) {
		// Large files permanently increase memory usage of the tab until reset,
		// but native will accept without issue so this warning is for map makers.
		Output::Warning("BGM {} is {} MB, consider reducing file size", name, data.size() / 1024 / 1024);
	}

	return Filesystem_Stream::InputStream(
		new Filesystem_Stream::InputMemoryStreamBuf(std::move(data)), std::move(name));
}

void EmscriptenAudio::BGM_Play(Filesystem_Stream::InputStream stream, int volume, int pitch, int fadein, int balance) {
	GenericAudio::BGM_Play(ReadIntoMemory(std::move(stream)), volume, pitch, fadein, balance);
}

void EmscriptenAudio::PullSamples(float* outputs, int channels) {
	channels = std::min<int>(channels, output_channels);

	LockMutex();
	Decode(mix.data(), quantum_size * channels * sizeof(mix[0]));
	UnlockMutex();

	for (int ch = 0; ch < channels; ++ch) {
		float* out = outputs + ch * quantum_size;
		for (size_t i = 0; i < quantum_size; ++i) {
			out[i] = mix[i * channels + ch];
		}
	}
}

EmscriptenAudio::EmscriptenAudio(const Game_ConfigAudio& cfg) : GenericAudio(cfg) {
	emscripten_lock_init(&lock);

	audioContext = emscripten_create_audio_context(nullptr);
	int output_rate = MAIN_THREAD_EM_ASM_INT({
		const context = Module.audioContext = emscriptenGetAudioObject($0);
		return context.sampleRate;
	}, audioContext);
	SetFormat(output_rate, AudioDecoder::Format::F32, output_channels);

	Output::Debug("Audio: worklet at {} Hz", output_rate);

	static uint8_t audioStack[512 * 1024];

	emscripten_start_wasm_audio_worklet_thread_async(audioContext, audioStack, sizeof(audioStack), [](EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void* self) {
	if (!success) return;

	WebAudioWorkletProcessorCreateOptions opts { "easyrpg-audio" };
	emscripten_create_wasm_audio_worklet_processor_async(audioContext, &opts, [](EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void* self) {
	if (!success) return;

	int outputChannelCounts[] { static_cast<int>(EmscriptenAudio::output_channels) };
	EmscriptenAudioWorkletNodeCreateOptions opts { 0, 1, outputChannelCounts };
	auto worklet = emscripten_create_wasm_audio_worklet_node(audioContext, "easyrpg-audio", &opts, audioCallback, self);
	MAIN_THREAD_EM_ASM({
		emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination);
		document.body.addEventListener("click", function resumeContext() {
			const context = emscriptenGetAudioObject($1);
			if (!context) {
				document.body.removeEventListener("click", resumeContext);
				return false;
			}
			if (context.state === "suspended") {
				context.resume();
			}
			return false;
		});
	}, worklet, audioContext);

	}, self);
	}, this);
}

EmscriptenAudio::~EmscriptenAudio() {
	quit.store(true, std::memory_order_relaxed);
	emscripten_destroy_audio_context(audioContext);
}

void EmscriptenAudio::LockMutex() const {
	// Neither the game thread nor the worklet is allowed to block in Atomics.wait
	// TODO: Change this to Atomics.waitAsync when caniuse reports higher availability
	emscripten_lock_busyspin_wait_acquire(&lock, 1);
}
void EmscriptenAudio::UnlockMutex() const {
	emscripten_lock_release(&lock);
}

#endif
