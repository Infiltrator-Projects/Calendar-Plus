<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security Policy

## Supported versions

Security fixes are applied to the current `main` branch and, where appropriate, the latest published release. Older releases should not be assumed to receive security backports.

## Reporting a vulnerability

Do not open a public issue for a vulnerability that could expose user data, local system information, package or installer integrity, release infrastructure, or other sensitive material.

If GitHub private vulnerability reporting is available for this repository, use the repository's **Security** reporting flow. Otherwise, contact `infiltratr@yandex.com` with the subject `Calendar Plus security report`.

Include the affected version or commit, operating system and Cinnamon version, impact, reliable reproduction steps and relevant logs where possible. Remove unrelated private information, credentials and tokens from logs or screenshots.

## Security-sensitive areas

Reports are especially useful for memory-safety faults, CalendarServer or D-Bus handling, native-library boundaries, local file handling, package or installer integrity, dependency pinning, release automation, and vulnerabilities reachable through untrusted calendar or event data.

## Handling and disclosure

Reports are assessed privately before public disclosure. Please allow maintainers a reasonable opportunity to reproduce the issue, assess impact and prepare a fix. Testing should be limited to systems and data you are authorised to use and should avoid unnecessary access, disruption or data exposure.
