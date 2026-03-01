# portctrl

`portctrl` is a CLI tool for inspecting and resolving local TCP port conflicts safely.
It helps you find what is listening, inspect a single port, and free a port.

## Project Status

This project is a **work in progress**.
It is usable for core workflows, and more features are planned.

Current platform support:
- macOS
- Ubuntu / Linux

Planned:
- Windows support

## Setup

### Prerequisites
- C++ compiler with C++20 support (`clang++` or `g++`)
- `make`
- `lsof` in `PATH`

Check with:
```bash
command -v lsof
```
If this prints a path (example: `/usr/sbin/lsof`), you are set.

### macOS
```bash
xcode-select --install
command -v lsof
```

### Ubuntu
```bash
sudo apt update
sudo apt install -y build-essential lsof
command -v lsof
```

## Build and Run

From the repository root:
```bash
make clean && make
./bin/portctrl
```

## Install

Install globally (default target: `/usr/local/bin`):
```bash
make install
```
Depending on your system permissions, you may need:
```bash
sudo make install
```

Install to a user-local prefix:
```bash
make install PREFIX=$HOME/.local
```

Uninstall:
```bash
make uninstall
```

## Commands

### `who <port>`
Shows which process(es) are listening on one TCP port.
```bash
./bin/portctrl who 3000
```
Output is shown in a table for easier scanning.

### `list`
Lists listening TCP endpoints.
```bash
./bin/portctrl list
```
Output is shown in a table for easier scanning.
Note: one PID can appear on multiple rows if one process listens on multiple ports/endpoints.

### `free <port> [--apply] [--force] [--yes] [--signal INT|TERM]`
Safely frees a busy port.

Behavior:
- default is dry-run (no signal sent)
- `--apply` sends a graceful signal (`SIGTERM` by default) to all listener PIDs on that port
- `--signal INT` switches graceful signal to `SIGINT`
- if still busy, `--force` (with `--apply`) can escalate to `SIGKILL` for remaining listener PIDs
- in non-interactive mode, destructive actions require `--yes`
- listener sets in `free` are displayed in tables

Examples:
```bash
# Dry-run (safe preview)
./bin/portctrl free 3000

# Graceful stop
./bin/portctrl free 3000 --apply

# Graceful stop with SIGINT
./bin/portctrl free 3000 --apply --signal INT

# Escalate to SIGKILL if graceful stop fails
./bin/portctrl free 3000 --apply --force

# Non-interactive usage
./bin/portctrl free 3000 --apply --yes
```

## Validation Rules

- Valid ports are numeric `1` to `65535`.
- `--force` is only valid with `--apply`.
- Invalid command options return an error and usage output.

## Troubleshooting

- `command not found: lsof`
  - Install `lsof` and verify with `command -v lsof`.
- `./bin/portctrl: no such file or directory`
  - Build first with `make clean && make`.
- `kill(...) failed: Operation not permitted`
  - Target process is likely protected or owned by another user.

## Roadmap

- Better project-aware suggestions (prefer changing app port when safer)
- Improved conflict workflows
- History/audit features
- Test coverage and packaging improvements
- Windows support

## Contributing

Contributions are welcome.
If you are interested, feel free to open an issue or submit a pull request.

## License

MIT
