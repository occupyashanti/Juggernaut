# Juggernaut User Manual

## Overview & Key Advantages

Juggernaut is a next-generation, modular, and extensible password cracking platform that combines:
- **AI/ML attack intelligence**
- **Hybrid hardware acceleration (CPU, GPU, FPGA, Cloud)**
- **Cloud-native orchestration**
- **Ethical use enforcement and auditability**

### Key Features
- AI-powered attack engine (rule auto-generation, context learning)
- Multi-accelerator: CPU, GPU, FPGA, Cloud
- Polymorphic cracking kernels
- Dynamic load balancing and smart scheduling
- Cloud bursting (AWS Lambda, GCP Functions)
- Universal hash support (150+ types, including blockchain/biometrics)
- Smart resume (state saved every 5s)
- Stealth/OpSec features (TOR, memory-only, zero-disk)
- Audit logging and ethical controls

### Feature Comparison

| Feature             | John the Ripper | Hashcat | Juggernaut |
|---------------------|-----------------|---------|------------|
| **AI-Guided Attacks**   | ❌              | ❌      | ✅         |
| **Multi-Cloud Support** | ❌              | ❌      | ✅         |
| **Live Hash Learning**  | ❌              | ❌      | ✅         |
| **Polymorphic Kernels** | ❌              | ❌      | ✅         |
| **Biometric Cracking**  | ❌              | ❌      | ✅         |
| **Zero-Disk Operation** | ❌              | ❌      | ✅         |

---

## Areas of Use
- Cybersecurity & Red Teaming
- Academic & Security Research
- Incident Response & Forensics
- Cloud-Scale Cracking
- Enterprise Security Operations

---

## Ethical Implementation
- **Restricted Ethical Use Only**: For cybersecurity professionals, red teamers, and academic researchers.
- **Audit Logging**: Cryptographically signed logs, automatic abuse detection, sandboxing for unauthorized use.
- **License**: See `config/ethics_policy.yml` and `LICENSE` for details.

---

## Build Instructions (CMake)

1. Install CMake (version 3.10+ required):
   ```bash
   sudo apt-get install cmake build-essential
   ```
2. Build the project:
   ```bash
   cd juggernaut
   mkdir build && cd build
   cmake ..
   make
   ```
3. Run the CLI:
   ```bash
   ./juggernaut_cli
   ```

---

## Project Structure

```
juggernaut/
├── core/                     # Core cracking engine
│   ├── cpu/                  # CPU kernels (SIMD/AVX)
│   ├── gpu/                  # GPU acceleration
│   ├── fpga/                 # FPGA bitstreams + Go driver
│   ├── hash_algorithms.c     # Hash algorithm dispatcher
│   └── scheduler.c           # Task scheduler and hardware orchestrator
├── ai_engine/                # AI/ML attack strategies (Python)
├── cloud/                    # Cloud-native components (Go, shell)
├── cli/                      # Command-line interface (C)
├── config/                   # Config files (YAML, TOML)
├── docs/                     # Documentation
├── tests/                    # Unit + benchmark tests
├── scripts/                  # Utility scripts
├── CONTRIBUTING.md
├── LICENSE
└── README.md
```

---

## Usage

See the CLI help (`./juggernaut_cli --help`) and the main README for command details.

---

## Contributing

We welcome contributions in:
- FPGA kernel optimization
- AI/ML attack model improvements
- Ethical usage policy design

See `CONTRIBUTING.md` and `docs/dev_guide.md` for developer setup and workflow.
