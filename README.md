# Movie Master

A free, offline screenplay editor for DOS, Windows and Apple Silicon Mac.
This repository contains the application source and its English landing page.

[Website](https://moviemaster.app) · [Download the app](https://github.com/Bubliska/movie-master/releases/latest)

## Writing

Six screenplay elements, Turkish F or system keyboard, undo/redo, scene list,
scene move/duplicate, Fountain import, and PDF/TXT/Fountain export. PDF embeds
Courier Prime. The DOS interface is monochrome, with a steady software cursor.
The scene list opens from the menu rather than occupying the writing screen.

- F1 Slugline, F2 Action, F3 Character, F4 Parenthetical, F5 Dialog, F6 Transition.
- Tab cycles elements or completes a repeated cue. Ctrl+Enter adds an element.
- F9/Esc opens menus. F10/Ctrl+S saves. Alt+H opens help.
- First Save, Save As, import and every export open the in-app folder browser.
  It starts at Desktop. Enter opens folders; F5 saves here; F3 accepts a path.
- Windows: extract the portable ZIP and run BASLAT.BAT. DOS: run WRITER.EXE with
  CWSDPMI.EXE and FONT.TTF alongside it. Mac: drag Movie Master.app to Applications.

## Scene numbers — v0.4.3

The status bar shows the scene count as `Sc:`. `L` means numbering is locked.
Use **Scenes → Show Scene Numbers** to number headings and PDF/TXT/Fountain
output. **Scenes → Lock Scene Numbers** freezes current labels and turns their
visibility on. A scene inserted between 11 and 12 receives 11A, then 11B.
Existing labels survive edits, moving, saving/reopening, recovery and undo/redo.
Duplicated/pasted scenes receive fresh labels. Deleted labels remain reserved.
Insertions before the first scene use 0A, 0B; additions at the end use the next
unused whole number. Suffixes continue after Z as AA, AB. Inserting into an
already revised group uses its next unused suffix, without renaming old scenes.

**Unlock and Renumber** asks for confirmation and restores 1, 2, 3… in document
order. Hiding numbers does not unlock them. The list still displays labels.

Numbered SEN documents use **WRITER 2** format; unnumbered documents keep
WRITER 1. This version opens both. Older app versions cannot open WRITER 2.
Fountain `#11A#` labels import as locked numbers. Fountain carries visible
labels, not the registry of deleted numbers; use SEN to preserve that registry.
Duplicate/invalid imported labels cancel import while preserving the open draft.

## Build and test

All application and website sources are at the repository root. Build commands
name their C inputs explicitly; browser_test.c is a separate test harness.
No source from the historical Movie Master or Fade In applications is included.

Native regression tests (Clang, macOS/Linux):

```sh
mkdir -p build/test build/browser
clang -std=c99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined core.c scenes.c pdf.c files.c fountain.c screen.c main.c tests.c -o build/hosttest
cp FONT.TTF build/test/
(cd build/test && ../hosttest /TEST)
clang -std=c99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined core.c scenes.c pdf.c files.c fountain.c tests.c browser_test.c -o build/browser-test
cp FONT.TTF build/browser/
(cd build/browser && ../browser-test)
```

Use an empty browser test directory each run. Tests create sample documents and
exports inside their working directory. FONT.TTF is required for PDF tests.
The native host build runs tests; the actual editor targets Windows or DOS.

DOS: install DJGPP 2.05/GCC 9.3 and CWSDPMI, set DJGPP and PATH, then run
`gcc @BUILD.RSP`. Use 128 MB DOSBox memory for compilation, 32 MB for the editor.
[DJGPP](https://www.delorie.com/djgpp/).

Windows (Zig 0.14.1; replace x86_64 with x86 for 32-bit):

```sh
zig cc -target x86_64-windows-gnu -std=c99 -O2 -s -Wall -Wextra -Werror core.c scenes.c pdf.c files.c fountain.c screen.c main.c tests.c -luser32 -lshell32 -o WRITER.EXE
```

Mac: see `python3 build_mac.py --help`. Supply a portable release directory
containing DOS/, SRC/ and LICENSES/, and the official DOSBox-X SDL2 ARM64 .app.
The wrapper preserves private Application Support/WRITER session storage and
opens file locations inside the editor. Its public app name is Movie Master;
WRITER remains the internal executable/file-format identifier.

## Website and deployment

The buildless landing page uses index.html, credits.html, style.css, app.js,
mark.svg and movie-master-dune.png. Serve the root with any static HTTP server.
The image is an unretouched screenshot of the running v0.4.2 DOS app, containing
an original Arrakis/Paul/Chani demo scene, not a published screenplay excerpt.
Eric Roth sources and image credits are on the credits page; no endorsement.
Donations show “Coming soon”; set donationUrl in app.js when a provider is chosen.

Import this repository in Vercel with the Other framework preset and root `.`.
vercel.json copies only the six public site files into public/. No dependencies
are installed. App downloads link to GitHub release assets, so the website does
not reconstruct large binaries in the browser. Update both download links and
version text together when publishing a new release. A push updates the site.

## License and limitations

Original application and website code: MIT, see LICENSE. Third-party components
retain their own licenses: see THIRD-PARTY.txt and LICENSE-* files. The Mac
release bundles unmodified DOSBox-X; GPL and source notices accompany it.

This beta was tested in DOSBox-X and with native Clang ASan/UBSan. Windows 32/64
builds compile, but no live Windows machine or physical DOS hardware was used.
Mac requires Apple Silicon and macOS 15+. The Mac app is ad-hoc signed and is not
Apple-notarized; no Intel Mac binary is included. Online collaboration, advanced
revision tracking and production scheduling are outside this app's scope.

Public visitors may view, download and fork this repository. They do not have
write access to this repository; proposed changes require the owner's review.
