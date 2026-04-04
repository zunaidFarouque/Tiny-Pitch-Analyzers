#pragma once

namespace pitchlab
{

/** Display strings for combo boxes (include version for user identification). */
inline constexpr const char* kUiFoldOctaveStack = "Fold: 2^j * f0 (doc) v1.0";
inline constexpr const char* kUiFoldIntegerHarmonics = "Fold: k * f0 integer v0.2";

inline constexpr const char* kUiSpectrumStft = "Spectrum: STFT v1.0";
inline constexpr const char* kUiSpectrumConstQApprox = "Spectrum: Const-Q approx v0.1";
inline constexpr const char* kUiSpectrumVarQApprox = "Spectrum: Var-Q approx v0.1";
inline constexpr const char* kUiSpectrumMultiResStft = "Spectrum: Multi-res STFT v1.0";

inline constexpr const char* kUiChromaShapeNone = "Chroma: off";
inline constexpr const char* kUiChromaShapeLog = "Chroma: log compress";
inline constexpr const char* kUiChromaShapeNoiseFloor = "Chroma: noise floor";
inline constexpr const char* kUiChromaShapePercentile = "Chroma: percentile gate";

} // namespace pitchlab
