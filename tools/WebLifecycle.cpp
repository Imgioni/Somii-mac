#include "plugin/WebEditor.h"
#include <iostream>
#include <windows.h>

// Runs real WebView2 through open / close / reopen, closing the way JUCE's VST3 wrapper does on
// Windows (peer HWND destroyed before the editor). Each reopen is screen-captured, because a
// dead WebView shows as a white or blank window rather than as an error.
struct GeminusLifecycleCheck : juce::Timer
{
    SuperGeminiProcessor processor;
    GeminusWebSession& session = processor.getWebSession();
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outDir;
    double openedAt = 0, deadline = 0;
    int round = 0, failures = 0;
    static constexpr int kRounds = 5;

    explicit GeminusLifecycleCheck (juce::File dir) : outDir (dir) { open(); startTimer (20); }

    void check (bool ok, const juce::String& message)
    {
        std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
        if (! ok) ++failures;
    }

    void open()
    {
        editor.reset (processor.createEditor());
        editor->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
        editor->setTopLeftPosition (40, 40);
        editor->setVisible (true);
        editor->toFront (true);
        if (auto* peer = editor->getPeer())   // keep it above other apps so the capture sees it
            SetWindowPos ((HWND) peer->getNativeHandle(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        openedAt = juce::Time::getMillisecondCounterHiRes();
        deadline = openedAt + (round == 0 ? 20000 : 3000);
    }

    // Fraction of sampled pixels that are near-white, and how many distinct colours there are.
    void capture (float& whiteFraction, int& colours)
    {
        auto r = editor->getScreenBounds() * juce::Desktop::getInstance().getGlobalScaleFactor();
        const int w = r.getWidth(), h = r.getHeight();
        juce::Image img (juce::Image::RGB, w, h, true);
        auto screen = GetDC (nullptr);
        auto mem = CreateCompatibleDC (screen);
        auto bmp = CreateCompatibleBitmap (screen, w, h);
        SelectObject (mem, bmp);
        BitBlt (mem, 0, 0, w, h, screen, r.getX(), r.getY(), SRCCOPY);
        int white = 0, total = 0;
        std::set<juce::uint32> seen;
        for (int y = 0; y < h; y += 2)
            for (int x = 0; x < w; x += 2)
            {
                auto c = GetPixel (mem, x, y);
                const auto rr = GetRValue (c), gg = GetGValue (c), bb = GetBValue (c);
                img.setPixelAt (x, y, juce::Colour (rr, gg, bb));
                if (rr > 245 && gg > 245 && bb > 245) ++white;
                seen.insert ((juce::uint32) c >> 3);
                ++total;
            }
        DeleteObject (bmp); DeleteDC (mem); ReleaseDC (nullptr, screen);
        whiteFraction = total > 0 ? (float) white / (float) total : 1.0f;
        colours = (int) seen.size();
        juce::FileOutputStream os (outDir.getChildFile ("reopen" + juce::String (round) + ".png"));
        if (os.openedOk()) { os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (img, os); }
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const bool ready = session.pageLoaded && session.isShowing();
        if (! ready && now < deadline) return;
        // Give the compositor a moment to present, then look at what is on screen.
        if (ready && now - openedAt < (round == 0 ? 2500 : 600)) return;

        float white = 1; int colours = 0;
        capture (white, colours);
        check (ready, "round " + juce::String (round) + " page ready after "
                          + juce::String (now - openedAt, 0) + " ms");
        check (white < 0.5f && colours > 50, "round " + juce::String (round) + " panel drawn (white "
                          + juce::String (white * 100, 1) + "%, colours " + juce::String (colours) + ")");

        editor->removeFromDesktop();   // what JUCE's VST3 wrapper does first on Windows
        editor.reset();
        check (session.getParentComponent() == nullptr, "session detached");

        if (++round > kRounds)
        {
            stopTimer();
            juce::MessageManager::getInstance()->stopDispatchLoop();
            return;
        }
        open();
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    GeminusLifecycleCheck test (argc > 1 ? juce::File (juce::String (argv[1]))
                                         : juce::File::getSpecialLocation (juce::File::tempDirectory));
    juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout << (test.failures == 0 ? "ALL PASS" : "FAILURES: " + std::to_string (test.failures)) << std::endl;
    return test.failures == 0 ? 0 : 1;
}
