# The screen lock

How the lock is built and why. The use of it is described in
[Screen lock](../lock.md).

The password is the password of the system, so the lock has to ask the system.
Four programs do it, and each has only what it needs:

```
tiny-shell ──spawn──► tiny-lock ──pipe──► tiny-lock-auth ──stdin──► tiny-lock-pwcheck
 (a request only)     (the window,         (PAM, or a checker        (the only one with
                       the grabs, Entry)    that is configured)       privileges)
```

| Program | Source | Logic | Rights | Links |
| ------- | ------ | ----- | ------ | ----- |
| `tiny-shell` | `src/main.cc` | `lib/wm/lock_guard.*`, the action `lock` in `lib/wm/manager.cc` | none | X |
| `tiny-lock` | `src/lock.cc` | `lib/app/lock_screen.*`, `lib/app/lock_session.*` | none | X, no libpam |
| `tiny-lock-auth` | `src/lock_auth.cc` | `lib/app/lock_auth.*`, `lib/app/lock_pam.*`, `lib/app/lock_protocol.*`, `lib/app/lock_check.*` | none | libpam (optional) |
| `tiny-lock-pwcheck` | `src/lock_pwcheck.cc` | `lib/app/pwcheck.*`, `lib/app/lock_check.*` | set-group-ID `shadow` or set-user-ID root | libcrypt |

1. **tiny-shell never sees a password.** It only asks for the lock (Super + L,
   the main menu, the action `lock`) and watches it: `LockGuard` starts it again
   when it ends by a signal or with another status than 0 (unlocked) or 1 (it
   could not lock), up to five times in a row. `spawn.h` keeps the exit status of
   one watched child, because `reap_children()` collects all of them.
2. **tiny-lock** is a program of its own, so a crash or a restart of the shell
   does not unlock the screen: the window and the grabs belong to its connection
   to the X server. It does not link libpam, so a bug of a PAM module cannot reach
   the process that holds the grabs, and the process stays small. The window is
   an `override_redirect` window over the whole root window (every monitor); the
   keyboard is grabbed to the lock window (a key that has not come to the entry
   is given to it), so the typing can't reach another client, and the pointer is
   confined to the lock window with no cursor. The screen has three states
   (`LockScreen::State`): `Idle` (the time, the date and a hint), `Dialog` (the
   entry; **`tiny-lock-auth` is started only here and stopped when the dialog
   closes**, so biometrics run when somebody is there) and `Black` (no window
   mapped; after `[lock] blank_after` in `Idle`, woken by any input, which is
   not used for anything else). `handle_input()` decides it for every key and
   pointer event. It rises when something covers it. If a grab fails (another client
   holds one) it tries for two seconds and then exits with 1.
3. **tiny-lock-auth** runs one try and ends. It chooses the way (`resolve_backend`:
   PAM where it can work without the hash, then the checker, then
   `tiny-lock-pwcheck` if it has the bits) and asks through the protocol below.
4. **tiny-lock-pwcheck** is the only privileged code, kept to some hundreds of
   lines: the user is `getuid()` and never an argument, the password comes on the
   standard input, no environment is used, the only output is the exit status
   (0 right, 1 wrong, 2 can't be checked). It reads the hash, drops the rights
   for good (and verifies it), switches off core dumps, and wipes the password
   and the hash. A wrong password waits two seconds.

## The protocol of tiny-lock and tiny-lock-auth

One line each way, text without a newline (`lib/app/lock_protocol.h`):

```
auth -> lock   PROMPT_HIDDEN Password:     ask, the answer is not shown
               PROMPT_VISIBLE Code:        ask, the answer is shown
               INFO text / ERROR text      tell
               RESULT OK | WRONG | FAILED text
lock -> auth   ANSWER text | CANCEL
```

There can be more questions in one try (a fingerprint, then a password; a code),
because PAM asks them. A result that is not understood is a failure: a damaged
line never unlocks. After `WRONG`, `tiny-lock` starts a new try at once (the check
has waited already); after `FAILED` or an end without a result it waits a second
so that it can't spin. A line longer than 4 kB ends the program.

## Rules

- **Fail closed.** With no way to check the password `tiny-lock` does not start
  (`tiny-lock --probe`), because a lock that can't be unlocked is worse than none.
- **No special bits from `make install`.** The mode is set by `make install-setuid`
  or by the packager.
- **The lock must survive its own crash:** see `LockGuard`. The gap is not closed
  completely (between the crash and the new start the screen is open).
- **Passwords in memory.** The entry overwrites the text (`Entry::clear()`, the
  destructor, `TextField::wipe()`); the buffers of the other programs do the same
  (`wipe()`). A password is never put in a command line, in the environment or in
  a log. The checkers are started with an empty environment.
- **Everything that can be tested without a display is in `lib/`** and has tests
  (`tests/app/test_lock_*.cc`, `test_pwcheck.cc`, `tests/wm/test_lock_guard.cc`).
  The tests use scripts as the programs that check, so the PAM and the rights are
  the only parts they can't run everywhere: the privileged path is tried by hand
  (see below).

## Checked by hand

Where the tests can't go (they run as an ordinary user), `tiny-lock-pwcheck` was
tried as a user with a known password and the group bit set: Debian stable and
Alpine (right, wrong and empty password, `--probe`, an argument, a locked
account, and without the bit). PAM through `tiny-lock-auth` was tried on Debian.
NetBSD: it builds and says "can't check" (2) without the bit; the set-user-ID
case is not tried.

## What is left

The plan is in `TODO-LOCK.md`: logind (`Lock`, `PrepareForSleep` with an
inhibitor, `SetLockedHint`), the idle time, a command to switch the user, OpenBSD
(`auth_userokay`), the change of the monitors, and the files of `/etc/pam.d` for
the distributions.
