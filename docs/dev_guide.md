# Juggernaut Developer Guide

## Overview & Unique Selling Points

Juggernaut is a next-generation, modular password cracking platform with:
- AI-powered attack engine and rule generation
- Multi-accelerator support (CPU, GPU, FPGA, Cloud)
- Polymorphic cracking kernels and dynamic scheduling
- Cloud-native orchestration and batch support
- Universal hash support (150+ types)
- Smart resume, audit logging, and ethical controls

See the main `README.md` for a full feature list, advantages, and use cases.

---

This guide will cover the architecture, code structure, and development workflow for the modular C version of Juggernaut.

## Architecture
- Modular C core for CPU, GPU, FPGA, and cloud cracking
- Python AI engine for attack strategy and rule generation
- CLI and API for automation and integration

## Project Structure
- See README and manual for directory and module breakdown

## Development Workflow
- Fork, branch, and submit PRs for new features or bugfixes
- Add tests for new modules (see `tests/`)
- Follow ethical guidelines and contribute to `config/ethics_policy.yml` if needed

## Contribution Areas
- FPGA kernel optimization
- AI/ML attack model improvements
- Ethical usage policy design
- C core engine enhancements

See `CONTRIBUTING.md` for more details.
