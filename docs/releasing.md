# Releasing

This repo's release flow is **tag-driven**.

## What `scripts/release.sh` does

- reads the first `CHANGELOG.md` section (`## [X.Y.Z]` or `## [X.Y.Z] - YYYY-MM-DD`)
- prints the notes that will become the GitHub Release body
- refuses to run on a dirty working tree
- refuses to reuse an existing local or remote tag unless you pass `--retry`
- creates annotated tag `vX.Y.Z`
- pushes the current branch and the tag to `origin`

`--retry` deletes the existing `vX.Y.Z` tag locally and on `origin`, then retags `HEAD`. Use that only after a failed release CI run.

## GitHub Actions

Pushing `vX.Y.Z` triggers [`.github/workflows/release.yml`](../.github/workflows/release.yml), which:

- builds `webview2gtk-setup.exe`
- builds the MSYS2 package `mingw-w64-ucrt-x86_64-webview2gtk-*.pkg.tar.zst`
- signs the package on tag releases
- renders `release-notes.md` from the matching `CHANGELOG.md` section
- publishes the artifacts and the changelog text as the GitHub Release body

## Changelog format

The first section in `CHANGELOG.md` is the current version heading. It may already be tagged — **check `git tag -l vX.Y.Z` before adding work.** If that tag exists, the version is closed: open the next `## [X.Y.Z]` and bump meson / PKGBUILD. Do not keep writing into a tagged release.

```md
## [0.5.2]
```

or fill in the date when you remember:

```md
## [0.5.2] - 2026-09-16
```

Do not use Unreleased.
