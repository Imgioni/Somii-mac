#include "WebEditor.h"

// juce_add_binary_data names every generated header "BinaryData.h" whatever the NAMESPACE, so
// GeminusArt's and GeminusWebUI's would collide on the include path. CMake writes a shim that
// points unambiguously at the UI one.
#include <GeminusUiData.h>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
// The page asks for bare filenames ("panel.png"), which is how juce_add_binary_data
// stores them; strip any leading path and query so both forms resolve.
juce::String leafOf (const juce::String& url)
{
    auto p = url.upToFirstOccurrenceOf ("?", false, false);
    p = p.fromLastOccurrenceOf ("/", false, false);
    return p.isEmpty() ? juce::String ("index.html") : p;
}

// WebView2 caches what the resource provider serves, and it keys that cache by the user data
// folder. A fixed folder means a stale page can survive a rebuild and mask every UI change,
// which looks exactly like a broken UI. Keying the folder to the content of the page makes a
// changed ui/ land in a fresh cache instead.
juce::String uiCacheKey()
{
    uint64_t h = 1469598103934665603ULL;              // FNV-1a over the served page
    for (int i = 0; i < GeminusUiData::namedResourceListSize; ++i)
    {
        const juce::String resource (GeminusUiData::originalFilenames[i]);
        if (resource != "index.html" && ! resource.endsWith (".js") && ! resource.endsWith (".css")
            && ! resource.endsWith (".svg"))
            continue;

        int size = 0;
        if (const auto* d = GeminusUiData::getNamedResource (GeminusUiData::namedResourceList[i], size))
            for (int j = 0; j < size; ++j)
            {
                h ^= static_cast<unsigned char> (d[j]);
                h *= 1099511628211ULL;
            }
    }
    return juce::String::toHexString (static_cast<juce::int64> (h));
}

int argInt (const juce::Array<juce::var>& a, int i, int def = 0)
{
    return a.size() > i ? static_cast<int> (a[i]) : def;
}
float argFloat (const juce::Array<juce::var>& a, int i, float def = 0.0f)
{
    return a.size() > i ? static_cast<float> (static_cast<double> (a[i])) : def;
}

juce::PropertiesFile::Options globalSettingsOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Geminus";
    options.filenameSuffix = ".settings";
    options.folderName = "SPKR";
    options.osxLibrarySubFolder = "Application Support";
    options.millisecondsBeforeSaving = 0;
    return options;
}
} // namespace

juce::String GeminusWebSession::mimeFor (const juce::String& path)
{
    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html") return "text/html";
    if (ext == "css")  return "text/css";
    if (ext == "js" || ext == "mjs") return "text/javascript";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "woff2") return "font/woff2";
    if (ext == "json") return "application/json";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource> GeminusWebSession::provide (const juce::String& url)
{
    const auto wanted = leafOf (url);

    for (int i = 0; i < GeminusUiData::namedResourceListSize; ++i)
    {
        if (juce::String (GeminusUiData::originalFilenames[i]) != wanted)
            continue;

        int size = 0;
        if (const auto* data = GeminusUiData::getNamedResource (GeminusUiData::namedResourceList[i], size))
        {
            const auto* bytes = reinterpret_cast<const std::byte*> (data);
            return juce::WebBrowserComponent::Resource {
                std::vector<std::byte> (bytes, bytes + size), mimeFor (wanted) };
        }
    }
    return std::nullopt;
}

// Every parameter gets a relay, taken from the processor itself rather than a table kept in
// step with the markup. A relay whose control does not exist in the page yet simply goes
// unused, so adding a control is a change to ui/ alone - no C++ edit, nothing to forget.
void GeminusWebSession::buildRelays()
{
    auto& apvts = proc.apvts;

    for (auto* p : proc.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;

        const auto id = ranged->paramID;

        if (dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr)
        {
            comboRelays.push_back (std::make_unique<juce::WebComboBoxRelay> (id));
            comboAttach.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (
                *ranged, *comboRelays.back(), apvts.undoManager));
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
        {
            toggleRelays.push_back (std::make_unique<juce::WebToggleButtonRelay> (id));
            toggleAttach.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (
                *ranged, *toggleRelays.back(), apvts.undoManager));
        }
        else
        {
            sliderRelays.push_back (std::make_unique<juce::WebSliderRelay> (id));
            sliderAttach.push_back (std::make_unique<juce::WebSliderParameterAttachment> (
                *ranged, *sliderRelays.back(), apvts.undoManager));
        }
    }
}

// ── patch files ──────────────────────────────────────────────────────────────────────────────
juce::File GeminusWebSession::defaultPatchFolder()
{
    // Documents/002/Patches (renamed from Geminus, 2026-09-19). The first time it is needed, patches
    // saved under the old name are copied across; the old folder is left as it was.
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto f = docs.getChildFile ("002").getChildFile ("Patches");
    const auto old = docs.getChildFile ("Geminus").getChildFile ("Patches");
    if (! f.isDirectory() && old.isDirectory())
    {
        f.createDirectory();
        for (const auto& p : old.findChildFiles (juce::File::findFiles, false, "*.gpatch"))
            p.copyFileTo (f.getChildFile (p.getFileName()));
    }
    f.createDirectory();
    return f;
}

// A patch's TYPE travels inside the file (state property "patchType"), so a patch keeps its type
// wherever it is copied. Reading it means parsing the file, which is a few KB.
juce::String GeminusWebSession::readPatchType (const juce::File& f)
{
    juce::MemoryBlock data;
    if (! f.loadFileAsData (data)) return {};
    const auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), static_cast<int> (data.getSize()));
    return xml != nullptr ? xml->getStringAttribute ("patchType") : juce::String();
}

// The favourites are a list of paths in the global settings, shared by every instance.
juce::StringArray GeminusWebSession::favourites() const
{
    return juce::StringArray::fromLines (globalSettings->getValue ("favourites", {}));
}

void GeminusWebSession::addPatches (juce::Array<juce::var>& out, const juce::File& folder, const juce::String& bank)
{
    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, false, "*.gpatch");
    files.sort();
    const auto favs = favourites();
    for (const auto& f : files)
    {
        juce::DynamicObject::Ptr o (new juce::DynamicObject());
        o->setProperty ("name", f.getFileNameWithoutExtension());
        o->setProperty ("path", f.getFullPathName());
        o->setProperty ("bank", bank);
        o->setProperty ("type", readPatchType (f));
        o->setProperty ("fav", favs.contains (f.getFullPathName()));
        out.add (juce::var (o.get()));
    }
}

// Every sub-folder of the patch folder is a bank; patches sitting loose count as no bank.
juce::var GeminusWebSession::listPatches (const juce::File& folder)
{
    juce::Array<juce::var> out;
    if (! folder.isDirectory()) return out;
    addPatches (out, folder, {});
    juce::Array<juce::File> banks;
    folder.findChildFiles (banks, juce::File::findDirectories, false);
    banks.sort();
    for (const auto& b : banks) addPatches (out, b, b.getFileName());
    return out;
}

bool GeminusWebSession::loadPatchFile (const juce::File& f)
{
    juce::MemoryBlock data;
    if (! f.existsAsFile() || ! f.loadFileAsData (data)) return false;
    const auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), static_cast<int> (data.getSize()));
    if (xml == nullptr || ! xml->hasTagName (proc.apvts.state.getType())) return false;
    proc.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    setPatchName (f.getFileNameWithoutExtension());
    return true;
}

// Saving writes into the bank's folder, making it on the way if it is new (that folder IS the bank).
juce::File GeminusWebSession::savePatchFile (const juce::File& folder, juce::String name, const juce::String& bank, const juce::String& type)
{
    name = name.trim();
    if (! folder.isDirectory() || name.isEmpty()) return {};
    auto dir = folder;
    if (bank.trim().isNotEmpty())
    {
        dir = folder.getChildFile (juce::File::createLegalFileName (bank.trim()));
        dir.createDirectory();
    }
    setPatchName (name);
    proc.apvts.state.setProperty ("patchType", type.trim(), nullptr);
    auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".gpatch");
    juce::MemoryBlock data;
    proc.getStateInformation (data);
    return file.replaceWithData (data.getData(), data.getSize()) ? file : juce::File {};
}

void GeminusWebSession::setPatchName (const juce::String& name)
{
    proc.apvts.state.setProperty ("patchName", name, nullptr);
}

juce::String GeminusWebSession::patchName() const
{
    return proc.apvts.state.getProperty ("patchName", "INIT").toString();
}

// The host's typing keyboard (FL Studio and others) only sees keys while a window of the host's
// own thread has focus. WebView2 takes focus on every click, and its window lives in another
// process, so after a gesture the page asks for focus to be handed back.
void GeminusWebSession::returnFocusToHost()
{
   #if JUCE_WINDOWS
    if (auto* peer = getPeer())
        if (auto hwnd = static_cast<HWND> (peer->getNativeHandle()))
            SetFocus (hwnd);
   #endif
}

juce::WebBrowserComponent::Options GeminusWebSession::makeOptions()
{
    auto opts = juce::WebBrowserComponent::Options {}
                    .withNativeIntegrationEnabled()
                    .withKeepPageLoadedWhenBrowserIsHidden();

    for (auto& r : sliderRelays) opts = opts.withOptionsFrom (*r);
    for (auto& r : toggleRelays) opts = opts.withOptionsFrom (*r);
    for (auto& r : comboRelays)  opts = opts.withOptionsFrom (*r);

    using Args = juce::Array<juce::var>;
    using Done = juce::WebBrowserComponent::NativeFunctionCompletion;

    // Commands that are not parameters go through the same lock-free FIFO the audio thread
    // already drains.
    auto cmd = [this] (UiCommand c) { proc.getUiBridge().push (c); };

    opts = opts.withNativeFunction ("pageSize", [this] (const Args& a, Done done)
    {
        if (a.size() >= 2) adoptPageSize (static_cast<int> (a[0]), static_cast<int> (a[1]));
        done (juce::var());
    });

    opts = opts.withNativeFunction ("desktopLayout", [this] (const Args& a, Done done)
    {
        if (! a.isEmpty()) setDesktopLayout (static_cast<bool> (a[0]));
        done (juce::var (desktopLayout));
    });

    // UI theme: "gemini" (default, original hardware colours) or "super6"; global like desktopLayout.
    // Anything else stored (e.g. the retired "spkr") falls back to the default.
    opts = opts.withNativeFunction ("uiTheme", [this] (const Args& a, Done done)
    {
        const auto requested = a.isEmpty() ? juce::String() : a[0].toString();
        const auto valid = [] (const juce::String& t) { return t == "gemini" || t == "super6"; };
        if (valid (requested))
        {
            globalSettings->setValue ("uiTheme", requested);
            globalSettings->saveIfNeeded();
        }
        const auto stored = globalSettings->getValue ("uiTheme", "gemini");
        done (juce::var (valid (stored) ? stored : juce::String ("gemini")));
    });

    opts = opts.withNativeFunction ("hostFocus", [this] (const Args&, Done done)
    {
        returnFocusToHost();
        done (juce::var());
    });

    // ── keys, ribbon, bender ──
    opts = opts.withNativeFunction ("noteOn", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::NoteOn; c.a = argInt (a, 0, 60); c.f = argFloat (a, 1, 0.8f);
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("noteOff", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::NoteOff; c.a = argInt (a, 0, 60);
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("allNotesOff", [cmd] (const Args&, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::AllNotesOff;
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("ribbonTouch", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::RibbonTouch; c.f = juce::jlimit (0.0f, 1.0f, argFloat (a, 0, 0.5f));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("ribbonMove", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::RibbonMove; c.f = juce::jlimit (0.0f, 1.0f, argFloat (a, 0, 0.5f));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("ribbonRelease", [cmd] (const Args&, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::RibbonRelease; cmd (c);
        UiCommand p; p.type = UiCommand::Type::Pressure; p.f = 0.0f; cmd (p);
        done (juce::var());
    });
    // Vertical position on the ribbon = pressure = channel aftertouch (the hardware ribbon has
    // no second axis; this is the plugin's expression axis, see ui_prompt_v2 §4).
    opts = opts.withNativeFunction ("ribbonPressure", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::Pressure; c.f = juce::jlimit (0.0f, 1.0f, argFloat (a, 0));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("bend", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::Bend; c.f = juce::jlimit (-1.0f, 1.0f, argFloat (a, 0));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("push", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::Push; c.f = juce::jlimit (0.0f, 1.0f, argFloat (a, 0));
        cmd (c); done (juce::var());
    });

    // ── sequencer ──
    // seqSetStep(layer, index, notes[], velocity, tie, accent, rest)
    opts = opts.withNativeFunction ("seqSetStep", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqSetStep;
        c.layer = juce::jlimit (0, 1, argInt (a, 0)); c.a = juce::jlimit (0, 63, argInt (a, 1));
        c.step.count = 0;
        if (a.size() > 2 && a[2].isArray())
            for (const auto& n : *a[2].getArray())
                if (c.step.count < 8) c.step.notes[c.step.count++] = static_cast<int8_t> (juce::jlimit (0, 127, static_cast<int> (n)));
        c.step.velocity = juce::jlimit (0.0f, 1.0f, argFloat (a, 3, 0.8f));
        c.step.tie = argInt (a, 4) != 0; c.step.accent = argInt (a, 5) != 0; c.step.rest = argInt (a, 6) != 0;
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("seqSetLength", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqSetLength; c.layer = juce::jlimit (0, 1, argInt (a, 0)); c.a = juce::jlimit (1, 64, argInt (a, 1, 16));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("seqRecord", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqRecord; c.layer = juce::jlimit (0, 1, argInt (a, 0)); c.a = argInt (a, 1); c.b = argInt (a, 2, -1);
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("seqLoad", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqLoad; c.layer = juce::jlimit (0, 1, argInt (a, 0)); c.a = juce::jlimit (0, 15, argInt (a, 1));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("seqStore", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqStore; c.layer = juce::jlimit (0, 1, argInt (a, 0)); c.a = juce::jlimit (0, 15, argInt (a, 1));
        cmd (c); done (juce::var());
    });
    opts = opts.withNativeFunction ("seqClear", [cmd] (const Args& a, Done done)
    {
        UiCommand c; c.type = UiCommand::Type::SeqClear; c.layer = juce::jlimit (0, 1, argInt (a, 0));
        cmd (c); done (juce::var());
    });

    // ── FX rack: impulse responses dropped on a CONVOLVER display (standard base64 of the file) ──
    opts = opts.withNativeFunction ("fxLoadIr", [this] (const Args& a, Done done)
    {
        juce::MemoryOutputStream bytes;
        const bool ok = a.size() > 0 && juce::Base64::convertFromBase64 (bytes, a[0].toString())
                        && proc.getFxRack().loadImpulse (bytes.getData(), bytes.getDataSize(),
                                                         a.size() > 1 ? a[1].toString().upToLastOccurrenceOf (".", false, false) : juce::String());
        done (juce::var (ok));
    });
    // drag FX 1/2/3 onto each other: the two slots trade places, settings and all
    opts = opts.withNativeFunction ("fxSwap", [this] (const Args& a, Done done)
    {
        proc.getFxRack().swapSlots (argInt (a, 0, -1), argInt (a, 1, -1));
        done (juce::var (true));
    });
    // ── DDS 1 CUSTOM: customLoad(layer, standard base64 of the file, file name) · customClear(layer) ──
    opts = opts.withNativeFunction ("customLoad", [this] (const Args& a, Done done)
    {
        juce::MemoryOutputStream bytes;
        const bool ok = a.size() > 1 && juce::Base64::convertFromBase64 (bytes, a[1].toString())
                        && proc.loadCustomSample (juce::jlimit (0, 1, argInt (a, 0)), bytes.getData(), bytes.getDataSize(),
                                                  a.size() > 2 ? a[2].toString().upToLastOccurrenceOf (".", false, false) : juce::String());
        done (juce::var (ok));
    });
    opts = opts.withNativeFunction ("customClear", [this] (const Args& a, Done done)
    {
        proc.loadCustomSample (juce::jlimit (0, 1, argInt (a, 0)), nullptr, 0, {});
        done (juce::var (true));
    });
    opts = opts.withNativeFunction ("fxDefaultIr", [this] (const Args&, Done done)
    {
        proc.getFxRack().loadImpulse (nullptr, 0, {});
        done (juce::var (true));
    });

    // ── patches ──
    opts = opts.withNativeFunction ("initPatch", [this] (const Args& a, Done done)
    {
        proc.loadInitPatch (juce::jlimit (0, 1, argInt (a, 0)));
        setPatchName ("INIT");
        done (juce::var());
    });
    // A/B: abCopy stores the current sound as the other slot; abToggle swaps between them.
    opts = opts.withNativeFunction ("abCopy", [this] (const Args&, Done done)
    {
        abStore.reset();
        proc.getStateInformation (abStore);
        done (juce::var (true));
    });
    opts = opts.withNativeFunction ("abToggle", [this] (const Args&, Done done)
    {
        if (abStore.getSize() == 0) { proc.getStateInformation (abStore); done (juce::var (abIsB)); return; }
        juce::MemoryBlock cur;
        proc.getStateInformation (cur);
        proc.setStateInformation (abStore.getData(), static_cast<int> (abStore.getSize()));
        abStore = cur;
        abIsB = ! abIsB;
        done (juce::var (abIsB));
    });
    opts = opts.withNativeFunction ("patchFolder", [] (const Args&, Done done)
    {
        done (juce::var (defaultPatchFolder().getFullPathName()));
    });
    opts = opts.withNativeFunction ("patchList", [this] (const Args& a, Done done)
    {
        const auto folder = a.size() > 0 && a[0].toString().isNotEmpty() ? juce::File (a[0].toString()) : defaultPatchFolder();
        done (listPatches (folder));
    });
    opts = opts.withNativeFunction ("patchLoad", [this] (const Args& a, Done done)
    {
        const bool ok = a.size() > 0 && loadPatchFile (juce::File (a[0].toString()));
        done (juce::var (ok ? patchName() : juce::String()));
    });
    // patchSave(folder, name, bank, type)
    opts = opts.withNativeFunction ("patchSave", [this] (const Args& a, Done done)
    {
        const auto folder = a.size() > 0 && a[0].toString().isNotEmpty() ? juce::File (a[0].toString()) : defaultPatchFolder();
        const auto f = savePatchFile (folder, a.size() > 1 ? a[1].toString() : patchName(),
                                      a.size() > 2 ? a[2].toString() : juce::String(), a.size() > 3 ? a[3].toString() : juce::String());
        done (juce::var (f == juce::File {} ? juce::String() : f.getFullPathName()));
    });

    // the banks (sub-folders) of a patch folder, and making a new one
    opts = opts.withNativeFunction ("patchBanks", [this] (const Args& a, Done done)
    {
        const auto folder = a.size() > 0 && a[0].toString().isNotEmpty() ? juce::File (a[0].toString()) : defaultPatchFolder();
        juce::Array<juce::File> dirs;
        juce::Array<juce::var> out;
        folder.findChildFiles (dirs, juce::File::findDirectories, false);
        dirs.sort();
        for (const auto& d : dirs) out.add (juce::var (d.getFileName()));
        done (out);
    });
    opts = opts.withNativeFunction ("patchNewBank", [this] (const Args& a, Done done)
    {
        const auto folder = a.size() > 0 && a[0].toString().isNotEmpty() ? juce::File (a[0].toString()) : defaultPatchFolder();
        const auto name = a.size() > 1 ? a[1].toString().trim() : juce::String();
        if (name.isEmpty()) { done (juce::var (false)); return; }
        done (juce::var (folder.getChildFile (juce::File::createLegalFileName (name)).createDirectory().wasOk()));
    });
    // the star on a row: kept in the global settings, so it follows the user, not the project
    opts = opts.withNativeFunction ("patchFavourite", [this] (const Args& a, Done done)
    {
        const auto path = a.size() > 0 ? a[0].toString() : juce::String();
        auto favs = favourites();
        favs.removeEmptyStrings();
        if (a.size() > 1 && static_cast<bool> (a[1])) favs.addIfNotAlreadyThere (path); else favs.removeString (path);
        globalSettings->setValue ("favourites", favs.joinIntoString ("\n"));
        globalSettings->saveIfNeeded();
        done (juce::var (true));
    });
    // the type of the sound in the editor right now
    opts = opts.withNativeFunction ("patchType", [this] (const Args& a, Done done)
    {
        if (a.size() > 0) proc.apvts.state.setProperty ("patchType", a[0].toString(), nullptr);
        done (juce::var (proc.apvts.state.getProperty ("patchType", "").toString()));
    });
    opts = opts.withNativeFunction ("patchName", [this] (const Args& a, Done done)
    {
        if (a.size() > 0) setPatchName (a[0].toString());
        done (juce::var (patchName()));
    });

    opts = opts.withNativeFunction ("patchSaveAs", [this] (const Args&, Done done)
    {
        patchChooser = std::make_unique<juce::FileChooser> ("Save 002 patch",
            defaultPatchFolder().getChildFile (juce::File::createLegalFileName (patchName()) + ".gpatch"), "*.gpatch");
        patchChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, done] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                juce::var result;
                if (file != juce::File {})
                {
                    // "save a copy as": the chosen folder is used as-is, so no bank is added
                    const auto saved = savePatchFile (file.getParentDirectory(), file.getFileNameWithoutExtension(), {},
                                                      proc.apvts.state.getProperty ("patchType", "").toString());
                    if (saved != juce::File {}) result = saved.getFullPathName();
                }
                patchChooser.reset();
                done (result);
            });
    });

    opts = opts.withNativeFunction ("patchOpen", [this] (const Args&, Done done)
    {
        patchChooser = std::make_unique<juce::FileChooser> ("Open 002 patch", defaultPatchFolder(), "*.gpatch");
        patchChooser->launchAsync (juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectFiles,
            [this, done] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                juce::var result;
                if (file != juce::File {} && loadPatchFile (file)) result = file.getFullPathName();
                patchChooser.reset();
                done (result);
            });
    });

    opts = opts.withNativeFunction ("patchChooseFolder", [this] (const Args&, Done done)
    {
        patchChooser = std::make_unique<juce::FileChooser> ("Choose 002 patch folder", defaultPatchFolder(), juce::String());
        patchChooser->launchAsync (juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectDirectories,
            [this, done] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                const juce::var result = file != juce::File {} ? juce::var (file.getFullPathName()) : juce::var();
                patchChooser.reset();
                done (result);
            });
    });

   #if JUCE_WINDOWS
    opts = opts.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                            .withBackgroundColour (juce::Colour (0xff1c1d1f))
                                            .withStatusBarDisabled()
                                            .withUserDataFolder (juce::File::getSpecialLocation (
                                                juce::File::tempDirectory)
                                                .getChildFile ("GeminusWebView-" + uiCacheKey())));
   #endif

   #if ! GEMINUS_DEV_UI
    opts = opts.withResourceProvider (provide);
   #endif

    return opts;
}

GeminusWebSession::GeminusWebSession (SuperGeminiProcessor& p)
    : proc (p)
{
    buildRelays();                       // relays must exist before the options fold them in
    globalSettings = std::make_unique<juce::PropertiesFile> (globalSettingsOptions());
    desktopLayout = globalSettings->getBoolValue ("desktopLayout", false);
    pageW = desktopLayout ? kDesktopPageW : kPageW;
    pageH = desktopLayout ? kDesktopPageH : kPageH;
    web = std::make_unique<Page> (makeOptions());
    web->onLoaded = [this]
    {
        pageLoaded = true;
        fitPending = true;
        trace ("page-loaded");
    };
    addAndMakeVisible (*web);

    // Stored scale, else the largest that fits the primary display comfortably (≤ 100 %).
    float s = static_cast<float> (proc.apvts.state.getProperty ("uiScale", 0.0));
    if (s <= 0.0f)
    {
        s = 1.0f;
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userBounds;
            s = juce::jmin (1.0f, 0.90f * static_cast<float> (area.getWidth())  / static_cast<float> (pageW),
                                  0.86f * static_cast<float> (area.getHeight()) / static_cast<float> (pageH));
        }
    }
    const auto scale = juce::jlimit (0.35f, 2.0f, s);
    savedSize = { juce::roundToInt (pageW * scale), juce::roundToInt (pageH * scale) };
    setSize (savedSize.x, savedSize.y);
    reloadUi();
    startTimerHz (30);
    trace ("session-created");
}

// Opt-in host verification; no logging or disk traffic in normal use.
void GeminusWebSession::trace (const char* event) const
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("GEMINUS_UI_TRACE_FILE", {});
    if (path.isNotEmpty())
        juce::File (path).appendText (juce::String (juce::Time::getMillisecondCounterHiRes(), 3)
            + " session=" + juce::String::toHexString (reinterpret_cast<juce::pointer_sized_int> (this))
            + " " + event + " " + juce::String (savedSize.x) + "x" + juce::String (savedSize.y) + "\n");
}

juce::var GeminusWebSession::sequenceToVar (const sg::Sequence& s)
{
    juce::DynamicObject::Ptr o (new juce::DynamicObject());
    o->setProperty ("length", s.length);
    juce::Array<juce::var> steps;
    for (const auto& st : s.steps)
    {
        juce::DynamicObject::Ptr so (new juce::DynamicObject());
        juce::Array<juce::var> notes;
        for (int i = 0; i < st.count; ++i) notes.add (static_cast<int> (st.notes[static_cast<size_t> (i)]));
        so->setProperty ("n", notes);
        so->setProperty ("v", st.velocity);
        so->setProperty ("t", st.tie);
        so->setProperty ("a", st.accent);
        so->setProperty ("r", st.rest);
        steps.add (juce::var (so.get()));
    }
    o->setProperty ("steps", steps);
    return juce::var (o.get());
}

// One batched event per tick. Sending a separate event per value would cost far more in
// round trips than the payload is worth.
void GeminusWebSession::timerCallback()
{
    if (web == nullptr || ! pageLoaded || ! isShowing())
        return;
    if (fitPending)
    {
        // Relays suppress delivery while hidden; refresh values without rebuilding
        // any bindings when the retained document becomes visible again.
        for (auto& a : sliderAttach) a->sendInitialUpdate();
        for (auto& a : toggleAttach) a->sendInitialUpdate();
        for (auto& a : comboAttach) a->sendInitialUpdate();
        applyZoom();
        fitPending = false;
    }

    auto& b = proc.getUiBridge();

    // Peaks are max-since-last-read, so exchanging clears them for the next window.
    juce::DynamicObject::Ptr o (new juce::DynamicObject());
    o->setProperty ("peakL", b.peakL.exchange (0.0f));
    o->setProperty ("peakR", b.peakR.exchange (0.0f));
    o->setProperty ("bpm",   b.bpm.load());
    o->setProperty ("lastNote", b.lastNote.exchange (-1));

    juce::Array<juce::var> voices, env1, env2, cutoff, step, running, recording, recStep, loops;
    for (auto& l : b.layer)
    {
        voices.add (l.voices.load());
        env1.add   (l.env1.load());
        env2.add   (l.env2.load());
        cutoff.add (l.cutoffNorm.load());
        step.add   (l.step.load());
        running.add (l.running.load());
        recording.add (l.recording.load());
        recStep.add (l.recStep.load());
        loops.add (static_cast<int> (l.loopCycles.load()));
    }
    o->setProperty ("voices", voices);
    o->setProperty ("env1", env1);
    o->setProperty ("env2", env2);
    o->setProperty ("cutoff", cutoff);
    o->setProperty ("step", step);
    o->setProperty ("running", running);
    o->setProperty ("recording", recording);
    o->setProperty ("recStep", recStep);
    o->setProperty ("loops", loops);

    // The working sequences travel only when one of them changed.
    const auto ver = b.getSequenceVersion();
    if (ver != sentSeqVersion)
    {
        juce::Array<juce::var> seqs;
        for (int l = 0; l < 2; ++l)
        {
            sg::Sequence s;
            b.readSequence (l, s);
            seqs.add (sequenceToVar (s));
        }
        o->setProperty ("seq", seqs);
        sentSeqVersion = ver;
    }

    // FX rack: each slot's live display values, and the impulse response when it changed
    auto& rack = proc.getFxRack();
    juce::Array<juce::var> fxVis;
    for (int s = 0; s < fx::Rack::kSlots; ++s)
        for (int i = 0; i < 8; ++i) fxVis.add (rack.getVis (s, i));
    o->setProperty ("fx", fxVis);
    if (rack.getImpulseVersion() != sentIrVersion)
    {
        sentIrVersion = rack.getImpulseVersion();
        const auto& ir = rack.getImpulse();
        juce::Array<juce::var> peaks;
        const int len = ir.buffer.getNumSamples(), points = 240;
        float top = 1.0e-6f;
        for (int c = 0; c < ir.buffer.getNumChannels(); ++c) top = juce::jmax (top, ir.buffer.getMagnitude (c, 0, len));
        for (int p = 0; p < points; ++p)
        {
            const int a = p * len / points, e = juce::jmax (a + 1, (p + 1) * len / points);
            float pk = 0.0f;
            for (int c = 0; c < ir.buffer.getNumChannels(); ++c) pk = juce::jmax (pk, ir.buffer.getMagnitude (c, a, juce::jmin (e, len) - a));
            peaks.add (pk / top);
        }
        o->setProperty ("irName", ir.name);
        o->setProperty ("irWave", peaks);
        o->setProperty ("irSeconds", len / ir.rate);
    }

    // DDS 1 CUSTOM: name, length and a min/max outline per layer, when one changed
    for (int l = 0; l < 2; ++l)
    {
        const auto& cs = proc.getCustomSample (l);
        if (cs.version == sentCustomVersion[l]) continue;
        sentCustomVersion[l] = cs.version;
        juce::var info;
        if (const auto s = cs.now)
        {
            juce::DynamicObject::Ptr d (new juce::DynamicObject());
            juce::Array<juce::var> wave;
            const int points = 600;
            for (int p = 0; p < points; ++p)
            {
                const int a = p * s->frames / points, e = juce::jmax (a + 1, (p + 1) * s->frames / points);
                float lo = 0.0f, hi = 0.0f;
                for (int i = a; i < e; ++i)
                {
                    const float v = 0.5f * (s->l[static_cast<size_t> (i)] + s->r[static_cast<size_t> (i)]);
                    lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
                }
                wave.add (lo); wave.add (hi);
            }
            float top = 1.0e-6f;                       // the outline fills the display whatever the gain
            for (const auto& v : wave) top = juce::jmax (top, std::abs (static_cast<float> (v)));
            for (auto& v : wave) v = static_cast<float> (v) / top;
            d->setProperty ("name", cs.name);
            d->setProperty ("seconds", s->frames / static_cast<double> (s->rate));
            d->setProperty ("rate", s->rate);
            d->setProperty ("wave", wave);
            info = juce::var (d.get());
        }
        o->setProperty (l == 0 ? "customUpper" : "customLower", info);
    }

    const auto name = patchName();
    if (name != sentPatchName)
    {
        o->setProperty ("patch", name);
        sentPatchName = name;
    }

    web->emitEventIfBrowserIsVisible ("geminusState", juce::var (o.get()));
}

GeminusWebSession::~GeminusWebSession()
{
    stopTimer();
    patchChooser.reset();
    web->onLoaded = {};
    web.reset(); // browser callbacks end before relay/attachment destruction
    trace ("session-destroyed");
}

void GeminusWebSession::reloadUi()
{
    sentSeqVersion = ~0u;      // a fresh page needs the sequences and the patch name again
    sentPatchName = {};
    sentIrVersion = -1;
    pageLoaded = false;
   #if GEMINUS_DEV_UI
    const auto page = juce::File (GEMINUS_UI_DIR).getChildFile ("index.html");
    web->goToURL (page.getFullPathName().isNotEmpty() ? page.getURL().toString (false)
                                                      : juce::String ("about:blank"));
   #else
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
   #endif
}

// The page measures itself and tells us, so the window's aspect ratio and the zoom
// basis follow the design instead of a constant that drifts every time it is re-laid.
void GeminusWebSession::adoptPageSize (int w, int h)
{
    if (w < 100 || h < 100 || (w == pageW && h == pageH))
        return;

    pageW = w;
    pageH = h;
    fitPending = true;
}

void GeminusWebSession::setDesktopLayout (bool shouldUseDesktopLayout)
{
    if (desktopLayout == shouldUseDesktopLayout)
        return;

    desktopLayout = shouldUseDesktopLayout;
    globalSettings->setValue ("desktopLayout", desktopLayout);
    globalSettings->saveIfNeeded();
    pageW = desktopLayout ? kDesktopPageW : kPageW;
    pageH = desktopLayout ? kDesktopPageH : kPageH;
    fitPending = true;
    if (onLayoutChanged) onLayoutChanged();
}

// The page owns the fit - see fitToWindow() in ui/geminus.js. It is computed there and not
// here because CSS zoom works in CSS pixels while this component is measured in device
// pixels; on a display at anything but 100% the two differ by the display scale factor, and
// a zoom derived from device pixels overflows the window by exactly that much. The page also
// refits on its own resize event, so this is only a nudge for the cases that do not raise one.
void GeminusWebSession::applyZoom()
{
    if (web == nullptr || ! pageLoaded || ! isShowing() || getWidth() < 100)
        return;

    web->evaluateJavascript ("window.geminusFit && window.geminusFit();", nullptr);
}

void GeminusWebSession::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());

}

void GeminusWebSession::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1d1f));   // shows only before the first paint of the page
}

// Only this small host wrapper is disposable. Native callbacks capture the session.
GeminusWebEditor::GeminusWebEditor (SuperGeminiProcessor& p)
    : AudioProcessorEditor (&p), session (p.getWebSession())
{
    setResizable (true, true);
    session.onLayoutChanged = [this] { applyDesktopLayout(); };
    applyDesktopLayout();
    startTimer (1);
}

GeminusWebEditor::~GeminusWebEditor()
{
    stopTimer();
    cancelPendingUpdate();
    session.onLayoutChanged = {};
    session.trace ("editor-destroyed");
    // Detach before host peer destruction, without resizing or navigating the page.
    removeChildComponent (&session);
}

void GeminusWebEditor::timerCallback()
{
    if (getPeer() == nullptr || ! isShowing())
        return;
    stopTimer();
    setSize (session.savedSize.x, session.savedSize.y);
    session.setBounds (getLocalBounds());
    addAndMakeVisible (session);
    session.toBack(); // keep JUCE's resize corner above the retained browser component
    attached = true;
    session.fitAfterAttach();
    session.trace ("editor-attached");
}

void GeminusWebEditor::resized()
{
    if (attached)
        triggerAsyncUpdate();
}

void GeminusWebEditor::applyDesktopLayout()
{
    constexpr int minWidth = 1190, maxWidth = 6800;
    const auto heightFor = [this] (int width)
    {
        return juce::roundToInt (static_cast<float> (width) * session.pageH / session.pageW);
    };

    setResizeLimits (minWidth, heightFor (minWidth), maxWidth, heightFor (maxWidth));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (session.pageW) / session.pageH);

    const auto width = juce::jlimit (minWidth, maxWidth, session.savedSize.x);
    session.savedSize = { width, heightFor (width) };
    setSize (session.savedSize.x, session.savedSize.y);
}

void GeminusWebEditor::handleAsyncUpdate()
{
    // Commit after the host finishes its resize callback. Closing can clamp zero
    // to the native minimum; destruction cancels that provisional update too.
    if (! attached || ! isShowing() || getWidth() < 1190
        || getHeight() < juce::roundToInt (1190.0f * session.pageH / session.pageW))
        return;
    session.savedSize = { getWidth(), getHeight() };
    session.setBounds (getLocalBounds());
    session.trace ("editor-resized");
    // Browser resize already fits the page; never queue speculative JS before load.
}

void GeminusWebEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1d1f));
}
