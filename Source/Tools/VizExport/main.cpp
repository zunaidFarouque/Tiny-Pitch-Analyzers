#include "VizCpuRenderer.h"

#include "PitchLabEngine.h"
#include "StaticTables.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <array>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace
{
struct CliOptions
{
    juce::File wavFile;
    juce::File outDir;
    juce::File verifyDir;
    double seekSeconds = 0.0;
    int width = 1024;
    int height = 384;
    float maxMeanAbsDiff = 0.0f;
    juce::Array<pitchlab::VizMode> modes;
    pitchlab::WaterfallRenderParams waterfallParams;
};

void printUsage()
{
    std::cout << "PitchLabVizExport\n"
                 "  --wav <path>            input wav file\n"
                 "  --seek-sec <float>      seconds to skip before analysis (default 0)\n"
                 "  --out <dir>             output directory (default Tests/test-output/manual)\n"
                 "  --modes <csv|all>       waveform,waterfall,needle,stroberadial,chordmatrix\n"
                 "  --width <int>           output width (default 1024)\n"
                 "  --height <int>          output height (default 384)\n"
                 "  --wf-energy <float>     waterfall energy scale (default 0.03)\n"
                 "  --wf-alpha-pow <float>  waterfall alpha power (default 1.0)\n"
                 "  --wf-thresh <float>     waterfall alpha threshold (default 0.005)\n"
                 "  --wf-curve <linear|sqrt|logdb> waterfall curve mode (default sqrt)\n"
                 "  --verify <dir>          optional golden directory to compare\n"
                 "  --max-mean-abs-diff <f> allowed per-channel mean absolute diff (default 0)\n";
}

bool parseArgs (int argc, char** argv, CliOptions& opt)
{
    opt.outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("Tests").getChildFile ("test-output").getChildFile ("manual");
    opt.modes.addArray ({ pitchlab::VizMode::Waveform, pitchlab::VizMode::Waterfall, pitchlab::VizMode::Needle, pitchlab::VizMode::StrobeRadial, pitchlab::VizMode::ChordMatrix });

    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        auto needValue = [&] () { return (i + 1) < argc; };

        if (a == "--wav" && needValue()) opt.wavFile = juce::File (juce::String (argv[++i]));
        else if (a == "--seek-sec" && needValue()) opt.seekSeconds = juce::String (argv[++i]).getDoubleValue();
        else if (a == "--out" && needValue()) opt.outDir = juce::File (juce::String (argv[++i]));
        else if (a == "--verify" && needValue()) opt.verifyDir = juce::File (juce::String (argv[++i]));
        else if (a == "--width" && needValue()) opt.width = juce::String (argv[++i]).getIntValue();
        else if (a == "--height" && needValue()) opt.height = juce::String (argv[++i]).getIntValue();
        else if (a == "--max-mean-abs-diff" && needValue()) opt.maxMeanAbsDiff = juce::String (argv[++i]).getFloatValue();
        else if (a == "--wf-energy" && needValue()) opt.waterfallParams.energyScale = juce::String (argv[++i]).getFloatValue();
        else if (a == "--wf-alpha-pow" && needValue()) opt.waterfallParams.alphaPower = juce::String (argv[++i]).getFloatValue();
        else if (a == "--wf-thresh" && needValue()) opt.waterfallParams.alphaThreshold = juce::String (argv[++i]).getFloatValue();
        else if (a == "--wf-curve" && needValue())
        {
            const auto t = juce::String (argv[++i]).toLowerCase();
            if (t == "linear") opt.waterfallParams.curveMode = pitchlab::WaterfallDisplayCurveMode::Linear;
            else if (t == "sqrt") opt.waterfallParams.curveMode = pitchlab::WaterfallDisplayCurveMode::Sqrt;
            else if (t == "logdb") opt.waterfallParams.curveMode = pitchlab::WaterfallDisplayCurveMode::LogDb;
            else return false;
        }
        else if (a == "--modes" && needValue())
        {
            juce::Array<pitchlab::VizMode> parsed;
            if (! pitchlab::parseModeList (juce::String (argv[++i]), parsed))
                return false;
            opt.modes = parsed;
        }
        else if (a == "--help" || a == "-h")
            return false;
        else
            return false;
    }

    if (! opt.wavFile.existsAsFile())
        return false;
    if (opt.width <= 0 || opt.height <= 0)
        return false;
    return true;
}

bool writePng (const juce::Image& img, const juce::File& out)
{
    out.getParentDirectory().createDirectory();
    juce::FileOutputStream fos (out);
    if (! fos.openedOk())
        return false;
    juce::PNGImageFormat png;
    return png.writeImageToStream (img, fos);
}

float computeMeanAbsDiff (const juce::Image& a, const juce::Image& b)
{
    if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight())
        return std::numeric_limits<float>::infinity();

    juce::Image::BitmapData pa (a, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData pb (b, juce::Image::BitmapData::readOnly);

    double sum = 0.0;
    const int w = a.getWidth();
    const int h = a.getHeight();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto ca = pa.getPixelColour (x, y);
            const auto cb = pb.getPixelColour (x, y);
            sum += std::abs (static_cast<int> (ca.getRed()) - static_cast<int> (cb.getRed()));
            sum += std::abs (static_cast<int> (ca.getGreen()) - static_cast<int> (cb.getGreen()));
            sum += std::abs (static_cast<int> (ca.getBlue()) - static_cast<int> (cb.getBlue()));
        }
    }

    const double denom = static_cast<double> (w) * static_cast<double> (h) * 3.0;
    return static_cast<float> (sum / denom);
}

bool readAnalysisWindow (const juce::File& wav, double seekSeconds, int windowSize, std::vector<float>& mono, double& sampleRate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (wav));
    if (reader == nullptr)
        return false;

    sampleRate = reader->sampleRate;
    const auto total = static_cast<juce::int64> (reader->lengthInSamples);
    const auto start = static_cast<juce::int64> (std::max (0.0, seekSeconds) * sampleRate);
    if (start >= total)
        return false;

    const int n = std::min (windowSize, static_cast<int> (total - start));
    juce::AudioBuffer<float> tmp (1, n);
    if (! reader->read (&tmp, 0, n, start, true, false))
        return false;

    mono.resize (static_cast<std::size_t> (windowSize), 0.0f);
    for (int i = 0; i < n; ++i)
        mono[static_cast<std::size_t> (i)] = tmp.getSample (0, i);
    return true;
}

void buildFrame (pitchlab::PitchLabEngine& eng, const std::vector<float>& mono, pitchlab::VizFrameData& frame)
{
    const float* ch[] = { mono.data() };
    eng.processAudioInterleaved (ch, 1, static_cast<int> (mono.size()));

    eng.copyChromaRow384 (std::span<float> { frame.chromaRow.data(), frame.chromaRow.size() });
    eng.copyIngressLatest (std::span<std::int16_t> { frame.waveform.data(), frame.waveform.size() });
    std::copy (eng.state().chordProbabilities.begin(), eng.state().chordProbabilities.end(), frame.chordProbabilities.begin());
    frame.tuningError = eng.state().tuningError;
    frame.strobePhase = eng.state().strobePhase;
    frame.waterfallWriteY = 1;
}

bool writeMeta (const CliOptions& opt, double sr, int fftSize)
{
    const juce::File meta = opt.outDir.getChildFile ("meta.json");
    juce::String text;
    text << "{\n";
    text << "  \"wav\": \"" << opt.wavFile.getFullPathName().replaceCharacter ('\\', '/') << "\",\n";
    text << "  \"seekSeconds\": " << juce::String (opt.seekSeconds, 3) << ",\n";
    text << "  \"sampleRate\": " << juce::String (sr, 1) << ",\n";
    text << "  \"fftSize\": " << fftSize << ",\n";
    text << "  \"width\": " << opt.width << ",\n";
    text << "  \"height\": " << opt.height << "\n";
    text << "}\n";
    return meta.replaceWithText (text);
}
} // namespace

int main (int argc, char** argv)
{
    CliOptions opt;
    if (! parseArgs (argc, argv, opt))
    {
        printUsage();
        return 1;
    }

    opt.outDir.createDirectory();

    pitchlab::PitchLabEngine eng;
    std::vector<float> mono;
    double sampleRate = 44100.0;
    if (! readAnalysisWindow (opt.wavFile, opt.seekSeconds, eng.state().fftSize, mono, sampleRate))
    {
        std::cerr << "Failed to load analysis window from wav.\n";
        return 2;
    }

    eng.prepareToPlay (sampleRate, static_cast<int> (mono.size()));

    pitchlab::VizFrameData frame;
    buildFrame (eng, mono, frame);

    if (! writeMeta (opt, sampleRate, eng.state().fftSize))
    {
        std::cerr << "Failed to write meta.json\n";
        return 3;
    }

    pitchlab::VizCpuRenderer renderer (opt.width, opt.height);
    renderer.setWaterfallRenderParams (opt.waterfallParams);
    const auto* tables = eng.tables();
    for (auto mode : opt.modes)
    {
        const juce::Image img = renderer.render (mode, frame, tables);
        const juce::File out = opt.outDir.getChildFile (juce::String (pitchlab::modeToName (mode)) + ".png");
        if (! writePng (img, out))
        {
            std::cerr << "Failed to write image: " << out.getFullPathName() << "\n";
            return 4;
        }

        if (opt.verifyDir.exists())
        {
            const juce::File gold = opt.verifyDir.getChildFile (juce::String (pitchlab::modeToName (mode)) + ".png");
            if (! gold.existsAsFile())
            {
                std::cerr << "Missing golden file: " << gold.getFullPathName() << "\n";
                return 5;
            }

            std::unique_ptr<juce::InputStream> inA (out.createInputStream());
            std::unique_ptr<juce::InputStream> inB (gold.createInputStream());
            if (inA == nullptr || inB == nullptr)
                return 6;
            const juce::Image a = juce::ImageFileFormat::loadFrom (*inA);
            const juce::Image b = juce::ImageFileFormat::loadFrom (*inB);
            if (! a.isValid() || ! b.isValid())
                return 7;

            const float mad = computeMeanAbsDiff (a, b);
            if (mad > opt.maxMeanAbsDiff)
            {
                std::cerr << "Verification failed for " << pitchlab::modeToName (mode) << ": mad=" << mad << "\n";
                return 8;
            }
        }
    }

    std::cout << "Wrote " << opt.modes.size() << " image(s) to " << opt.outDir.getFullPathName() << "\n";
    return 0;
}

