# Advisory: pilot-link build failures on modern autoconf (Debian 12 / Ubuntu 22.04+)

**Date:** March 2026  
**Affects:** pilot-link source from https://github.com/jichu4n/pilot-link  
**Symptom:** `./configure` aborts with `syntax error near unexpected token ')'`  
**Platforms confirmed affected:** Debian 12 (Bookworm), likely any distro shipping autoconf 2.71+ and automake 1.16+

---

## The problem

The `configure.ac` file contains several constructs that generated valid shell code under older versions of autoconf but produce broken output under autoconf 2.71+. There are two distinct failure points.

### Failure 1 — Missing `AC_BLUEZ` macro

`configure.ac` calls `AC_BLUEZ(...)` to detect BlueZ Bluetooth support. This macro is not part of the standard autoconf library and is not defined anywhere in the source tree. When autoconf encounters an undefined macro used as a shell construct, it silently omits it — but the surrounding code (a `PKG_CHECK_MODULES` fallback block) is left structurally incomplete, producing a `fi,` with a trailing comma and orphaned `)` in the generated `configure` script.

**The error:**
```
./configure: line XXXXX: syntax error near unexpected token `)'
./configure: line XXXXX: `              )'
```

### Failure 2 — Deeply nested AC_CHECK_* macros in popt detection

The popt library detection uses `AC_CHECK_HEADER` containing `AC_CHECK_DECL` containing `AC_CHECK_LIB`. This degree of nesting was tolerated by older autoconf but generates malformed shell under 2.71+, producing the same class of syntax error.

---

## Recommended fixes

These are minimal fixes to unblock the build. See the "Ideal fix" section below for what a proper upstream patch would look like.

### Fix 1 — BlueZ detection

Remove the entire BlueZ detection block and replace with empty substitutions. The lines to remove will be approximately 347–378 in the current source — verify with:

```bash
grep -n 'bluetooth\|bluez\|BLUETOOTH' configure.ac
```

Replace the removed block with:

```m4
AM_CONDITIONAL([WITH_BLUEZ],[false])
AC_SUBST([BLUEZ_CFLAGS],[])
AC_SUBST([BLUEZ_LIBS],[])
```

**Impact:** BlueZ (Bluetooth HotSync) support is disabled. USB and serial HotSync are unaffected. The vast majority of Palm-era devices sync via USB or serial cradles; Bluetooth sync was only supported by a small number of later devices.

If Bluetooth support is important to you, the correct fix is to write a proper `AC_BLUEZ` macro (or replace the whole block with a standard `PKG_CHECK_MODULES([BLUEZ],[bluez], ...)` call) and contribute it back upstream.

### Fix 2 — popt detection

Force use of the bundled popt library, which ships with pilot-link, and remove the now-redundant nested detection block. First, change the default in `configure.ac`:

```bash
sed -i 's/\[with_included_popt="auto"\]/[with_included_popt="yes"]/' configure.ac
```

Then remove the inner `if test "x$with_included_popt" != "xyes"` block and any orphaned `fi` / `else...fi` left behind. The exact line numbers will depend on your copy of the file — use `grep -n 'with_included_popt' configure.ac` to locate them before editing.

**Impact:** The system `libpopt` library is not used even if present. Pilot-link uses its own bundled popt. There is no functional difference for HotSync operations.

---

## After patching

Regenerate the build system from the patched `configure.ac`:

```bash
autoreconf -fi
./configure --prefix=/usr/local
make
sudo make install
sudo /sbin/ldconfig
```

---

## Ideal upstream fix

The patches above are intentionally minimal. A proper upstream fix would address the following:

**Replace `AC_BLUEZ` with standard pkg-config detection:**
```m4
PKG_CHECK_MODULES([BLUEZ], [bluez],
  [have_bluez=yes],
  [have_bluez=no])
AM_CONDITIONAL([WITH_BLUEZ], [test "$have_bluez" = "yes"])
```

**Replace nested AC_CHECK_* with a simpler popt check:**
```m4
PKG_CHECK_MODULES([POPT], [popt],
  [with_included_popt=no],
  [with_included_popt=yes])
```

**Modernise deprecated macros** throughout `configure.ac`. Autoconf will warn about all of these during `autoreconf`:

| Deprecated | Replacement |
|------------|-------------|
| `AC_TRY_LINK` | `AC_LINK_IFELSE` |
| `AC_TRY_COMPILE` | `AC_COMPILE_IFELSE` |
| `AC_TRY_RUN` | `AC_RUN_IFELSE` |
| `AC_HEADER_STDC` | (remove — C89 headers always present) |
| `AC_HEADER_TIME` | (remove — sys/time.h always present) |
| `AC_TYPE_SIGNAL` | (remove — signal handlers always void) |
| `AM_PROG_MKDIR_P` | `AC_PROG_MKDIR_P` |
| `AM_CONFIG_HEADER` | `AC_CONFIG_HEADERS` |
| `AC_CANONICAL_SYSTEM` | `AC_CANONICAL_HOST` |
| `AM_PROG_LIBTOOL` | `LT_INIT` |
| `AC_HELP_STRING` | `AS_HELP_STRING` |
| `AC_LIBTOOL_DLOPEN` | `LT_INIT([dlopen])` |

Running `autoupdate` in the source tree will handle most of these automatically.

---

## Reporting

If you've encountered this issue and applied these fixes, please consider opening a GitHub issue at https://github.com/jichu4n/pilot-link referencing this document. A pull request modernising `configure.ac` would benefit anyone trying to build on Debian 12, Ubuntu 22.04+, Fedora 38+, or any other distribution shipping autoconf 2.71+.
