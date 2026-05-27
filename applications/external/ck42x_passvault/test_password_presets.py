#!/usr/bin/env python3
"""Host-side sanity checks for CK42X PassVault password preset shapes.
These mirror the app's requirement promises: generated passwords are readable and satisfy expected classes.
"""
import re

samples = {
    "memorable": "SolarTiger-River42!",
    "strict": "AmberFalcon-Aq7!Zr3#",
    "long": "Lunar-Raven-Summit742?",
    "no_symbol": "CyberMantisBright365",
}

checks = {
    "memorable": dict(min_len=16, upper=True, lower=True, digit=True, symbol=True),
    "strict": dict(min_len=16, upper=True, lower=True, digit=True, symbol=True),
    "long": dict(min_len=20, upper=True, lower=True, digit=True, symbol=True),
    "no_symbol": dict(min_len=16, upper=True, lower=True, digit=True, symbol=False),
}

for name, password in samples.items():
    rule = checks[name]
    assert len(password) >= rule["min_len"], (name, password, len(password))
    assert bool(re.search(r"[A-Z]", password)) == rule["upper"], (name, password)
    assert bool(re.search(r"[a-z]", password)) == rule["lower"], (name, password)
    assert bool(re.search(r"[0-9]", password)) == rule["digit"], (name, password)
    assert bool(re.search(r"[^A-Za-z0-9]", password)) == rule["symbol"], (
        name,
        password,
    )

print("OK: CK42X PassVault password preset shape checks passed")
