# Security policy

## Reporting a vulnerability

Please report security issues privately, through GitHub: **Security** tab of this repository, then
**Report a vulnerability**. Do not open a public issue for them. Reports in English or French are welcome.

You can expect a first answer within a week. Once a fix is released, the advisory is published with credit to the
reporter, unless they prefer otherwise.

Only the latest release is supported: fixes are not backported.

## What this project touches

entry-aad runs on a car's head unit. It is installed by hand over SSH on the car's ENET (OBD to Ethernet) cable, as
described in [docs/INSTALLATION.md](docs/INSTALLATION.md). Things worth knowing when assessing a report:

| Area | Behaviour |
|---|---|
| Head unit root password | Not handled, stored or changed by this project; the user types it into their own SSH client. EntryNav2 units are known to share a default root password: treat SSH on the unit as unprotected and keep the ENET port physically controlled. |
| Changes on the car | Two new folders on the system partition and settings in `/var/local/data/AA`. No BMW file is modified. The system is writable only during the copy. |
| Network on the car | The Android Auto service listens on the address of the car's Wi-Fi access point (on every interface if that address is not up yet) and accepts connections from that network only; the session is TLS-encrypted as Android Auto requires. The iDrive app talks to the service over UDP on `127.0.0.1` only. Guidance goes to the instrument cluster over SOME/IP on the car's internal network. |
| Release integrity | Each release archive comes with its SHA-256 checksum, and its files with MD5 sums checked on the car before and after the copy. Archives are not signed: download them only from this repository's releases. |

## Scope

In scope: this repository's code, its installation procedure and the archives published in its releases.

Out of scope: vulnerabilities in BMW's own software, in Android Auto or in the phone, unless this project makes them
reachable or worse. Please report those to their vendors.
