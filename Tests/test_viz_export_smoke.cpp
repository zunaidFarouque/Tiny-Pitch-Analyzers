#include <gtest/gtest.h>

#include <juce_core/juce_core.h>

namespace
{
juce::File findRepoRoot()
{
    juce::File dir = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 8; ++i)
    {
        if (dir.getChildFile ("CMakeLists.txt").existsAsFile())
            return dir;
        dir = dir.getParentDirectory();
    }
    return {};
}

juce::File findAnyWav()
{
    const juce::File root = findRepoRoot();
    if (! root.isDirectory())
        return {};

    juce::File a = root.getChildFile ("Source")
                       .getChildFile ("App")
                       .getChildFile ("ExampleAudio")
                       .getChildFile ("Monophonic Vox - Aj sraboner.wav");
    if (a.existsAsFile())
        return a;

    juce::File b = root.getChildFile ("test-assets")
                       .getChildFile ("Monophonic Vox - Aj sraboner.wav");
    if (b.existsAsFile())
        return b;

    return {};
}
} // namespace

TEST(VizExport, SmokeGeneratesPngOutputs)
{
    const juce::File wav = findAnyWav();
    ASSERT_TRUE(wav.existsAsFile()) << "No wav file found for smoke test";

    const juce::File outDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("pitchlab-vizexport-smoke");
    if (outDir.exists())
        outDir.deleteRecursively();
    outDir.createDirectory();

    const juce::String exporter (PITCHLAB_VIZ_EXPORT_EXE);
    juce::StringArray args;
    args.add (exporter);
    args.add ("--wav");
    args.add (wav.getFullPathName());
    args.add ("--seek-sec");
    args.add ("2.0");
    args.add ("--out");
    args.add (outDir.getFullPathName());
    args.add ("--modes");
    args.add ("waveform,waterfall,needle,stroberadial,chordmatrix");

    juce::ChildProcess proc;
    const bool started = proc.start (args);
    ASSERT_TRUE(started);
    const bool finished = proc.waitForProcessToFinish (120000);
    ASSERT_TRUE(finished);
    const int rc = proc.getExitCode();
    EXPECT_EQ(rc, 0);

    const char* names[] = { "waveform.png", "waterfall.png", "needle.png", "stroberadial.png", "chordmatrix.png", "meta.json" };
    for (const char* n : names)
    {
        const juce::File f = outDir.getChildFile (n);
        EXPECT_TRUE(f.existsAsFile()) << n;
        EXPECT_GT(f.getSize(), 16) << n;
    }
}

