The evolution of this project would be based on the writing on real applications 
to stress test the interface, and propose actual useful (aka, actually used) new features.

Some things to muse about:

* The core interfaces need to be stable, slim and minimalists to avoid cluttering
the develloper mind with optional features he would never use. Thus, an early path for
evolution would be to streamline, not to add.

* Evolution of the core (long term): the old interfaces need to stay for compatibility 
reasons, which means evolutions need to be made in optional modules with added 
interfaces. Major versions evolution would be done by refactoring those modules 
interfaces in a coherent manner.

* One core, many interfaces (prospectif): various applications will often gravitate
around a small subset of features instead of the full set. Consider having duplicates
interfaces tailored for various use case.

Major evolutions:

* standard interfaces for files, streams and path management. Binary streams are the 
most generic, but text streams are also incredibly important for things like configuration
files, scripts, CLI.

* A browser for our memory structure.

* A language that would be a perfect fit for our core architecture (script, or compiled).

* SDL graphic interface for things like gui, or 2D graphical interactive applications.

* Glyph art graphics: grid based graphics with paletted background, foreground, and glyph 
layers (+optional layers like normal or depth) for things like scalables sprites, icons, fonts
or even textures.

* Vulkan interface: as a portal for interactive 3D applications, or advanced 2D shaders.

Projects suggestion:

* An emulator architecture: based around backbones that are a strict timeline on which 
components evolves, and bus based component plugs (busses being: memory address, or I/O 
lines). The timeline follow internal time, synced on realtime dependent on key components
like sound output or scanlines. Emulated machines could range from zx spectrum up to
PSX.

* A Glyph art editor.

* A simple text editor, for script editing, or in application block note like component.

* A terminal window, that could be repurposed into log window, shat window or other
similar component.

