/**
 * @file wavloader.hpp
 * @brief From-scratch WAVE/RIFF file loader — parses uncompressed PCM audio
 * (8-bit unsigned or 16-bit signed, mono or stereo) into a flat interleaved
 * sample buffer, the same way ObjLoader/TgaLoader hand-parse their own
 * formats. No third-party audio library (libsndfile, dr_wav, ...).
 */
#pragma once

#include "../vre.hpp"
#include "wavclip.hpp"

namespace vre
{
class WavLoader
{
  public:
	WavLoader();
	WavLoader(const WavLoader &other);
	WavLoader &operator=(const WavLoader &other);
	~WavLoader();

	/**
		* @brief Loads a PCM WAVE file from disk.
		* @param path Filesystem path to a `.wav` file.
		* @param out_clip Filled with the decoded samples/format on success; left
		* untouched on failure.
		* @return true on success, false if the file is missing, isn't a valid
		* RIFF/WAVE container, or uses a codec other than uncompressed PCM
		* (`fmt` tag other than 1) — this loader intentionally does not decode
		* compressed formats (ADPCM, MP3-in-WAV, etc.),
			matching the subject's "no
		* non-system library" constraint: a full audio codec is exactly the kind of
		* thing a real dependency (libsndfile, ffmpeg) exists for, and is out of
		* scope for a hand-written loader.
		*/
	static bool load(const char *path, WavClip *out_clip);

  private:
	/**
		* @brief Reads a little-endian value of type T from a raw byte
		* buffer at `offset`.
		*
		* A template, so — like Pool<T>/Registry's template methods — this
		* stays defined here in the header rather than moving to a .cpp.
		*/
	template <typename T> static T read_le(const uint8_t *data, size_t offset)
	{
		T value;
		std::memcpy(&value, data + offset, sizeof(T));
		return (value);
			// x86/ARM64 are both little-endian; WAVE's fields are too.
	}
};

} // namespace vre
