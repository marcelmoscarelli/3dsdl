Description
-	An ongoing gorilla-style project of a basic 3D engine using only SDL2

Dependencies, for now
-	libsdl2
-	libsdl2-ttf

Build & Run
-	`make` to build the project (requires pkg-config and SDL2 development packages).
-	`./bin/3D_SDL` after building.

Notes
-	The program attempts to open a few system fonts as fallbacks.
-   If no compatible font is found, the stats overlay will not be displayed.