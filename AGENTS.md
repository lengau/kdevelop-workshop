# Developer & Agent Guide: KDevelop Workshop Plugin

This guide documents the context, development rules, building, testing, packaging, and continuous integration for the `kdevelop-workshop` project. Future agents and human developers should follow these guidelines strictly.

---

## 1. Project Context & Rules

- **Repository**: [kdevelop-workshop](https://github.com/lengau/kdevelop-workshop)
- **YAML Extension Rule**: All YAML configuration files (including GitHub workflows) **must** use the `.yaml` extension (e.g., `.github/workflows/package.yaml`). Do not use `.yml`.
- **License**: GPL-3.0.
- **Maintainer**: Alex M. Lowe <alex@lowe.dev>

---

## 2. Packaging & Dependencies (`debian/`)

The plugin is packaged using standard Debian/Ubuntu conventions under the `debian/` directory:

- **Rules (`debian/rules`)**:
  - Uses the `kf6` sequence and buildsystem: `dh $@ --with kf6 --buildsystem=kf6`.
- **Control (`debian/control`)**:
  - **Build-Depends**: Includes `pkg-kde-tools`, `extra-cmake-modules`, and other KF6/KDevelop development packages.
  - **Depends**: Has a versioned constraint of `kdevelop (>= 4:24.08)`.
- **Changelog (`debian/changelog`)**:
  - Targets the Ubuntu 26.10 release (`stonking` / `unstable`).

---

## 3. Practices & Processes

### Practices

- **Backward Compatibility**: KDevelop and KF6 API compatibility is a **hard requirement**. Changes that alter behaviors, configuration, APIs, defaults, or validation rules must be carefully designed to preserve functionality for current users.
- **Smallest Safe Change**: Make the smallest safe change necessary to resolve any issue. Avoid unrelated bug fixes, opportunistic cleanup, and refactoring unless explicitly required. The right amount of complexity is the minimum needed for the current task.
- **Inspect, Don't Speculate**: Never speculate about code or behavior you have not directly inspected.
- **Existing Conventions**: Follow the project's existing conventions regarding style, logging, comments, and testing.
- **Documentation Integrity**: Maintain documentation integrity. Preserve all existing comments and docstrings that are unrelated to your code changes, unless specified otherwise. Comments should explain complex business logic, non-obvious algorithms, or "gotchas". They should be brief, explaining "why" rather than "how", and be helpful for future maintainers.
- **KDE HIG & Design**: Adhere strictly to the KDevelop and KDE Human Interface Guidelines (HIG) and design patterns for user interface and interaction design.
- **Library Reuse**: Prioritize reusing existing KF6 and Qt6 libraries/components (such as standard KDE widgets, dialogs, and models) instead of writing custom equivalents.

### KDevelop Coding Style

When writing C++ code for the KDevelop plugin, adhere to the upstream KDevelop `.clang-format` configuration, which is based on WebKit but with specific deviations:

- **Indentation**: 4 spaces.
- **Line Length**: 120 columns maximum.
- **Braces**: Placed on a new line after classes, structs, and functions. However, for control statements (`if`, `while`, `for`), they stay on the same line.
- **Pointers**: Pointer asterisks are aligned to the left (`Type* name;`).
- **Namespaces**: No indentation inside namespace blocks.
- **Templates**: No space after the template keyword (`template<typename T>`).
- **Single Line Blocks**: Single line functions and enums are explicitly disallowed.
- **Comments**: Be mindful not to enforce a mandatory space after `//` comments if editing legacy code, as this is historically disabled in KDevelop to prevent large formatting diffs.

### Processes

- **Branch Hygiene**: Do active work on a dedicated branch created from the current `origin/main`
  state. Work branches must start with `work/`. Keep `main` clean, rebase the work branch onto
  `origin/main` before opening or updating a PR, and delete the branch locally and remotely after
  merge. If you need to switch back to `main` during a task, stash or commit the work first so
  uncommitted changes do not carry over onto the new branch.
- **Conventional Commits**: Commit messages must follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) and be no longer than 80 characters. Use the following types:
  - `ci`, `build`, `feat`, `fix`, `perf`, `refactor`, `style`, `test`, `docs`, `chore`
- **Verification**: Always verify building, formatting, and installation tests locally or check GitHub Actions run results before concluding a task.

---

## 4. Upstream KDE & KDevelop Guidelines

If the time comes to upstream your work to the main KDevelop codebase, follow the official community guidelines:

### General KDE Developer Documentation

The official [KDE Developer Documentation](https://develop.kde.org/docs/) contains the full suite of resources. You should consult these references when building apps or plugins:

- **Design & HIG**: Adhere to the Human Interface Guidelines (HIG) for visually integrated and usable software.
- **Tutorials**: Use the official tutorials for learning how to use KDE frameworks and Qt.
- **API**: Check [api.kde.org](https://api.kde.org) for detailed API references.

### Contributing to KDevelop

Official KDevelop contribution guidelines can be found at [kdevelop.org/contribute-kdevelop](https://kdevelop.org/contribute-kdevelop/):

- **Communication**: Reach out to the core team on `#kdevelop` on `irc.libera.chat` or email the `kdevelop-devel` mailing list.
- **Code Review**: Submit patches via Merge Requests to the [KDE GitLab (Invent)](https://invent.kde.org/kdevelop/kdevelop).
- **Issue Tracking**: Bugs, feature requests, and "Junior Jobs" (for getting started) are tracked on [KDE Bugzilla](https://bugs.kde.org/) under the `kdevelop` and `kdevplatform` products.
- **Documentation & Translation**: Documentation is maintained on the [UserBase Wiki](https://userbase.kde.org/KDevelop), and translations are managed via [l10n.kde.org](https://l10n.kde.org/).
