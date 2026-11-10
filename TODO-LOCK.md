# TODO-LOCK

What is left of the screen lock. What is done (the four programs, the rules, the
protocol) is described in
[doc/developers/screen-lock.md](doc/developers/screen-lock.md) and for users in
[doc/lock.md](doc/lock.md). The short items with priorities go to todo.txt when
the work is accepted; this file keeps the reasons and the details.

## Not done

- **logind** (after the D-Bus base, see TODO-DBUS.md): the signals `Lock` and
  `Unlock` of the session (the user ran `loginctl lock-session`), `LockSession`,
  `SetLockedHint`, and `PrepareForSleep` with a delay inhibitor, so that the
  screen is locked before the computer sleeps. The shell starts `tiny-lock`, as
  it does for Super + L.
- **Switch the user** from the lock screen, as GNOME does: a command in
  `[lock] switch_user_command` (GDM: `CreateTransientDisplay` and
  `ActivateSessionOnSeat` of logind; LightDM: `dm-tool switch-to-greeter`; SDDM
  not checked; XDM has nothing). It is shown only when it is set.
- **OpenBSD:** no PAM; `tiny-lock-pwcheck` would use `auth_userokay` of
  `bsd_auth` and a set-group-ID `auth` binary. Not tried: no OpenBSD here.
- **PAM files of the distributions:** a service without a file uses `other`. That
  is the login password on Debian and Alpine (NetBSD and FreeBSD do not use PAM
  without the right to read the hash, see below); Fedora and Arch are not
  checked (`other` may deny everything there). The files for `/etc/pam.d`
  (`@include common-auth` on Debian, `system-auth` on Fedora and Arch) can be
  installed by `make install` when it is known they are right.
- **A change of the monitors while locked:** the window has the size of the screen
  at the start; a connected monitor shows the desktop. It should follow RandR.
- **The gap after a crash of `tiny-lock`:** between the crash and the new start
  the screen is open. A window of the lock that is kept by the X server (a
  second client that holds the screen) would close it; an idea only.
- **Systems that were not checked:** Void, Buildroot, Fedora, Arch, ARM (no
  emulation in Docker here), Raspberry Pi OS and Armbian (the same packages as
  Debian, so `tiny-lock-auth` should work the same).
- **PAM on NetBSD and FreeBSD:** `auto` leaves PAM out there, because an ordinary
  user can't read `master.passwd` and there is no `unix_chkpwd`. So only the
  password works; PAM modules for a camera or a fingerprint (which are for Linux
  anyway) can't be used. A privileged PAM host (the set bit on `tiny-lock-auth`)
  would change it, but it puts PAM modules into a program with root, so it was
  not built. Decide only if somebody needs it.
