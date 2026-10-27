package org.sudokura.prototype;

import org.libsdl.app.SDLActivity;

/** Thin host; use the Java Activity and the native SDL libraries from one SDL revision. */
public final class PrototypeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_ttf", "SDL2_mixer", "main"};
    }
}
