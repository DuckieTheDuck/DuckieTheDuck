#pragma once

// Makes CTRPF work in games built with libctru, like DuckLab.
// See libctru_compat.cpp for how it works.
namespace CTRPluginFramework
{
namespace LibctruCompat
{
    // Call from PatchProcess(). Returns false if it couldn't set itself up.
    bool Install(void);

    // Call at the start of main(), once CTRPF has finished booting.
    void OnCtrpfReady(void);
}
}
