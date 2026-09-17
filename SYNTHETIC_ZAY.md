# Synthetic Zay LMMS customization

This branch is a personal LMMS customization workspace for Synthetic Zay and
Natural Psychosis production.

## Current status

- [x] Add an optional dark Synthetic Zay theme
- [ ] Add workflow-oriented defaults and shortcuts
- [ ] Tune build/runtime choices for Fedora KDE
- [ ] Improve external plugin and instrument workflow
- [ ] Prototype new production features

## Theme

The theme lives at:

```text
data/themes/synthetic-zay/
```

It keeps the complete classic LMMS stylesheet as a compatibility baseline and
adds focused overrides for a near-black interface with cyan, magenta, and
violet accents.

To test it in a local build:

1. Open **Edit > Settings**.
2. Find **Theme directory** in the paths section.
3. Select the installed or source-tree `data/themes/synthetic-zay` directory.
4. Restart LMMS.

LMMS searches the selected theme first and the default theme second, so image
resources that are not customized continue to use the default assets.

## Planned work order

1. **Theme QA** — verify contrast and readability in Song Editor, Beat/Bassline
   Editor, Piano Roll, Automation Editor, FX Mixer, and plugin windows.
2. **Workflow** — add production-friendly defaults without breaking existing
   projects or changing upstream behavior unexpectedly.
3. **Fedora performance** — document a reproducible build and test audio-buffer,
   realtime-priority, and plugin-discovery behavior.
4. **Plugins and instruments** — improve LV2, LADSPA, VST3/Wine bridge, and
   Vital-oriented workflows where LMMS supports them.
5. **Features** — prototype one focused feature per branch, with a clear test
   and rollback path.

## Branch policy

Keep `master` synchronized with upstream LMMS. Develop each customization in
a dedicated branch and merge only after it builds and passes a short manual
audio/UI check.

## Fedora build helper

The feature branch includes an isolated Fedora build workflow:

```bash
./buildtools/build-fedora-synthetic-zay.sh deps
./buildtools/build-fedora-synthetic-zay.sh configure
./buildtools/build-fedora-synthetic-zay.sh build
./buildtools/build-fedora-synthetic-zay.sh test
./buildtools/build-fedora-synthetic-zay.sh run
```

Only `deps` installs system packages and uses `sudo`. The build is written to
`build-synthetic-zay`, and the optional user installation is written to
`target-synthetic-zay`, so the normal Fedora LMMS package remains untouched.

To create a distributable AppImage after a successful build:

```bash
./buildtools/build-fedora-synthetic-zay.sh package
```

