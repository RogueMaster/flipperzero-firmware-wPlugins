# AES-EAX test vectors

`aes_eax_test.json` is vendored from [C2SP Project Wycheproof](https://github.com/C2SP/wycheproof),
file `testvectors_v1/aes_eax_test.json`, version 0.9. The vectors are
copyright Google and contributors and are distributed under the **Apache
License, Version 2.0**. The applicable license text is available at
<https://www.apache.org/licenses/LICENSE-2.0>.

The RFC-style vectors in the EAX tests are from M. Bellare, P. Rogaway, and
D. Wagner, *The EAX Mode of Operation*, ePrint 2003/069, Appendix G:
<https://eprint.iacr.org/2003/069>. The paper states that the EAX work is
placed in the **public domain** and is free and unencumbered for all uses.

These files are test inputs only; they are not linked into embedded library
builds.

`eax_prime_worked.json` contains twelve Wycheproof-style positive worked
vectors generated independently with Python's `cryptography` AES backend and
the C12.22 Annex I algorithm. The C implementation checks ciphertext and tag
against these values as a cross-implementation regression set.

Run `python3 verify_eax_prime.py` to verify the same corpus with OpenSSL's
AES-128 implementation. This is a separate third-party cross-check from the
Python implementation used to generate the values.

The EAX' vector in `c12-22-eax-prime.txt` is transcribed from ANSI C12.22-2008,
Example 9 and Annex I.4. It is included for interoperability testing only;
the source standard and its permissions govern the reproduced reference
material. The local source PDF used for the transcription was
`/Users/mistial/Downloads/C12.22 Standard.pdf`.
