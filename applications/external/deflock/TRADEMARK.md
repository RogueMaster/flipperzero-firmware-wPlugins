# FlipDeFlock name and logo policy

The **code** is free software under GPL-3.0-or-later. Fork it, modify it, sell it —
the licence grants you all of that and this policy does not take any of it back.

The **name "FlipDeFlock" and the project logo** are a different thing. They are the
project's identity, not part of the licensed code, and no licence to use them is
granted by the GPL, by this file, or by the act of forking. That distinction is
normal for open-source projects and it exists for one reason: so that a person
downloading something called FlipDeFlock can tell whether it came from this project.

## What you may do without asking

- State accurately that your work is **based on**, **derived from**, or **compatible
  with** FlipDeFlock. Nominative references like that are fine and welcome.
- Keep the name in the copyright and licence headers of files you copied. Those are
  attribution, and the GPL requires you to preserve them.
- Fork the repository on GitHub. A GitHub fork displays its origin, so the
  relationship stays visible without anyone having to be told.
- Write about the project, review it, screenshot it, teach it, package it for a
  distribution.

## What requires renaming first

If you publish, distribute, or sell a modified version, give it a different name and
a different logo. A "based on FlipDeFlock" credit in the description is welcome; the
product itself should not be called FlipDeFlock.

Specifically, do not use the name or logo:

- as the name of a repository, release, store listing, or product that is not this
  project;
- as a Gumroad, Etsy, eBay, Tindie, Ko-fi or app-store listing title for a build you
  did not get from this project;
- as an account, channel, or organisation name;
- in a way that suggests your version is the official one, is endorsed by this
  project, or is where users should go for support.

## Verifying an official build

Official releases come from **<https://github.com/ReconGrunt/FlipDeFlock/releases>**
and only from there. Every release publishes `SHA256SUMS.txt` alongside its assets;
check your download against it:

```sh
sha256sum --ignore-missing -c SHA256SUMS.txt
```

`SHA256SUMS.txt` lists every asset on the release, and you have probably downloaded
only some of them (there is one `.fap` per firmware). `--ignore-missing` checks what
you actually have; without it the command reports `FAILED` for the assets you skipped
and exits non-zero, which looks exactly like the failure it is meant to detect. A
genuine mismatch still fails, which is the case that matters.

A copy that does not match, or that came from anywhere else, is not an official build.
That does not automatically make it malicious — but this is counter-surveillance
software, so verify before you trust it, and read the source if anything is unclear.
That option is the entire point of the licence.

## Reporting misuse

Impersonation — a repackage or listing passing itself off as official — hurts users
who cannot tell the difference, and is the one thing this policy exists to stop.
Report it via [SECURITY.md](.github/SECURITY.md) or by opening an issue.

Note that a fork which renames itself, keeps the licence headers intact, and publishes
its source is **not** misuse. That is the licence working as intended, and it is not
something this project objects to.

## Why this matters here

A previous re-upload of this project kept the FlipDeFlock name, gave no attribution,
and served a large opaque archive as its "download". Users had no way to tell it from
the real thing. This policy exists so that case has a name and a remedy, and so the
line between a legitimate fork and an impersonation is written down in advance rather
than argued about afterwards.
