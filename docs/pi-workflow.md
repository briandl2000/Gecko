# Raspberry Pi / aarch64 Workflow

Complete guide for developing Gecko against a Raspberry Pi (or any aarch64
Linux target) from an x86_64 Linux workstation.

This is the **host-side workflow only**. The Pi itself is not a build machine;
it only runs binaries.

---

## How the flow works

```
┌─────────────────────────┐       ┌──────────────────────────┐
│ Your x86_64 workstation │       │ Raspberry Pi (aarch64)   │
│                         │       │                          │
│  gk-pi build            │       │                          │
│   ↓ (docker)            │       │                          │
│  cross-compile to       │       │                          │
│  out/Linux-aarch64/     │       │                          │
│                         │       │                          │
│  gk-pi deploy  ─────────┼──rsync→  ~/gecko/                │
│                         │       │                          │
│  gk-pi run <exe>  ──────┼──ssh──→  ./exe                   │
│  gk-pi gdbserver <exe> ─┼──ssh──→  gdbserver :2345 ./exe   │
│       ↑                 │       │                          │
│       └── VS Code F5 ───┼───────┼── remote debug session   │
└─────────────────────────┘       └──────────────────────────┘
```

No code leaves the x86_64 workstation. The Pi never needs a compiler, CMake,
or any part of the Gecko source tree.

---

## One-time setup

### 1. Docker (x86_64 workstation)

You need Docker. No QEMU, no `binfmt_misc`, no multi-arch buildx — this flow
runs the compiler natively on your host.

```bash
# Arch
sudo pacman -S docker
sudo systemctl enable --now docker
sudo usermod -aG docker "$USER"    # log out / back in to apply

# Debian / Ubuntu
sudo apt-get install docker.io
sudo systemctl enable --now docker
sudo usermod -aG docker "$USER"
```

### 2. Raspberry Pi side

Install only the runtime libraries Gecko links against. Nothing else:

```bash
# On the Pi
sudo apt-get update
sudo apt-get install -y \
    libvulkan1 mesa-vulkan-drivers \
    libx11-6 libxext6 libxrandr2 \
    libwayland-client0 libxkbcommon0 \
    gdbserver                         # only if you want remote debugging
```

Also confirm you can SSH in password-less (or with an SSH agent) —
`ssh pi@raspberrypi.local 'uname -a'` should Just Work. If it doesn't, fix
SSH first; every `gk-pi` verb that touches the Pi uses plain `ssh`/`rsync`.

### 3. Repo config

Copy the template and fill in your Pi's details:

```bash
cd /path/to/Gecko
cp .gecko-pi.env.example .gecko-pi.env
$EDITOR .gecko-pi.env
```

Contents:

```bash
GECKO_PI_HOST="pi@raspberrypi.local"    # user@host or ~/.ssh/config alias
GECKO_PI_DIR="~/gecko"                  # remote dir for binaries (tilde OK)
GECKO_PI_GDB_PORT="2345"                # gdbserver port
```

`.gecko-pi.env` is gitignored.

### 4. Load the helper in your shell

```bash
source scripts/pi-dev.sh
```

First time, you'll see:

```
[gk-pi] loaded. Host: pi@raspberrypi.local  Path: ~/gecko. Try 'gk-pi help'.
```

If you use the flow often, drop the `source` line into your shell rc.

---

## Daily usage

Every verb is invoked as `gk-pi <verb> ...`. Under the hood:

- **build / test / format / clean / any `gk` verb** → runs inside the
  aarch64 container via `python3 scripts/cli.py <verb> ...`. Writes aarch64
  artefacts to `out/Linux-aarch64/bin/<Config>/`.
- **deploy / run / gdbserver / fetch-logs** → plain `ssh` + `rsync`, no
  container involved.
- **shell** → drops you into a bash prompt inside the container.
- **rebuild-image** → wipe the cached image and rebuild it.

### Build

```bash
gk-pi build debug        # → out/Linux-aarch64/bin/Debug/
gk-pi build release      # → out/Linux-aarch64/bin/Release/
```

First `build` pulls `ubuntu:25.04` and sets up the cross-toolchain
(~2 min). After that, configure + full build is as fast as your host.

### Test

Unit tests are aarch64 ELF binaries — they will not run on your x86_64 host
directly. Two ways to exercise them:

```bash
# Deploy tests to the Pi and run them there
gk-pi deploy debug
ssh "$GECKO_PI_HOST" 'cd ~/gecko/tests && ./core_tests'

# Or just validate they build and link cleanly (fast)
gk-pi build debug                   # full build includes the test targets
```

(Adding `gk-pi test` that ssh-runs the test suite on the Pi is a one-liner —
ask and it'll be wired up.)

### Deploy binaries to the Pi

```bash
gk-pi deploy debug
```

This rsyncs `out/Linux-aarch64/bin/Debug/` → `$GECKO_PI_HOST:$GECKO_PI_DIR/`
and also syncs repo-local `working_dir/` (log/trace output location).

### Run on the Pi

```bash
gk-pi run graphics_example debug
gk-pi run platform_example release
```

Equivalent to `gk-pi deploy && ssh pi "cd ~/gecko/working_dir && LD_LIBRARY_PATH=~/gecko ~/gecko/<exe>"`.

### Remote-debug with VS Code

1. Start gdbserver on the Pi:

   ```bash
   gk-pi gdbserver graphics_example debug
   ```

   Leaves a gdbserver waiting on `:2345`.

2. In VS Code, hit **F5** and pick **Pi: Remote debug**. That launch config
   (in [.vscode/launch.json](../.vscode/launch.json)) connects `gdb-multiarch`
   to `:2345`, uses the host-side debug symbols in
   `out/Linux-aarch64/bin/Debug/`, and maps `/workspace` → repo root.

   (You need `gdb-multiarch` installed on the x86_64 side:
   `sudo pacman -S gdb-multiarch` / `sudo apt-get install gdb-multiarch`.)

### Fetch logs / traces back

```bash
gk-pi fetch-logs
```

Pulls `$GECKO_PI_DIR/working_dir/` back to local `working_dir/`. Open
`working_dir/gecko_trace.json` with `chrome://tracing/` or Perfetto.

### Shell inside the container

```bash
gk-pi shell
# now inside ubuntu:25.04 with the cross-toolchain on PATH
$ aarch64-linux-gnu-gcc --version
$ python3 scripts/cli.py build debug
```

Useful when you're diagnosing a weird CMake/toolchain issue without having
`gk-pi` wrap the call.

### Rebuild the container image

```bash
gk-pi rebuild-image
```

Run this if you change `docker/aarch64.Dockerfile` or apt packages in the
image.

---

## VS Code integration

Two sets of tasks are preconfigured in [.vscode/tasks.json](../.vscode/tasks.json):

| Task label              | Runs                           |
|-------------------------|--------------------------------|
| `gk: build debug`       | local x86_64 debug build       |
| `gk: test debug`        | local x86_64 tests             |
| `gk-pi: build debug`    | aarch64 debug build in docker  |
| `gk-pi: deploy debug`   | rsync to the Pi                |
| `gk-pi: run example`    | prompt for exe, run on the Pi  |
| `gk-pi: gdbserver`      | start remote gdbserver         |

Launch configs ([.vscode/launch.json](../.vscode/launch.json)):

- **Local: Debug example** — runs a local x86_64 binary under gdb.
- **Pi: Remote debug** — attaches gdb-multiarch to `:2345` on the Pi.

---

## Where files live

```
Host (x86_64)                                     Container                    Pi (aarch64)
─────────────                                     ─────────                    ────────────
<repo>/                                            /workspace/                  $GECKO_PI_DIR
├── src/                                           ├── src/                     ├── graphics_example   ← rsynced
├── docker/                                        ├── docker/                  ├── libCoreServices.so ← rsynced
│   ├── aarch64.Dockerfile                         │   ├── aarch64.Dockerfile   ├── tests/             ← rsynced
│   └── aarch64-toolchain.cmake                    │   └── aarch64-toolchain.cmake
├── scripts/pi-dev.sh                              └── out/                     └── working_dir/       ← rsynced
├── .gecko-pi.env                                      └── build/                   ├── log.txt
└── out/                                               └── Linux-aarch64/           └── gecko_trace.json
    ├── build/Linux-aarch64/   ← cmake cache
    └── Linux-aarch64/bin/
        ├── Debug/             ← aarch64 ELF
        └── Release/
```

Artefacts on the host are owned by **your user** (the container runs with
`--user $(id -u):$(id -g)`). Nothing in `out/Linux-aarch64/` needs sudo to
clean.

---

## Troubleshooting

**"Cannot connect to the Docker daemon"**
You're not in the `docker` group, or the daemon isn't running.
`sudo systemctl start docker`, log out/back in after `usermod -aG docker`.

**"Permission denied" trying to `rm -rf out/Linux-aarch64`**
Old artefacts from before this workflow switched to `--user`. One-time fix:
`sudo rm -rf out/build/Linux-aarch64 out/Linux-aarch64`. New builds are
owned by you.

**`gk-pi run` hangs / "Permission denied (publickey)"**
SSH is the transport. Fix `ssh "$GECKO_PI_HOST" echo ok` first. Use an
ssh-agent or `~/.ssh/config` IdentityFile.

**Image build fails at apt stage**
Check internet, check `docker/aarch64.Dockerfile` comments, then try
`gk-pi rebuild-image`. The Dockerfile targets Ubuntu 25.04 because that's
where `gcc-15-aarch64-linux-gnu` lives in the main repos (no PPA needed).

**CMake says "compiler is broken" / "Exec format error"**
You have a stale `out/build/Linux-aarch64/` from an older
QEMU-based flow. `rm -rf out/build/Linux-aarch64 out/Linux-aarch64` and
rebuild.

**Binaries segfault on the Pi, but not under gdbserver**
Library mismatch. The container uses Ubuntu 25.04 arm64 runtime libs; your
Pi OS is usually Raspberry Pi OS (Debian bookworm/trixie). Check:

```bash
ssh "$GECKO_PI_HOST" 'ldd ~/gecko/graphics_example | grep "not found"'
```

Install any missing `libXXX` packages from Pi apt. Gecko only links
against widely-available runtime libs, so this should be rare.

---

## CI parity

[.github/workflows/ci.yml](../.github/workflows/ci.yml) has a `build-aarch64`
job that runs exactly the same recipe (`ubuntu:25.04` + aarch64 cross
toolchain) on `ubuntu-latest`, so a red CI means your local Pi build is
broken too. No special ARM runner required.

---

## Further reading

- [docs/build.md](build.md) — overall build system, local x86_64 flow.
- [docker/aarch64.Dockerfile](../docker/aarch64.Dockerfile) — the image recipe.
- [docker/aarch64-toolchain.cmake](../docker/aarch64-toolchain.cmake) — the
  CMake toolchain file.
- [scripts/pi-dev.sh](../scripts/pi-dev.sh) — every `gk-pi` verb is defined
  here.
