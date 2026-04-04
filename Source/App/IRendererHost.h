#pragma once

#include <JuceHeader.h>
#include <RenderFrameData.h>
#include "VisualizationMode.h"

namespace pitchlab
{
class StaticTables;
}

class IRendererHost
{
public:
    virtual ~IRendererHost() = default;

    virtual juce::Component& component() noexcept = 0;
    virtual void setMode (VisualizationMode mode) noexcept = 0;
    virtual VisualizationMode mode() const noexcept = 0;
    virtual void setStaticTablesPtr (const pitchlab::StaticTables* tables) noexcept = 0;
    virtual void setRenderFrame (const pitchlab::RenderFrameData& frame) noexcept = 0;
    virtual void pushWaterfallRow (std::span<const float> row384) = 0;
    /** Row-major 384×384 chroma grid (left=older time, bottom row index = newest). */
    virtual void commitWaterfallGrid384 (std::span<const float> rowMajor384x384) noexcept = 0;
};

