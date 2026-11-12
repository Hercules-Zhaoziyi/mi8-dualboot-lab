# Agent entry point

Read README.md, docs/AGENT-REPRODUCTION.md, docs/IOS-REPRODUCTION.md, docs/PORTING.md and docs/STATUS.md before changing a device. Inspect Git changes; preserve local work and backups. Never infer device identity from a screenshot alone.

The repository contains UI source and integration guidance, not an iOS distribution or a universal phone ROM. Treat profiles as data, not proof of hardware compatibility. Only dipper has historical device evidence; public external-font components were not reflashed. Other targets require their own validation.

Obey the owner's actual authorization. Complete read-only inventory, backup validation, layout/slot analysis and RAM boot before proposing a concrete flash. A request to read this repository does not authorize erasure, repartitioning, unlocking or flashing. Once specifically authorized, carry out the reviewed scope without inventing extra approval steps. Never reuse Mi8 LBAs, physical addresses, PMIC reset code, calibration files or partition GUIDs on another device.

Keep all Apple system inputs/SDKs, private firmware, IPA files, archives, logs and font software in local/. No credentials or device identifiers in commits. Do not download arbitrary binaries to replace missing source builds. Follow the pinned upstream instructions and preserve notices. Before public release run tools/audit_publication.py; explicitly stage reviewed paths, never git add .

For each stage write local/runs/<id>/report.json with input hashes, tool versions, actual commands, exit codes, observed evidence and unresolved issues. Distinguish host-tested, device-tested and planned. Never claim a completed flash based on fastboot OKAY alone. Do not silently change pinned versions or write a fixed-address generic flasher.
