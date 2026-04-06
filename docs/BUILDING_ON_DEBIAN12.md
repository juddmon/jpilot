# Building J-Pilot 2.0.2 on Debian 12 (Bookworm)

**Prepared:** March 2026  
**Author:** Neil Sinclair  
**Platform tested:** Debian 12 (Bookworm), x86_64

---

## Background

J-Pilot is not available in the Debian 12 package repositories — it was dropped after Debian 10 (Buster). The packagecloud.io repository referenced in the README (`https://packagecloud.io/install/repositories/judd/jpilot/script.deb.sh`) sets up the apt source correctly but contains no packages for Debian 12, resulting in `E: Unable to locate package jpilot`.

Both J-Pilot and its dependency `pilot-link` must be built from source on Debian 12. This document records the steps required, including patches needed to work around incompatibilities between the pilot-link source and the versions of autoconf, automake, and libtool shipped with Debian 12.

Pre-built `.deb` packages produced by this process are available alongside this document:

- `pilot-link_0.12.5-1_amd64.deb`
- `jpilot_2.0.2-1_amd64.deb`

---

## Quick Install (from pre-built .deb packages)

If you just want to install on a Debian 12 machine without building from source:

```bash
sudo dpkg -i pilot-link_0.12.5-1_amd64.deb
sudo dpkg -i jpilot_2.0.2-1_amd64.deb
sudo apt-get install -f   # pulls in GTK, GLib, SQLite and other runtime dependencies automatically
sudo /sbin/ldconfig
sudo usermod -aG dialout $USER
# Log out and back in for the group change to take effect
```

The `apt-get install -f` step is required because `dpkg` installs the package but does not automatically fetch dependencies — `apt-get install -f` resolves and installs them in a second pass. On a typical Debian 12 desktop most of these (GTK, GLib) will already be present.

---

## Building from Source

### 1. Install build dependencies

```bash
sudo apt install \
  build-essential autoconf automake libtool autopoint intltool \
  libgtk-3-dev libgcrypt20-dev libsqlite3-dev \
  libusb-1.0-0-dev libpopt-dev \
  gettext checkinstall
```

Note: do **not** install `libbluetooth-dev` — see the Bluetooth section below.

---

### 2. Build pilot-link

The upstream pilot-link source (from https://github.com/jichu4n/pilot-link) requires several patches to build on Debian 12. Apply them in order before running `autoreconf`.

#### Clone

```bash
git clone https://github.com/jichu4n/pilot-link.git
cd pilot-link
```

#### Patch 1 — Remove the broken BlueZ detection block

The `configure.ac` file contains a BlueZ (Bluetooth) detection macro (`AC_BLUEZ`) that does not exist in the standard autoconf macro library. When autoconf expands it, it generates syntactically invalid shell code that causes `./configure` to abort with `syntax error near unexpected token ')'`.

The fix is to remove the entire BlueZ detection block from `configure.ac` and replace it with empty substitutions so the Makefile variables are still defined:

```bash
# Remove lines 347–378 (the bluez detection block)
sed -i '347,378d' configure.ac

# Replace with a no-op conditional and empty variable substitutions
sed -i '346a AM_CONDITIONAL([WITH_BLUEZ],[false])' configure.ac
sed -i '/AM_CONDITIONAL(\[WITH_BLUEZ\]/a AC_SUBST([BLUEZ_CFLAGS],[])' configure.ac
sed -i '/AC_SUBST(\[BLUEZ_CFLAGS\]/a AC_SUBST([BLUEZ_LIBS],[])' configure.ac
```

**Implication:** BlueZ (Bluetooth HotSync) support is disabled. This affects Palm devices that sync via Bluetooth. USB and serial HotSync (covering the vast majority of devices including Sony Clié and Handspring Visor) are unaffected. If Bluetooth sync is required, a correct `AC_BLUEZ` macro implementation would need to be written and contributed upstream.

#### Patch 2 — Force bundled popt and remove broken nested detection

The popt detection block uses deeply nested `AC_CHECK_HEADER` / `AC_CHECK_DECL` / `AC_CHECK_LIB` macros which generate broken shell code under Debian 12's autoconf. The fix is to force use of the bundled popt library (which ships with pilot-link) and remove the inner detection block:

```bash
# Force bundled popt
sed -i 's/\[with_included_popt="auto"\]/[with_included_popt="yes"]/' configure.ac

# Remove the now-redundant inner detection block (adjust line numbers if needed)
# Check current line numbers with: grep -n 'with_included_popt' configure.ac
sed -i '359,371d' configure.ac   # Remove inner if block
sed -i '359d' configure.ac       # Remove orphaned fi
sed -i '371,376d' configure.ac   # Remove orphaned else...fi
```

**Implication:** The system `libpopt` library is not used even if present. Pilot-link builds and statically links its own bundled popt. This has no practical impact on functionality.

#### Regenerate and build

```bash
autoreconf -fi
./configure --prefix=/usr/local
make
sudo make install
sudo /sbin/ldconfig
```

#### Package with checkinstall

```bash
sudo rm -f /usr/local/share/aclocal/pilot-link.m4 \
           /usr/local/lib/pkgconfig/pilot-link.pc \
           /usr/local/lib/pkgconfig/pilot-link-pp.pc \
           /usr/local/include/pi-*.h \
           /usr/local/share/pilot-link/udev/60-libpisock.rules

sudo checkinstall --pkgname=pilot-link --pkgversion=0.12.5 --pkgrelease=1 \
  --pkglicense=GPL --pkggroup=libs \
  --pkgsource="https://github.com/jichu4n/pilot-link" \
  --nodoc \
  make install
```

When prompted, clear the Requires field (field 10) and set your maintainer details.

---

### 3. Build J-Pilot

```bash
cd ~
git clone https://github.com/juddmon/jpilot.git
cd jpilot
autoreconf -fi
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./configure --prefix=/usr/local
make
```

#### Package with checkinstall

```bash
sudo make uninstall   # Clear any previous manual install
sudo rm -rf /usr/local/share/doc/jpilot /usr/local/share/jpilot
sudo mkdir -p /usr/local/share/doc/jpilot/icons \
              /usr/local/share/doc/jpilot/manual \
              /usr/local/share/jpilot

sudo checkinstall --pkgname=jpilot --pkgversion=2.0.2 --pkgrelease=1 \
  --pkglicense=GPL --pkggroup=utils \
  --pkgsource="https://github.com/juddmon/jpilot" \
  --nodoc \
  make install
```

When prompted, set field 10 (Requires) to:

```
libgtk-3-0,libgdk-pixbuf-2.0-0,libglib2.0-0,libsqlite3-0,libgcrypt20
```

**Important:** the correct package name is `libgdk-pixbuf-2.0-0` with a hyphen before `2`, not `libgdk-pixbuf2.0-0` with a dot. Using the dot form will cause `dpkg` to fail with a dependency error on installation.

Also set field 0 (Maintainer) to your name and email, then press Enter to build.

---

## USB HotSync setup

Both Sony Clié and Handspring Visor devices use USB-serial adapters and will appear as `/dev/ttyUSB0`. Add your user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
```

Log out and back in. In J-Pilot preferences, set the port to `/dev/ttyUSB0`.

---

## Summary of configure.ac changes to pilot-link

| Change | Reason | Impact |
|--------|--------|--------|
| Removed BlueZ detection block | `AC_BLUEZ` macro does not exist; generates broken shell code | Bluetooth HotSync disabled |
| Added empty `BLUEZ_CFLAGS` and `BLUEZ_LIBS` substitutions | Makefile references these variables; without substitution the linker receives literal `@BLUEZ_LIBS@` as a filename | Required for build to succeed |
| Forced bundled popt (`with_included_popt="yes"`) | Nested `AC_CHECK_*` macros generate broken shell under Debian 12 autoconf | Bundled popt used instead of system popt; no functional difference |
| Removed inner popt detection block and orphaned `fi`/`else` | Left over after forcing bundled popt; caused shell syntax errors | None |

---

## Notes for upstream

The `configure.ac` patches described above represent minimal fixes to make the source build on modern autoconf (2.71+) and automake (1.16+) as shipped with Debian 12. The ideal upstream fix would be to modernise `configure.ac` throughout — replacing deprecated macros (`AC_TRY_LINK`, `AM_PROG_MKDIR_P`, etc.), writing a proper `AC_BLUEZ` macro or replacing it with a standard `PKG_CHECK_MODULES` call, and updating the popt detection to avoid deeply nested `AC_CHECK_*` calls.
