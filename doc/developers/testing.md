# Development setup and tests



```sh
sudo apt install build-essential pkg-config \
    libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev libxinerama-dev libfontconfig-dev xvfb libgtest-dev xserver-xephyr libpam0g-dev
make                  # build (no debugging information)
make DEBUG=1          # build with -Og -g, for gdb, and with TINY_LOG() messages
make check            # unit tests (run against Xvfb)
make help             # all the targets and variables
tools/run-xephyr.sh   # run the window manager in a nested Xephyr
```

Install the git hooks once, they check the formatting, spelling and shell
scripts on every commit and run the static analysis and the tests before a
push:

```sh
sudo apt install pre-commit clang-format clang-tidy cppcheck
pre-commit install --install-hooks
pre-commit install --hook-type pre-push
```

Before sending a change, build without warnings and run `make check`. When
you work on memory handling, also build with the sanitizers:

```sh
make asan        # all the tests with AddressSanitizer and UBSan
```

The same checks run in GitHub Actions (`.github/workflows/ci.yml`) with GCC,
Clang, on Alpine (musl) and NetBSD, plus pre-commit, clang-tidy, cppcheck,
the sanitizers, a coverage minimum and a binary size limit (`make release`
builds the small programs). A
change should pass all of them; the NetBSD job is best effort.

## Tests

Tests live in `tests/`, which mirrors `lib/`: the tests of `lib/wm/dock.cc`
are in `tests/wm/test_dock.cc`. They use googletest, a test suite is named
`<Subject>Test` (`DockTest`, `DockLifeTest`). Tests that need a
display run against Xvfb through `make check`; `tests/helpers.h` and
`tests/fake_client.h` have helpers for fake clients. Every new behavior and every
bug fix needs a test. New files are picked up automatically by the Makefile.
Use `make coverage` to see what is not covered.

Every file in `lib/` is covered by tests at least from 90 %, the ideal is
100 %; the coverage of a file never goes down. A line that cannot be reached
in a test (for example code that runs only in a forked child, whose counters
are lost on `_exit`) is the only acceptable gap, and it is better to keep such
code small.

## The documentation and the web site

The documentation is the Markdown files in `doc/`. They are read on GitHub as
they are, and `doc/build.py` makes the web site from them: the Markdown is
converted by [m2r2](https://pypi.org/project/m2r2/) to reStructuredText and by
docutils to HTML, and put into one template with `doc/static/style.css`.

```sh
python3 -m venv .venv && .venv/bin/pip install -r doc/requirements.txt
.venv/bin/python doc/build.py      # writes build-site/, open build-site/index.html
```

- A new page must be added to the menu in `doc/build.py` (`NAV`, `EXTRA` or
  `DEVELOPERS`), the build fails otherwise.
- Link to another page as `other.md#heading`; the build turns it into `.html`
  and fails when the page or the heading is not there. Do not use a link text
  that is the same as a heading of the same page (reStructuredText makes the
  names of both the same target).
- The icons of the site come from `data/`. The screenshots in
  `doc/static/screenshots/` are not made by the build: `tools/screenshots.sh`
  makes them on request, when the look of the shell changes much (it runs the
  shell in Xvfb with a configuration of its own; `FAVORITES` sets the
  applications in the dock, see the script), and the pictures are committed.
- The comparison (`doc/comparison.md`) uses the symbols ✓ (has it), ✗ (does not
  have it), ◔ (planned for tiny-shell) and ? (not decided); the build colors
  them.
- The workflow `.github/workflows/pages.yml` builds the site for every pull
  request and publishes it with GitHub Pages when `master` changes.
