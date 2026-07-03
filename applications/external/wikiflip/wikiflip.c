#include "wikiflip.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>

#define WIKIFLIP_SETTINGS_ITEM_INDEX 0xFFFFFFFFUL

typedef enum {
    WikiFlipLanguageEnglish = 0,
    WikiFlipLanguageSpanish,
} WikiFlipLanguage;

struct WikiFlipApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* categories_menu;
    Submenu* terms_menu;
    Submenu* settings_menu;
    Widget* definition_widget;
    FuriString* definition_buffer;
    const WikiFlipCategory* active_categories;
    size_t active_category_count;
    const WikiFlipCategory* current_category;
    uint32_t current_category_index;
    uint32_t current_term_index;
    WikiFlipLanguage language;
    WikiFlipView current_view;
};

static const WikiFlipTerm wikiflip_protocols_terms_en[] = {
    {
        .title = "TLS 1.3",
        .definition =
            "Transport Layer Security 1.3 protects TCP-based sessions with authenticated encryption, "
            "ephemeral key exchange, and modern AEAD cipher suites. It removes legacy RSA key exchange, "
            "CBC suites, and renegotiation.\n\n"
            "Audit focus: certificate validity, SNI, ALPN, minimum protocol version, HSTS, cipher suites, "
            "OCSP stapling, and downgrade resistance.",
    },
    {
        .title = "DNSSEC",
        .definition =
            "DNSSEC adds authenticity and integrity to DNS records with DNSKEY, DS, and RRSIG data. "
            "It does not encrypt DNS traffic; it prevents forged answers when resolvers validate the chain.\n\n"
            "Audit focus: parent DS records, KSK/ZSK rollover, RRSIG expiry, NSEC/NSEC3 behavior, and resolver validation failures.",
    },
    {
        .title = "SSH",
        .definition =
            "SSH provides encrypted remote administration, public-key authentication, and TCP tunneling. "
            "Its main risk areas are login policy, accepted algorithms, key handling, and port forwarding.\n\n"
            "Audit focus: root login, password authentication, authorized_keys, weak KEX algorithms, MFA, and bastion host controls.",
    },
    {
        .title = "HTTP/2",
        .definition =
            "HTTP/2 uses multiplexing, HPACK compression, and streams over a single connection. It improves latency but can introduce "
            "stream abuse, request smuggling edge cases, and resource exhaustion.\n\n"
            "Audit focus: stream limits, reverse proxies, header normalization, and HTTP/1.1 backend translation.",
    },
    {
        .title = "HTTP/3 QUIC",
        .definition =
            "HTTP/3 runs over QUIC on UDP and integrates TLS 1.3. It improves connection mobility and latency, while changing network "
            "visibility and inspection patterns.\n\n"
            "Audit focus: UDP/443 policy, fallback behavior, certificates, logging, rate limiting, CDN, and WAF visibility.",
    },
    {
        .title = "OAuth 2.0",
        .definition =
            "OAuth 2.0 delegates authorization with access tokens, scopes, and grant flows. It is not an authentication protocol by itself; "
            "identity use cases should use OpenID Connect.\n\n"
            "Audit focus: redirect_uri validation, PKCE, scopes, token audience, token lifetime, token storage, and unsafe grants.",
    },
    {
        .title = "OpenID Connect",
        .definition =
            "OpenID Connect extends OAuth 2.0 for federated authentication with signed ID tokens, claims, discovery metadata, and JWKS.\n\n"
            "Audit focus: issuer, audience, nonce, signature verification, alg none rejection, JWKS rotation, sensitive claims, and logout.",
    },
    {
        .title = "SAML 2.0",
        .definition =
            "SAML 2.0 enables enterprise federation through XML assertions exchanged between an Identity Provider and a Service Provider.\n\n"
            "Audit focus: mandatory signatures, XML canonicalization, audience, recipient, NameID, assertion lifetime, and XML signature wrapping.",
    },
    {
        .title = "Kerberos",
        .definition =
            "Kerberos authenticates principals with tickets issued by a Key Distribution Center. In Active Directory it supports SSO, SPNs, "
            "delegation, TGTs, and TGS tickets.\n\n"
            "Audit focus: Kerberoasting, AS-REP roasting, unconstrained delegation, SPN hygiene, clock skew, and password policy.",
    },
    {
        .title = "LDAP/LDAPS",
        .definition =
            "LDAP queries directory services; LDAPS adds TLS. In Active Directory it is commonly used for identity, groups, attributes, "
            "and application authentication.\n\n"
            "Audit focus: signing, channel binding, anonymous binds, LDAPS enforcement, service account privileges, and LDAP injection.",
    },
    {
        .title = "SMB",
        .definition =
            "Server Message Block provides file sharing, named pipes, and remote administration on Windows networks. SMBv1 is obsolete "
            "and high risk.\n\n"
            "Audit focus: SMBv1 removal, signing, share permissions, NTFS permissions, anonymous sessions, and TCP/445 exposure.",
    },
    {
        .title = "MQTT",
        .definition =
            "MQTT is a lightweight publish/subscribe protocol common in IoT. It uses brokers, topics, QoS levels, and client credentials "
            "or certificates.\n\n"
            "Audit focus: TLS, authentication, topic ACLs, retained messages, anonymous access, bridge configuration, and telemetry leakage.",
    },
};

static const WikiFlipTerm wikiflip_owasp_terms_en[] = {
    {
        .title = "A01:2025 - Broken Access Control",
        .definition =
            "Authorization is not enforced consistently. This includes IDOR, horizontal or vertical privilege escalation, endpoint bypass, "
            "metadata abuse, insecure CORS, SSRF grouped into this category, and controls enforced only in the client.\n\n"
            "Mitigation: deny by default, centralized server-side authorization, object-level checks, ownership validation, role-based tests, "
            "rate limiting, and access failure logging.",
    },
    {
        .title = "A02:2025 - Security Misconfiguration",
        .definition =
            "Unsafe configuration in apps, cloud, containers, servers, or frameworks. Examples include defaults, unneeded services, broad "
            "permissions, debug mode, missing headers, permissive CORS, verbose errors, and public storage.\n\n"
            "Mitigation: automated hardening, versioned infrastructure as code, secure baselines, minimum configuration, secret scanning, "
            "and continuous validation.",
    },
    {
        .title = "A03:2025 - Software Supply Chain Failures",
        .definition =
            "Risk introduced by dependencies, build systems, packages, pipelines, repositories, artifacts, signatures, updates, or vendors. "
            "This goes beyond known vulnerable components.\n\n"
            "Mitigation: SBOM, pinning, SCA, trusted repositories, signatures, provenance, CI/CD review, least privilege tokens, and release controls.",
    },
    {
        .title = "A04:2025 - Cryptographic Failures",
        .definition =
            "Missing or incorrect cryptography can expose data or weaken trust. Examples include obsolete algorithms, weak keys, bad TLS, "
            "cleartext storage, and poor secret handling.\n\n"
            "Mitigation: modern cryptography, key management, rotation, KMS/HSM use, strong TLS, salted password hashing, and design review.",
    },
    {
        .title = "A05:2025 - Injection",
        .definition =
            "Untrusted input changes queries, commands, interpreters, or templates. This includes SQL, NoSQL, OS command, LDAP, XPath, "
            "template injection, and XSS where context permits.\n\n"
            "Mitigation: prepared statements, positive validation, contextual escaping, safe ORM usage, sandboxing, and strict data/code separation.",
    },
    {
        .title = "A06:2025 - Insecure Design",
        .definition =
            "Architectural or business-logic flaws that cannot be fixed with a small implementation patch. Examples include abusable workflows, "
            "missing compensating controls, absent limits, and unmodeled threats.\n\n"
            "Mitigation: threat modeling, secure design reviews, abuse cases, verifiable security requirements, and defensive patterns.",
    },
    {
        .title = "A07:2025 - Authentication Failures",
        .definition =
            "Weaknesses in identity, login, sessions, MFA, and account recovery. Examples include credential stuffing, weak passwords, unsafe "
            "reset flows, predictable sessions, and incomplete token validation.\n\n"
            "Mitigation: phishing-resistant MFA, rate limits, strong hashing, secure sessions, token rotation, abuse detection, and robust recovery.",
    },
    {
        .title = "A08:2025 - Software or Data Integrity Failures",
        .definition =
            "Software, updates, data, serialization, or pipelines are trusted without integrity verification. Examples include compromised CI/CD, "
            "untrusted plugins, unsafe deserialization, and unsigned critical data.\n\n"
            "Mitigation: signatures, artifact verification, pipeline controls, allowlists, safe deserialization, and change control.",
    },
    {
        .title = "A09:2025 - Security Logging and Alerting Failures",
        .definition =
            "Logging, monitoring, or alerting is insufficient to detect, investigate, and respond. Examples include missing critical events, "
            "uncorrelatable logs, context-free alerts, and poor retention.\n\n"
            "Mitigation: defined security events, correlation IDs, SIEM coverage, severity-aware alerts, protected logs, retention, and detection tests.",
    },
    {
        .title = "A10:2025 - Mishandling of Exceptional Conditions",
        .definition =
            "Errors, exceptions, timeouts, partial states, or dependency failures are handled unsafely. Impact can include information leakage, "
            "crashes, control bypass, data inconsistency, or denial of service.\n\n"
            "Mitigation: fail securely, generic user-facing errors, safe internal logging, timeouts, bounded retries, circuit breakers, and resilience tests.",
    },
};

static const WikiFlipTerm wikiflip_nist_terms_en[] = {
    {
        .title = "NIST CSF 2.0",
        .definition =
            "The Cybersecurity Framework organizes capabilities into Govern, Identify, Protect, Detect, Respond, and Recover. It supports "
            "maturity planning, risk communication, and prioritization.\n\n"
            "Audit focus: current and target profiles, control mapping, risk gaps, measurable actions, and governance ownership.",
    },
    {
        .title = "NIST SP 800-53",
        .definition =
            "A broad catalog of security and privacy controls, including AC, AU, CM, IA, IR, RA, SC, SI, and other families.\n\n"
            "Audit focus: selected baseline, control evidence, implementation testing, exceptions, and risk traceability.",
    },
    {
        .title = "NIST SP 800-61",
        .definition =
            "Incident handling guidance covering preparation, detection, analysis, containment, eradication, recovery, and post-incident activity.\n\n"
            "Audit focus: playbooks, severity model, chain of custody, communication paths, response times, and lessons learned.",
    },
    {
        .title = "NIST SP 800-30",
        .definition =
            "Risk assessment guidance for threats, vulnerabilities, likelihood, impact, existing controls, and treatment recommendations.\n\n"
            "Audit focus: repeatable methodology, risk criteria, asset traceability, and risk-based prioritization.",
    },
    {
        .title = "NIST SP 800-37 RMF",
        .definition =
            "Risk Management Framework steps include categorize, select, implement, assess, authorize, and monitor controls.\n\n"
            "Audit focus: authorization, inherited controls, POA&M tracking, continuous monitoring, and significant changes.",
    },
    {
        .title = "NIST SP 800-63",
        .definition =
            "Digital identity guidance defining IAL, AAL, and FAL levels for identity proofing, authenticators, and federation.\n\n"
            "Audit focus: MFA strength, phishing resistance, credential lifecycle, recovery, and federated claims.",
    },
    {
        .title = "NIST SP 800-171",
        .definition =
            "Protection of Controlled Unclassified Information in non-federal systems, often used in government supply chains.\n\n"
            "Audit focus: control implementation, SSP, POA&M, access, configuration, incident response, and CUI handling.",
    },
    {
        .title = "NIST SP 800-207",
        .definition =
            "Zero Trust Architecture uses continuous verification, dynamic policy, and minimum access for each session.\n\n"
            "Audit focus: policy engine, identity, device posture, segmentation, telemetry, and contextual decisions.",
    },
    {
        .title = "NIST SP 800-218 SSDF",
        .definition =
            "Secure Software Development Framework practices prepare the organization, protect software, produce secure software, and respond "
            "to vulnerabilities.\n\n"
            "Audit focus: SDLC controls, code review, SCA, threat modeling, secrets, builds, and vulnerability response.",
    },
    {
        .title = "FIPS 140-3",
        .definition =
            "A standard for validating cryptographic modules. It defines requirements for algorithms, roles, authentication, physical security, "
            "and self-tests.\n\n"
            "Audit focus: validated modules, FIPS mode, current certificates, and actual runtime configuration.",
    },
};

static const WikiFlipTerm wikiflip_cve_terms_en[] = {
    {
        .title = "CVE-2021-44228 Log4Shell",
        .definition =
            "Critical RCE in Apache Log4j 2 through attacker-controlled JNDI lookups. Impact was broad because Log4j is commonly included "
            "as a transitive Java dependency.\n\n"
            "Validation: affected versions, indirect dependencies, log patterns, egress paths, and historical IoCs.",
    },
    {
        .title = "CVE-2017-0144 EternalBlue",
        .definition =
            "SMBv1 vulnerability in Microsoft Windows associated with MS17-010. It can enable remote code execution on unpatched systems "
            "with TCP/445 exposed.\n\n"
            "Mitigation: patch, disable SMBv1, segment networks, and restrict unnecessary SMB exposure.",
    },
    {
        .title = "CVE-2023-34362 MOVEit",
        .definition =
            "Critical MOVEit Transfer flaw exploited for unauthenticated access and data exfiltration.\n\n"
            "Validation: version, webshells, transfer logs, unusual accounts, and abnormal data movement. Mitigation: patch, isolate, and rotate credentials.",
    },
    {
        .title = "CVE-2024-3094 XZ Utils",
        .definition =
            "Backdoor introduced into specific XZ Utils versions that affected software supply chain integrity and could impact sshd in some Linux builds.\n\n"
            "Mitigation: remove affected versions, verify packages, review provenance, and strengthen supply chain controls.",
    },
    {
        .title = "CVE-2023-23397 Outlook",
        .definition =
            "Microsoft Outlook privilege escalation where crafted reminder properties can force NTLM authentication to attacker-controlled resources.\n\n"
            "Mitigation: patch, block outbound NTLM, detect abnormal MAPI properties, and monitor external authentication.",
    },
    {
        .title = "CVE-2023-46805 Ivanti",
        .definition =
            "Authentication bypass in Ivanti Connect Secure and Policy Secure, observed in exploit chains with other flaws for remote code execution.\n\n"
            "Mitigation: patch, run integrity checks, review web roots, admin sessions, and unusual VPN access.",
    },
    {
        .title = "CVE-2024-21887 Ivanti",
        .definition =
            "Command injection in Ivanti Connect Secure and Policy Secure, exploitable by authenticated users or in chains with a prior bypass.\n\n"
            "Mitigation: update, isolate management, review logs, inspect persistence artifacts, and rotate exposed credentials.",
    },
    {
        .title = "CVE-2024-6387 regreSSHion",
        .definition =
            "Race condition in OpenSSH server related to async-unsafe signal handler behavior. It may allow pre-auth RCE on some platforms and versions.\n\n"
            "Mitigation: update OpenSSH, restrict exposure, use conservative LoginGraceTime, and filter SSH access.",
    },
    {
        .title = "CVE-2023-3519 Citrix ADC",
        .definition =
            "Unauthenticated RCE in Citrix ADC and Gateway, widely exploited against exposed appliances.\n\n"
            "Validation: version, webshells, sessions, HTTP logs, and configuration changes. Mitigation: patch immediately and hunt for compromise.",
    },
    {
        .title = "CVE-2020-1472 Zerologon",
        .definition =
            "Weakness in the Netlogon Remote Protocol that can enable domain controller takeover under vulnerable conditions.\n\n"
            "Mitigation: patch, enforce secure Netlogon, monitor DC events, and rotate secrets if compromise is suspected.",
    },
    {
        .title = "CVE-2022-30190 Follina",
        .definition =
            "Abuse of Microsoft Support Diagnostic Tool through documents or links that invoke ms-msdt for command execution.\n\n"
            "Mitigation: patch, block protocol abuse, deploy EDR rules, and detect Office processes spawning diagnostic or script interpreters.",
    },
    {
        .title = "CVE-2024-1709 ScreenConnect",
        .definition =
            "Authentication bypass in ConnectWise ScreenConnect that can allow administrative access or takeover of a vulnerable instance.\n\n"
            "Mitigation: update, review users, sessions, extensions, logs, and remotely managed hosts.",
    },
};

static const WikiFlipTerm wikiflip_blue_team_terms_en[] = {
    {
        .title = "SIEM",
        .definition =
            "Security Information and Event Management centralizes logs, normalizes events, correlates signals, and produces alerts.\n\n"
            "Good practice: source inventory, parser quality, use-case tuning, retention, dashboards, and rules with context.",
    },
    {
        .title = "EDR",
        .definition =
            "Endpoint Detection and Response monitors processes, memory, network, persistence, and endpoint changes. It supports isolation, "
            "evidence collection, and hunting.\n\n"
            "Audit focus: coverage, exclusions, tamper protection, offline sensors, and remote response actions.",
    },
    {
        .title = "YARA",
        .definition =
            "YARA describes file or memory patterns with strings, conditions, and modules. It is useful for malware triage, hunting, and forensics.\n\n"
            "Good practice: specific rules, metadata, false-positive testing, and version control.",
    },
    {
        .title = "Sigma",
        .definition =
            "Sigma is a generic detection rule format for logs. It allows the same detection logic to be converted to multiple SIEM query languages.\n\n"
            "Good practice: map fields, define logsource, test conversions, and document expected false positives.",
    },
    {
        .title = "SOAR",
        .definition =
            "Security Orchestration, Automation and Response automates playbooks, enrichment, tickets, and containment actions.\n\n"
            "Audit focus: approvals, limits, rollback, secrets, integrations, and error handling.",
    },
    {
        .title = "UEBA",
        .definition =
            "User and Entity Behavior Analytics detects anomalies by comparing activity to baselines for users, hosts, or services.\n\n"
            "Use cases: credential abuse, abnormal hours, impossible travel, and access volume changes.",
    },
    {
        .title = "DFIR",
        .definition =
            "Digital Forensics and Incident Response combines evidence preservation, forensic analysis, and technical response.\n\n"
            "Good practice: chain of custody, triage, timeline, memory, disk, logs, and executive plus technical reporting.",
    },
    {
        .title = "Windows 4624/4625",
        .definition =
            "Windows events for successful and failed logons. They are key for password spraying, lateral movement, and abnormal access analysis.\n\n"
            "Important fields: LogonType, AccountName, WorkstationName, IpAddress, AuthenticationPackage, Status, and SubStatus.",
    },
    {
        .title = "Sysmon",
        .definition =
            "Sysmon records detailed telemetry for processes, network, DNS, drivers, files, and registry activity.\n\n"
            "Good practice: maintained configuration, noise filtering, EDR correlation, and log integrity protection.",
    },
    {
        .title = "NDR",
        .definition =
            "Network Detection and Response analyzes traffic for beaconing, C2, exfiltration, lateral movement, and unusual protocols.\n\n"
            "Audit focus: SPAN/TAP coverage, TLS metadata, DNS, NetFlow, Zeek, and alert response.",
    },
    {
        .title = "Incident Playbook",
        .definition =
            "A repeatable response procedure for scenarios such as ransomware, phishing, BEC, leaked credentials, or compromised hosts.\n\n"
            "It should define severity, roles, containment, evidence, communications, and recovery.",
    },
    {
        .title = "Threat Hunting",
        .definition =
            "Proactive searching based on hypotheses about adversary activity that may not have triggered alerts.\n\n"
            "Outputs: findings, new detections, logging gaps, and control improvements.",
    },
};

static const WikiFlipTerm wikiflip_red_team_terms_en[] = {
    {
        .title = "Phishing Simulation",
        .definition =
            "A controlled exercise to measure email controls, user behavior, and reporting workflow. Metrics include delivery, open, click, "
            "credential avoidance, and reporting.\n\n"
            "Controls assessed: SPF, DKIM, DMARC, sandboxing, banners, URL blocking, and SOC response.",
    },
    {
        .title = "Lateral Movement",
        .definition =
            "Movement between systems after initial access. It may abuse sessions, shares, RDP, WinRM, WMI, SSH, Kerberos, or admin tools.\n\n"
            "Controls: segmentation, local admin hygiene, detection, and credential protection.",
    },
    {
        .title = "C2",
        .definition =
            "Command and Control infrastructure lets an operator send tasks and receive results during an authorized test. It may use HTTPS, "
            "DNS, mTLS, redirectors, and traffic profiles.\n\n"
            "Controls assessed: egress filtering, TLS inspection, DNS monitoring, and beaconing detection.",
    },
    {
        .title = "Initial Access",
        .definition =
            "The entry phase into an environment through credentials, VPN, phishing, public exposure, supply chain, or vulnerable services.\n\n"
            "Audit focus: real attack surface, preventive controls, and detection traceability.",
    },
    {
        .title = "Privilege Escalation",
        .definition =
            "Obtaining higher privileges on a host, domain, cloud account, or application through misconfigurations, weak permissions, vulnerable "
            "kernels, or local secrets.\n\n"
            "Mitigation: hardening, least privilege, patching, and monitoring privileged changes.",
    },
    {
        .title = "Persistence",
        .definition =
            "Mechanisms used to maintain access, such as services, scheduled tasks, run keys, startup items, OAuth apps, webshells, or created accounts.\n\n"
            "Blue team focus: monitor creation and modification, then compare against baselines.",
    },
    {
        .title = "Credential Access",
        .definition =
            "Techniques to obtain secrets, such as LSASS access, dumps, browser stores, keychains, configs, cloud tokens, Kerberos, and password spraying.\n\n"
            "Controls: MFA, Credential Guard, secret scanning, vaulting, and detection.",
    },
    {
        .title = "Kerberoasting",
        .definition =
            "Requesting TGS tickets for SPN-backed service accounts and cracking the service hash offline. It often requires only a standard AD user.\n\n"
            "Mitigation: long passwords, gMSA, AES, anomalous TGS monitoring, and SPN cleanup.",
    },
    {
        .title = "Pass-the-Hash",
        .definition =
            "Using NTLM hashes without knowing the plaintext password to authenticate laterally. It impacts environments with NTLM enabled and reused privileges.\n\n"
            "Mitigation: limit local admins, LAPS, segmentation, NTLM restrictions, and EDR.",
    },
    {
        .title = "Living off the Land",
        .definition =
            "Using legitimate system binaries and tools for execution, download, persistence, or evasion.\n\n"
            "Examples: PowerShell, certutil, mshta, rundll32, wmic, bitsadmin, and schtasks.",
    },
    {
        .title = "OPSEC",
        .definition =
            "Operational security for reducing noise, preserving authorized traceability, and avoiding impact. It includes timing, volume, payloads, "
            "infrastructure, and artifact handling.\n\n"
            "In formal audits, critical actions and rollback are documented.",
    },
    {
        .title = "Payload Staging",
        .definition =
            "Splitting a small first-stage loader from later components. This can reduce initial size but increases network dependencies and observable signals.\n\n"
            "Controls: egress blocking, inspection, EDR, and allowlisting.",
    },
};

static const WikiFlipTerm wikiflip_threat_intelligence_terms_en[] = {
    {
        .title = "IOC",
        .definition =
            "Indicator of Compromise: a hash, IP, domain, path, mutex, certificate, or log pattern associated with malicious activity.\n\n"
            "Management: source, date, confidence, context, expiration, and correlation with TTPs.",
    },
    {
        .title = "TTP",
        .definition =
            "Tactics, Techniques, and Procedures describe adversary behavior. They are usually more durable than IoCs and commonly map to MITRE ATT&CK.\n\n"
            "Use: emulation, detection, gap analysis, and defensive prioritization.",
    },
    {
        .title = "STIX/TAXII",
        .definition =
            "STIX models threat intelligence; TAXII transports it. They can share indicators, malware, campaigns, relationships, and courses of action.\n\n"
            "Use: feeds, enrichment, normalization, and source traceability.",
    },
    {
        .title = "MITRE ATT&CK",
        .definition =
            "A knowledge base of adversary tactics and techniques organized by platform and operational phase.\n\n"
            "Use: coverage mapping, detection engineering, adversary emulation, and gap reporting.",
    },
    {
        .title = "Diamond Model",
        .definition =
            "An analytic model with four vertices: adversary, capability, infrastructure, and victim. It helps relate events and form hypotheses.\n\n"
            "Use: clustering, pivoting, and campaign analysis.",
    },
    {
        .title = "Cyber Kill Chain",
        .definition =
            "A phase model: reconnaissance, weaponization, delivery, exploitation, installation, C2, and actions on objectives.\n\n"
            "Use: place controls and detections across attack phases.",
    },
    {
        .title = "OSINT",
        .definition =
            "Open Source Intelligence collects information from public sources such as domains, leaks, repositories, networks, metadata, certificates, and cloud exposure.\n\n"
            "Good practice: validate source, legality, date, and reliability.",
    },
    {
        .title = "Threat Actor",
        .definition =
            "An entity or group with motivation, resources, TTPs, and objectives. It can be criminal, state-linked, hacktivist, insider, or opportunistic.\n\n"
            "Analysis: keep attribution evidence-based and state confidence clearly.",
    },
    {
        .title = "Campaign",
        .definition =
            "A set of related activities grouped by objective, infrastructure, malware, victims, or time period. It does not require firm attribution.\n\n"
            "Use: group IoCs, TTPs, timing, sectors, and responses.",
    },
    {
        .title = "Confidence Scoring",
        .definition =
            "A measure of certainty for an intelligence judgment, based on source quality, corroboration, recency, and evidence quality.\n\n"
            "Good practice: separate facts, inferences, and analytic judgments.",
    },
};

static const WikiFlipTerm wikiflip_hardware_terms_en[] = {
    {
        .title = "UART",
        .definition =
            "A common serial interface for debug consoles. It usually exposes TX, RX, GND, and sometimes VCC.\n\n"
            "Audit focus: voltage, baud rate, boot logs, prompts, credentials, and privileged diagnostic commands.",
    },
    {
        .title = "JTAG",
        .definition =
            "A test and debug interface that can allow CPU halt, memory access, firmware dumping, and low-level debugging.\n\n"
            "Mitigation: disable production debug, protect fuses, and enforce secure boot.",
    },
    {
        .title = "SWD",
        .definition =
            "Serial Wire Debug is a two-wire ARM debug interface using SWDIO and SWCLK. It may expose flash, registers, and execution control.\n\n"
            "Audit focus: readout protection, mass erase behavior, and physical access assumptions.",
    },
    {
        .title = "SPI Flash",
        .definition =
            "External memory used for firmware, configuration, or data. It can often be dumped with a clip or programmer if protections are absent.\n\n"
            "Audit focus: firmware dumps, strings, binwalk, secrets, partitions, and firmware signing.",
    },
    {
        .title = "I2C",
        .definition =
            "A two-wire bus for sensors, EEPROM, and peripherals. It uses SDA/SCL lines and device addresses.\n\n"
            "Audit focus: enumerate devices, observe traffic, read EEPROM, and validate controls over sensitive data.",
    },
    {
        .title = "Logic Analyzer",
        .definition =
            "A tool for capturing digital signals and decoding buses such as UART, I2C, SPI, or 1-Wire.\n\n"
            "Use: identify protocols, timing, commands, and secrets transmitted in cleartext.",
    },
    {
        .title = "RF Sniffing",
        .definition =
            "Capturing radio communications to analyze modulation, frequency, frames, and replay behavior.\n\n"
            "Audit focus: encryption, rolling codes, replay resistance, and metadata exposure.",
    },
    {
        .title = "NFC/RFID",
        .definition =
            "Radio identification technologies including LF RFID, HF NFC, MIFARE, DESFire, and tag emulation.\n\n"
            "Audit focus: tag type, keys, UID-only access, sectors, authentication, and replay.",
    },
    {
        .title = "GPIO",
        .definition =
            "General-purpose input/output pins used for control, debug, boot straps, or hidden features.\n\n"
            "Audit focus: pin mapping, logic levels, boot states, hidden functions, and electrical protection.",
    },
    {
        .title = "Bootloader",
        .definition =
            "Early code that initializes hardware and loads firmware. If exposed, it may allow dumping, flashing, or bypass.\n\n"
            "Audit focus: commands, passwords, secure boot, rollback, recovery mode, and signature enforcement.",
    },
    {
        .title = "Secure Boot",
        .definition =
            "A boot chain that verifies signatures before running firmware. It protects integrity when keys and fuses are managed correctly.\n\n"
            "Audit focus: enforcement, anti-rollback, key storage, and debug modes.",
    },
    {
        .title = "Fault Injection",
        .definition =
            "Inducing physical faults through voltage, clock, laser, or electromagnetic glitching to alter execution flow.\n\n"
            "Mitigation: redundancy, sensors, timing checks, secure elements, and robust boot design.",
    },
};

static const WikiFlipTerm wikiflip_iso_terms_en[] = {
    {
        .title = "ISO 27001",
        .definition =
            "Requirements for an Information Security Management System. It covers scope, risk, leadership, support, operation, evaluation, and improvement.\n\n"
            "Audit focus: Statement of Applicability, risk treatment, control evidence, internal audits, and corrective actions.",
    },
    {
        .title = "ISO 27002",
        .definition =
            "Implementation guidance for security controls. It complements ISO 27001 with practical control guidance and attributes.\n\n"
            "Audit focus: design, implementation, maturity, gaps, and risk alignment.",
    },
    {
        .title = "ISO 27005",
        .definition =
            "Guidance for information security risk management, including context, analysis, evaluation, treatment, communication, and monitoring.\n\n"
            "Use: build a repeatable and traceable risk methodology.",
    },
    {
        .title = "ISO 27017",
        .definition =
            "Security controls for cloud services, covering both cloud providers and cloud customers.\n\n"
            "Audit focus: shared responsibility, configuration, virtualization, administration, and tenant separation.",
    },
    {
        .title = "ISO 27018",
        .definition =
            "Protection of personal data in public cloud environments for processors of personally identifiable information.\n\n"
            "Audit focus: consent, limited use, disclosure, deletion, subprocessors, and transparency.",
    },
    {
        .title = "ISO 22301",
        .definition =
            "Business Continuity Management System standard covering BIA, strategies, plans, exercises, and continual improvement.\n\n"
            "Audit focus: RTO/RPO, testing, dependencies, crisis management, and recovery.",
    },
    {
        .title = "ISO 27701",
        .definition =
            "Privacy extension for ISO 27001 and ISO 27002. It defines a PIMS and controls for controllers and processors.\n\n"
            "Audit focus: PII inventory, legal basis, rights, DPIA, and third parties.",
    },
    {
        .title = "ISO 27035",
        .definition =
            "Information security incident management guidance covering planning, detection, reporting, response, and learning.\n\n"
            "Audit focus: roles, severity, channels, evidence, lessons learned, and metrics.",
    },
    {
        .title = "ISO 27032",
        .definition =
            "Cybersecurity guidance focused on collaboration, information sharing, and cyberspace risks.\n\n"
            "Use: align information security, network security, internet security, and critical protection.",
    },
    {
        .title = "ISO 42001",
        .definition =
            "Management system standard for artificial intelligence, covering governance, risks, lifecycle, transparency, oversight, and improvement.\n\n"
            "Audit focus: AI inventory, controls, data, assessments, monitoring, and responsibilities.",
    },
};

static const WikiFlipCategory wikiflip_categories_en[] = {
    {
        .name = "Protocols",
        .terms = wikiflip_protocols_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_protocols_terms_en),
    },
    {
        .name = "OWASP",
        .terms = wikiflip_owasp_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_owasp_terms_en),
    },
    {
        .name = "NIST",
        .terms = wikiflip_nist_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_nist_terms_en),
    },
    {
        .name = "CVE",
        .terms = wikiflip_cve_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_cve_terms_en),
    },
    {
        .name = "Blue Team",
        .terms = wikiflip_blue_team_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_blue_team_terms_en),
    },
    {
        .name = "Red Team",
        .terms = wikiflip_red_team_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_red_team_terms_en),
    },
    {
        .name = "Threat Intelligence",
        .terms = wikiflip_threat_intelligence_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_threat_intelligence_terms_en),
    },
    {
        .name = "Hardware Hacking",
        .terms = wikiflip_hardware_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_hardware_terms_en),
    },
    {
        .name = "ISO",
        .terms = wikiflip_iso_terms_en,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_iso_terms_en),
    },
};

static const WikiFlipTerm wikiflip_protocols_terms_es[] = {
    {
        .title = "TLS 1.3",
        .definition =
            "Transport Layer Security 1.3 protege canales TCP con cifrado autenticado, "
            "intercambio de claves efimero y suites AEAD. Elimina RSA key exchange, CBC "
            "y renegociacion heredada.\n\n"
            "Auditoria: validar certificados, SNI, ALPN, versiones minimas, HSTS, cipher "
            "suites, OCSP stapling y bloqueo de downgrade.",
    },
    {
        .title = "DNSSEC",
        .definition =
            "DNSSEC agrega autenticidad e integridad a DNS mediante firmas digitales, "
            "DNSKEY, DS y RRSIG. No cifra consultas; evita respuestas falsificadas cuando "
            "el resolver valida la cadena de confianza.\n\n"
            "Auditoria: revisar DS padre, rollover KSK/ZSK, expiracion RRSIG, NSEC/NSEC3 "
            "y fallos de validacion.",
    },
    {
        .title = "SSH",
        .definition =
            "SSH provee administracion remota cifrada, autenticacion por clave publica y "
            "tuneles TCP. La superficie critica incluye politicas de login, algoritmos, "
            "manejo de claves y port forwarding.\n\n"
            "Auditoria: bloquear root directo, limitar password auth, revisar authorized_keys, "
            "KEX debiles, MFA y bastion host.",
    },
    {
        .title = "HTTP/2",
        .definition =
            "HTTP/2 usa multiplexacion, compresion HPACK y streams sobre una sola conexion. "
            "Reduce latencia pero introduce riesgos de abuso de streams, request smuggling "
            "y agotamiento de recursos.\n\n"
            "Auditoria: validar limites de streams, proxies intermedios, normalizacion de "
            "headers y comportamiento HTTP/1.1 hacia backend.",
    },
    {
        .title = "HTTP/3 QUIC",
        .definition =
            "HTTP/3 usa QUIC sobre UDP con TLS 1.3 integrado. Mejora latencia y movilidad "
            "de conexion, pero cambia telemetria, inspeccion y controles de red tradicionales.\n\n"
            "Auditoria: validar politica UDP/443, fallback, certificados, logging, rate limiting "
            "y visibilidad en WAF/CDN.",
    },
    {
        .title = "OAuth 2.0",
        .definition =
            "OAuth 2.0 delega autorizacion mediante access tokens, scopes y grants. No es "
            "autenticacion por si mismo; para identidad debe complementarse con OpenID Connect.\n\n"
            "Auditoria: revisar redirect_uri, PKCE, scopes, expiracion, audience, token storage "
            "y grants inseguros.",
    },
    {
        .title = "OpenID Connect",
        .definition =
            "OIDC extiende OAuth 2.0 para autenticacion federada usando ID tokens firmados, "
            "claims, discovery y JWKS. Es comun en SSO moderno.\n\n"
            "Auditoria: validar issuer, audience, nonce, firma, alg none, rotacion JWKS, "
            "claims sensibles y logout.",
    },
    {
        .title = "SAML 2.0",
        .definition =
            "SAML 2.0 permite federacion empresarial basada en assertions XML firmadas entre "
            "Identity Provider y Service Provider.\n\n"
            "Auditoria: comprobar firma obligatoria, canonicalizacion XML, audience, recipient, "
            "NameID, expiracion y proteccion contra XML signature wrapping.",
    },
    {
        .title = "Kerberos",
        .definition =
            "Kerberos autentica entidades mediante tickets emitidos por un KDC. En Windows AD "
            "sostiene SSO, delegacion, SPN y tickets TGT/TGS.\n\n"
            "Auditoria: revisar Kerberoasting, AS-REP roasting, delegacion unconstrained, SPN "
            "expuestos, clock skew y politicas de contrasena.",
    },
    {
        .title = "LDAP/LDAPS",
        .definition =
            "LDAP consulta directorios; LDAPS agrega TLS. En AD se usa para identidades, grupos, "
            "atributos y autenticacion de aplicaciones.\n\n"
            "Auditoria: exigir signing/channel binding, bloquear binds anonimos, usar LDAPS, "
            "limitar cuentas de servicio y revisar filtros LDAP injection.",
    },
    {
        .title = "SMB",
        .definition =
            "Server Message Block permite archivos compartidos, named pipes y administracion "
            "remota en Windows. SMBv1 es obsoleto y de alto riesgo.\n\n"
            "Auditoria: deshabilitar SMBv1, exigir signing donde aplique, revisar shares, "
            "permisos NTFS, sesiones anonimas y exposicion 445/TCP.",
    },
    {
        .title = "MQTT",
        .definition =
            "MQTT es mensajeria publish/subscribe ligera usada en IoT. Usa brokers, topics, QoS "
            "y clientes con credenciales o certificados.\n\n"
            "Auditoria: revisar TLS, autenticacion, ACL por topic, retained messages, anonymous "
            "access, bridge configs y fuga de telemetria.",
    },
};

static const WikiFlipTerm wikiflip_owasp_terms_es[] = {
    {
        .title = "A01:2025 - Broken Access Control",
        .definition =
            "Fallo al imponer autorizacion de forma consistente. Incluye IDOR, escalacion "
            "horizontal o vertical, bypass de endpoints, abuso de metadatos, CORS inseguro, "
            "SSRF consolidado en esta categoria y controles aplicados solo en cliente.\n\n"
            "Mitigacion: deny by default, autorizacion server-side centralizada, controles por "
            "objeto, ownership checks, pruebas por rol, rate limiting y logs de fallos de acceso.",
    },
    {
        .title = "A02:2025 - Security Misconfiguration",
        .definition =
            "Configuraciones inseguras en aplicaciones, cloud, contenedores, servidores o "
            "frameworks. Incluye defaults, servicios innecesarios, permisos amplios, debug, "
            "headers ausentes, CORS permisivo, errores verbosos y storage publico.\n\n"
            "Mitigacion: hardening automatizado, IaC versionado, baselines, configuracion minima, "
            "secret scanning y validacion continua.",
    },
    {
        .title = "A03:2025 - Software Supply Chain Failures",
        .definition =
            "Riesgos introducidos por dependencias, build systems, paquetes, pipelines, "
            "repositorios, artefactos, firmas, actualizaciones y proveedores. Amplia el enfoque "
            "mas alla de componentes vulnerables conocidos.\n\n"
            "Mitigacion: SBOM, pinning, SCA, repositorios confiables, firmas, provenance, revision "
            "de CI/CD, least privilege en tokens y control de releases.",
    },
    {
        .title = "A04:2025 - Cryptographic Failures",
        .definition =
            "Uso incorrecto o ausente de criptografia que termina exponiendo datos o debilitando "
            "confianza. Incluye algoritmos obsoletos, claves debiles, TLS mal configurado, "
            "almacenamiento en claro y manejo deficiente de secretos.\n\n"
            "Mitigacion: criptografia moderna, gestion de claves, rotacion, KMS/HSM, TLS fuerte, "
            "hashing de contrasenas con sal y revisiones de diseno.",
    },
    {
        .title = "A05:2025 - Injection",
        .definition =
            "Datos no confiables alteran consultas, comandos, interpretes o plantillas. Incluye "
            "SQL, NoSQL, OS command, LDAP, XPath, template injection y XSS cuando el contexto lo "
            "permite.\n\n"
            "Mitigacion: parametros preparados, validacion positiva, escaping contextual, ORMs "
            "bien usados, sandboxing y separacion estricta entre datos y codigo.",
    },
    {
        .title = "A06:2025 - Insecure Design",
        .definition =
            "Defectos de arquitectura o logica que no se corrigen con parches puntuales. Incluye "
            "flujos de negocio abusables, falta de controles compensatorios, limites ausentes y "
            "amenazas no modeladas.\n\n"
            "Mitigacion: threat modeling, secure design reviews, abuso de casos, requisitos de "
            "seguridad verificables y patrones defensivos desde el diseno.",
    },
    {
        .title = "A07:2025 - Authentication Failures",
        .definition =
            "Debilidades en identidad, login, sesiones, MFA y recuperacion de cuenta. Incluye "
            "credential stuffing, contrasenas debiles, reset inseguro, sesiones predecibles y "
            "validacion incompleta de tokens.\n\n"
            "Mitigacion: MFA resistente, rate limiting, hashing fuerte, sesiones seguras, rotacion "
            "de tokens, deteccion de abuso y procesos robustos de recuperacion.",
    },
    {
        .title = "A08:2025 - Software or Data Integrity Failures",
        .definition =
            "Confianza indebida en software, actualizaciones, datos, serializacion o pipelines sin "
            "verificar integridad. Incluye CI/CD comprometido, plugins no confiables, deserializacion "
            "insegura y datos criticos sin firma.\n\n"
            "Mitigacion: firmas, verificacion de artefactos, controles de pipeline, allowlists, "
            "deserializacion segura y control de cambios.",
    },
    {
        .title = "A09:2025 - Security Logging and Alerting Failures",
        .definition =
            "Registro, monitoreo o alerta insuficiente para detectar, investigar y responder. "
            "Incluye eventos criticos ausentes, logs no correlacionables, alertas sin contexto y "
            "retencion inadecuada.\n\n"
            "Mitigacion: eventos de seguridad definidos, IDs de correlacion, SIEM, alertas con "
            "severidad, proteccion de logs, retencion y pruebas de deteccion.",
    },
    {
        .title = "A10:2025 - Mishandling of Exceptional Conditions",
        .definition =
            "Manejo incorrecto de errores, excepciones, timeouts, estados parciales y fallos de "
            "dependencias. Puede provocar fugas de informacion, crashes, bypass de controles, "
            "inconsistencia de datos o denegacion de servicio.\n\n"
            "Mitigacion: fail secure, errores genericos al usuario, logging interno seguro, "
            "timeouts, retries controlados, circuit breakers y pruebas de caos controladas.",
    },
};

static const WikiFlipTerm wikiflip_nist_terms_es[] = {
    {
        .title = "NIST CSF 2.0",
        .definition =
            "Framework de ciberseguridad con funciones Govern, Identify, Protect, Detect, Respond "
            "y Recover. Sirve para madurez, comunicacion de riesgo y priorizacion.\n\n"
            "Auditoria: mapear controles, brechas, perfiles actuales/objetivo, riesgos y acciones "
            "medibles por funcion.",
    },
    {
        .title = "NIST SP 800-53",
        .definition =
            "Catalogo de controles de seguridad y privacidad. Incluye familias AC, AU, CM, IA, "
            "IR, RA, SC, SI y otras.\n\n"
            "Auditoria: seleccionar baseline, levantar evidencias, probar implementacion, registrar "
            "excepciones y relacionar controles con riesgos.",
    },
    {
        .title = "NIST SP 800-61",
        .definition =
            "Guia de manejo de incidentes: preparacion, deteccion, analisis, contencion, "
            "erradicacion, recuperacion y post-incidente.\n\n"
            "Auditoria: revisar playbooks, severidad, cadena de custodia, comunicaciones, tiempos "
            "de respuesta y lecciones aprendidas.",
    },
    {
        .title = "NIST SP 800-30",
        .definition =
            "Guia para evaluacion de riesgos. Cubre amenazas, vulnerabilidades, probabilidad, "
            "impacto, controles existentes y recomendacion de tratamiento.\n\n"
            "Auditoria: comprobar metodologia, criterios repetibles, trazabilidad de activos y "
            "priorizacion basada en riesgo.",
    },
    {
        .title = "NIST SP 800-37 RMF",
        .definition =
            "Risk Management Framework para categorizar, seleccionar, implementar, evaluar, "
            "autorizar y monitorear controles.\n\n"
            "Auditoria: revisar autorizacion, herencia de controles, POA&M, monitoreo continuo y "
            "cambios significativos.",
    },
    {
        .title = "NIST SP 800-63",
        .definition =
            "Guia de identidad digital con niveles IAL, AAL y FAL. Define criterios para prueba "
            "de identidad, autenticadores y federacion.\n\n"
            "Auditoria: evaluar MFA, phishing resistance, lifecycle de credenciales, recovery y "
            "claims federados.",
    },
    {
        .title = "NIST SP 800-171",
        .definition =
            "Proteccion de Controlled Unclassified Information en sistemas no federales. Muy usado "
            "en entornos de cadena de suministro gubernamental.\n\n"
            "Auditoria: revisar controles, SSP, POA&M, acceso, configuracion, incidentes y manejo "
            "de CUI.",
    },
    {
        .title = "NIST SP 800-207",
        .definition =
            "Arquitectura Zero Trust basada en verificacion continua, politicas dinamicas y acceso "
            "minimo por sesion.\n\n"
            "Auditoria: revisar policy engine, identity, posture, segmentacion, telemetria y "
            "decisiones contextuales.",
    },
    {
        .title = "NIST SP 800-218 SSDF",
        .definition =
            "Secure Software Development Framework. Organiza practicas para preparar la organizacion, "
            "proteger software, producir software seguro y responder a vulnerabilidades.\n\n"
            "Auditoria: mapear SDLC, code review, SCA, threat modeling, secrets, builds y respuesta.",
    },
    {
        .title = "FIPS 140-3",
        .definition =
            "Estandar para validacion de modulos criptograficos. Define niveles y requisitos para "
            "algoritmos, roles, autenticacion, fisico y self-tests.\n\n"
            "Auditoria: verificar uso de modulos validados, modo FIPS, certificados vigentes y "
            "configuracion real.",
    },
};

static const WikiFlipTerm wikiflip_cve_terms_es[] = {
    {
        .title = "CVE-2021-44228 Log4Shell",
        .definition =
            "RCE critica en Apache Log4j 2 por lookups JNDI controlables por atacante. Impacto "
            "alto por uso transitivo en aplicaciones Java.\n\n"
            "Validacion: versiones afectadas, dependencias indirectas, patrones en logs, egress y "
            "IoC historicos. Mitigacion: actualizar y restringir JNDI/egress.",
    },
    {
        .title = "CVE-2017-0144 EternalBlue",
        .definition =
            "Vulnerabilidad SMBv1 en Windows asociada a MS17-010. Permite ejecucion remota en "
            "sistemas sin parche con 445/TCP expuesto.\n\n"
            "Mitigacion: aplicar parches, deshabilitar SMBv1, segmentar y bloquear SMB no necesario.",
    },
    {
        .title = "CVE-2023-34362 MOVEit",
        .definition =
            "Fallo critico en MOVEit Transfer explotado para acceso no autenticado y exfiltracion "
            "de datos.\n\n"
            "Validacion: version, webshells, logs, transferencias inusuales y cuentas creadas. "
            "Mitigacion: parches, aislamiento y rotacion de credenciales.",
    },
    {
        .title = "CVE-2024-3094 XZ Utils",
        .definition =
            "Backdoor introducido en versiones especificas de XZ Utils que afectaba integridad de "
            "la cadena de suministro y podia impactar sshd en ciertos builds Linux.\n\n"
            "Mitigacion: retirar versiones afectadas, verificar paquetes, revisar provenance y "
            "fortalecer controles de supply chain.",
    },
    {
        .title = "CVE-2023-23397 Outlook",
        .definition =
            "Elevacion de privilegios en Microsoft Outlook por procesamiento de recordatorios que "
            "pueden forzar autenticacion NTLM hacia recursos controlados por atacante.\n\n"
            "Mitigacion: parches, bloqueo NTLM saliente, deteccion de propiedades MAPI anormales "
            "y monitoreo de autenticacion externa.",
    },
    {
        .title = "CVE-2023-46805 Ivanti",
        .definition =
            "Bypass de autenticacion en Ivanti Connect Secure/Policy Secure, observado en cadenas "
            "de explotacion con otras fallas para ejecucion remota.\n\n"
            "Mitigacion: aplicar parches, ejecutar integrity checker, revisar web roots, sesiones "
            "admin y accesos VPN inusuales.",
    },
    {
        .title = "CVE-2024-21887 Ivanti",
        .definition =
            "Command injection en Ivanti Connect Secure/Policy Secure explotable por usuarios "
            "autenticados o en cadena con bypass previo.\n\n"
            "Mitigacion: actualizar, aislar administracion, revisar logs, artefactos persistentes "
            "y credenciales expuestas.",
    },
    {
        .title = "CVE-2024-6387 regreSSHion",
        .definition =
            "Condicion de carrera en OpenSSH server relacionada con signal handler async-unsafe. "
            "Puede permitir RCE pre-auth en ciertas plataformas y versiones.\n\n"
            "Mitigacion: actualizar OpenSSH, limitar exposicion, aplicar LoginGraceTime conservador "
            "y filtrar acceso SSH.",
    },
    {
        .title = "CVE-2023-3519 Citrix ADC",
        .definition =
            "RCE no autenticada en Citrix ADC/Gateway ampliamente explotada contra appliances "
            "expuestos.\n\n"
            "Validacion: version, webshells, sesiones, logs HTTP y cambios de configuracion. "
            "Mitigacion: parche inmediato y hunting.",
    },
    {
        .title = "CVE-2020-1472 Zerologon",
        .definition =
            "Debilidad en Netlogon Remote Protocol que permite tomar control de un domain "
            "controller bajo condiciones vulnerables.\n\n"
            "Mitigacion: parches, enforcement mode, monitoreo de Netlogon, eventos DC y rotacion "
            "si hubo compromiso.",
    },
    {
        .title = "CVE-2022-30190 Follina",
        .definition =
            "Abuso de Microsoft Support Diagnostic Tool mediante documentos o enlaces que invocan "
            "ms-msdt para ejecucion de comandos.\n\n"
            "Mitigacion: parches, bloqueo de protocolo, reglas EDR y deteccion de procesos Office "
            "lanzando msdt/script interpreters.",
    },
    {
        .title = "CVE-2024-1709 ScreenConnect",
        .definition =
            "Authentication bypass en ConnectWise ScreenConnect que permite acceso administrativo "
            "o takeover de instancia vulnerable.\n\n"
            "Mitigacion: actualizar, revisar usuarios, sesiones, extensiones, logs y hosts remotos "
            "administrados.",
    },
};

static const WikiFlipTerm wikiflip_blue_team_terms_es[] = {
    {
        .title = "SIEM",
        .definition =
            "Centraliza logs, normaliza eventos, correlaciona senales y genera alertas. Su valor "
            "depende de cobertura, parsing, casos de uso y respuesta operativa.\n\n"
            "Buenas practicas: inventario de fuentes, tuning, retencion, dashboards y reglas con "
            "contexto.",
    },
    {
        .title = "EDR",
        .definition =
            "Monitorea procesos, memoria, red, persistencia y cambios en endpoints. Permite "
            "aislamiento, recoleccion de evidencias y hunting.\n\n"
            "Auditoria: cobertura, exclusions, tamper protection, sensores caidos y acciones de "
            "respuesta remota.",
    },
    {
        .title = "YARA",
        .definition =
            "Lenguaje para describir patrones de archivos o memoria con strings, condiciones y "
            "modulos. Util para malware triage y busqueda forense.\n\n"
            "Practica: reglas especificas, metadatos, pruebas de falsos positivos y versionado.",
    },
    {
        .title = "Sigma",
        .definition =
            "Formato generico para reglas de deteccion sobre logs. Permite convertir una logica "
            "a multiples SIEMs y backend queries.\n\n"
            "Practica: mapear campos, definir logsource, probar conversion y documentar falsos "
            "positivos esperados.",
    },
    {
        .title = "SOAR",
        .definition =
            "Security Orchestration, Automation and Response automatiza playbooks, enriquecimiento, "
            "tickets y acciones de contencion.\n\n"
            "Auditoria: revisar aprobaciones, limites, rollback, secretos, integraciones y manejo "
            "de errores.",
    },
    {
        .title = "UEBA",
        .definition =
            "User and Entity Behavior Analytics detecta anomalias comparando actividad contra "
            "baselines de usuarios, hosts o servicios.\n\n"
            "Uso: detectar abuso de credenciales, horarios anormales, viajes imposibles y cambios "
            "en volumen de acceso.",
    },
    {
        .title = "DFIR",
        .definition =
            "Digital Forensics and Incident Response combina preservacion de evidencia, analisis "
            "forense y respuesta tecnica.\n\n"
            "Practica: cadena de custodia, triage, timeline, memoria, disco, logs y reporte "
            "ejecutivo/tecnico.",
    },
    {
        .title = "Windows 4624/4625",
        .definition =
            "Eventos de inicio de sesion exitoso y fallido. Claves para detectar password spraying, "
            "lateral movement y accesos fuera de patron.\n\n"
            "Campos: LogonType, AccountName, WorkstationName, IpAddress, AuthenticationPackage y "
            "Status/SubStatus.",
    },
    {
        .title = "Sysmon",
        .definition =
            "Componente de Sysinternals que registra telemetria detallada de procesos, red, DNS, "
            "drivers, archivos y registry.\n\n"
            "Practica: usar config mantenida, filtrar ruido, correlacionar con EDR y proteger la "
            "integridad de logs.",
    },
    {
        .title = "NDR",
        .definition =
            "Network Detection and Response analiza trafico para detectar beaconing, C2, exfiltracion, "
            "movimiento lateral y protocolos raros.\n\n"
            "Auditoria: cobertura SPAN/TAP, TLS metadata, DNS, NetFlow, Zeek y respuesta a alertas.",
    },
    {
        .title = "Incident Playbook",
        .definition =
            "Procedimiento repetible para responder a un escenario: ransomware, phishing, BEC, "
            "credenciales filtradas o host comprometido.\n\n"
            "Debe incluir criterios de severidad, roles, contencion, evidencias, comunicacion y "
            "recuperacion.",
    },
    {
        .title = "Threat Hunting",
        .definition =
            "Busqueda proactiva basada en hipotesis sobre actividad adversaria no alertada. Usa "
            "telemetria, TTP, estadistica y conocimiento del entorno.\n\n"
            "Salida: hallazgos, nuevas detecciones, gaps de logging y mejoras de controles.",
    },
};

static const WikiFlipTerm wikiflip_red_team_terms_es[] = {
    {
        .title = "Phishing Simulation",
        .definition =
            "Ejercicio controlado para medir correo, usuarios y proceso de reporte. Mide entrega, "
            "apertura, clic, credenciales evitadas y reporte.\n\n"
            "Evalua SPF, DKIM, DMARC, sandboxing, banners, bloqueo URL y respuesta SOC.",
    },
    {
        .title = "Lateral Movement",
        .definition =
            "Movimiento entre sistemas despues de acceso inicial. Puede abusar sesiones, shares, "
            "RDP, WinRM, WMI, SSH, Kerberos o herramientas administrativas.\n\n"
            "Controles: segmentacion, privilegios locales, deteccion y proteccion de credenciales.",
    },
    {
        .title = "C2",
        .definition =
            "Command and Control permite enviar tareas y recibir resultados de implantes en una "
            "prueba autorizada. Puede usar HTTPS, DNS, mTLS, redirectors y perfiles.\n\n"
            "Evalua egress filtering, TLS inspection, DNS y deteccion de beaconing.",
    },
    {
        .title = "Initial Access",
        .definition =
            "Fase de entrada al entorno: credenciales, VPN, phishing, exposicion publica, supply "
            "chain o servicios vulnerables.\n\n"
            "Auditoria: medir superficie real, controles preventivos y trazabilidad de deteccion.",
    },
    {
        .title = "Privilege Escalation",
        .definition =
            "Obtencion de mayores permisos en host, dominio, cloud o aplicacion. Incluye servicios "
            "mal configurados, permisos debiles, kernels vulnerables y secretos locales.\n\n"
            "Mitigacion: hardening, least privilege, patching y monitoreo de cambios privilegiados.",
    },
    {
        .title = "Persistence",
        .definition =
            "Mecanismos para mantener acceso: servicios, scheduled tasks, run keys, startup items, "
            "OAuth apps, webshells o cuentas creadas.\n\n"
            "Blue Team: monitorear creacion/modificacion y establecer baselines.",
    },
    {
        .title = "Credential Access",
        .definition =
            "Tecnicas para obtener secretos: LSASS, dumps, browser stores, keychains, configs, "
            "tokens cloud, Kerberos y password spraying.\n\n"
            "Controles: MFA, Credential Guard, secret scanning, vaulting y deteccion.",
    },
    {
        .title = "Kerberoasting",
        .definition =
            "Solicitud de tickets TGS para cuentas con SPN y cracking offline del hash del servicio. "
            "No requiere privilegios elevados en AD tradicional.\n\n"
            "Mitigacion: contrasenas largas, gMSA, AES, monitoreo de TGS anomalo y limpieza de SPN.",
    },
    {
        .title = "Pass-the-Hash",
        .definition =
            "Uso de hashes NTLM sin conocer la contrasena en claro para autenticarse lateralmente. "
            "Impacta entornos con NTLM habilitado y privilegios reutilizados.\n\n"
            "Mitigacion: limitar admins locales, LAPS, segmentacion, bloqueo NTLM y EDR.",
    },
    {
        .title = "Living off the Land",
        .definition =
            "Uso de binarios y herramientas legitimas del sistema para ejecucion, descarga, "
            "persistencia o evasiones.\n\n"
            "Ejemplos: PowerShell, certutil, mshta, rundll32, wmic, bitsadmin y schtasks.",
    },
    {
        .title = "OPSEC",
        .definition =
            "Disciplina operacional para reducir ruido, preservar trazabilidad autorizada y evitar "
            "impacto. Incluye horarios, volumen, payloads, infraestructura y manejo de artefactos.\n\n"
            "En auditoria formal se documenta cada accion critica y rollback.",
    },
    {
        .title = "Payload Staging",
        .definition =
            "Separacion entre un primer cargador pequeno y componentes posteriores. Reduce tamano "
            "inicial pero aumenta dependencias de red y senales observables.\n\n"
            "Controles: bloqueo egress, inspeccion, EDR y allowlisting.",
    },
};

static const WikiFlipTerm wikiflip_threat_intelligence_terms_es[] = {
    {
        .title = "IOC",
        .definition =
            "Indicator of Compromise: hash, IP, dominio, ruta, mutex, certificado o patron de log "
            "asociado a actividad maliciosa.\n\n"
            "Gestion: fuente, fecha, confianza, contexto, expiracion y correlacion con TTP.",
    },
    {
        .title = "TTP",
        .definition =
            "Tactics, Techniques and Procedures describen comportamiento adversario. Son mas "
            "duraderos que IoC y se mapean comunmente a MITRE ATT&CK.\n\n"
            "Uso: emulacion, deteccion, gap analysis y priorizacion defensiva.",
    },
    {
        .title = "STIX/TAXII",
        .definition =
            "STIX modela inteligencia de amenazas; TAXII la transporta. Permiten compartir "
            "indicadores, malware, campanas, relaciones y cursos de accion.\n\n"
            "Uso: feeds, enriquecimiento, normalizacion y trazabilidad.",
    },
    {
        .title = "MITRE ATT&CK",
        .definition =
            "Base de conocimiento de tacticas y tecnicas adversarias. Organiza comportamiento "
            "por plataformas y fases operativas.\n\n"
            "Uso: mapear cobertura, construir detecciones, emular adversarios y reportar gaps.",
    },
    {
        .title = "Diamond Model",
        .definition =
            "Modelo analitico con cuatro vertices: adversary, capability, infrastructure y victim. "
            "Ayuda a relacionar eventos y formular hipotesis.\n\n"
            "Uso: clustering, pivoting y analisis de campanas.",
    },
    {
        .title = "Cyber Kill Chain",
        .definition =
            "Modelo de fases: reconnaissance, weaponization, delivery, exploitation, installation, "
            "C2 y actions on objectives.\n\n"
            "Uso: ubicar controles y detecciones por fase de ataque.",
    },
    {
        .title = "OSINT",
        .definition =
            "Open Source Intelligence obtiene informacion de fuentes publicas: dominios, leaks, "
            "repositorios, redes, metadatos, certificados y exposicion cloud.\n\n"
            "Practica: validar fuente, legalidad, fecha y confiabilidad.",
    },
    {
        .title = "Threat Actor",
        .definition =
            "Entidad o grupo con motivacion, recursos, TTP y objetivos. Puede ser criminal, estatal, "
            "hacktivista, insider o oportunista.\n\n"
            "Analisis: atribucion prudente, evidencia, confianza y sesgos.",
    },
    {
        .title = "Campaign",
        .definition =
            "Conjunto de actividades relacionadas por objetivos, infraestructura, malware, victimas "
            "o periodo. No implica atribucion definitiva.\n\n"
            "Uso: agrupar IoC, TTP, tiempos, sectores y respuestas.",
    },
    {
        .title = "Confidence Scoring",
        .definition =
            "Medida de certeza de una conclusion de inteligencia. Depende de fuente, corroboracion, "
            "actualidad y calidad de evidencia.\n\n"
            "Practica: separar hechos, inferencias y juicios analiticos.",
    },
};

static const WikiFlipTerm wikiflip_hardware_terms_es[] = {
    {
        .title = "UART",
        .definition =
            "Interfaz serial comun para consolas de debug. Normalmente expone TX, RX, GND y a veces "
            "VCC.\n\n"
            "Auditoria: identificar voltaje, baud rate, boot logs, prompts, credenciales y comandos "
            "privilegiados.",
    },
    {
        .title = "JTAG",
        .definition =
            "Interfaz de test/debug que puede permitir halt de CPU, memoria, firmware dumping y "
            "debug bajo nivel.\n\n"
            "Mitigacion: deshabilitar debug en produccion, proteger fuses y usar secure boot.",
    },
    {
        .title = "SWD",
        .definition =
            "Serial Wire Debug es debug ARM de dos hilos: SWDIO y SWCLK. Puede exponer flash, "
            "registros y control de ejecucion.\n\n"
            "Auditoria: verificar readout protection, mass erase y acceso fisico.",
    },
    {
        .title = "SPI Flash",
        .definition =
            "Memoria externa usada para firmware, configs o datos. Puede extraerse con clip o "
            "programador si no hay protecciones.\n\n"
            "Auditoria: dump, strings, binwalk, secretos, particiones y firmware signing.",
    },
    {
        .title = "I2C",
        .definition =
            "Bus de dos hilos para sensores, EEPROM y perifericos. Usa direcciones y lineas SDA/SCL.\n\n"
            "Auditoria: enumerar dispositivos, observar trafico, leer EEPROM y validar controles "
            "sobre datos sensibles.",
    },
    {
        .title = "Logic Analyzer",
        .definition =
            "Herramienta para capturar senales digitales y decodificar buses como UART, I2C, SPI "
            "o 1-Wire.\n\n"
            "Uso: identificar protocolos, timing, comandos y secretos transmitidos en claro.",
    },
    {
        .title = "RF Sniffing",
        .definition =
            "Captura de comunicaciones radio para analizar modulacion, frecuencia, tramas y "
            "repeticion.\n\n"
            "Auditoria: validar cifrado, rolling codes, replay resistance y exposicion de metadatos.",
    },
    {
        .title = "NFC/RFID",
        .definition =
            "Tecnologias de identificacion por radio. Incluyen LF RFID, HF NFC, MIFARE, DESFire y "
            "emulacion de tags.\n\n"
            "Auditoria: tipo de tag, claves, UID-only access, sectores, autenticacion y replay.",
    },
    {
        .title = "GPIO",
        .definition =
            "Pines de entrada/salida general usados para control, debug o boot straps.\n\n"
            "Auditoria: identificar pines, niveles logicos, estados de arranque, funciones ocultas "
            "y proteccion electrica.",
    },
    {
        .title = "Bootloader",
        .definition =
            "Codigo inicial que prepara hardware y carga firmware. Si queda expuesto puede permitir "
            "dump, flash o bypass.\n\n"
            "Auditoria: comandos, password, secure boot, rollback, recovery mode y firmas.",
    },
    {
        .title = "Secure Boot",
        .definition =
            "Cadena de arranque que verifica firmas antes de ejecutar firmware. Protege integridad "
            "si las claves y fuses estan bien gestionados.\n\n"
            "Auditoria: verificar enforcement, anti-rollback, key storage y modos debug.",
    },
    {
        .title = "Fault Injection",
        .definition =
            "Tecnica de inducir fallos fisicos con glitch de voltaje, clock, laser o EM para alterar "
            "flujo de ejecucion.\n\n"
            "Mitigacion: redundancia, sensores, timing checks, secure elements y boot robusto.",
    },
};

static const WikiFlipTerm wikiflip_iso_terms_es[] = {
    {
        .title = "ISO 27001",
        .definition =
            "Requisitos para un Sistema de Gestion de Seguridad de la Informacion. Centra alcance, "
            "riesgos, liderazgo, soporte, operacion, evaluacion y mejora.\n\n"
            "Auditoria: SoA, tratamiento de riesgos, evidencias, auditorias internas y acciones "
            "correctivas.",
    },
    {
        .title = "ISO 27002",
        .definition =
            "Guia de implementacion de controles de seguridad. Complementa ISO 27001 con medidas "
            "practicas y atributos.\n\n"
            "Auditoria: diseno, implementacion, madurez, brechas y alineacion con riesgo.",
    },
    {
        .title = "ISO 27005",
        .definition =
            "Guia para gestion de riesgos de seguridad de la informacion. Cubre contexto, analisis, "
            "evaluacion, tratamiento, comunicacion y monitoreo.\n\n"
            "Uso: construir metodologia repetible y trazable.",
    },
    {
        .title = "ISO 27017",
        .definition =
            "Controles de seguridad para servicios cloud, tanto proveedores como clientes.\n\n"
            "Auditoria: responsabilidad compartida, configuracion, virtualizacion, administracion "
            "y segregacion de clientes.",
    },
    {
        .title = "ISO 27018",
        .definition =
            "Proteccion de datos personales en cloud publica para procesadores de PII.\n\n"
            "Auditoria: consentimiento, uso limitado, disclosure, deletion, subprocesadores y "
            "transparencia.",
    },
    {
        .title = "ISO 22301",
        .definition =
            "Sistema de gestion de continuidad del negocio. Orienta BIA, estrategias, planes, "
            "ejercicios y mejora continua.\n\n"
            "Auditoria: RTO/RPO, pruebas, dependencias, crisis management y recuperacion.",
    },
    {
        .title = "ISO 27701",
        .definition =
            "Extension de privacidad para ISO 27001/27002. Define PIMS y controles para responsables "
            "y procesadores de datos personales.\n\n"
            "Auditoria: inventario PII, bases legales, derechos, DPIA y terceros.",
    },
    {
        .title = "ISO 27035",
        .definition =
            "Gestion de incidentes de seguridad. Cubre planificacion, deteccion, reporte, respuesta "
            "y aprendizaje.\n\n"
            "Auditoria: roles, severidad, canales, evidencias, lecciones y metricas.",
    },
    {
        .title = "ISO 27032",
        .definition =
            "Guia de ciberseguridad enfocada en colaboracion, intercambio de informacion y riesgos "
            "en el ciberespacio.\n\n"
            "Uso: alinear seguridad de informacion, redes, internet y proteccion critica.",
    },
    {
        .title = "ISO 42001",
        .definition =
            "Sistema de gestion para inteligencia artificial. Define gobierno, riesgos, ciclo de "
            "vida, transparencia, supervision y mejora.\n\n"
            "Auditoria: inventario de IA, controles, datos, evaluaciones, monitoreo y responsabilidades.",
    },
};

static const WikiFlipCategory wikiflip_categories_es[] = {
    {
        .name = "Protocolos",
        .terms = wikiflip_protocols_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_protocols_terms_es),
    },
    {
        .name = "OWASP",
        .terms = wikiflip_owasp_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_owasp_terms_es),
    },
    {
        .name = "NIST",
        .terms = wikiflip_nist_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_nist_terms_es),
    },
    {
        .name = "CVE",
        .terms = wikiflip_cve_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_cve_terms_es),
    },
    {
        .name = "Equipo Azul",
        .terms = wikiflip_blue_team_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_blue_team_terms_es),
    },
    {
        .name = "Equipo Rojo",
        .terms = wikiflip_red_team_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_red_team_terms_es),
    },
    {
        .name = "Intel Amenazas",
        .terms = wikiflip_threat_intelligence_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_threat_intelligence_terms_es),
    },
    {
        .name = "Hardware Hacking",
        .terms = wikiflip_hardware_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_hardware_terms_es),
    },
    {
        .name = "ISO",
        .terms = wikiflip_iso_terms_es,
        .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_iso_terms_es),
    },
};

static void wikiflip_term_selected_callback(void* context, uint32_t index);

static void wikiflip_select_language(WikiFlipApp* app, WikiFlipLanguage language);
static void wikiflip_category_selected_callback(void* context, uint32_t index);

static const WikiFlipCategory* wikiflip_get_categories(WikiFlipLanguage language, size_t* count) {
    if(language == WikiFlipLanguageSpanish) {
        *count = WIKIFLIP_ARRAY_SIZE(wikiflip_categories_es);
        return wikiflip_categories_es;
    }

    *count = WIKIFLIP_ARRAY_SIZE(wikiflip_categories_en);
    return wikiflip_categories_en;
}

static const char* wikiflip_settings_menu_label(WikiFlipLanguage language) {
    return (language == WikiFlipLanguageSpanish) ? "Configuracion" : "Settings";
}

static const char* wikiflip_settings_header(WikiFlipLanguage language) {
    return (language == WikiFlipLanguageSpanish) ? "Idioma" : "Language";
}

static const char* wikiflip_english_option_label(WikiFlipLanguage language) {
    return (language == WikiFlipLanguageEnglish) ? "[x] English" : "[ ] English";
}

static const char* wikiflip_spanish_option_label(WikiFlipLanguage language) {
    return (language == WikiFlipLanguageSpanish) ? "[x] Espanol" : "[ ] Espanol";
}

static void wikiflip_switch_to_view(WikiFlipApp* app, WikiFlipView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void wikiflip_show_definition(WikiFlipApp* app, const WikiFlipTerm* term) {
    widget_reset(app->definition_widget);
    furi_string_printf(app->definition_buffer, "%s\n\n%s", term->title, term->definition);
    widget_add_text_scroll_element(
        app->definition_widget,
        0,
        0,
        WIKIFLIP_SCREEN_WIDTH,
        WIKIFLIP_SCREEN_HEIGHT,
        furi_string_get_cstr(app->definition_buffer));
}

static void wikiflip_terms_menu_rebuild(WikiFlipApp* app) {
    furi_assert(app->current_category);

    submenu_reset(app->terms_menu);
    submenu_set_header(app->terms_menu, app->current_category->name);

    for(size_t i = 0; i < app->current_category->term_count; i++) {
        submenu_add_item(
            app->terms_menu,
            app->current_category->terms[i].title,
            (uint32_t)i,
            wikiflip_term_selected_callback,
            app);
    }
}

static void wikiflip_settings_selected_callback(void* context, uint32_t index) {
    WikiFlipApp* app = context;
    furi_assert(app);

    if(index == WikiFlipLanguageSpanish) {
        wikiflip_select_language(app, WikiFlipLanguageSpanish);
    } else {
        wikiflip_select_language(app, WikiFlipLanguageEnglish);
    }

    wikiflip_switch_to_view(app, WikiFlipViewSettings);
}

static void wikiflip_settings_menu_rebuild(WikiFlipApp* app) {
    submenu_reset(app->settings_menu);
    submenu_set_header(app->settings_menu, wikiflip_settings_header(app->language));
    submenu_add_item(
        app->settings_menu,
        wikiflip_english_option_label(app->language),
        WikiFlipLanguageEnglish,
        wikiflip_settings_selected_callback,
        app);
    submenu_add_item(
        app->settings_menu,
        wikiflip_spanish_option_label(app->language),
        WikiFlipLanguageSpanish,
        wikiflip_settings_selected_callback,
        app);
    submenu_set_selected_item(app->settings_menu, app->language);
}

static void wikiflip_categories_menu_build(WikiFlipApp* app) {
    submenu_reset(app->categories_menu);
    submenu_set_header(app->categories_menu, "WikiFlip");

    for(size_t i = 0; i < app->active_category_count; i++) {
        submenu_add_item(
            app->categories_menu,
            app->active_categories[i].name,
            (uint32_t)i,
            wikiflip_category_selected_callback,
            app);
    }

    submenu_add_item(
        app->categories_menu,
        wikiflip_settings_menu_label(app->language),
        WIKIFLIP_SETTINGS_ITEM_INDEX,
        wikiflip_category_selected_callback,
        app);
}

static void wikiflip_select_language(WikiFlipApp* app, WikiFlipLanguage language) {
    app->language = language;
    app->active_categories = wikiflip_get_categories(app->language, &app->active_category_count);
    app->current_category_index = 0;
    app->current_term_index = 0;
    app->current_category = &app->active_categories[0];

    wikiflip_categories_menu_build(app);
    submenu_set_selected_item(app->categories_menu, app->active_category_count);
    wikiflip_terms_menu_rebuild(app);
    wikiflip_settings_menu_rebuild(app);
}

static void wikiflip_term_selected_callback(void* context, uint32_t index) {
    WikiFlipApp* app = context;
    furi_assert(app);
    furi_assert(app->current_category);

    if(index >= app->current_category->term_count) {
        return;
    }

    app->current_term_index = index;
    wikiflip_show_definition(app, &app->current_category->terms[index]);
    wikiflip_switch_to_view(app, WikiFlipViewDefinition);
}

static void wikiflip_category_selected_callback(void* context, uint32_t index) {
    WikiFlipApp* app = context;
    furi_assert(app);

    if(index == WIKIFLIP_SETTINGS_ITEM_INDEX) {
        wikiflip_settings_menu_rebuild(app);
        wikiflip_switch_to_view(app, WikiFlipViewSettings);
        return;
    }

    if(index >= app->active_category_count) {
        return;
    }

    app->current_category_index = index;
    app->current_term_index = 0;
    app->current_category = &app->active_categories[index];

    wikiflip_terms_menu_rebuild(app);
    submenu_set_selected_item(app->terms_menu, app->current_term_index);

    wikiflip_switch_to_view(app, WikiFlipViewTerms);
}

static bool wikiflip_navigation_callback(void* context) {
    WikiFlipApp* app = context;
    furi_assert(app);

    switch(app->current_view) {
    case WikiFlipViewDefinition:
        submenu_set_selected_item(app->terms_menu, app->current_term_index);
        wikiflip_switch_to_view(app, WikiFlipViewTerms);
        return true;
    case WikiFlipViewTerms:
        submenu_set_selected_item(app->categories_menu, app->current_category_index);
        wikiflip_switch_to_view(app, WikiFlipViewCategories);
        return true;
    case WikiFlipViewSettings:
        submenu_set_selected_item(app->categories_menu, app->active_category_count);
        wikiflip_switch_to_view(app, WikiFlipViewCategories);
        return true;
    case WikiFlipViewCategories:
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static WikiFlipApp* wikiflip_alloc(void) {
    WikiFlipApp* app = malloc(sizeof(WikiFlipApp));
    furi_assert(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->categories_menu = submenu_alloc();
    app->terms_menu = submenu_alloc();
    app->settings_menu = submenu_alloc();
    app->definition_widget = widget_alloc();
    app->definition_buffer = furi_string_alloc();
    app->language = WikiFlipLanguageEnglish;
    app->active_categories = wikiflip_get_categories(app->language, &app->active_category_count);
    app->current_category = &app->active_categories[0];
    app->current_category_index = 0;
    app->current_term_index = 0;
    app->current_view = WikiFlipViewCategories;

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wikiflip_navigation_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewCategories, submenu_get_view(app->categories_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewTerms, submenu_get_view(app->terms_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewSettings, submenu_get_view(app->settings_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, WikiFlipViewDefinition, widget_get_view(app->definition_widget));

    wikiflip_categories_menu_build(app);
    wikiflip_terms_menu_rebuild(app);
    wikiflip_settings_menu_rebuild(app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void wikiflip_free(WikiFlipApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewDefinition);
    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewTerms);
    view_dispatcher_remove_view(app->view_dispatcher, WikiFlipViewCategories);

    widget_free(app->definition_widget);
    submenu_free(app->settings_menu);
    submenu_free(app->terms_menu);
    submenu_free(app->categories_menu);
    view_dispatcher_free(app->view_dispatcher);
    furi_string_free(app->definition_buffer);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t wikiflip_app(void* p) {
    UNUSED(p);

    WikiFlipApp* app = wikiflip_alloc();
    wikiflip_switch_to_view(app, WikiFlipViewCategories);
    view_dispatcher_run(app->view_dispatcher);
    wikiflip_free(app);

    return 0;
}
