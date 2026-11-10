# Screen lock

`tiny-lock` covers the screen and asks for the password of the user. The
password is the password of the system: there is no password of tiny-shell to
set and to forget.

## Locking the screen

- **Super + L** or the item **Lock Screen** of the [main menu](usage.md#the-main-menu).
- `tiny-lock` from a terminal, a script or a launcher.
- **After some time without input**, when `[lock] idle_after` is set (see
  [the section below](#lock-when-idle)).
- `[commands] lock` in the [configuration](configuration.md#the-lock) says
  another program (`slock`, `i3lock`, `xsecurelock`), and then the key and the
  menu start that one.

The lock puts a black window over all the monitors and takes the keyboard and
the pointer. When a monitor is connected or the layout changes while it is
locked (RandR), the window covers the new screen at once and the dialog moves to
the middle of the primary monitor. The sizes follow `[theme] scale` and the font. It has three states:

1. **The time.** The time in big letters, under it the date in smaller ones (the
   format of the clock of the panel, `[panel] clock_format`, without the time of
   the day), and in the place of the dialog the text "Click or press a key to
   unlock".
2. **The dialog.** A click or a key (it is not typed) opens it: the whole name
   of the user (the first part of the GECOS field of the passwd entry, the login
   name when there is none), what is asked, and the entry where the typed
   characters are shown as bullets. **The check of the password starts only now**,
   so a fingerprint or a face (Howdy) is looked for when somebody is there. Return
   sends the answer. A wrong password says so and asks again. Escape closes the
   dialog, stops the check and forgets what was typed: back to the time. While the
   dialog is open the screen does not go black.
3. **Black.** After `[lock] blank_after` without input (1, 5, 10 or 15 minutes, or
   `never`; the default is 1; it is Blank after in the section Power of
   `tiny-settings`) the first state turns to an entirely black screen with no text
   at all. Any input (a key, a click, a move of the pointer) shows the time
   again; a key or a click that wakes the screen is not used for anything else.
   The time starts again after Escape.

The screen opens only when the check says the password is right, never because
of an error.

If the lock dies (a crash, a kill), tiny-shell starts it again, up to five times
in a row. A restart of tiny-shell itself does not unlock the screen, because the
lock is a program of its own.

## Lock when idle

`[lock] idle_after = 10` makes tiny-shell lock the screen after 10 minutes
without a key or a move of the pointer (5, 10, 15, 30 and 60 minutes, or
`never`, which is the default). It is the section Power of `tiny-settings`,
Lock after. The time is the idle time of the X server (the counter `IDLETIME`
of the SYNC extension, which every Xorg has), so it counts all input, in any
program, and a video that runs full screen without input is locked too: there
is no way to tell it to the shell yet. A change is read when tiny-shell starts and when it
is told to read the settings again (`tiny-shell --reload`). If the X server has
no such counter, the shell says so and does not lock.

The shell only looks at the idle time when the time that is left is over, so an
unused computer is not woken every few seconds. A lock that is already shown is
not started again.

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
| NetBSD, FreeBSD | OpenPAM in the base system, but `pam_unix` reads the hash itself and an ordinary user can't, so PAM refuses the right password (tried on NetBSD); there is no `unix_chkpwd` | `make install-setuid` and `PAM=no`; `tiny-lock-pwcheck` checks the password (tried on NetBSD and FreeBSD) |
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
`[lock]` (`blank_after`, `idle_after`, `auth`, `pam_service`, `checker`, `pwcheck`).

PAM is asked for the service `tiny-lock`. No file is needed for it: a service
without a file uses `other`, and on Debian, Alpine and NetBSD that is the login
password. Where `other` denies everything (not tried on Fedora and Arch), add
the file `/etc/pam.d/tiny-lock` (on Debian `@include common-auth`, on Fedora
and Arch `auth include system-auth`), or set `pam_service = login`.

## What it does not do

- It does not stop a switch to a text console (Ctrl+Alt+F1) or Ctrl+Alt+Backspace
  where the X server allows them. No program that runs inside X can.
- It does not lock before the computer sleeps and does not let another user log
  in from the lock screen. These need logind (`TODO-LOCK.md` has the plan).
- A program of the same user can start `tiny-lock-pwcheck` or `unix_chkpwd` and
  try passwords; only the wait after a wrong one limits it. It is the same with
  every locker.
