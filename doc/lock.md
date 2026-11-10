# Screen lock

`tiny-lock` covers the screen and asks for the password of the user. The
password is the password of the system: there is no password of tiny-shell to
set and to forget.

## Locking the screen

- **Super + L** or the item **Lock Screen** of the [main menu](usage.md#the-main-menu).
- `tiny-lock` from a terminal, a script or a launcher.
- `[commands] lock` in the [configuration](configuration.md#the-lock) says
  another program (`slock`, `i3lock`, `xsecurelock`), and then the key and the
  menu start that one.

The lock puts a black window over all the monitors and takes the keyboard and
the pointer. In the middle of the first monitor is the time in big letters, and
under it the dialog: the whole name of the user (the first part of the GECOS
field of the passwd entry, the login name when there is none), what is asked,
and the entry where the typed characters are shown as bullets. The sizes follow
`[theme] scale` and the font. Return sends the answer, Escape forgets what
was typed. A wrong password says so and asks again. The screen opens only when
the check says the password is right, never because of an error.

If the lock dies (a crash, a kill), tiny-shell starts it again, up to five times
in a row. A restart of tiny-shell itself does not unlock the screen, because the
lock is a program of its own.

## How the password is checked

The question does not come from `tiny-lock`: it comes from the system. The
checking is done by `tiny-lock-auth`, which uses the first one of these that
fits (`[lock] auth = auto`):

1. **PAM.** Whatever the system has set up for the login works: a password, a
   fingerprint (`fprintd`), a face (Howdy), a token. The questions of PAM appear
   in the dialog, so a code that is asked besides the password works too. It is
   used where PAM can work without the right to read the passwords, which is
   Linux (`unix_chkpwd` does that), and in a build with PAM (see below).
2. **`unix_chkpwd`**, the helper of Linux-PAM, when this build has no PAM.
3. **`tiny-lock-pwcheck`**, a small program of tiny-shell that is the only one
   that has special rights (see below). It is for the systems without PAM and
   without `unix_chkpwd`, and for the BSD systems.

If there is no way to check the password, `tiny-lock` does not lock the screen
and says why: a screen that can't be unlocked is worse than an open one. Try it
before you rely on it:

```sh
tiny-lock-auth --probe      # prints how the password would be checked
tiny-lock --probe           # the same check that tiny-lock makes before it locks
```

### Where PAM is

| System | PAM | What to do |
| ------ | --- | ---------- |
| Debian, Ubuntu, Raspberry Pi OS, Armbian | there (`login` needs it) | install `libpam0g-dev` to build with it; nothing more |
| Fedora, Arch | there | install the development package (`pam-devel`, `pam`); not tried by us |
| Alpine | `linux-pam` is a package, a plain install has none | `apk add linux-pam linux-pam-dev` and build with it, or use `tiny-lock-pwcheck` |
| NetBSD, FreeBSD | OpenPAM in the base system, but an ordinary user can't read the hashes of the passwords | `make install-setuid` (see below) |
| OpenBSD | no PAM (it has `bsd_auth`) | not supported yet |

`make` finds PAM by itself (`PAM=auto`); `make PAM=no` builds without it and
`make PAM=yes` insists on it.

### tiny-lock-pwcheck and its rights

Only the hash of the password needs a right: Linux keeps it in `/etc/shadow`
(readable by the group `shadow`), the BSD systems in `master.passwd` (readable
by root). `tiny-lock-pwcheck` is the only program of tiny-shell that has it:

```sh
sudo make install-setuid    # Linux: group shadow and the set-group-ID bit,
                            # BSD: owner root and the set-user-ID bit
```

`make install` does not set any special bit. `tiny-lock-pwcheck` checks only
the user that runs it (never a name from its arguments), reads the hash, gives
the rights up, compares the password with `crypt()` and answers with its exit
status. It never writes the password or the hash anywhere. After a wrong
password it waits two seconds, like PAM does.

An account that has no password or a locked one is refused: the lock does not
open for an empty password.

## Configuration

See [The lock](configuration.md#the-lock): `[commands] lock` and the section
`[lock]` (`auth`, `pam_service`, `checker`, `pwcheck`).

PAM is asked for the service `tiny-lock`. No file is needed for it: a service
without a file uses `other`, and on Debian, Alpine and NetBSD that is the login
password. Where `other` denies everything (not tried on Fedora and Arch), add
the file `/etc/pam.d/tiny-lock` (on Debian `@include common-auth`, on Fedora
and Arch `auth include system-auth`), or set `pam_service = login`.

## What it does not do

- It does not stop a switch to a text console (Ctrl+Alt+F1) or Ctrl+Alt+Backspace
  where the X server allows them. No program that runs inside X can.
- It does not follow a change of the monitors while it is locked.
- It does not lock before the computer sleeps, does not lock after some idle time
  and does not let another user log in from the lock screen. These need logind
  (`TODO-LOCK.md` has the plan).
- A program of the same user can start `tiny-lock-pwcheck` or `unix_chkpwd` and
  try passwords; only the wait after a wrong one limits it. It is the same with
  every locker.
