#ifndef PIPER_APP_AUTOSAVE_H
#define PIPER_APP_AUTOSAVE_H

#include <string>
#include <vector>

#include "piper/app/document.h"

namespace piper::studio
{
    // XDG-aware autosave directory. Empty when neither XDG_DATA_HOME
    // nor HOME is set in the environment.
    std::string autosave_dir();

    // Atomic-ish: serialize, write to <path>.tmp, rename to <path>.
    // Updates `doc.autosave_path` and `doc.last_autosave_at` on success.
    // Best-effort on failure (logs to stderr, leaves doc unchanged).
    void autosave_write(Document& doc);

    // Removes `doc.autosave_path` from disk if non-empty and clears it.
    void autosave_remove(Document& doc);

    // Returns the list of `.piper` files currently sitting in the
    // autosave directory (typically leftover from a previous session
    // that crashed before they could be cleaned up). Files whose
    // session-<pid>-<id> name points at a still-running process are
    // skipped.
    std::vector<std::string> scan_autosave_dir();
}

#endif
